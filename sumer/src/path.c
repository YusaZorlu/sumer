// Finding a way: when something stands between a person and where they are going (a house, a wall, the
// ziggurat, the river), A* on a 1 m grid laid over the ground as it is: up stairs and terraces, over
// bridges, across canals narrow enough to jump, round everything else. Searched only when the straight
// line is blocked, a few times a frame at most, and only so far each time: if the goal is beyond that,
// the way leads to the place nearest it that was found, and is looked for again from there.
#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CELL_M     1.0f
#define MAX_OPEN   4000      // cells taken off the open list in one search, at most
#define MAX_SEEN   16384     // cells looked at in one search
#define HASH_N     32768     // a power of two, twice MAX_SEEN
#define PLANS      2         // searches a frame, for everyone together (each at most ~2 ms)
#define DROP       1.3f      // the deepest step down taken

typedef struct { int ix, iz, lvl, parent; float x, z, y, g, h; bool open, shut, blocked, wet; } Node;
static Node nodes[MAX_SEEN];
static int seen, hash[HASH_N], heap[MAX_SEEN], heapN, plansLeft = PLANS;

void PathNewFrame(void)
{
    plansLeft = PLANS;
}

// Water too wide to jump (the river, the harbor, the ponds): everything wet but the narrow canals.
static bool WideWater(float x, float z)
{
    for (int c = 0; c < ChannelCount(); c++) {
        bool wet;
        Rectangle r = ChannelRect(c, &wet, NULL);
        if (wet && fminf(r.width, r.height) <= 6.0f && x >= r.x - 0.6f && x <= r.x + r.width + 0.6f && z >= r.y - 0.6f && z <= r.y + r.height + 0.6f)
            return false;
    }
    return true;
}

// Whether something solid stands at (x, z) for someone at height y, within 'r'.
static bool Walled(float x, float z, float y, float r)
{
    int n;
    const int *near = BoxesNear(x - r, z - r, x + r, z + r, &n);
    for (int k = 0; k < n; k++) {
        const Box *b = &G.boxes[near[k]];
        if (b->water || !IsBlocking(b, y)) continue;
        if (fabsf(x - b->pos.x) <= b->size.x / 2 + r && fabsf(z - b->pos.z) <= b->size.z / 2 + r) return true;
    }
    return false;
}

// The height someone at height y stands on at (x, z) (as GroundHeight), and whether something solid
// stands within 'r' of them there: one look at the boxes about for both.
static float Footing(float x, float z, float y, float r, bool *walled, bool *onBox)
{
    float h = TerrainHeight(x, z);
    *onBox = false;
    int n;
    const int *near = BoxesNear(x - r, z - r, x + r, z + r, &n);
    for (int k = 0; k < n; k++) {
        const Box *b = &G.boxes[near[k]];
        if (!b->solid || b->water) continue;
        float top = b->pos.y + b->size.y / 2;
        if (top > h && top <= y + STEP_HEIGHT && fabsf(x - b->pos.x) <= b->size.x / 2 && fabsf(z - b->pos.z) <= b->size.z / 2) {
            h = top;
            *onBox = true;
        }
    }
    *walled = false;
    for (int k = 0; k < n && !*walled; k++) {
        const Box *b = &G.boxes[near[k]];
        *walled = !b->water && IsBlocking(b, h) && fabsf(x - b->pos.x) <= b->size.x / 2 + r && fabsf(z - b->pos.z) <= b->size.z / 2 + r;
    }
    return h;
}

// Where someone standing at height y at (px, pz) comes to by stepping onto (x, z): the height there, or
// NAN if they cannot (a wall in the way, a climb or a drop too big, deep wide water). Narrow water counts,
// as wet. The way there is taken in quarter steps, so stairs whose steps are shorter than a cell are
// climbed (and come down) one step at a time.
static float StepTo(float px, float pz, float x, float z, float y, bool fromBox, bool *wet)
{
    // Bare ground (most of the world) rises and falls smoothly: no need to go in quarter steps.
    bool walled, onBox;
    float flat = Footing(x, z, y, 0.3f, &walled, &onBox);
    *wet = false;
    if (!onBox && !fromBox && flat - y <= STEP_HEIGHT && y - flat <= DROP && flat > WATER_LEVEL + 0.2f && !walled &&
        !Walled(Lerp(px, x, 0.5f), Lerp(pz, z, 0.5f), y, 0.25f))
        return flat;
    for (int k = 1; k < 4; k++) {
        float sx = Lerp(px, x, (float)k / 4), sz = Lerp(pz, z, (float)k / 4), sy = Footing(sx, sz, y, 0.25f, &walled, &onBox);
        if (sy < WATER_LEVEL + 0.2f && InWater(sx, sz)) continue;   // a canal on the way is jumped
        if (y - sy > DROP || walled) return NAN;
        y = sy;
    }
    float ny = Footing(x, z, y, 0.3f, &walled, &onBox);
    if (ny < WATER_LEVEL + 0.2f && InWater(x, z)) {
        if (WideWater(x, z)) return NAN;
        *wet = true;   // jumped across: the canal's bed below its banks is no drop
        ny = fmaxf(ny, WATER_LEVEL);
        walled = Walled(x, z, ny, 0.3f);
    } else if (y - ny > DROP) {
        return NAN;
    }
    return walled ? NAN : ny;
}

