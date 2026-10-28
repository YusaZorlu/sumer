#include "game.h"
#include <math.h>
#include <stdlib.h>

bool InsideWalls(float x, float z, float margin)
{
    float dx = x / (CITY_A - margin), dz = (z - CITY_Z) / (CITY_B - margin);
    return dx * dx + dz * dz < 1.0f;
}

// The city's lands: everything inside the ring of canals (the river closes it on the west). The
// city's guards keep the peace here and nowhere else.
bool InCityLands(float x, float z)
{
    return x > RIVER_X1 && x < 120.0f && z > -104.0f && z < 87.0f;
}

// Where each bridge's deck begins and ends (a little onto the banks), for people finding a way over.
static Vector3 bridgeEnds[8][2];
static int bridgeCount;

int BridgeCount(void)
{
    return bridgeCount;
}

void BridgeEnds(int i, Vector3 *a, Vector3 *b)
{
    *a = bridgeEnds[i][0];
    *b = bridgeEnds[i][1];
}

static void Bridge(float x, float z, bool alongX, float len)
{
    if (bridgeCount < 8) {
        float h = len / 2 + 1.0f;
        bridgeEnds[bridgeCount][0] = alongX ? (Vector3){ x - h, 0, z } : (Vector3){ x, 0, z - h };
        bridgeEnds[bridgeCount][1] = alongX ? (Vector3){ x + h, 0, z } : (Vector3){ x, 0, z + h };
        bridgeCount++;
    }
    if (alongX) {
        AddBox(x, 0, z, len, 0.3f, 3.2f, COL_WOOD, true);
        AddBox(x, 0.3f, z - 1.5f, len - 2.0f, 0.6f, 0.12f, COL_WOOD, true);
        AddBox(x, 0.3f, z + 1.5f, len - 2.0f, 0.6f, 0.12f, COL_WOOD, true);
        KeepOut(x - len / 2 - 2, z - 3, x + len / 2 + 2, z + 3);
    } else {
        AddBox(x, 0, z, 3.2f, 0.3f, len, COL_WOOD, true);
        AddBox(x - 1.5f, 0.3f, z, 0.12f, 0.6f, len - 2.0f, COL_WOOD, true);
        AddBox(x + 1.5f, 0.3f, z, 0.12f, 0.6f, len - 2.0f, COL_WOOD, true);
        KeepOut(x - 3, z - len / 2 - 2, x + 3, z + len / 2 + 2);
    }
}

static void Boat(float x, float z, bool alongX)
{
    float l = 3.6f, w = 1.0f, y = WATER_LEVEL;
    if (alongX) {
        AddBox(x, y - 0.12f, z, l, 0.36f, w, COL_STRAW, false);
        AddBox(x - l / 2, y, z, 0.35f, 0.55f, w * 0.7f, COL_STRAW, false);
        AddBox(x + l / 2, y, z, 0.35f, 0.55f, w * 0.7f, COL_STRAW, false);
    } else {
        AddBox(x, y - 0.12f, z, w, 0.36f, l, COL_STRAW, false);
        AddBox(x, y, z - l / 2, w * 0.7f, 0.55f, 0.35f, COL_STRAW, false);
        AddBox(x, y, z + l / 2, w * 0.7f, 0.55f, 0.35f, COL_STRAW, false);
    }
}

// Water fills every wet channel the terrain cuts. The river is broken only at the far north, where a
// shallow ford can be waded over stepping stones.
static void BuildRiver(void)
{
    for (int c = 0; c < ChannelCount(); c++) {
        bool wet;
        Rectangle r = ChannelRect(c, &wet, NULL);
        if (wet) AddWater(r.x, fmaxf(r.y, -WORLD_HALF), r.x + r.width, fminf(r.y + r.height, WORLD_HALF));
    }
    for (int k = 0; k < 14; k++) {
        float x = RIVER_X0 + 1.5f + Hash2(k, 3) * (RIVER_X1 - RIVER_X0 - 3), z = FORD_Z0 + 1 + Hash2(k, 9) * (FORD_Z1 - FORD_Z0 - 2);
        AddShape(x, TerrainHeight(x, z) - 0.12f, z, 0.9f + Hash2(k, 5), 0.5f, 0.8f + Hash2(k, 7), COL_STONE, false, SHAPE_BLOB);
    }
}

static void BuildWaterways(void)
{
    BuildRiver();

    Bridge(0, 89, false, 8);
    Bridge(122, 9, true, 8);
    Bridge(-37, -106, false, 8);
    Bridge(-114, -10, false, 14);

    // Brick quays round the harbor basin, down into the water.
    AddBox(-66.5f, -3.0f, -26.5f, 18.0f, 3.35f, 1.0f, COL_BRICK, true);
    AddBox(-66.5f, -3.0f, 2.5f, 18.0f, 3.35f, 1.0f, COL_BRICK, true);
    AddBox(-57.5f, -3.0f, -12.0f, 1.0f, 3.35f, 29.0f, COL_BRICK, true);
    Boat(-70, -20, true);
    Boat(-64, -16, true);
    Boat(-71, -3, true);
    Boat(-62, -2, false);
    KeepOut(-80, -29, -54, 5);
    KeepOut(-128, -17, -74, -3);
}

static bool GateGap(float x, float z)
{
    if (z > CITY_Z && fabsf(x) < 5.5f) return true;
    if (z < CITY_Z && x > -42.5f && x < -31.5f) return true;
    if (x > 0 && z > 4.0f && z < 14.0f) return true;
    if (x < 0 && z > 21.0f && z < 31.0f) return true;
    if (x < 0 && z > -16.0f && z < -4.0f) return true;
    return false;
}

