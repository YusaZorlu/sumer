#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <string.h>

typedef struct { float x, z, r, h; } Bump;

// The far eastern hills ring a flat basin where the hunters live.
static const Bump EAST_HILLS[] = {
    { 208,  -60, 32, 18 }, { 240,   15, 40, 22 }, { 198,   58, 26,  9 }, { 245, -150, 45, 24 },
    { 180, -150, 30, 10 }, { 230,  125, 35, 14 }, { 180,  195, 30,  8 }, { 255,  -60, 30, 12 },
    { 300, -250, 50, 26 }, { 310,   90, 45, 20 }, { 290,  280, 40, 16 }, { 330,  -30, 40, 18 },
    { 240, -300, 40, 12 }, { 260,  330, 35, 10 },
    { 400, -150, 45, 20 }, { 470, -270, 55, 24 }, { 400, -400, 50, 18 }, { 500, -470, 40, 14 },
    { 370,  200, 45, 16 }, { 480,  280, 50, 20 }, { 420,  420, 55, 22 }, { 320,  470, 45, 14 },
    { 505,  120, 40, 15 }, { 395,  -75, 28,  8 }, { 210, -430, 45, 12 }, { 310, -500, 40, 14 },
    { 190,  420, 40, 10 }, { 520,  -40, 30, 10 },
};
// Sheep country south-west of the city, where the herders live.
static const Bump WEST_DOWNS[] = {
    { -70, 211, 38, 7 }, { -35, 238, 28, 5 }, { -95, 166, 26, 4 }, { -60, 300, 40, 8 },
    { -95, 365, 34, 6 }, { -28, 395, 30, 5 }, { -82, 492, 40, 8 }, { -22, 478, 30, 4 },
};
// Green hills across the river; the hollow at (-330, 175) holds a pond.
static const Bump WEST_BANK[] = {
    { -300, -380, 60, 22 }, { -430, -300, 55, 18 }, { -240, -470, 45, 14 }, { -470, -470, 50, 16 },
    { -270, -160, 50, 12 }, { -420,  -90, 60, 17 }, { -500,   30, 45, 14 }, { -350,   60, 40,  9 },
    { -255,  110, 35,  8 }, { -470,  170, 50, 15 }, { -300,  300, 55, 19 }, { -450,  390, 60, 21 },
    { -235,  440, 45, 13 }, { -370,  480, 40, 12 }, { -205, -300, 30,  7 }, { -215,  250, 30,  6 },
    { -330,  175, 22, -4.5f },
};
#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static float Smooth(float e0, float e1, float v)
{
    float t = Clamp((v - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float SumBumps(const Bump *b, int n, float x, float z)
{
    float h = 0;
    for (int i = 0; i < n; i++) {
        float dx = x - b[i].x, dz = z - b[i].z;
        h += b[i].h * expf(-(dx * dx + dz * dz) / (b[i].r * b[i].r));
    }
    return h;
}

// Channels cut into the ground: the river (broken by the shallow ford in the far north), the canals
// round the city and the harbor. Each bank slopes from the land down to water a little below it.
typedef struct { float x0, z0, x1, z1, depth, inner, outer; bool wet; } Channel;

static const Channel CHANNELS[] = {
    { RIVER_X0, -WORLD_HALF - 40.0f, RIVER_X1, FORD_Z0, 2.2f, 6.5f, 2.5f, true },
    { RIVER_X0, FORD_Z1, RIVER_X1, WORLD_HALF + 40.0f, 2.2f, 6.5f, 2.5f, true },
    { RIVER_X0, FORD_Z0, RIVER_X1, FORD_Z1, 0.62f, 1.0f, 2.5f, false },
    { -128.0f, 87.0f, 124.0f, 91.0f, 1.4f, 1.8f, 1.0f, true },
    { 120.0f, -108.0f, 124.0f, 91.0f, 1.4f, 1.8f, 1.0f, true },
    { -128.0f, -108.0f, 124.0f, -104.0f, 1.4f, 1.8f, 1.0f, true },
    { -128.0f, -15.0f, -74.0f, -5.0f, 1.6f, 2.0f, 1.0f, true },
    { -75.0f, -26.0f, -58.0f, 2.0f, 1.8f, 1.9f, 0.8f, true },
};
#define CHANNEL_COUNT COUNT_OF(CHANNELS)

int ChannelCount(void)
{
    return CHANNEL_COUNT;
}

// The channel's water, and the surface drawn for it: a little wider, so the banks decide the shore.
Rectangle ChannelRect(int i, bool *wet, Rectangle *surface)
{
    const Channel *c = &CHANNELS[i];
    float m = c->outer * 0.8f;
    if (wet) *wet = c->wet;
    if (surface) *surface = (Rectangle){ c->x0 - m, c->z0 - m, c->x1 - c->x0 + 2 * m, c->z1 - c->z0 + 2 * m };
    return (Rectangle){ c->x0, c->z0, c->x1 - c->x0, c->z1 - c->z0 };
}

// How far the ground is lowered: nothing away from water, a channel's full depth well inside it.
static float Carve(float x, float z)
{
    float best = 0.0f;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        const Channel *c = &CHANNELS[i];
        if (x < c->x0 - c->outer || x > c->x1 + c->outer || z < c->z0 - c->outer || z > c->z1 + c->outer) continue;
        float dx = fmaxf(c->x0 - x, x - c->x1), dz = fmaxf(c->z0 - z, z - c->z1);
        float ox = fmaxf(dx, 0.0f), oz = fmaxf(dz, 0.0f);
        float d = (dx > 0 || dz > 0) ? sqrtf(ox * ox + oz * oz) : fmaxf(dx, dz);
        float cut = -c->depth * (1.0f - Smooth(-c->inner, c->outer, d));
        if (cut < best) best = cut;
    }
    return best;
}

bool WestBank(float x)
{
    return x < RIVER_X0;
}

// How far (m) the nearest water is: the river and the canals, 0 on them.
float WaterDistance(float x, float z)
{
    float best = 1e9f;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        const Channel *c = &CHANNELS[i];
        if (!c->wet) continue;
        float ox = fmaxf(fmaxf(c->x0 - x, x - c->x1), 0.0f), oz = fmaxf(fmaxf(c->z0 - z, z - c->z1), 0.0f);
        best = fminf(best, sqrtf(ox * ox + oz * oz));
    }
    return best;
}

// Smooth noise in 0..1 over the land, for patches tens of metres across.
static float Patches(float x, float z, float scale, int seed)
{
    float fx = x / scale, fz = z / scale;
    int ix = (int)floorf(fx), iz = (int)floorf(fz);
    float tx = fx - (float)ix, tz = fz - (float)iz;
    tx = tx * tx * (3.0f - 2.0f * tx);
    tz = tz * tz * (3.0f - 2.0f * tz);
    float a = Hash2(ix * 7 + seed, iz), b = Hash2(ix * 7 + 7 + seed, iz), c = Hash2(ix * 7 + seed, iz + 1), d = Hash2(ix * 7 + 7 + seed, iz + 1);
    return Lerp(Lerp(a, b, tx), Lerp(c, d, tx), tz);
}

// The open land's look at x, z on the city's side of the river: dusty, patchy earth with sparse grass, and
// green only where water comes (the river, the canals, the irrigated fields and the villages' pastures),
// sand and mud along the banks. 'lush' 0..1 is how green it is (it also decides where grass grows).
float Greenness(float x, float z)
{
    float wd = WaterDistance(x, z);
    float byWater = 1.0f - Clamp((wd - 3.0f) / 38.0f, 0.0f, 1.0f);
    return fmaxf(byWater * byWater, Fertility(x, z) * 0.75f * (x > RIVER_X1 + 40.0f ? 1.0f : 0.0f));
}

static float Land(float x, float z)
{
    float edge = Smooth(WORLD_HALF, WORLD_HALF - 20.0f, fabsf(x)) * Smooth(WORLD_HALF, WORLD_HALF - 20.0f, fabsf(z));
    float east = Smooth(142.0f, 168.0f, x);
    float west = Smooth(156.0f, 181.0f, z) * Smooth(-12.0f, -30.0f, x) * Smooth(-122.0f, -104.0f, x);
    float bank = Smooth(-168.0f, -200.0f, x);
    float h = 0;

    if (east > 0) {
        float roll = 1.5f * (sinf(x * 0.071f) * cosf(z * 0.053f) + 1.0f);
        h += east * (SumBumps(EAST_HILLS, COUNT_OF(EAST_HILLS), x, z) + 3.0f + roll);
    }
    if (west > 0) h += west * SumBumps(WEST_DOWNS, COUNT_OF(WEST_DOWNS), x, z);
    if (bank > 0) {
        float roll = 1.3f * (sinf(x * 0.047f) * cosf(z * 0.061f) + 1.0f) + 0.5f * (sinf(x * 0.13f + z * 0.09f) + 1.0f);
        h += bank * (SumBumps(WEST_BANK, COUNT_OF(WEST_BANK), x, z) + roll);
    }
    return h * edge;
}

float TerrainHeight(float x, float z)
{
    return Land(x, z) + Carve(x, z);
}

// The drawn ground: each tile is two triangles split along its (x0,z0)-(x1,z1) diagonal.
float MeshHeight(float x, float z)
{
    float fx = Clamp((x + WORLD_HALF) / TILE_SIZE, 0.0f, (float)GROUND_TILES - 0.001f);
    float fz = Clamp((z + WORLD_HALF) / TILE_SIZE, 0.0f, (float)GROUND_TILES - 0.001f);
    int i = (int)fx, j = (int)fz;
    float u = fx - (float)i, v = fz - (float)j;
    float h00 = G.heights[i][j], h01 = G.heights[i][j + 1], h10 = G.heights[i + 1][j], h11 = G.heights[i + 1][j + 1];

    if (v >= u) return h00 + v * (h01 - h00) + u * (h11 - h01);
    return h00 + u * (h10 - h00) + v * (h11 - h10);
}

float RayTerrain(Vector3 o, Vector3 dir, float maxT)
{
    const float step = 1.0f;
    float prev = 0.0f;

    for (float t = 0.5f; t < maxT; t += step) {
        Vector3 p = Vector3Add(o, Vector3Scale(dir, t));
        if (p.y < TerrainHeight(p.x, p.z)) {
            float lo = prev, hi = t;
            for (int k = 0; k < 6; k++) {
                float mid = (lo + hi) / 2;
                Vector3 m = Vector3Add(o, Vector3Scale(dir, mid));
                if (m.y < TerrainHeight(m.x, m.z)) hi = mid;
                else lo = mid;
            }
            return lo;
        }
        prev = t;
    }
    return maxT;
}

static float SpanDist(float v, float lo, float hi)
{
    return fmaxf(fmaxf(lo - v, v - hi), 0.0f);
}

float Fertility(float x, float z)
{
    float river = Clamp(1.0f - (x + 128.0f) / 70.0f, 0.0f, 1.0f);
    float k = sqrtf((x / CITY_A) * (x / CITY_A) + ((z - CITY_Z) / CITY_B) * ((z - CITY_Z) / CITY_B));
    float ring = Clamp(1.0f - fabsf(k - 1.3f) / 0.5f, 0.0f, 1.0f) * 0.9f;
    float road = Clamp(1.0f - SpanDist(x, -70.0f, 70.0f) / 30.0f, 0.0f, 1.0f) * Clamp(1.0f - SpanDist(z, 92.0f, 141.0f) / 25.0f, 0.0f, 1.0f);
    float pasture = Clamp(1.3f - Vector2Distance((Vector2){ x, z }, (Vector2){ HERDERS_X, HERDERS_Z }) / 100.0f, 0.0f, 1.0f) * 0.8f;
    float glade = Clamp(1.3f - Vector2Distance((Vector2){ x, z }, (Vector2){ HUNTERS_X, HUNTERS_Z }) / 110.0f, 0.0f, 1.0f) * 0.75f;
    return fmaxf(fmaxf(river, ring), fmaxf(road, fmaxf(pasture, glade)));
}

static Color vertexTint[GROUND_TILES + 1][GROUND_TILES + 1];
static float vertexCut[GROUND_TILES + 1][GROUND_TILES + 1];

// Tiles that banks run through are drawn finer near the camera, so even a narrow canal is cut cleanly.
#define FINE_SUB   4
#define FINE_PTS   ((FINE_SUB + 1) * (FINE_SUB + 1))
#define MAX_FINE   2600
#define FINE_DIST  210.0f
static short fineIndex[GROUND_TILES][GROUND_TILES];
static float fineH[MAX_FINE][FINE_PTS], fineCut[MAX_FINE][FINE_PTS];
static int fineCount;
static bool fineNow[GROUND_TILES][GROUND_TILES];

static void FindFineTiles(void)
{
    const float s = TILE_SIZE / FINE_SUB;
    fineCount = 0;
    for (int i = 0; i < GROUND_TILES; i++) {
        for (int j = 0; j < GROUND_TILES; j++) {
            float x0 = -WORLD_HALF + (float)i * TILE_SIZE, z0 = -WORLD_HALF + (float)j * TILE_SIZE;
            bool near = false;
            for (int c = 0; c < CHANNEL_COUNT && !near; c++) {
                const Channel *ch = &CHANNELS[c];
                float m = ch->outer + 0.25f;
                near = x0 < ch->x1 + m && x0 + TILE_SIZE > ch->x0 - m && z0 < ch->z1 + m && z0 + TILE_SIZE > ch->z0 - m;
            }
            fineIndex[i][j] = -1;
            if (!near || fineCount >= MAX_FINE) continue;
            int k = fineCount++;
            fineIndex[i][j] = (short)k;
            for (int u = 0; u <= FINE_SUB; u++)
                for (int v = 0; v <= FINE_SUB; v++) {
                    float x = x0 + s * (float)u, z = z0 + s * (float)v;
                    fineH[k][u * (FINE_SUB + 1) + v] = TerrainHeight(x, z);
                    fineCut[k][u * (FINE_SUB + 1) + v] = Carve(x, z);
                }
        }
    }
}

// The ground as drawn near the camera: finely cut along the water, coarse triangles elsewhere.
float GroundSurface(float x, float z)
{
    int i = (int)Clamp((x + WORLD_HALF) / TILE_SIZE, 0.0f, (float)GROUND_TILES - 1.0f);
    int j = (int)Clamp((z + WORLD_HALF) / TILE_SIZE, 0.0f, (float)GROUND_TILES - 1.0f);
    return fineIndex[i][j] >= 0 ? TerrainHeight(x, z) : MeshHeight(x, z);
}

static void BlendCorners(void)
{
    const int n = GROUND_TILES;
    for (int i = 0; i <= n; i++) {
        for (int j = 0; j <= n; j++) {
            int r = 0, g = 0, b = 0, k = 0;
            for (int di = -1; di <= 0; di++) {
                for (int dj = -1; dj <= 0; dj++) {
                    int ti = i + di, tj = j + dj;
                    if (ti < 0 || tj < 0 || ti >= n || tj >= n) continue;
                    r += G.groundTint[ti][tj].r;
                    g += G.groundTint[ti][tj].g;
                    b += G.groundTint[ti][tj].b;
                    k++;
                }
            }
            vertexTint[i][j] = (Color){ (unsigned char)(r / k), (unsigned char)(g / k), (unsigned char)(b / k), 255 };
        }
    }
}

void BuildTerrain(void)
{
    const Color dust = { 164, 148, 122, 255 }, mud = { 130, 110, 90, 255 }, pale = { 184, 172, 146, 255 };
    const Color sparse = { 132, 132, 94, 255 }, lush = { 104, 124, 70, 255 }, sand = { 178, 162, 132, 255 }, earth = { 176, 154, 110, 255 };
    const Color scrub = { 128, 126, 92, 255 }, rock = { 146, 134, 110, 255 };
    const Color meadow = { 124, 146, 76, 255 }, woods = { 86, 114, 58, 255 }, track = { 150, 132, 96, 255 };
    const int n = GROUND_TILES;

    for (int i = 0; i <= n; i++)
        for (int j = 0; j <= n; j++) {
            float x = -WORLD_HALF + (float)i * TILE_SIZE, z = -WORLD_HALF + (float)j * TILE_SIZE;
            G.heights[i][j] = TerrainHeight(x, z);
            vertexCut[i][j] = Carve(x, z);
        }
    FindFineTiles();

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float x = -WORLD_HALF + ((float)i + 0.5f) * TILE_SIZE, z = -WORLD_HALF + ((float)j + 0.5f) * TILE_SIZE;
            float h = (G.heights[i][j] + G.heights[i + 1][j] + G.heights[i][j + 1] + G.heights[i + 1][j + 1]) / 4;
            // The open land: dust with patches of cracked mud and paler ground, sparse grass here and there,
            // green by the water and the fields, sand and mud on the banks.
            Color c = Mix(dust, mud, Clamp((Patches(x, z, 34.0f, 3) - 0.4f) * 2.2f, 0.0f, 1.0f));
            c = Mix(c, pale, Clamp((Patches(x, z, 52.0f, 11) - 0.55f) * 2.0f, 0.0f, 0.7f));
            c = Mix(c, sparse, Clamp((Patches(x, z, 26.0f, 19) - 0.5f) * 2.2f, 0.0f, 0.75f));
            float wd = WaterDistance(x, z);

            // Across the river: meadows and darker woodland, bare rock only on the highest tops.
            if (WestBank(x)) {
                float patch = 0.5f + 0.5f * sinf(x * 0.021f + 1.3f) * cosf(z * 0.017f - 0.4f);
                c = Mix(meadow, woods, patch);
                if (h > 15.0f) c = Mix(c, rock, Clamp((h - 15.0f) / 9.0f, 0.0f, 0.55f));
            } else {
                c = Mix(c, lush, Greenness(x, z));
                if (h > 1.0f) c = Mix(c, scrub, Clamp((h - 1.0f) / 5.0f, 0.0f, 0.8f));
                if (h > 11.0f) c = Mix(c, rock, Clamp((h - 11.0f) / 8.0f, 0.0f, 0.8f));
            }
            if (wd < 7.0f) c = Mix(c, sand, 0.85f * (1.0f - Clamp((wd - 1.5f) / 5.5f, 0.0f, 1.0f)));
            if (InsideWalls(x, z, -4.0f)) c = Mix(c, earth, 0.85f);
            // Worn ground along the roads, so they still show where their strips are not drawn.
            float rd = RoadDistance(x, z);
            if (rd < 7.0f && !InsideWalls(x, z, -4.0f)) c = Mix(c, track, 0.4f * (1.0f - rd / 7.0f));

            float hx = (G.heights[i + 1][j] + G.heights[i + 1][j + 1] - G.heights[i][j] - G.heights[i][j + 1]) / (2 * TILE_SIZE);
            float hz = (G.heights[i][j + 1] + G.heights[i + 1][j + 1] - G.heights[i][j] - G.heights[i + 1][j]) / (2 * TILE_SIZE);
            Vector3 normal = Vector3Normalize((Vector3){ -hx, 1.0f, -hz });
            float shade = 0.72f + 0.28f * Vector3DotProduct(normal, Vector3Normalize((Vector3){ 0.45f, 1.0f, 0.3f }));
            float noise = 0.93f + 0.07f * Hash2(i, j);
            float f = shade * noise / 0.98f;

            G.groundTint[i][j] = (Color){ (unsigned char)Clamp(c.r * f, 0, 255), (unsigned char)Clamp(c.g * f, 0, 255),
                                          (unsigned char)Clamp(c.b * f, 0, 255), 255 };
        }
    }
    BlendCorners();
}

