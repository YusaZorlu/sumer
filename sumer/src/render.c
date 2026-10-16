#include "game.h"
#include "rlgl.h"
#include <math.h>

float RandF(float lo, float hi)
{
    return lo + (hi - lo) * (float)GetRandomValue(0, 10000) / 10000.0f;
}

float Hash2(int a, int b)
{
    unsigned int h = ((unsigned int)a * 73856093u) ^ ((unsigned int)b * 19349663u);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (float)(h & 0xffffu) / 65535.0f;
}

float XZDist(Vector3 a, Vector3 b)
{
    return Vector2Distance((Vector2){ a.x, a.z }, (Vector2){ b.x, b.z });
}

Color Mix(Color a, Color b, float t)
{
    return (Color){ (unsigned char)Lerp(a.r, b.r, t), (unsigned char)Lerp(a.g, b.g, t),
                    (unsigned char)Lerp(a.b, b.b, t), (unsigned char)Lerp(a.a, b.a, t) };
}

// Daylight plus warm torchlight set by LightFor() for the object being drawn.
Color Lit(Color c, float shade)
{
    float t = G.drawTorch, f = G.drawFog;
    float r = Clamp(c.r * shade * (G.light.x + 1.00f * t), 0.0f, 255.0f);
    float g = Clamp(c.g * shade * (G.light.y + 0.62f * t), 0.0f, 255.0f);
    float b = Clamp(c.b * shade * (G.light.z + 0.30f * t), 0.0f, 255.0f);
    if (f > 0) {
        r += ((float)G.sky.r - r) * f;
        g += ((float)G.sky.g - g) * f;
        b += ((float)G.sky.b - b) * f;
    }
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, c.a };
}

static Color Shade(Color c, float f, bool lit)
{
    if (lit) return Lit(c, f);
    f = 0.85f + 0.15f * f;
    return (Color){ (unsigned char)(c.r * f), (unsigned char)(c.g * f), (unsigned char)(c.b * f), c.a };
}

static void SetColor(Color c)
{
    rlColor4ub(c.r, c.g, c.b, c.a);
}

// Fixed per-face shading stands in for real lighting.
static void BoxQuads(Vector3 c, Vector3 s, Color col, bool lit)
{
    float x0 = c.x - s.x / 2, x1 = c.x + s.x / 2;
    float y0 = c.y - s.y / 2, y1 = c.y + s.y / 2;
    float z0 = c.z - s.z / 2, z1 = c.z + s.z / 2;

    rlCheckRenderBatchLimit(24);
    rlBegin(RL_QUADS);
    SetColor(Shade(col, 1.00f, lit));
    rlVertex3f(x0, y1, z0); rlVertex3f(x0, y1, z1); rlVertex3f(x1, y1, z1); rlVertex3f(x1, y1, z0);
    SetColor(Shade(col, 0.86f, lit));
    rlVertex3f(x0, y0, z1); rlVertex3f(x1, y0, z1); rlVertex3f(x1, y1, z1); rlVertex3f(x0, y1, z1);
    SetColor(Shade(col, 0.70f, lit));
    rlVertex3f(x1, y0, z0); rlVertex3f(x0, y0, z0); rlVertex3f(x0, y1, z0); rlVertex3f(x1, y1, z0);
    SetColor(Shade(col, 0.78f, lit));
    rlVertex3f(x1, y0, z1); rlVertex3f(x1, y0, z0); rlVertex3f(x1, y1, z0); rlVertex3f(x1, y1, z1);
    SetColor(Shade(col, 0.62f, lit));
    rlVertex3f(x0, y0, z0); rlVertex3f(x0, y0, z1); rlVertex3f(x0, y1, z1); rlVertex3f(x0, y1, z0);
    if (y0 > 0.05f) {
        SetColor(Shade(col, 0.5f, lit));
        rlVertex3f(x0, y0, z0); rlVertex3f(x1, y0, z0); rlVertex3f(x1, y0, z1); rlVertex3f(x0, y0, z1);
    }
    rlEnd();
}

void DrawShadedBox(Vector3 c, Vector3 s, Color col)
{
    BoxQuads(c, s, col, true);
}

void DrawGlowBox(Vector3 c, Vector3 s, Color col)
{
    BoxQuads(c, s, col, false);
}

static float NormalShade(float nx, float ny, float nz)
{
    return Clamp(0.74f + 0.26f * ny + 0.08f * nz + 0.08f * nx, 0.45f, 1.0f);
}