// Axis-snapped so the ladder lies flat on one face of the block.
static void AddLadder(int i, const float *px, const float *pz, int n)
{
    if (G.ladderCount >= MAX_LADDERS) return;
    float x = px[i], z = pz[i];
    float gx = x / (CITY_A * CITY_A), gz = (z - CITY_Z) / (CITY_B * CITY_B);
    Vector3 in = fabsf(gx) * CITY_A > fabsf(gz) * CITY_B ? (Vector3){ gx > 0 ? -1.0f : 1.0f, 0, 0 } : (Vector3){ 0, 0, gz > 0 ? -1.0f : 1.0f };
    Vector3 t = { -in.z, 0, in.x };
    int a = (i + n - 1) % n, b = (i + 1) % n;
    float da = (px[a] - x) * in.x + (pz[a] - z) * in.z, db = (px[b] - x) * in.x + (pz[b] - z) * in.z;
    int low = da < db ? a : b;
    float side = ((px[low] - x) * t.x + (pz[low] - z) * t.z) > 0 ? 1.0f : -1.0f;

    Vector3 c = { x + t.x * 0.6f * side, WALL_TOP, z + t.z * 0.6f * side };
    Vector3 face = { c.x + in.x * 1.7f, 0, c.z + in.z * 1.7f };
    float rx = fabsf(t.x) * 0.62f + 0.06f, rz = fabsf(t.z) * 0.62f + 0.06f;

    for (int k = -1; k <= 1; k += 2)
        AddBox(face.x + in.x * 0.12f + t.x * 0.28f * (float)k, 0, face.z + in.z * 0.12f + t.z * 0.28f * (float)k, 0.08f, WALL_TOP + 0.7f, 0.08f, COL_WOOD, false);
    for (float y = 0.35f; y < WALL_TOP + 0.4f; y += 0.42f)
        AddBox(face.x + in.x * 0.12f, y, face.z + in.z * 0.12f, rx, 0.05f, rz, COL_PALM_TRUNK, false);

    Ladder *l = &G.ladders[G.ladderCount++];
    l->n = in;
    l->wall = c;
    l->top = WALL_TOP;
    l->base = (Vector3){ face.x + in.x * 0.8f, 0, face.z + in.z * 0.8f };
    l->base.y = TerrainHeight(l->base.x, l->base.z);
    KeepOut(l->base.x - 1.6f, l->base.z - 1.6f, l->base.x + 1.6f, l->base.z + 1.6f);
}

// The towers either side of each of the city's gates are hollow, and people go up them by stairs: a
// door from the city at the foot, stairs round the inside walls, a middle floor level with the wall
// walk (a door out onto it), and a roof with a parapet where an archer keeps watch. Each tower is square
// and set along the world's axes: t runs along the wall away from the gate, n into the city.
#define GT_HALF   3.5f
#define GT_WALL   0.6f
#define GT_MID    WALL_TOP
#define GT_TOP    10.0f
#define GT_STAIR  1.1f
#define GT_LOW    16   // steps from the ground to the middle floor
#define GT_HIGH   9    // and on to the roof
#define MAX_GATE_TOWERS 12

typedef struct { float cx, cz, tx, tz, nx, nz; } GateTower;
static GateTower gateTowers[MAX_GATE_TOWERS];
static int gateTowerCount;

static float GX(const GateTower *g, float t, float n) { return g->cx + g->tx * t + g->nx * n; }
static float GZ(const GateTower *g, float t, float n) { return g->cz + g->tz * t + g->nz * n; }

// A box given in the tower's own axes: centre (t, n), size st x sn, from y0 up h.
static void TBox(const GateTower *g, float t, float n, float y0, float st, float sn, float h, Color c, bool solid)
{
    AddBox(GX(g, t, n), y0, GZ(g, t, n), fabsf(g->tx) * st + fabsf(g->nx) * sn, h, fabsf(g->tz) * st + fabsf(g->nz) * sn, c, solid);
}

// The stairs run round the inside walls: a square landing step in each corner and six steps along each
// wall between, from the corner by the door up the far wall, along the outer one, back along the gate's
// side and over the door. Step k's middle (t, n), its half sizes and its top.
#define GT_RUN 0.6f
static void StairStep(int k, float *t, float *n, float *ht, float *hn, float *top)
{
    const float in = GT_HALF - GT_WALL - GT_STAIR / 2;
    static const float corner[5][2] = { { 1, 1 }, { 1, -1 }, { -1, -1 }, { -1, 1 }, { 1, 1 } };
    int side = k / 7, j = k % 7;
    float ct = corner[side][0] * in, cn = corner[side][1] * in;
    if (j == 0) {
        *t = ct;
        *n = cn;
        *ht = *hn = GT_STAIR / 2;
    } else {
        float dt = (corner[side + 1][0] - corner[side][0]) / 2, dn = (corner[side + 1][1] - corner[side][1]) / 2;
        float along = GT_STAIR / 2 + GT_RUN * ((float)j - 0.5f);
        *t = ct + dt * along;
        *n = cn + dn * along;
        *ht = dt != 0 ? GT_RUN / 2 : GT_STAIR / 2;
        *hn = dt != 0 ? GT_STAIR / 2 : GT_RUN / 2;
    }
    *top = k < GT_LOW ? GT_MID * (float)(k + 1) / GT_LOW : GT_MID + (GT_TOP - GT_MID) * (float)(k + 1 - GT_LOW) / GT_HIGH;
}