// Whether someone at a can walk straight to b (every half metre a step they can take; narrow water is
// jumped) and come to its height, not just under or over it. Long lines are only looked along their
// first 40 m.
bool PathClear(Vector3 a, Vector3 b)
{
    float d = XZDist(a, b), len = fminf(d, 40.0f), y = a.y, px = a.x, pz = a.z;
    for (float s = 0.5f; s < len; s += 0.5f) {
        bool wet;
        float t = s / d, x = Lerp(a.x, b.x, t), z = Lerp(a.z, b.z, t);
        y = StepTo(px, pz, x, z, y, y > TerrainHeight(px, pz) + 0.02f, &wet);
        if (isnan(y)) return false;
        px = x;
        pz = z;
    }
    if (d > 40.0f) return true;
    bool wet;
    y = d > 0.05f ? StepTo(px, pz, b.x, b.z, y, y > TerrainHeight(px, pz) + 0.02f, &wet) : y;
    return !isnan(y) && fabsf(y - b.y) < 1.2f;
}

static unsigned Hash(int ix, int iz, int lvl)
{
    return ((unsigned)ix * 73856093u ^ (unsigned)iz * 19349663u ^ (unsigned)lvl * 83492791u) & (HASH_N - 1);
}

// Where in cell (ix, iz) someone at (px, pz), height y, can stand: its middle, or failing that a point a
// quarter of a cell off it, so a way narrower than a cell (stairs along a wall) is still found. NAN if none.
static float Stand(int ix, int iz, float px, float pz, float y, bool fromBox, float *x, float *z, bool *wet)
{
    static const float SPOTS[13][2] = { { 0.5f, 0.5f }, { 0.25f, 0.5f }, { 0.75f, 0.5f }, { 0.5f, 0.25f }, { 0.5f, 0.75f },
                                        { 0.25f, 0.25f }, { 0.75f, 0.25f }, { 0.25f, 0.75f }, { 0.75f, 0.75f },
                                        { 0.1f, 0.5f }, { 0.9f, 0.5f }, { 0.5f, 0.1f }, { 0.5f, 0.9f } };
    for (int k = 0; k < 13; k++) {
        *x = ((float)ix + SPOTS[k][0]) * CELL_M;
        *z = ((float)iz + SPOTS[k][1]) * CELL_M;
        float ny = StepTo(px, pz, *x, *z, y, fromBox, wet);
        if (!isnan(ny)) return ny;
    }
    *x = ((float)ix + 0.5f) * CELL_M;
    *z = ((float)iz + 0.5f) * CELL_M;
    return NAN;
}

// The cell (ix, iz) as reached from (px, pz) at height y: found again, or looked at for the first time;
// -1 when full. A cell is known by the height stood on there, so the floor under stairs and the step
// over it in the same cell are two places.
static int Find(int ix, int iz, int lvl)
{
    for (unsigned h = Hash(ix, iz, lvl); hash[h]; h = (h + 1) & (HASH_N - 1)) {
        const Node *n = &nodes[hash[h] - 1];
        if (n->ix == ix && n->iz == iz && n->lvl == lvl) return hash[h] - 1;
    }
    return -1;
}