static void Quad(float x0, float z0, float x1, float z1, float h00, float h10, float h01, float h11, Color c)
{
    VColor(c);
    VNormal(0, 1, 0);
    VVertex(x0, h00, z0); VVertex(x0, h01, z1); VVertex(x1, h11, z1); VVertex(x1, h10, z0);
}

// The ground's normal for the shaders: across the coarse grid at its corners, from the terrain itself
// on the finely cut tiles.
static void GridNormal(int i, int j)
{
    int a = i > 0 ? i - 1 : i, b = i < GROUND_TILES ? i + 1 : i, c = j > 0 ? j - 1 : j, d = j < GROUND_TILES ? j + 1 : j;
    VNormal((G.heights[a][j] - G.heights[b][j]) / ((float)(b - a) * TILE_SIZE), 1.0f, (G.heights[i][c] - G.heights[i][d]) / ((float)(d - c) * TILE_SIZE));
}

static void FineNormal(float x, float z)
{
    const float e = 0.4f;
    VNormal((TerrainHeight(x - e, z) - TerrainHeight(x + e, z)) / (2 * e), 1.0f, (TerrainHeight(x, z - e) - TerrainHeight(x, z + e)) / (2 * e));
}

static float tileLight[GROUND_TILES][GROUND_TILES];