// A floor from y0 to y1 over the inside, in tiles, leaving open whatever lies over stairs a head below it.
static void TowerFloor(const GateTower *g, float y0, float y1, Color c)
{
    enum { TILES = 10 };
    const float inner = GT_HALF - GT_WALL, tile = 2 * inner / TILES;
    for (int j = 0; j < TILES; j++) {
        float n = -inner + tile * ((float)j + 0.5f);
        int run = -1;
        for (int i = 0; i <= TILES; i++) {
            bool open = i == TILES;
            float t = -inner + tile * ((float)i + 0.5f);
            for (int k = 0; k < GT_LOW + GT_HIGH && !open; k++) {
                float st, sn, ht, hn, top;
                StairStep(k, &st, &sn, &ht, &hn, &top);
                if (top > y0 - 2.2f && top < y1 - 0.01f && fabsf(t - st) < ht + tile / 2 - 0.05f && fabsf(n - sn) < hn + tile / 2 - 0.05f)
                    open = true;
            }
            if (!open && run < 0) run = i;
            if (open && run >= 0) {
                float a = -inner + tile * (float)run, b = -inner + tile * (float)i;
                TBox(g, (a + b) / 2, n, y0, b - a, tile, y1 - y0, c, true);
                run = -1;
            }
        }
    }
}

// One side of the tower: the wall at 'v' across, running along the other axis ('alongT': along t),
// with up to two openings, each { from, to, bottom, top } along the wall.
static void TowerSide(const GateTower *g, bool alongT, float v, const float (*holes)[4], int nh, Color c)
{
    float cuts[6] = { -GT_HALF, GT_HALF };
    int nc = 2;
    for (int h = 0; h < nh; h++) {
        cuts[nc++] = holes[h][0];
        cuts[nc++] = holes[h][1];
    }
    for (int a = 0; a < nc; a++)
        for (int b = a + 1; b < nc; b++)
            if (cuts[b] < cuts[a]) {
                float x = cuts[a];
                cuts[a] = cuts[b];
                cuts[b] = x;
            }
    for (int a = 0; a + 1 < nc; a++) {
        float u0 = cuts[a], u1 = cuts[a + 1], mid = (u0 + u1) / 2, y = 0;
        if (u1 - u0 < 0.01f) continue;
        for (int h = 0; h <= nh; h++) {
            // Up from y to the next opening over this stretch (or to the top).
            float lo = GT_TOP, hi = GT_TOP;
            for (int k = 0; k < nh; k++)
                if (mid > holes[k][0] && mid < holes[k][1] && holes[k][2] >= y - 0.01f && holes[k][2] < lo) {
                    lo = holes[k][2];
                    hi = holes[k][3];
                }
            if (lo > y + 0.01f) {
                if (alongT) TBox(g, mid, v, y, u1 - u0, GT_WALL, lo - y, c, true);
                else TBox(g, v, mid, y, GT_WALL, u1 - u0, lo - y, c, true);
            }
            y = hi;
            if (y >= GT_TOP) break;
        }
    }
}

