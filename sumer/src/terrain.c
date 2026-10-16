#include "game.h"
#include "rlgl.h"
#include <math.h>

typedef struct { float x, z, r, h; } Bump;

static const Bump EAST_HILLS[] = {
    { 208,  -60, 32, 18 }, { 240,   15, 40, 22 }, { 198,   58, 26,  9 }, { 245, -150, 45, 24 },
    { 180, -150, 30, 10 }, { 230,  125, 35, 14 }, { 180,  195, 30,  8 }, { 255,  -60, 30, 12 },
    { 300, -250, 50, 26 }, { 310,   90, 45, 20 }, { 290,  280, 40, 16 }, { 330,  -30, 40, 18 },
    { 240, -300, 40, 12 }, { 260,  330, 35, 10 },
};
static const Bump WEST_DOWNS[] = { { -70, 211, 38, 7 }, { -35, 238, 28, 5 }, { -95, 166, 26, 4 }, { -60, 300, 40, 8 } };

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

float TerrainHeight(float x, float z)
{
    float edge = Smooth(WORLD_HALF, WORLD_HALF - 20.0f, fabsf(x)) * Smooth(WORLD_HALF, WORLD_HALF - 20.0f, fabsf(z));
    float east = Smooth(142.0f, 168.0f, x);
    float west = Smooth(156.0f, 181.0f, z) * Smooth(-12.0f, -30.0f, x) * Smooth(-122.0f, -104.0f, x);
    float h = 0;

    if (east > 0) {
        float roll = 1.5f * (sinf(x * 0.071f) * cosf(z * 0.053f) + 1.0f);
        h += east * (SumBumps(EAST_HILLS, (int)(sizeof(EAST_HILLS) / sizeof(Bump)), x, z) + 3.0f + roll);
    }
    if (west > 0) h += west * SumBumps(WEST_DOWNS, (int)(sizeof(WEST_DOWNS) / sizeof(Bump)), x, z);
    return h * edge;
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
    return fmaxf(river, fmaxf(ring, road));
}

static Color vertexTint[GROUND_TILES + 1][GROUND_TILES + 1];

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
    const Color dry = { 166, 160, 104, 255 }, lush = { 98, 132, 60, 255 }, earth = { 176, 154, 110, 255 };
    const Color scrub = { 118, 128, 74, 255 }, rock = { 146, 134, 110, 255 };
    const int n = GROUND_TILES;

    for (int i = 0; i <= n; i++)
        for (int j = 0; j <= n; j++)
            G.heights[i][j] = TerrainHeight(-WORLD_HALF + (float)i * TILE_SIZE, -WORLD_HALF + (float)j * TILE_SIZE);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float x = -WORLD_HALF + ((float)i + 0.5f) * TILE_SIZE, z = -WORLD_HALF + ((float)j + 0.5f) * TILE_SIZE;
            float h = (G.heights[i][j] + G.heights[i + 1][j] + G.heights[i][j + 1] + G.heights[i + 1][j + 1]) / 4;
            Color c = Mix(dry, lush, powf(Fertility(x, z), 0.8f));

            if (h > 1.0f) c = Mix(c, scrub, Clamp((h - 1.0f) / 5.0f, 0.0f, 1.0f));
            if (h > 11.0f) c = Mix(c, rock, Clamp((h - 11.0f) / 8.0f, 0.0f, 0.8f));
            if (InsideWalls(x, z, -4.0f)) c = Mix(c, earth, 0.85f);

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
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlVertex3f(x0, h00, z0); rlVertex3f(x0, h01, z1); rlVertex3f(x1, h11, z1); rlVertex3f(x1, h10, z0);
}

void DrawGround(void)
{
    static float tileLight[GROUND_TILES][GROUND_TILES];
    static Color lit[GROUND_TILES + 1][GROUND_TILES + 1];
    const int n = GROUND_TILES;

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) tileLight[i][j] = TileLight(i, j);
    for (int i = 0; i <= n; i++) {
        for (int j = 0; j <= n; j++) {
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
            lit[i][j] = Lit(vertexTint[i][j], 1.0f);
        }
    }

    for (int i = 0; i < n; i++) {
        rlCheckRenderBatchLimit(4 * n);
        rlBegin(RL_QUADS);
        for (int j = 0; j < n; j++) {
            float x0 = -WORLD_HALF + (float)i * TILE_SIZE, z0 = -WORLD_HALF + (float)j * TILE_SIZE;
            float x1 = x0 + TILE_SIZE, z1 = z0 + TILE_SIZE;
            Color a = lit[i][j], b = lit[i][j + 1], c = lit[i + 1][j + 1], d = lit[i + 1][j];
            rlColor4ub(a.r, a.g, a.b, 255); rlVertex3f(x0, G.heights[i][j], z0);
            rlColor4ub(b.r, b.g, b.b, 255); rlVertex3f(x0, G.heights[i][j + 1], z1);
            rlColor4ub(c.r, c.g, c.b, 255); rlVertex3f(x1, G.heights[i + 1][j + 1], z1);
            rlColor4ub(d.r, d.g, d.b, 255); rlVertex3f(x1, G.heights[i + 1][j], z0);
        }
        rlEnd();
    }

    G.drawTorch = 0;
    G.drawFog = G.night * 0.8f;
    Color outer = Lit((Color){ 150, 150, 98, 255 }, 0.95f);
    const float W = WORLD_HALF, F = 900.0f;
    rlCheckRenderBatchLimit(16);
    rlBegin(RL_QUADS);
    Quad(-F, -F, F, -W, 0, 0, 0, 0, outer);
    Quad(-F, W, F, F, 0, 0, 0, 0, outer);
    Quad(-F, -W, -W, W, 0, 0, 0, 0, outer);
    Quad(W, -W, F, W, 0, 0, 0, 0, outer);
    rlEnd();
    G.drawFog = 0;
}