// Torchlight on the ground this frame, for things drawn flat on it like the roads.
float GroundTorch(float x, float z)
{
    int i = (int)Clamp((x + WORLD_HALF) / TILE_SIZE, 0.0f, (float)GROUND_TILES - 1.0f);
    int j = (int)Clamp((z + WORLD_HALF) / TILE_SIZE, 0.0f, (float)GROUND_TILES - 1.0f);
    return tileLight[i][j];
}

static bool tileShown[GROUND_TILES][GROUND_TILES];

// Skips tiles well to the side of or behind the view. Tiles near the player are always drawn,
// since a camera looking steeply down sees ground on every side.
static void CullTiles(void)
{
    const float nearR = 60.0f, cone = 0.26f;
    Vector3 eye = G.cam.position, look = Vector3Subtract(G.cam.target, eye);
    float flat = sqrtf(look.x * look.x + look.z * look.z);
    bool steep = flat < 0.3f * Vector3Length(look);
    float fx = steep ? 0.0f : look.x / flat, fz = steep ? 0.0f : look.z / flat;

    for (int i = 0; i < GROUND_TILES; i++) {
        float dx = -WORLD_HALF + ((float)i + 0.5f) * TILE_SIZE - eye.x;
        for (int j = 0; j < GROUND_TILES; j++) {
            float dz = -WORLD_HALF + ((float)j + 0.5f) * TILE_SIZE - eye.z, d2 = dx * dx + dz * dz;
            tileShown[i][j] = steep || d2 < nearR * nearR || dx * fx + dz * fz > cone * sqrtf(d2);
        }
    }
}