static void BuildGateTower(GateTower *g)
{
    const float in = GT_HALF - GT_WALL, wv = GT_HALF - GT_WALL / 2;
    const Color wall = COL_HOUSE_DARK, floor = COL_BRICK;
    // The door from the city; a window above it.
    const float door[2][4] = { { -0.7f, 0.7f, 0.0f, 2.4f }, { -0.35f, 0.35f, GT_MID + 0.9f, GT_MID + 2.0f } };
    TowerSide(g, true, wv, door, 2, wall);
    // The door out onto the wall walk, on the far side from the gate.
    const float walk[1][4] = { { -0.75f, 0.75f, GT_MID, GT_MID + 2.4f } };
    TowerSide(g, false, wv, walk, 1, wall);
    // Arrow slits looking out of the city, and the gate's side.
    const float slits[2][4] = { { -1.4f, -1.15f, GT_MID + 0.7f, GT_MID + 2.0f }, { 1.15f, 1.4f, GT_MID + 0.7f, GT_MID + 2.0f } };
    TowerSide(g, true, -wv, slits, 2, wall);
    const float gate[1][4] = { { -0.15f, 0.15f, GT_MID + 0.7f, GT_MID + 2.0f } };
    TowerSide(g, false, -wv, gate, 1, wall);

    // Floors: the ground's bricks, the middle floor and the roof.
    TBox(g, 0, 0, 0, 2 * in, 2 * in, 0.04f, floor, false);
    TowerFloor(g, GT_MID - 0.3f, GT_MID, floor);
    TowerFloor(g, GT_TOP - 0.3f, GT_TOP, COL_WALL);
    // The stairs: solid mud brick up to the middle floor, then on it up to the roof.
    for (int k = 0; k < GT_LOW + GT_HIGH; k++) {
        float t, n, ht, hn, top;
        StairStep(k, &t, &n, &ht, &hn, &top);
        float base = k < GT_LOW ? 0.0f : GT_MID;
        TBox(g, t, n, base, 2 * ht, 2 * hn, top - base, k & 1 ? COL_WALL : COL_MUD_PALE, true);
    }
    // The roof's parapet and its merlons.
    for (int s = -1; s <= 1; s += 2) {
        TBox(g, (float)s * (GT_HALF - 0.25f), 0, GT_TOP, 0.5f, 2 * GT_HALF, 0.9f, wall, true);
        TBox(g, 0, (float)s * (GT_HALF - 0.25f), GT_TOP, 2 * GT_HALF - 1.0f, 0.5f, 0.9f, wall, true);
        for (int m = -1; m <= 1; m++) {
            TBox(g, (float)s * (GT_HALF - 0.25f), (float)m * 2.4f, GT_TOP + 0.9f, 0.55f, 0.8f, 0.5f, wall, false);
            TBox(g, (float)m * 2.4f, (float)s * (GT_HALF - 0.25f), GT_TOP + 0.9f, 0.8f, 0.55f, 0.5f, wall, false);
        }
    }
    // Life inside. At the foot: jars and sacks, a crate, a torch on the wall.
    AddShape(GX(g, -2.3f, 2.3f), 0, GZ(g, -2.3f, 2.3f), 0.55f, 0.75f, 0.55f, COL_POT, true, SHAPE_BLOB);
    AddShape(GX(g, -1.6f, 2.45f), 0, GZ(g, -1.6f, 2.45f), 0.45f, 0.6f, 0.45f, COL_POT, true, SHAPE_BLOB);
    AddShape(GX(g, -2.35f, -0.2f), 0, GZ(g, -2.35f, -0.2f), 0.6f, 0.5f, 0.6f, COL_STRAW, true, SHAPE_CYL);
    AddShape(GX(g, -2.35f, 0.5f), 0, GZ(g, -2.35f, 0.5f), 0.6f, 0.45f, 0.6f, COL_STRAW, true, SHAPE_CYL);
    TBox(g, 1.0f, -1.0f, 0, 0.8f, 0.8f, 0.7f, COL_WOOD, true);
    AddTorch(GX(g, -in + 0.1f, 1.2f), 2.4f, GZ(g, -in + 0.1f, 1.2f), false);
    // The middle floor: the guards' room, a table, a sleeping mat, spears against the wall, a torch.
    TBox(g, 0.8f, 1.0f, GT_MID, 0.9f, 0.7f, 0.75f, COL_WOOD, true);
    AddShape(GX(g, 0.8f, 1.0f), GT_MID + 0.75f, GZ(g, 0.8f, 1.0f), 0.25f, 0.3f, 0.25f, COL_POT, false, SHAPE_BLOB);
    TBox(g, -0.9f, 0.0f, GT_MID, 0.8f, 1.8f, 0.06f, COL_BLANKET, false);
    for (int k = 0; k < 3; k++) TBox(g, 1.7f + 0.3f * (float)k, in - 0.12f, GT_MID, 0.06f, 0.06f, 2.2f, COL_WOOD, false);
    TBox(g, 2.0f, in - 0.2f, GT_MID + 1.6f, 1.0f, 0.12f, 0.08f, COL_WOOD, false);
    AddTorch(GX(g, 1.0f, in - 0.1f), GT_MID + 2.1f, GZ(g, 1.0f, in - 0.1f), false);
    // The roof: a brazier.
    AddShape(GX(g, 2.2f, 2.2f), GT_TOP, GZ(g, 2.2f, 2.2f), 0.6f, 0.45f, 0.6f, COL_CLAY_DARK, true, SHAPE_CYL);
    AddFireLight(GX(g, 2.2f, 2.2f), GT_TOP + 0.7f, GZ(g, 2.2f, 2.2f));
}

// Where step k of tower i's stairs is (its middle, on its top).
Vector3 GateTowerStair(int i, int k)
{
    float t, n, ht, hn, top;
    StairStep(k, &t, &n, &ht, &hn, &top);
    return (Vector3){ GX(&gateTowers[i], t, n), top, GZ(&gateTowers[i], t, n) };
}

int GateTowerCount(void)
{
    return gateTowerCount;
}

// Tower i's middle on the ground, the way along the wall away from its gate, and the way into the city.
void GateTowerFrame(int i, Vector3 *centre, Vector3 *t, Vector3 *n)
{
    const GateTower *g = &gateTowers[i];
    *centre = (Vector3){ g->cx, 0, g->cz };
    *t = (Vector3){ g->tx, 0, g->tz };
    *n = (Vector3){ g->nx, 0, g->nz };
}

// Where tower i's people are: 0 the archer on the roof, 1 the guard at the table on the middle floor;
// 'face' which way they look.
Vector3 GateTowerPost(int i, int who, float *face)
{
    const GateTower *g = &gateTowers[i];
    if (who == 0) {
        *face = atan2f(-g->nx, -g->nz);
        return (Vector3){ GX(g, 1.0f, -1.6f), GT_TOP, GZ(g, 1.0f, -1.6f) };
    }
    *face = atan2f(-g->tx, -g->tz);
    return (Vector3){ GX(g, 1.6f, 1.0f), GT_MID, GZ(g, 1.6f, 1.0f) };
}