// The top cap is a fan of degenerate quads.
void DrawShadedCylinder(Vector3 base, float rx, float rz, float h, float topScale, int sides, Color col)
{
    float slope = (1.0f - topScale) * fmaxf(rx, rz) / fmaxf(h, 0.01f);
    float y0 = base.y, y1 = base.y + h;

    rlCheckRenderBatchLimit(sides * 8);
    rlBegin(RL_QUADS);
    for (int i = 0; i < sides; i++) {
        float a0 = 2.0f * PI * (float)i / (float)sides, a1 = 2.0f * PI * (float)(i + 1) / (float)sides, am = (a0 + a1) / 2;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        float ny = slope / sqrtf(1.0f + slope * slope);
        SetColor(Lit(col, NormalShade(cosf(am), ny, sinf(am))));
        rlVertex3f(base.x + rx * c0, y0, base.z + rz * s0);
        rlVertex3f(base.x + rx * c0 * topScale, y1, base.z + rz * s0 * topScale);
        rlVertex3f(base.x + rx * c1 * topScale, y1, base.z + rz * s1 * topScale);
        rlVertex3f(base.x + rx * c1, y0, base.z + rz * s1);
        if (topScale > 0.01f) {
            SetColor(Lit(col, 1.0f));
            rlVertex3f(base.x, y1, base.z);
            rlVertex3f(base.x + rx * c1 * topScale, y1, base.z + rz * s1 * topScale);
            rlVertex3f(base.x + rx * c0 * topScale, y1, base.z + rz * s0 * topScale);
            rlVertex3f(base.x + rx * c0 * topScale, y1, base.z + rz * s0 * topScale);
        }
    }
    rlEnd();
}

enum { BLOB_SLICES = 8, BLOB_STACKS = 5 };

// A blob at two levels of detail: the full one, and a coarser one for things far away.
typedef struct {
    int slices, stacks;
    float ct[BLOB_SLICES + 1], st[BLOB_SLICES + 1], cp[BLOB_STACKS + 1], sp[BLOB_STACKS + 1];
    float shade[BLOB_STACKS][BLOB_SLICES];
} BlobMesh;

static BlobMesh blobs[2] = { { .slices = BLOB_SLICES, .stacks = BLOB_STACKS }, { .slices = 6, .stacks = 3 } };
static bool blobReady;

static void BlobTables(void)
{
    for (int m = 0; m < 2; m++) {
        BlobMesh *b = &blobs[m];
        for (int i = 0; i <= b->slices; i++) {
            b->ct[i] = cosf(2.0f * PI * (float)i / (float)b->slices);
            b->st[i] = sinf(2.0f * PI * (float)i / (float)b->slices);
        }
        for (int j = 0; j <= b->stacks; j++) {
            b->cp[j] = cosf(-PI / 2 + PI * (float)j / (float)b->stacks);
            b->sp[j] = sinf(-PI / 2 + PI * (float)j / (float)b->stacks);
        }
        for (int j = 0; j < b->stacks; j++) {
            float pm = -PI / 2 + PI * ((float)j + 0.5f) / (float)b->stacks;
            for (int i = 0; i < b->slices; i++) {
                float tm = 2.0f * PI * ((float)i + 0.5f) / (float)b->slices;
                b->shade[j][i] = NormalShade(cosf(pm) * cosf(tm), sinf(pm), cosf(pm) * sinf(tm));
            }
        }
    }
    blobReady = true;
}

static void BlobQuads(const BlobMesh *b, Vector3 c, Vector3 r, Color col)
{
    if (!blobReady) BlobTables();
    rlCheckRenderBatchLimit(b->slices * b->stacks * 4);
    rlBegin(RL_QUADS);
    for (int j = 0; j < b->stacks; j++) {
        float cp0 = b->cp[j], sp0 = b->sp[j], cp1 = b->cp[j + 1], sp1 = b->sp[j + 1];
        for (int i = 0; i < b->slices; i++) {
            float ct0 = b->ct[i], st0 = b->st[i], ct1 = b->ct[i + 1], st1 = b->st[i + 1];
            SetColor(Lit(col, b->shade[j][i]));
            rlVertex3f(c.x + r.x * cp0 * ct0, c.y + r.y * sp0, c.z + r.z * cp0 * st0);
            rlVertex3f(c.x + r.x * cp1 * ct0, c.y + r.y * sp1, c.z + r.z * cp1 * st0);
            rlVertex3f(c.x + r.x * cp1 * ct1, c.y + r.y * sp1, c.z + r.z * cp1 * st1);
            rlVertex3f(c.x + r.x * cp0 * ct1, c.y + r.y * sp0, c.z + r.z * cp0 * st1);
        }
    }
    rlEnd();
}