static bool VertexShown(int i, int j)
{
    for (int ti = i - 1; ti <= i; ti++)
        for (int tj = j - 1; tj <= j; tj++)
            if (ti >= 0 && tj >= 0 && ti < GROUND_TILES && tj < GROUND_TILES && tileShown[ti][tj]) return true;
    return false;
}

// Banks darken toward the water, as wet mud does.
static Color Wet(Color c, float cut)
{
    float k = Clamp(-cut / 0.5f, 0.0f, 1.0f);
    if (k <= 0.0f) return c;
    return (Color){ (unsigned char)((float)c.r * (1.0f - 0.32f * k)), (unsigned char)((float)c.g * (1.0f - 0.38f * k)),
                    (unsigned char)((float)c.b * (1.0f - 0.46f * k)), 255 };
}

static Color Bilerp(Color a, Color b, Color c, Color d, float u, float v)
{
    float w00 = (1 - u) * (1 - v), w10 = u * (1 - v), w01 = (1 - u) * v, w11 = u * v;
    return (Color){ (unsigned char)(a.r * w00 + b.r * w10 + c.r * w01 + d.r * w11), (unsigned char)(a.g * w00 + b.g * w10 + c.g * w01 + d.g * w11),
                    (unsigned char)(a.b * w00 + b.b * w10 + c.b * w01 + d.b * w11), 255 };
}