static void BuildWalls(void)
{
    enum { N = 240 };
    static const int ladders[] = { 15, 45, 75, 111, 159, 200, 225 };
    float px[N], pz[N];
    bool gap[N], skip[N];

    for (int i = 0; i < N; i++) {
        float a = 2.0f * PI * (float)i / (float)N;
        px[i] = CITY_A * cosf(a);
        pz[i] = CITY_Z + CITY_B * sinf(a);
        gap[i] = GateGap(px[i], pz[i]);
        skip[i] = false;
    }
    // A tower beside each gate, a little back from the gap so the way through stays as wide.
    gateTowerCount = 0;
    for (int i = 0; i < N && gateTowerCount < MAX_GATE_TOWERS; i++) {
        int before = (i + N - 1) % N, after = (i + 1) % N;
        if (gap[i] || (gap[before] == gap[after])) continue;
        int away = gap[before] ? after : before;
        float gx = px[i] / (CITY_A * CITY_A), gz = (pz[i] - CITY_Z) / (CITY_B * CITY_B);
        GateTower g = { 0 };
        if (fabsf(gx) * CITY_A > fabsf(gz) * CITY_B) {
            g.nx = gx > 0 ? -1.0f : 1.0f;
            g.tz = pz[away] > pz[i] ? 1.0f : -1.0f;
        } else {
            g.nz = gz > 0 ? -1.0f : 1.0f;
            g.tx = px[away] > px[i] ? 1.0f : -1.0f;
        }
        g.cx = px[i] + g.tx * 1.0f;
        g.cz = pz[i] + g.tz * 1.0f;
        for (int k = 0; k < N; k++) {
            float dt = (px[k] - g.cx) * g.tx + (pz[k] - g.cz) * g.tz, dn = (px[k] - g.cx) * g.nx + (pz[k] - g.cz) * g.nz;
            // A plain stretch of wall reaches 1.7 m either side of its point, a wall tower 2.6 m.
            if (fabsf(dt) < GT_HALF + (k % 12 == 0 ? 2.7f : 1.0f) && fabsf(dn) < 4.0f) skip[k] = true;
        }
        gateTowers[gateTowerCount++] = g;
    }
    for (int i = 0; i < N; i++) {
        if (gap[i] || skip[i]) continue;
        bool tower = i % 12 == 0;
        if (tower) {
            AddBox(px[i], 0, pz[i], 5.2f, WALL_TOP + 0.5f, 5.2f, COL_HOUSE_DARK, true);
            AddBox(px[i], WALL_TOP + 0.4f, pz[i], 5.5f, 0.12f, 5.5f, COL_WALL, false);
            for (int k = 0; k < 4; k++)
                AddBox(px[i] + ((k & 1) ? 2.2f : -2.2f), WALL_TOP + 0.5f, pz[i] + ((k & 2) ? 2.2f : -2.2f), 0.9f, 0.9f, 0.9f, COL_HOUSE_DARK, true);
        } else {
            AddBox(px[i], 0, pz[i], 3.4f, WALL_TOP, 3.4f, COL_WALL, true);
            float gx = px[i] / (CITY_A * CITY_A), gz = (pz[i] - CITY_Z) / (CITY_B * CITY_B), gl = sqrtf(gx * gx + gz * gz);
            AddBox(px[i] + gx / gl * 1.45f, WALL_TOP, pz[i] + gz / gl * 1.45f, 0.7f, 0.8f, 0.7f, COL_WALL, true);
        }
    }
    for (int k = 0; k < gateTowerCount; k++) {
        GateTower *g = &gateTowers[k];
        BuildGateTower(g);
        // The wall walk joined to the tower's door.
        TBox(g, GT_HALF + 1.2f, 0, 0, 2.6f, 3.4f, WALL_TOP, COL_WALL, true);
        KeepOut(fminf(GX(g, -GT_HALF, -GT_HALF), GX(g, GT_HALF, GT_HALF)) - 0.5f, fminf(GZ(g, -GT_HALF, -GT_HALF), GZ(g, GT_HALF, GT_HALF)) - 0.5f,
                fmaxf(GX(g, -GT_HALF, -GT_HALF), GX(g, GT_HALF, GT_HALF)) + 0.5f, fmaxf(GZ(g, -GT_HALF, -GT_HALF), GZ(g, GT_HALF, GT_HALF)) + 0.5f);
    }
    G.ladderCount = 0;
    for (int k = 0; k < (int)(sizeof(ladders) / sizeof(ladders[0])); k++)
        if (!gap[ladders[k]] && !skip[ladders[k]]) AddLadder(ladders[k], px, pz, N);
}

#define MAX_FIELDS 8
static int fieldFirst[MAX_FIELDS], fieldLast[MAX_FIELDS], fieldCount;

static void Field(float x0, float z0, float x1, float z1)
{
    const Color bank = { 112, 90, 64, 252 }, ditch = { 76, 60, 46, 252 };
    float cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
    AddBox(cx, 0, cz, x1 - x0, 0.1f, z1 - z0, COL_SOIL, false);
    AddBox(cx, 0, z0 - 0.4f, x1 - x0 + 1.6f, 0.22f, 0.8f, bank, false);
    AddBox(cx, 0, z1 + 0.4f, x1 - x0 + 1.6f, 0.22f, 0.8f, bank, false);
    AddBox(x0 - 0.4f, 0, cz, 0.8f, 0.22f, z1 - z0, bank, false);
    AddBox(x1 + 0.4f, 0, cz, 0.8f, 0.22f, z1 - z0, bank, false);
    AddBox(cx, 0, z0 - 1.3f, x1 - x0 + 1.6f, 0.05f, 1.0f, ditch, false);
    // the shaduf: a sweep on a post that lifts the ditch water in a skin bucket
    AddBox(x0 + 1.0f, 0, z0 - 1.9f, 0.18f, 2.2f, 0.18f, COL_WOOD, false);
    AddBox(x0 + 1.0f, 2.1f, z0 - 1.9f, 3.6f, 0.1f, 0.1f, COL_WOOD, false);
    AddShape(x0 - 0.6f, 1.95f, z0 - 1.9f, 0.45f, 0.35f, 0.45f, (Color){ 118, 96, 74, 255 }, false, SHAPE_BLOB);
    AddBox(x0 + 2.6f, 0.6f, z0 - 1.9f, 0.04f, 1.5f, 0.04f, (Color){ 150, 128, 92, 255 }, false);
    AddShape(x0 + 2.6f, 0.3f, z0 - 1.9f, 0.4f, 0.35f, 0.4f, (Color){ 120, 84, 54, 255 }, false, SHAPE_CYL);
    for (int k = 0; k < 3; k++)
        AddShape(x1 + 1.4f + 0.9f * (float)k, 0, z1 + 1.3f, 0.7f, 1.1f, 0.7f, TAG_CROP(COL_STRAW), false, SHAPE_CONE);
    if (fieldCount < MAX_FIELDS) fieldFirst[fieldCount] = G.boxCount;
    for (float z = z0 + 0.7f; z < z1 - 0.4f; z += 1.6f)
        AddBox((x0 + x1) / 2, 0.1f, z, x1 - x0 - 0.8f, 0.6f, 0.6f, TAG_CROP(COL_WHEAT_GREEN), false);
    if (fieldCount < MAX_FIELDS) fieldLast[fieldCount++] = G.boxCount;
    KeepOut(x0 - 2.0f, z0 - 2.5f, x1 + 3.6f, z1 + 2.0f);
}

