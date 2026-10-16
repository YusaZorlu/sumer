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

enum { BLOB_SLICES = 16, BLOB_STACKS = 10, BLOB_FINE = 0, BLOB_MID = 1, BLOB_FAR = 2 };

// A blob at three levels of detail: fine for people and buildings up close, the usual one, and a
// coarse one for things far away.
typedef struct {
    int slices, stacks;
    float ct[BLOB_SLICES + 1], st[BLOB_SLICES + 1], cp[BLOB_STACKS + 1], sp[BLOB_STACKS + 1];
    float shade[BLOB_STACKS][BLOB_SLICES];
} BlobMesh;

static BlobMesh blobs[3] = { { .slices = 16, .stacks = 10 }, { .slices = 8, .stacks = 5 }, { .slices = 6, .stacks = 3 } };
static bool blobReady;

static void BlobTables(void)
{
    for (int m = 0; m < 3; m++) {
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
    BlobQuads(&blobs[BLOB_MID], c, r, col);
}

void DrawShadedBlobFar(Vector3 c, Vector3 r, Color col)
{
    BlobQuads(&blobs[BLOB_FAR], c, r, col);
}

void DrawShadedBlobFine(Vector3 c, Vector3 r, Color col)
{
    BlobQuads(&blobs[BLOB_FINE], c, r, col);
}

// A box with its edges cut off: 6 faces, 12 bevels and 8 corner triangles. Each corner of the
// plain box becomes three points, one on each face that meets there.
typedef struct { signed char n[3]; unsigned char count, corner[4], axis[4]; float shade; } BevelPoly;
static BevelPoly bevels[26];
static bool bevelReady;

static Vector3 BevelPoint(int corner, int axis, Vector3 e, float b)
{
    float sx = (corner & 1) ? 1.0f : -1.0f, sy = (corner & 2) ? 1.0f : -1.0f, sz = (corner & 4) ? 1.0f : -1.0f;
    Vector3 v = { sx * (e.x - b), sy * (e.y - b), sz * (e.z - b) };
    if (axis == 0) v.x = sx * e.x;
    if (axis == 1) v.y = sy * e.y;
    if (axis == 2) v.z = sz * e.z;
    return v;
}

static void BevelTables(void)
{
    static const float faceShade[3][2] = { { 0.62f, 0.78f }, { 0.5f, 1.0f }, { 0.70f, 0.86f } };
    int count = 0;

    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
            for (int dz = -1; dz <= 1; dz++) {
                if (!dx && !dy && !dz) continue;
                BevelPoly *p = &bevels[count++];
                int n[3] = { dx, dy, dz }, faces = 0;
                float shade = 0;
                *p = (BevelPoly){ { (signed char)dx, (signed char)dy, (signed char)dz }, 0, { 0 }, { 0 }, 0 };
                for (int a = 0; a < 3; a++)
                    if (n[a]) {
                        shade += faceShade[a][n[a] > 0];
                        faces++;
                    }
                p->shade = shade / (float)faces;
                for (int c = 0; c < 8; c++) {
                    int s[3] = { (c & 1) ? 1 : -1, (c & 2) ? 1 : -1, (c & 4) ? 1 : -1 };
                    bool match = true;
                    for (int a = 0; a < 3; a++)
                        if (n[a] && s[a] != n[a]) match = false;
                    for (int a = 0; a < 3 && match; a++)
                        if (n[a]) {
                            p->corner[p->count] = (unsigned char)c;
                            p->axis[p->count++] = (unsigned char)a;
                        }
                }
                // Counter-clockwise seen from outside, as every face drawn here is.
                Vector3 nn = Vector3Normalize((Vector3){ (float)dx, (float)dy, (float)dz }), pts[4], mid = { 0 };
                Vector3 helper = fabsf(nn.y) < 0.9f ? (Vector3){ 0, 1, 0 } : (Vector3){ 1, 0, 0 };
                Vector3 u = Vector3Normalize(Vector3CrossProduct(helper, nn)), v = Vector3CrossProduct(nn, u);
                float ang[4];
                for (int k = 0; k < p->count; k++) {
                    pts[k] = BevelPoint(p->corner[k], p->axis[k], (Vector3){ 1, 1, 1 }, 0.3f);
                    mid = Vector3Add(mid, pts[k]);
                }
                mid = Vector3Scale(mid, 1.0f / (float)p->count);
                for (int k = 0; k < p->count; k++) {
                    Vector3 d = Vector3Subtract(pts[k], mid);
                    ang[k] = atan2f(Vector3DotProduct(d, v), Vector3DotProduct(d, u));
                }
                for (int i = 1; i < p->count; i++)
                    for (int k = i; k > 0 && ang[k] < ang[k - 1]; k--) {
                        float ta = ang[k]; ang[k] = ang[k - 1]; ang[k - 1] = ta;
                        unsigned char tc = p->corner[k]; p->corner[k] = p->corner[k - 1]; p->corner[k - 1] = tc;
                        unsigned char tx = p->axis[k]; p->axis[k] = p->axis[k - 1]; p->axis[k - 1] = tx;
                    }
            }
    bevelReady = true;
}

// A box standing on the ground keeps its foot square, so it meets the ground without a gap.
void DrawBeveledBox(Vector3 c, Vector3 s, Color col, float b)
{
    if (!bevelReady) BevelTables();
    Vector3 e = Vector3Scale(s, 0.5f);
    bool grounded = c.y - e.y <= 0.05f;

    rlCheckRenderBatchLimit(26 * 4);
    rlBegin(RL_QUADS);
    for (int i = 0; i < 26; i++) {
        const BevelPoly *p = &bevels[i];
        if (grounded && p->n[1] < 0) continue;
        SetColor(Lit(col, p->shade));
        for (int k = 0; k < 4; k++) {
            int at = k < p->count ? k : p->count - 1;
            Vector3 v = BevelPoint(p->corner[at], p->axis[at], e, b);
            if (grounded && !(p->corner[at] & 2)) v.y = -e.y;
            rlVertex3f(c.x + v.x, c.y + v.y, c.z + v.z);
        }
    }
    rlEnd();
}

static void Limb(float x, float y0, float z, float r, float h, Color c, int sides)
{
    DrawShadedCylinder((Vector3){ x, y0, z }, r, r, h, 0.85f, sides, c);
}

static float Luma(Color c)
{
    return 0.299f * (float)c.r + 0.587f * (float)c.g + 0.114f * (float)c.b;
}

static float ColorGap(Color a, Color b)
{
    float dr = (float)a.r - (float)b.r, dg = (float)a.g - (float)b.g, db = (float)a.b - (float)b.b;
    return sqrtf(2.0f * dr * dr + 4.0f * dg * dg + 3.0f * db * db);
}

// Cloth too close to its wearer's skin is bleached or darkened until the two stand apart.
Color Clothe(Color cloth, Color skin)
{
    const float need = 100.0f;
    if (ColorGap(cloth, skin) >= need) return cloth;
    bool lighter = Luma(cloth) >= Luma(skin) - 6.0f;
    Color toward = lighter ? (Color){ 238, 230, 210, 255 } : (Color){ 58, 44, 34, 255 };
    for (float t = 0.05f; t < 1.0f; t += 0.05f) {
        Color c = Mix(cloth, toward, t);
        if (ColorGap(c, skin) >= need) return c;
    }
    return toward;
}

static bool SameColor(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// Up close a person gets twice the facets, hands, a belt and eyes.
static void HumanModel(float stride, Look look, bool crouch, bool bow, float draw, bool fine)
{
    const int limb = fine ? 12 : 6, body = fine ? 16 : 8;
    const BlobMesh *bm = &blobs[fine ? BLOB_FINE : BLOB_MID];
    float swing = sinf(stride) * (crouch ? 0.1f : 0.18f);
    float drop = crouch ? 0.38f : 0.0f;
    float legH = 0.84f - drop;
    Color skin = look.skin, foot = Mix(skin, COL_HAIR, 0.35f);

    if (!SameColor(look.top, skin)) look.top = Clothe(look.top, skin);
    look.cloth = Clothe(look.cloth, skin);

    Limb(-0.11f, 0, swing, 0.085f, legH + 0.05f, skin, limb);
    Limb(0.11f, 0, -swing, 0.085f, legH + 0.05f, skin, limb);
    if (fine) {
        BlobQuads(bm, (Vector3){ -0.11f, 0.05f, swing + 0.06f }, (Vector3){ 0.075f, 0.05f, 0.13f }, foot);
        BlobQuads(bm, (Vector3){ 0.11f, 0.05f, -swing + 0.06f }, (Vector3){ 0.075f, 0.05f, 0.13f }, foot);
    } else {
        DrawShadedBox((Vector3){ -0.11f, 0.04f, swing + 0.05f }, (Vector3){ 0.15f, 0.08f, 0.26f }, foot);
        DrawShadedBox((Vector3){ 0.11f, 0.04f, -swing + 0.05f }, (Vector3){ 0.15f, 0.08f, 0.26f }, foot);
    }
    rlTranslatef(0, -drop, crouch ? 0.06f : 0.0f);
    DrawShadedCylinder((Vector3){ 0, 0.68f, 0 }, 0.28f, 0.19f, 0.56f, 0.78f, body, look.cloth);
    DrawShadedCylinder((Vector3){ 0, 1.16f, 0 }, 0.23f, 0.15f, 0.42f, 0.95f, body, look.top);
    BlobQuads(bm, (Vector3){ 0, 1.56f, 0 }, (Vector3){ 0.27f, 0.08f, 0.15f }, look.top);
    if (fine) DrawShadedCylinder((Vector3){ 0, 1.15f, 0 }, 0.232f, 0.158f, 0.07f, 1.0f, body, Mix(look.cloth, COL_HAIR, 0.5f));

    if (bow) {
        float string = 0.6f - 0.42f * draw;
        DrawShadedBox((Vector3){ -0.22f, 1.48f, 0.3f }, (Vector3){ 0.11f, 0.11f, 0.6f }, skin);
        DrawShadedBox((Vector3){ 0.02f, 1.48f, fmaxf(string, 0.2f) / 2 }, (Vector3){ 0.11f, 0.11f, fmaxf(string, 0.2f) }, skin);
        DrawShadedBox((Vector3){ -0.22f, 1.48f, 0.64f }, (Vector3){ 0.05f, 0.8f, 0.06f }, COL_WOOD);
        DrawShadedBox((Vector3){ -0.22f, 1.95f, 0.58f }, (Vector3){ 0.04f, 0.2f, 0.06f }, COL_WOOD);
        DrawShadedBox((Vector3){ -0.22f, 1.01f, 0.58f }, (Vector3){ 0.04f, 0.2f, 0.06f }, COL_WOOD);
        DrawShadedBox((Vector3){ -0.22f, 1.48f, string }, (Vector3){ 0.02f, 0.9f, 0.02f }, COL_LINEN);
        if (draw > 0.05f)
            DrawShadedBox((Vector3){ -0.2f, 1.5f, string + 0.38f }, (Vector3){ 0.03f, 0.03f, 0.76f }, COL_REED);
    } else {
        Limb(-0.31f, 0.98f, -swing, 0.065f, 0.6f, skin, limb);
        Limb(0.31f, 0.98f, swing, 0.065f, 0.6f, skin, limb);
        if (fine) {
            BlobQuads(bm, (Vector3){ -0.31f, 0.95f, -swing }, (Vector3){ 0.06f, 0.075f, 0.055f }, skin);
            BlobQuads(bm, (Vector3){ 0.31f, 0.95f, swing }, (Vector3){ 0.06f, 0.075f, 0.055f }, skin);
        }
    }

    Limb(0, 1.58f, 0, 0.07f, 0.1f, skin, limb);
    BlobQuads(bm, (Vector3){ 0, 1.79f, 0.01f }, (Vector3){ 0.15f, 0.17f, 0.16f }, skin);
    BlobQuads(bm, (Vector3){ 0, 1.86f, -0.02f }, (Vector3){ 0.165f, 0.12f, 0.17f }, look.hair);
    DrawShadedBox((Vector3){ 0, 1.77f, 0.16f }, (Vector3){ 0.05f, 0.07f, 0.05f }, skin);
    if (fine)
        for (int s = -1; s <= 1; s += 2)
            DrawShadedBox((Vector3){ 0.055f * (float)s, 1.825f, 0.148f }, (Vector3){ 0.035f, 0.024f, 0.02f }, (Color){ 34, 26, 22, 255 });
}

static bool NearCamera(Vector3 p)
{
    return Vector3Distance(p, G.cam.position) < 26.0f;
}

void DrawHuman(Vector3 pos, float facing, float stride, Look look, bool crouch, bool bow, float draw)
{
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    HumanModel(stride, look, crouch, bow, draw, NearCamera(pos));
    rlPopMatrix();
}

void DrawPersonBody(Vector3 pos, float facing, Look look)
{
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + 0.16f, pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    rlRotatef(-90.0f, 1, 0, 0);
    HumanModel(0, look, false, false, 0, NearCamera(pos));
    rlPopMatrix();
}