void DrawShadedBlob(Vector3 c, Vector3 r, Color col)
{
    BlobQuads(&blobs[0], c, r, col);
}

void DrawShadedBlobFar(Vector3 c, Vector3 r, Color col)
{
    BlobQuads(&blobs[1], c, r, col);
}

static void Limb(float x, float y0, float z, float r, float h, Color c)
{
    DrawShadedCylinder((Vector3){ x, y0, z }, r, r, h, 0.85f, 6, c);
}

void DrawHuman(Vector3 pos, float facing, float stride, Look look, bool crouch, bool bow, float draw)
{
    float swing = sinf(stride) * (crouch ? 0.1f : 0.18f);
    float drop = crouch ? 0.38f : 0.0f;
    float legH = 0.84f - drop;

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    Limb(-0.11f, 0, swing, 0.085f, legH + 0.05f, look.skin);
    Limb(0.11f, 0, -swing, 0.085f, legH + 0.05f, look.skin);
    DrawShadedBox((Vector3){ -0.11f, 0.04f, swing + 0.05f }, (Vector3){ 0.15f, 0.08f, 0.26f }, Mix(look.skin, COL_HAIR, 0.35f));
    DrawShadedBox((Vector3){ 0.11f, 0.04f, -swing + 0.05f }, (Vector3){ 0.15f, 0.08f, 0.26f }, Mix(look.skin, COL_HAIR, 0.35f));
    rlTranslatef(0, -drop, crouch ? 0.06f : 0.0f);
    DrawShadedCylinder((Vector3){ 0, 0.68f, 0 }, 0.28f, 0.19f, 0.56f, 0.78f, 8, look.cloth);
    DrawShadedCylinder((Vector3){ 0, 1.16f, 0 }, 0.23f, 0.15f, 0.42f, 0.95f, 8, look.top);
    DrawShadedBlob((Vector3){ 0, 1.56f, 0 }, (Vector3){ 0.27f, 0.08f, 0.15f }, look.top);

    if (bow) {
        float string = 0.6f - 0.42f * draw;
        DrawShadedBox((Vector3){ -0.22f, 1.48f, 0.3f }, (Vector3){ 0.11f, 0.11f, 0.6f }, look.skin);
        DrawShadedBox((Vector3){ 0.02f, 1.48f, fmaxf(string, 0.2f) / 2 }, (Vector3){ 0.11f, 0.11f, fmaxf(string, 0.2f) }, look.skin);
        DrawShadedBox((Vector3){ -0.22f, 1.48f, 0.64f }, (Vector3){ 0.05f, 0.8f, 0.06f }, COL_WOOD);
        DrawShadedBox((Vector3){ -0.22f, 1.95f, 0.58f }, (Vector3){ 0.04f, 0.2f, 0.06f }, COL_WOOD);
        DrawShadedBox((Vector3){ -0.22f, 1.01f, 0.58f }, (Vector3){ 0.04f, 0.2f, 0.06f }, COL_WOOD);
        DrawShadedBox((Vector3){ -0.22f, 1.48f, string }, (Vector3){ 0.02f, 0.9f, 0.02f }, COL_LINEN);
        if (draw > 0.05f)
            DrawShadedBox((Vector3){ -0.2f, 1.5f, string + 0.38f }, (Vector3){ 0.03f, 0.03f, 0.76f }, COL_REED);
    } else {
        Limb(-0.31f, 0.98f, -swing, 0.065f, 0.6f, look.skin);
        Limb(0.31f, 0.98f, swing, 0.065f, 0.6f, look.skin);
    }

    Limb(0, 1.58f, 0, 0.07f, 0.1f, COL_SKIN);
    DrawShadedBlob((Vector3){ 0, 1.79f, 0.01f }, (Vector3){ 0.15f, 0.17f, 0.16f }, COL_SKIN);
    DrawShadedBlob((Vector3){ 0, 1.86f, -0.02f }, (Vector3){ 0.165f, 0.12f, 0.17f }, look.hair);
    DrawShadedBox((Vector3){ 0, 1.77f, 0.16f }, (Vector3){ 0.05f, 0.07f, 0.05f }, COL_SKIN);
    rlPopMatrix();
}

void DrawPersonBody(Vector3 pos, float facing, Look look)
{
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + 0.16f, pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    rlRotatef(-90.0f, 1, 0, 0);
    DrawHuman((Vector3){ 0, 0, 0 }, 0, 0, look, false, false, 0);
    rlPopMatrix();
}