// Each field runs a little ahead of or behind its neighbour.
void UpdateFields(void)
{
    for (int f = 0; f < fieldCount; f++) {
        float growth;
        bool stubble, bare = !CropStage(0.06f * (float)(f % 3) - 0.06f, &growth, &stubble);
        float h = bare ? 0.03f : stubble ? 0.14f : 0.1f + 0.75f * growth;
        Color c = bare ? (Color){ 84, 64, 44, 252 } : TAG_CROP(stubble ? COL_STRAW
                : growth >= 1.0f ? COL_WHEAT_RIPE : Mix(COL_WHEAT_GREEN, COL_WHEAT_RIPE, growth * 0.4f));
        for (int i = fieldFirst[f]; i < fieldLast[f]; i++) {
            G.boxes[i].size.y = h;
            G.boxes[i].pos.y = 0.1f + h / 2;
            G.boxes[i].color = c;
        }
    }
}

// The rows of crops grow all day, so the shaders' still meshes leave them out and they are drawn
// afresh every frame.
bool FieldBox(int i)
{
    for (int f = 0; f < fieldCount; f++)
        if (i >= fieldFirst[f] && i < fieldLast[f]) return true;
    return false;
}

void DrawFields(void)
{
    for (int f = 0; f < fieldCount; f++) {
        if (fieldLast[f] <= fieldFirst[f] || XZDist(G.boxes[fieldFirst[f]].pos, G.cam.position) > 260.0f ||
            !InView(G.boxes[(fieldFirst[f] + fieldLast[f]) / 2].pos, 36.0f)) continue;
        for (int i = fieldFirst[f]; i < fieldLast[f]; i++) DrawShadedBox(G.boxes[i].pos, G.boxes[i].size, G.boxes[i].color);
    }
}

void BuildCity(void)
{
    fieldCount = 0;
    bridgeCount = 0;
    BuildWaterways();
    BuildWalls();
    BuildTemple();

    KeepOut(-4.5f, -8, 4.5f, 82);
    KeepOut(-104, 6, 104, 12);
    KeepOut(-104, 23, -40, 29);
    KeepOut(-104, 37, 104, 41);
    KeepOut(-40, -94, -34, 6);

    AddBox(0, 0, 38, 7, 0.03f, 88, COL_ROAD, false);

    Field(-62, 96, -10, 136);
    Field(10, 96, 62, 136);
    Field(-84, -150, -44, -122);
    Field(-30, -150, 26, -122);
    Field(36, -150, 90, -122);
}

static void CourtyardHouse(float x, float z, float w, float d, float h, Color c)
{
    const float t = 0.6f;
    AddBox(x, 0, z - d / 2 + t / 2, w, h, t, c, true);
    AddBox(x, 0, z + d / 2 - t / 2, w, h, t, c, true);
    AddBox(x - w / 2 + t / 2, 0, z, t, h, d - 2 * t, c, true);
    AddBox(x + w / 2 - t / 2, 0, z, t, h, d - 2 * t, c, true);
    AddBox(x, 0, z, w - 2 * t, 0.03f, d - 2 * t, COL_BRICK, false);
}

// The houses built, for the ways up onto their roofs (HouseClimbs).
typedef struct { float x, z, w, d, h; int door, ix, iz; } HouseSpot;
#define MAX_HOUSE_SPOTS 700
static HouseSpot houseSpots[MAX_HOUSE_SPOTS];
static int houseSpotCount, climbCount;

int HouseClimbCount(void)
{
    return climbCount;
}

// Whether a strip of ground from (x0, z0) to (x1, z1) is clear for something 'r' wide: no street, water,
// building or wall.
static bool ClearStrip(float x0, float z0, float x1, float z1, float r)
{
    for (int k = 0; k <= 4; k++) {
        float x = x0 + (x1 - x0) * (float)k / 4, z = z0 + (z1 - z0) * (float)k / 4;
        if (!InsideWalls(x, z, 4.0f + r) || InKeepOut(x, z, r) || InWater(x, z) || SolidAt(x, z, r)) return false;
    }
    return true;
}

