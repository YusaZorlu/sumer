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
#define PLANS      3         // searches a frame, for everyone together
#define DROP       1.3f      // the deepest step down taken

typedef struct { int ix, iz, lvl, parent; float y, g, h; bool open, shut, blocked, wet; } Node;
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

// Where someone standing at height y comes to by stepping onto (x, z): the height there, or NAN if they
// cannot (a wall in the way, a climb or a drop too big, deep wide water). Narrow water counts, as wet.
static float StepTo(float x, float z, float y, bool *wet)
{
    float ny = GroundHeight(x, z, y);
    *wet = false;
    if (ny < WATER_LEVEL + 0.2f && InWater(x, z)) {
        if (WideWater(x, z)) return NAN;
        *wet = true;   // jumped across: the canal's bed below its banks is no drop
        ny = fmaxf(ny, WATER_LEVEL);
    } else if (y - ny > DROP) {
        return NAN;
    }
    int n;
    const int *near = BoxesNear(x - 0.3f, z - 0.3f, x + 0.3f, z + 0.3f, &n);
    for (int k = 0; k < n; k++) {
        const Box *b = &G.boxes[near[k]];
        if (b->water || !IsBlocking(b, ny)) continue;
        if (fabsf(x - b->pos.x) <= b->size.x / 2 + 0.3f && fabsf(z - b->pos.z) <= b->size.z / 2 + 0.3f) return NAN;
    }
    return ny;
}

// Whether someone at a can walk straight to b (every half metre a step they can take; narrow water is
// jumped). Long lines are only looked along their first 40 m.
bool PathClear(Vector3 a, Vector3 b)
{
    float d = XZDist(a, b), len = fminf(d, 40.0f), y = a.y;
    for (float s = 0.5f; s < len; s += 0.5f) {
        bool wet;
        float t = s / d;
        y = StepTo(Lerp(a.x, b.x, t), Lerp(a.z, b.z, t), y, &wet);
        if (isnan(y)) return false;
    }
    return true;
}

static int Level(float y)
{
    return (int)floorf(y / 2.5f);
}

static unsigned Hash(int ix, int iz, int lvl)
{
    return ((unsigned)ix * 73856093u ^ (unsigned)iz * 19349663u ^ (unsigned)lvl * 83492791u) & (HASH_N - 1);
}

// The cell (ix, iz) as reached from height y: found again, or looked at for the first time; -1 when full.
static int Cell(int ix, int iz, float y)
{
    int lvl = Level(y);   // the level it is reached from: the ground floor and a terrace above are two cells
    unsigned h = Hash(ix, iz, lvl);
    for (; hash[h]; h = (h + 1) & (HASH_N - 1)) {
        Node *n = &nodes[hash[h] - 1];
        if (n->ix == ix && n->iz == iz && n->lvl == lvl) return hash[h] - 1;
    }
    if (seen >= MAX_SEEN) return -1;
    Node *n = &nodes[seen];
    bool wet;
    float ny = StepTo(((float)ix + 0.5f) * CELL_M, ((float)iz + 0.5f) * CELL_M, y, &wet);
    *n = (Node){ ix, iz, lvl, -1, isnan(ny) ? y : ny, 1e9f, 0, false, false, isnan(ny), wet };
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
    int start = Cell((int)floorf(from.x / CELL_M), (int)floorf(from.z / CELL_M), from.y);
    nodes[start].blocked = false;   // wherever it stands, it can leave from there
    nodes[start].y = from.y;
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
        for (int k = 0; k < 8; k++) {
            int c = Cell(n->ix + DX[k], n->iz + DZ[k], n->y);
            if (c < 0) break;
            Node *m = &nodes[c];
            if (m->blocked || m->shut) continue;
            if (k >= 4) {   // no cutting a corner past a wall
                int a = Cell(n->ix + DX[k], n->iz, n->y), b = Cell(n->ix, n->iz + DZ[k], n->y);
                if (a < 0 || b < 0 || nodes[a].blocked || nodes[b].blocked) continue;
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
    static Vector3 way[MAX_SEEN];
    int count = 0, ddx = 9, ddz = 9;
    for (int i = end; i >= 0; i = nodes[i].parent) {
        int p = nodes[i].parent, sx = p >= 0 ? nodes[p].ix - nodes[i].ix : 0, sz = p >= 0 ? nodes[p].iz - nodes[i].iz : 0;
        if (sx != ddx || sz != ddz || p < 0)
            way[count++] = (Vector3){ ((float)nodes[i].ix + 0.5f) * CELL_M, nodes[i].y, ((float)nodes[i].iz + 0.5f) * CELL_M };
        ddx = sx;
        ddz = sz;
    }
    if (end != best || XZDist(way[0], to) < 1.5f) way[0] = (Vector3){ to.x, way[0].y, to.z };
    int n = 0, at = count - 1;
    Vector3 here = from;
    while (at > 0 && n < max) {
        int far = at - 1;
        while (far > 0 && at - far < 12 && PathClear(here, way[far - 1])) far--;
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
    while (v->pathAt < v->pathN && XZDist(v->at, v->path[v->pathAt]) < 0.7f) v->pathAt++;
    if (v->pathAt >= v->pathN) {
        v->pathN = 0;
        return goal;
    }
    return v->path[v->pathAt];
}