static int Cell(int ix, int iz, float px, float pz, float y, bool fromBox)
{
    int band = (int)floorf(y / 0.5f);
    // Known open, dry and at the very height it is come at from, both at the middles of their cells (a
    // wall between would have kept one of them off its middle): on level ground, nearly always.
    float mx = ((float)ix + 0.5f) * CELL_M, mz = ((float)iz + 0.5f) * CELL_M;
    bool central = fabsf(px - (floorf(px / CELL_M) + 0.5f) * CELL_M) < 0.01f && fabsf(pz - (floorf(pz / CELL_M) + 0.5f) * CELL_M) < 0.01f;
    for (int d = -1; d <= 1 && central; d++) {
        int i = Find(ix, iz, band + d);
        if (i >= 0 && !nodes[i].wet && nodes[i].x == mx && nodes[i].z == mz && fabsf(nodes[i].y - y) < 0.1f) return i;
    }
    float x, z;
    bool wet = false;
    float ny = Stand(ix, iz, px, pz, y, fromBox, &x, &z, &wet);
    int lvl = isnan(ny) ? -10000 : (int)floorf(ny / 0.5f);
    unsigned h = Hash(ix, iz, lvl);
    for (; hash[h]; h = (h + 1) & (HASH_N - 1)) {
        Node *n = &nodes[hash[h] - 1];
        if (n->ix == ix && n->iz == iz && n->lvl == lvl) return hash[h] - 1;
    }
    if (seen >= MAX_SEEN) return -1;
    Node *n = &nodes[seen];
    *n = (Node){ ix, iz, lvl, -1, x, z, isnan(ny) ? y : ny, 1e9f, 0, false, false, isnan(ny), wet };
    hash[h] = ++seen;
    return seen - 1;
}

static float F(int i)
{
    return nodes[i].g + nodes[i].h;
}

static void Push(int i)
{
    int k = heapN++;
    while (k > 0 && F(heap[(k - 1) / 2]) > F(i)) {
        heap[k] = heap[(k - 1) / 2];
        k = (k - 1) / 2;
    }
    heap[k] = i;
}

static int Pop(void)
{
    int top = heap[0], last = heap[--heapN], k = 0;
    for (;;) {
        int c = 2 * k + 1;
        if (c >= heapN) break;
        if (c + 1 < heapN && F(heap[c + 1]) < F(heap[c])) c++;
        if (F(last) <= F(heap[c])) break;
        heap[k] = heap[c];
        k = c;
    }
    if (heapN > 0) heap[k] = last;
    return top;
}

static float Guess(int ix, int iz, int gx, int gz)
{
    float dx = fabsf((float)(ix - gx)), dz = fabsf((float)(iz - gz));
    return (dx + dz - 0.586f * fminf(dx, dz)) * CELL_M;
}

// Whether a point is up on something built (stairs, a roof, a floor) rather than on the ground.
static bool Built(float x, float y, float z)
{
    return y > TerrainHeight(x, z) + 0.1f;
}