static bool CoarseAt(int i, int j)
{
    return i < 0 || j < 0 || i >= GROUND_TILES || j >= GROUND_TILES || !fineNow[i][j];
}

// A tile cut into FINE_SUB x FINE_SUB cells. An edge shared with a coarse neighbor follows that
// neighbor's straight edge, so no cracks open between them.
static void DrawFineTile(int i, int j, Color (*dry)[GROUND_TILES + 1], Color (*lit)[GROUND_TILES + 1])
{
    const int N = FINE_SUB;
    const float s = TILE_SIZE / N;
    const int k = fineIndex[i][j];
    float x0 = -WORLD_HALF + (float)i * TILE_SIZE, z0 = -WORLD_HALF + (float)j * TILE_SIZE;
    float h[FINE_SUB + 1][FINE_SUB + 1];
    Color c[FINE_SUB + 1][FINE_SUB + 1];
    bool west = CoarseAt(i - 1, j), east = CoarseAt(i + 1, j), north = CoarseAt(i, j - 1), south = CoarseAt(i, j + 1);

    for (int u = 0; u <= N; u++) {
        for (int v = 0; v <= N; v++) {
            float fu = (float)u / N, fv = (float)v / N;
            h[u][v] = fineH[k][u * (N + 1) + v];
            c[u][v] = Wet(Bilerp(dry[i][j], dry[i + 1][j], dry[i][j + 1], dry[i + 1][j + 1], fu, fv), fineCut[k][u * (N + 1) + v]);
            if (u == 0 && west) {
                h[u][v] = Lerp(G.heights[i][j], G.heights[i][j + 1], fv);
                c[u][v] = Mix(lit[i][j], lit[i][j + 1], fv);
            } else if (u == N && east) {
                h[u][v] = Lerp(G.heights[i + 1][j], G.heights[i + 1][j + 1], fv);
                c[u][v] = Mix(lit[i + 1][j], lit[i + 1][j + 1], fv);
            } else if (v == 0 && north) {
                h[u][v] = Lerp(G.heights[i][j], G.heights[i + 1][j], fu);
                c[u][v] = Mix(lit[i][j], lit[i + 1][j], fu);
            } else if (v == N && south) {
                h[u][v] = Lerp(G.heights[i][j + 1], G.heights[i + 1][j + 1], fu);
                c[u][v] = Mix(lit[i][j + 1], lit[i + 1][j + 1], fu);
            }
        }
    }
    bool normals = GfxOn();
    rlCheckRenderBatchLimit(4 * N * N);
    VBegin(RL_QUADS);
    for (int u = 0; u < N; u++) {
        for (int v = 0; v < N; v++) {
            float xa = x0 + s * (float)u, za = z0 + s * (float)v, xb = xa + s, zb = za + s;
            Color a = c[u][v], b = c[u][v + 1], cc = c[u + 1][v + 1], d = c[u + 1][v];
            VColor(a); if (normals) FineNormal(xa, za); VVertex(xa, h[u][v], za);
            VColor(b); if (normals) FineNormal(xa, zb); VVertex(xa, h[u][v + 1], zb);
            VColor(cc); if (normals) FineNormal(xb, zb); VVertex(xb, h[u + 1][v + 1], zb);
            VColor(d); if (normals) FineNormal(xb, za); VVertex(xb, h[u + 1][v], za);
        }
    }
    VEnd();
}