// Ways up onto the roofs, for whoever likes to run along them: mud-brick stairs up the side of some houses
// (as on many a house in Ur), and a crate by a pile of crates beside others. Added once every house
// stands, so a house built later never stands in their way.
static void HouseClimbs(void)
{
    climbCount = 0;
    for (int i = 0; i < houseSpotCount; i++) {
        const HouseSpot *hs = &houseSpots[i];
        float roll = Hash2(hs->ix + 211, hs->iz + 97);
        if (roll > 0.6f) continue;
        bool stairs = roll < 0.38f;
        int first = (int)(Hash2(hs->ix + 5, hs->iz + 151) * 3.99f);
        for (int k = 0; k < 4; k++) {
            int side = (first + k) % 4;
            if (side == hs->door) continue;
            // Along the wall on that side: 'ax', 'az' the way along it, 'ox', 'oz' out of the house.
            float ox = side == 0 ? -1.0f : side == 1 ? 1.0f : 0.0f, oz = side == 2 ? -1.0f : side == 3 ? 1.0f : 0.0f;
            float ax = oz != 0 ? 1.0f : 0.0f, az = ox != 0 ? 1.0f : 0.0f;
            float half = ox != 0 ? hs->w / 2 : hs->d / 2, len = (ox != 0 ? hs->d : hs->w) - 0.4f;
            float bx = hs->x + ox * (half + 0.5f), bz = hs->z + oz * (half + 0.5f);
            // The free ground is looked for a little off the wall, clear of the house itself.
            float fx = hs->x + ox * (half + 0.65f), fz = hs->z + oz * (half + 0.65f);
            if (stairs) {
                int n = (int)ceilf(hs->h / 0.45f);
                float step = len / (float)n;
                if (step < 0.42f || !ClearStrip(fx - ax * len / 2, fz - az * len / 2, fx + ax * len / 2, fz + az * len / 2, 0.45f)) continue;
                float dir = Hash2(hs->ix + 3, hs->iz + 8) < 0.5f ? -1.0f : 1.0f;
                for (int j = 0; j < n; j++) {
                    float along = dir * (-len / 2 + step * ((float)j + 0.5f)), top = hs->h * (float)(j + 1) / (float)n;
                    AddBox(bx + ax * along, 0, bz + az * along, ax != 0 ? step + 0.02f : 0.95f, top, az != 0 ? step + 0.02f : 0.95f,
                           j & 1 ? COL_MUD_PALE : Mix(COL_MUD_PALE, COL_HOUSE_DARK, 0.3f), true);
                }
            } else {
                // A crate to step on, and a pile of them a metre higher: up on the roof from the pile.
                if (hs->h > 3.2f || !ClearStrip(fx + ox * 0.1f - ax * 1.2f, fz + oz * 0.1f - az * 1.2f, fx + ox * 0.1f + ax * 1.2f, fz + oz * 0.1f + az * 1.2f, 0.5f))
                    continue;
                bx += ox * 0.1f;
                bz += oz * 0.1f;
                AddBox(bx - ax * 0.65f, 0, bz - az * 0.65f, 1.0f, 1.0f, 1.0f, COL_WOOD, true);
                AddBox(bx + ax * 0.5f, 0, bz + az * 0.5f, 1.1f, 2.0f, 1.1f, Mix(COL_WOOD, COL_STRAW, 0.3f), true);
                AddShape(bx + ax * 0.5f, 2.0f, bz + az * 0.5f, 0.5f, 0.5f, 0.5f, COL_POT, false, SHAPE_BLOB);
            }
            climbCount++;
            break;
        }
    }
}