// A way from 'from' to 'to' as up to 'max' points to walk between (the last is the goal, or the place
// nearest it that the search came to); 0 if there is none, -1 if no search is left this frame.
int FindPath(Vector3 from, Vector3 to, Vector3 *out, int max)
{
    static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DZ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    if (plansLeft <= 0) return -1;
    plansLeft--;
    memset(hash, 0, sizeof(hash));
    seen = heapN = 0;
    int gx = (int)floorf(to.x / CELL_M), gz = (int)floorf(to.z / CELL_M);
    int start = Cell((int)floorf(from.x / CELL_M), (int)floorf(from.z / CELL_M), from.x, from.z, from.y, from.y > TerrainHeight(from.x, from.z) + 0.02f);
    nodes[start].blocked = false;   // wherever it stands, it can leave from there
    nodes[start].y = from.y;
    nodes[start].x = from.x;
    nodes[start].z = from.z;
    nodes[start].g = 0;
    nodes[start].h = Guess(nodes[start].ix, nodes[start].iz, gx, gz);
    nodes[start].open = true;
    Push(start);
    int best = start, end = -1;
    for (int taken = 0; heapN > 0 && taken < MAX_OPEN; taken++) {
        int i = Pop();
        Node *n = &nodes[i];
        if (n->shut) continue;
        n->shut = true;
        if (n->h < nodes[best].h) best = i;
        if (abs(n->ix - gx) <= 1 && abs(n->iz - gz) <= 1 && fabsf(n->y - to.y) < 1.6f) {
            end = i;
            break;
        }
        float nx = n->x, nz = n->z;
        bool on = n->y > TerrainHeight(nx, nz) + 0.02f;   // on something built: steps are looked at closely
        int side[4];   // the four cells beside it, which also say whether a corner may be cut
        for (int k = 0; k < 8; k++) {
            int c = Cell(n->ix + DX[k], n->iz + DZ[k], nx, nz, n->y, on);
            if (c < 0) break;
            if (k < 4) side[k] = c;
            Node *m = &nodes[c];
            if (m->blocked || m->shut) continue;
            if (k >= 4) {   // no cutting a corner past a wall: the cells beside it on the way are open
                int a = side[DX[k] > 0 ? 0 : 1], b = side[DZ[k] > 0 ? 2 : 3];
                if (nodes[a].blocked || nodes[b].blocked) continue;
            }
            float g = n->g + (k >= 4 ? 1.414f : 1.0f) * CELL_M + (m->wet ? 1.0f : 0.0f) + fabsf(m->y - n->y) * 0.5f;
            if (g >= m->g) continue;
            m->g = g;
            m->h = Guess(m->ix, m->iz, gx, gz);
            m->parent = i;
            Push(c);
        }
    }
    if (end < 0) end = best;
    if (end == start) return 0;

    // Back from the end to the start keeping only the corners (where the way turns), then of those only
    // the ones worth walking to: from each point kept, on to the farthest corner in a straight line from it.
    // On stairs and roofs every cell's point is kept and none passed by: a straight line there runs along
    // an edge, and a step off it is a fall.
    static Vector3 way[MAX_SEEN];
    int count = 0, ddx = 9, ddz = 9;
    for (int i = end; i >= 0; i = nodes[i].parent) {
        int p = nodes[i].parent, sx = p >= 0 ? nodes[p].ix - nodes[i].ix : 0, sz = p >= 0 ? nodes[p].iz - nodes[i].iz : 0;
        if (sx != ddx || sz != ddz || p < 0 || Built(nodes[i].x, nodes[i].y, nodes[i].z))
            way[count++] = (Vector3){ nodes[i].x, nodes[i].y, nodes[i].z };
        ddx = sx;
        ddz = sz;
    }
    if (end != best || XZDist(way[0], to) < 1.5f) way[0] = (Vector3){ to.x, way[0].y, to.z };
    int n = 0, at = count - 1;
    Vector3 here = from;
    while (at > 0 && n < max) {
        int far = at - 1;
        while (far > 0 && at - far < 12 && !Built(here.x, here.y, here.z) && !Built(way[far - 1].x, way[far - 1].y, way[far - 1].z) &&
               !Built(way[far].x, way[far].y, way[far].z) && PathClear(here, way[far - 1]))
            far--;
        out[n++] = way[far];
        here = way[far];
        at = far;
    }
    return n;
}

// Where someone heading for 'goal' should make for now: the goal itself while the way to it is clear,
// else the next corner of a way found round what is in between.
Vector3 PathWay(Vitals *v, Vector3 goal, float dt)
{
    v->lineT -= dt;
    v->pathT -= dt;
    if (v->lineT <= 0) {
        v->lineT = 0.5f;
        v->lineClear = PathClear(v->at, goal);
    }
    if (v->lineClear) {
        v->pathN = 0;
        return goal;
    }
    bool moved = XZDist(goal, v->pathGoal) > 2.0f + 0.15f * XZDist(v->at, goal);
    if (moved || v->pathT <= 0) {
        int n = FindPath(v->at, goal, v->path, PATH_MAX);
        if (n >= 0) {   // (-1: no search left this frame; the old way, if any, serves till the next)
            v->pathN = (unsigned char)n;
            v->pathAt = 0;
            v->pathGoal = goal;
            v->pathT = n > 0 ? 3.0f : 1.0f;
        }
    }
    // A corner is passed when it is reached at its height; come off the way (fallen from stairs to the
    // floor under them), a new one is looked for.
    // On stairs or a roof, or at their foot, a corner is followed closely: cut, the way would step off an
    // edge or meet the stairs from the side.
    while (v->pathAt < v->pathN) {
        Vector3 c = v->path[v->pathAt], next = v->pathAt + 1 < v->pathN ? v->path[v->pathAt + 1] : c;
        bool built = c.y > TerrainHeight(c.x, c.z) + 0.1f || next.y > TerrainHeight(next.x, next.z) + 0.1f;
        float near = built ? 0.25f : 0.7f;
        if (XZDist(v->at, c) >= near || fabsf(v->at.y - c.y) >= 1.0f) break;
        v->pathAt++;
    }
    if (v->pathAt < v->pathN && XZDist(v->at, v->path[v->pathAt]) < 1.2f && fabsf(v->at.y - v->path[v->pathAt].y) >= 1.0f) v->pathT = fminf(v->pathT, 0.0f);
    if (v->pathAt >= v->pathN) {
        // A way that ran out short of the goal (only so many corners are kept): on at once.
        if (v->pathN > 0 && XZDist(v->at, goal) > 1.5f) v->pathT = 0;
        v->pathN = 0;
        return goal;
    }
    return v->path[v->pathAt];
}