// For the shaders' meshes (DrawingAll) every tile is drawn, finely cut wherever it can be.
void DrawGround(void)
{
    static Color dry[GROUND_TILES + 1][GROUND_TILES + 1], lit[GROUND_TILES + 1][GROUND_TILES + 1];
    const int n = GROUND_TILES;
    Vector3 eye = G.cam.position;
    bool all = DrawingAll(), normals = GfxOn();

    if (all) memset(tileShown, 1, sizeof(tileShown));
    else CullTiles();
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            tileLight[i][j] = tileShown[i][j] && !all ? TileLight(i, j) : 0.0f;
            float dx = -WORLD_HALF + ((float)i + 0.5f) * TILE_SIZE - eye.x, dz = -WORLD_HALF + ((float)j + 0.5f) * TILE_SIZE - eye.z;
            fineNow[i][j] = tileShown[i][j] && fineIndex[i][j] >= 0 && (all || dx * dx + dz * dz < FINE_DIST * FINE_DIST);
        }
    }
    for (int i = 0; i <= n; i++) {
        for (int j = 0; j <= n; j++) {
            if (!VertexShown(i, j)) continue;
            float sum = 0;
            int k = 0;
            for (int di = -1; di <= 0; di++) {
                for (int dj = -1; dj <= 0; dj++) {
                    int ti = i + di, tj = j + dj;
                    if (ti < 0 || tj < 0 || ti >= n || tj >= n) continue;
                    sum += tileLight[ti][tj];
                    k++;
                }
            }
            G.drawTorch = sum / (float)k;
            G.drawFog = FogAt((Vector3){ -WORLD_HALF + (float)i * TILE_SIZE, G.heights[i][j], -WORLD_HALF + (float)j * TILE_SIZE }) * (1.0f - 0.6f * G.drawTorch);
            dry[i][j] = Lit(vertexTint[i][j], 1.0f);
            lit[i][j] = Wet(dry[i][j], vertexCut[i][j]);
        }
    }

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (fineNow[i][j]) DrawFineTile(i, j, dry, lit);

    for (int i = 0; i < n; i++) {
        rlCheckRenderBatchLimit(4 * n);
        VBegin(RL_QUADS);
        for (int j = 0; j < n; j++) {
            if (!tileShown[i][j] || fineNow[i][j]) continue;
            float x0 = -WORLD_HALF + (float)i * TILE_SIZE, z0 = -WORLD_HALF + (float)j * TILE_SIZE;
            float x1 = x0 + TILE_SIZE, z1 = z0 + TILE_SIZE;
            Color a = lit[i][j], b = lit[i][j + 1], c = lit[i + 1][j + 1], d = lit[i + 1][j];
            VColor(a); if (normals) GridNormal(i, j); VVertex(x0, G.heights[i][j], z0);
            VColor(b); if (normals) GridNormal(i, j + 1); VVertex(x0, G.heights[i][j + 1], z1);
            VColor(c); if (normals) GridNormal(i + 1, j + 1); VVertex(x1, G.heights[i + 1][j + 1], z1);
            VColor(d); if (normals) GridNormal(i + 1, j); VVertex(x1, G.heights[i + 1][j], z0);
        }
        VEnd();
    }

    G.drawTorch = 0;
    G.drawFog = G.night * 0.8f;
    // Beyond the edge the land goes on: steppe on the city side, meadow across the river. The river
    // itself runs on between them (see DrawWater).
    Color outer = Lit((Color){ 150, 150, 98, 255 }, 0.95f), green = Lit((Color){ 96, 134, 60, 255 }, 0.95f);
    const float W = WORLD_HALF, F = 1150.0f, A = RIVER_X0 - 3.0f, B = RIVER_X1 + 3.0f;
    rlCheckRenderBatchLimit(24);
    VBegin(RL_QUADS);
    Quad(B, -F, F, -W, 0, 0, 0, 0, outer);
    Quad(B, W, F, F, 0, 0, 0, 0, outer);
    Quad(-F, -F, A, -W, 0, 0, 0, 0, green);
    Quad(-F, W, A, F, 0, 0, 0, 0, green);
    Quad(-F, -W, -W, W, 0, 0, 0, 0, green);
    Quad(W, -W, F, W, 0, 0, 0, 0, outer);
    VEnd();
    G.drawFog = 0;
}