// Fills every free cell inside the walls; a cell that touches a street tries a smaller, shifted house first.
void BuildCityHouses(void)
{
    houseSpotCount = 0;
    const float cell = 7.0f;
    const Color tones[4] = { COL_HOUSE, COL_HOUSE_DARK, COL_WALL, COL_MUD_PALE };
    static const float tries[5][3] = { { 0, 0, 1.0f }, { 0, 2.0f, 0.8f }, { 0, -2.0f, 0.8f }, { 2.0f, 0, 0.8f }, { -2.0f, 0, 0.8f } };
    int ix = 0;

    for (float x = -CITY_A; x <= CITY_A; x += cell, ix++) {
        int iz = 0;
        for (float z = CITY_Z - CITY_B; z <= CITY_Z + CITY_B; z += cell, iz++) {
            if (Hash2(ix + 101, iz + 57) < 0.06f) continue;

            float bw = 4.4f + 2.0f * Hash2(ix * 7 + 1, iz * 13 + 2);
            float bd = 4.4f + 2.0f * Hash2(ix + 31, iz + 17);
            float jx = (Hash2(ix * 3 + 5, iz * 5 + 9) - 0.5f) * fmaxf(0.0f, cell - bw - 0.8f);
            float jz = (Hash2(iz * 11 + 3, ix * 2 + 7) - 0.5f) * fmaxf(0.0f, cell - bd - 0.8f);
            float cx = 0, cz = 0, w = 0, d = 0;
            bool ok = false;

            for (int t = 0; t < 5 && !ok; t++) {
                w = bw * tries[t][2];
                d = bd * tries[t][2];
                cx = x + jx + tries[t][0];
                cz = z + jz + tries[t][1];
                float half = fmaxf(w, d) / 2;
                ok = InsideWalls(cx, cz, 3.2f + half) && !InKeepOut(cx, cz, half + 0.5f) && !InWater(cx, cz);
            }
            if (!ok) continue;

            float h = 2.8f + 1.8f * Hash2(ix + 7, iz + 3);
            Color c = tones[(int)(Hash2(ix + 19, iz + 23) * 3.99f)];
            bool court = w > 5.6f && d > 5.6f && Hash2(ix + 41, iz + 43) < 0.3f;
            if (court) CourtyardHouse(cx, cz, w, d, h, c);
            else AddBox(cx, 0, cz, w, h, d, c, true);
            Color trim = Mix(c, COL_MUD_DARK, 0.35f), base = Mix(c, COL_MUD_DARK, 0.5f);
            if (court) {
                for (int k = -1; k <= 1; k += 2) {
                    AddBox(cx, h - 0.06f, cz + (float)k * (d / 2 - 0.3f), w + 0.16f, 0.2f, 0.76f, trim, false);
                    AddBox(cx + (float)k * (w / 2 - 0.3f), h - 0.06f, cz, 0.76f, 0.2f, d - 0.6f, trim, false);
                }
            } else {
                AddBox(cx, h - 0.06f, cz, w + 0.16f, 0.2f, d + 0.16f, trim, false);
                AddBox(cx, 0, cz, w + 0.1f, 0.35f, d + 0.1f, base, false);
            }

            // A room up on the roof, and a crate beside it to climb onto it from the roof.
            if (Hash2(ix + 13, iz + 29) < 0.4f) {
                float ox = (Hash2(ix, iz + 61) < 0.5f ? -1.0f : 1.0f) * w / 4;
                AddBox(cx + ox, h, cz - d / 4, w / 2, 2.0f, d / 2, tones[(int)(Hash2(ix + 3, iz + 71) * 3.99f)], !court);
                if (!court) AddBox(cx + ox, h, cz + 0.5f, 0.9f, 1.0f, 0.9f, COL_WOOD, true);
            }
            int side = (int)(Hash2(ix + 77, iz + 5) * 3.99f);
            if (!court && houseSpotCount < MAX_HOUSE_SPOTS) houseSpots[houseSpotCount++] = (HouseSpot){ cx, cz, w, d, h, side, ix, iz };
            float dx = (side == 0) ? -w / 2 : (side == 1) ? w / 2 : 0.0f;
            float dz = (side == 2) ? -d / 2 : (side == 3) ? d / 2 : 0.0f;
            AddBox(cx + dx, 0, cz + dz, dx != 0 ? 0.12f : 1.0f, 2.0f, dz != 0 ? 0.12f : 1.0f, COL_DOORWAY, false);
            AddBox(cx + dx, 2.0f, cz + dz, dx != 0 ? 0.18f : 1.3f, 0.16f, dz != 0 ? 0.18f : 1.3f, COL_WOOD, false);
            for (int k = -1; k <= 1; k++) {
                float along = 0.9f * (float)k;
                AddBox(cx + dx * 1.04f + (dz != 0 ? along : 0), h - 0.42f, cz + dz * 1.04f + (dx != 0 ? along : 0), 0.13f, 0.13f, 0.13f, COL_PALM_TRUNK, false);
            }
            if (!court && h > 3.4f) {
                int wside = (side + 1 + (int)(Hash2(ix + 9, iz + 31) * 2.99f)) % 4;
                float wx = (wside == 0) ? -w / 2 : (wside == 1) ? w / 2 : 0.0f;
                float wz = (wside == 2) ? -d / 2 : (wside == 3) ? d / 2 : 0.0f;
                AddBox(cx + wx, h - 1.3f, cz + wz, wx != 0 ? 0.1f : 0.55f, 0.4f, wz != 0 ? 0.1f : 0.55f, COL_DOORWAY, false);
            }
            // Life at the door and on the roof: a cloth awning, jars and baskets, a reed shade up top.
            float ox = (float)((dx > 0) - (dx < 0)), oz = (float)((dz > 0) - (dz < 0));   // out of the door
            if (Hash2(ix + 61, iz + 7) < 0.35f) {
                static const Color cloths[4] = { { 222, 210, 180, 255 }, { 160, 70, 52, 255 }, { 70, 92, 130, 255 }, { 190, 150, 80, 255 } };
                Color cl = cloths[(int)(Hash2(ix + 2, iz + 66) * 3.99f)];
                AddBox(cx + dx + ox * 0.6f, 2.35f, cz + dz + oz * 0.6f, ox != 0 ? 1.2f : 2.0f, 0.05f, oz != 0 ? 1.2f : 2.0f, cl, false);
                for (int k = -1; k <= 1; k += 2)
                    AddBox(cx + dx + ox * 1.15f + oz * 0.95f * (float)k, 0, cz + dz + oz * 1.15f + ox * 0.95f * (float)k, 0.08f, 2.4f, 0.08f, COL_WOOD, false);
            }
            int jars = (int)(Hash2(ix + 33, iz + 12) * 3.99f) - 1;
            for (int k = 0; k < jars; k++) {
                float along = (k == 0 ? -1.0f : 1.0f) * (0.95f + 0.35f * (float)(k / 2)), out = 0.35f;
                float px = cx + dx + ox * out + oz * along, pz = cz + dz + oz * out + ox * along;
                if (Hash2(ix + k, iz + 44) < 0.6f) AddShape(px, -0.05f, pz, 0.48f, 0.62f, 0.48f, COL_POT, false, SHAPE_BLOB);
                else AddShape(px, 0, pz, 0.55f, 0.42f, 0.55f, COL_STRAW, false, SHAPE_CYL);
            }
            if (!court && Hash2(ix + 71, iz + 19) < 0.3f) {
                float sx = cx - (Hash2(ix, iz + 61) < 0.5f ? -1.0f : 1.0f) * w / 4, sz = cz + d / 4;
                for (int k = 0; k < 4; k++) AddBox(sx + ((k & 1) ? 0.9f : -0.9f), h, sz + ((k & 2) ? 0.9f : -0.9f), 0.08f, 1.6f, 0.08f, COL_WOOD, false);
                AddBox(sx, h + 1.6f, sz, 2.1f, 0.08f, 2.1f, COL_STRAW, false);
            }
            if (Hash2(ix + 5, iz + 91) < 0.16f) {
                float nx = (dx > 0) - (dx < 0), nz = (dz > 0) - (dz < 0);
                AddTorch(cx + dx + nx * 0.2f + nz * 0.9f, 2.3f, cz + dz + nz * 0.2f + nx * 0.9f, false);
            }
        }
    }
    HouseClimbs();
}
