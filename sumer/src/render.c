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

// Whether something at p, about 'size' across, can be in the picture or throw a shadow into it: anything
// within a few metres of the eye, else anything inside the view widened by a margin. People and animals
// behind the camera are not drawn at all, which is most of the cost of a frame.
bool InView(Vector3 p, float size)
{
    Vector3 d = Vector3Subtract(p, G.cam.position), f = Vector3Subtract(G.cam.target, G.cam.position);
    float dist = Vector3Length(d), fl = Vector3Length(f);
    if (dist < 10.0f + size || fl < 1e-4f) return true;
    float aspect = (float)GetScreenWidth() / fmaxf(1.0f, (float)GetScreenHeight());
    float half = atanf(tanf(G.cam.fovy * 0.5f * DEG2RAD) * sqrtf(1.0f + aspect * aspect));
    return Vector3DotProduct(d, f) / (dist * fl) > cosf(fminf(half + 0.26f + atanf(size / dist), PI));
}

Color Mix(Color a, Color b, float t)
{
    return (Color){ (unsigned char)Lerp(a.r, b.r, t), (unsigned char)Lerp(a.g, b.g, t),
                    (unsigned char)Lerp(a.b, b.b, t), (unsigned char)Lerp(a.a, b.a, t) };
}

// Daylight plus warm torchlight set by LightFor() for the object being drawn. With the shaders on
// the color goes out as it is: they do the light.
Color Lit(Color c, float shade)
{
    if (GfxOn()) return c;
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
    if (GfxOn()) return c;
    f = 0.85f + 0.15f * f;
    return (Color){ (unsigned char)(c.r * f), (unsigned char)(c.g * f), (unsigned char)(c.b * f), c.a };
}

static float flash;   // something just struck goes pale for a moment (FlashAt)

void SetDrawFlash(float k)
{
    flash = Clamp(k, 0.0f, 1.0f);
}

float DrawFlash(void)
{
    return flash;
}

static void SetColor(Color c)
{
    VColor(flash > 0 ? Mix(c, (Color){ 255, 244, 232, c.a }, flash * 0.75f) : c);
}

// Fixed per-face shading stands in for real lighting; the shaders use the face normals instead.
// Something glowing (fire, embers) is drawn as glowing whatever material is set.
static void BoxQuads(Vector3 c, Vector3 s, Color col, bool lit)
{
    float x0 = c.x - s.x / 2, x1 = c.x + s.x / 2;
    float y0 = c.y - s.y / 2, y1 = c.y + s.y / 2;
    float z0 = c.z - s.z / 2, z1 = c.z + s.z / 2;
    int mat = CurrentMaterial();

    if (!lit) SetMaterial(MAT_GLOW);
    rlCheckRenderBatchLimit(24);
    VBegin(RL_QUADS);
    SetColor(Shade(col, 1.00f, lit));
    VNormal(0, 1, 0);
    VVertex(x0, y1, z0); VVertex(x0, y1, z1); VVertex(x1, y1, z1); VVertex(x1, y1, z0);
    SetColor(Shade(col, 0.86f, lit));
    VNormal(0, 0, 1);
    VVertex(x0, y0, z1); VVertex(x1, y0, z1); VVertex(x1, y1, z1); VVertex(x0, y1, z1);
    SetColor(Shade(col, 0.70f, lit));
    VNormal(0, 0, -1);
    VVertex(x1, y0, z0); VVertex(x0, y0, z0); VVertex(x0, y1, z0); VVertex(x1, y1, z0);
    SetColor(Shade(col, 0.78f, lit));
    VNormal(1, 0, 0);
    VVertex(x1, y0, z1); VVertex(x1, y0, z0); VVertex(x1, y1, z0); VVertex(x1, y1, z1);
    SetColor(Shade(col, 0.62f, lit));
    VNormal(-1, 0, 0);
    VVertex(x0, y0, z0); VVertex(x0, y0, z1); VVertex(x0, y1, z1); VVertex(x0, y1, z0);
    if (y0 > 0.05f) {
        SetColor(Shade(col, 0.5f, lit));
        VNormal(0, -1, 0);
        VVertex(x0, y0, z0); VVertex(x1, y0, z0); VVertex(x1, y0, z1); VVertex(x0, y0, z1);
    }
    VEnd();
    SetMaterial(mat);
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

// A point's color and the way it faces: the shaders light it by the normal, the classic drawing by a
// fixed shade from it.
void VPaint(Color c, Vector3 n)
{
    if (GfxOn()) {
        VNormal(n.x, n.y, n.z);
        SetColor(c);
    } else {
        SetColor(Lit(c, NormalShade(n.x, n.y, n.z)));
    }
}

// The top cap is a fan of degenerate quads. The sides carry smooth normals, so a column looks round
// in the shaders' light.
void DrawShadedCylinder(Vector3 base, float rx, float rz, float h, float topScale, int sides, Color col)
{
    float slope = (1.0f - topScale) * fmaxf(rx, rz) / fmaxf(h, 0.01f);
    float y0 = base.y, y1 = base.y + h;
    float ny = slope / sqrtf(1.0f + slope * slope), nh = 1.0f / sqrtf(1.0f + slope * slope);
    bool normals = GfxOn();

    rlCheckRenderBatchLimit(sides * 8);
    VBegin(RL_QUADS);
    for (int i = 0; i < sides; i++) {
        float a0 = 2.0f * PI * (float)i / (float)sides, a1 = 2.0f * PI * (float)(i + 1) / (float)sides, am = (a0 + a1) / 2;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        float n0x = c0 * rz, n0z = s0 * rx, n1x = c1 * rz, n1z = s1 * rx;
        if (normals) {
            float l0 = sqrtf(n0x * n0x + n0z * n0z), l1 = sqrtf(n1x * n1x + n1z * n1z);
            n0x *= nh / l0; n0z *= nh / l0; n1x *= nh / l1; n1z *= nh / l1;
        }
        SetColor(Lit(col, NormalShade(cosf(am), ny, sinf(am))));
        if (normals) VNormal(n0x, ny, n0z);
        VVertex(base.x + rx * c0, y0, base.z + rz * s0);
        VVertex(base.x + rx * c0 * topScale, y1, base.z + rz * s0 * topScale);
        if (normals) VNormal(n1x, ny, n1z);
        VVertex(base.x + rx * c1 * topScale, y1, base.z + rz * s1 * topScale);
        VVertex(base.x + rx * c1, y0, base.z + rz * s1);
        if (topScale > 0.01f) {
            SetColor(Lit(col, 1.0f));
            if (normals) VNormal(0, 1, 0);
            VVertex(base.x, y1, base.z);
            VVertex(base.x + rx * c1 * topScale, y1, base.z + rz * s1 * topScale);
            VVertex(base.x + rx * c0 * topScale, y1, base.z + rz * s0 * topScale);
            VVertex(base.x + rx * c0 * topScale, y1, base.z + rz * s0 * topScale);
        }
    }
    VEnd();
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

// Leaves bulge in clumps: a few waves over the sphere, different for each crown, the same at every level
// of detail so the far crown keeps its outline.
static float Lump(Vector3 c, float x, float y, float z)
{
    float ph = c.x * 1.7f + c.z * 2.3f;
    return 1.0f + 0.17f * sinf(5.0f * x + ph) * sinf(4.0f * z + 1.3f * ph) + 0.12f * sinf(7.0f * y + 3.0f * x + 0.7f * ph)
         + 0.08f * sinf(9.0f * z - 6.0f * y + ph);
}

static void BlobPoint(Vector3 c, Vector3 r, float cp, float sp, float ct, float st, bool normals, bool lumpy)
{
    float x = cp * ct, z = cp * st, k = lumpy ? Lump(c, x, sp, z) : 1.0f;
    if (normals) VNormal(x / r.x, sp / r.y, z / r.z);
    VVertex(c.x + r.x * x * k, c.y + r.y * sp * k, c.z + r.z * z * k);
}

// Smooth normals in the shaders' light: a blob looks round however few its facets.
static void BlobQuads(const BlobMesh *b, Vector3 c, Vector3 r, Color col)
{
    // Leaves (alpha 253) are lumpy and lit facet by facet (no normal: the shader takes the face's).
    bool lumpy = col.a == 253, normals = GfxOn() && !lumpy;
    if (!blobReady) BlobTables();
    rlCheckRenderBatchLimit(b->slices * b->stacks * 4);
    VBegin(RL_QUADS);
    for (int j = 0; j < b->stacks; j++) {
        float cp0 = b->cp[j], sp0 = b->sp[j], cp1 = b->cp[j + 1], sp1 = b->sp[j + 1];
        for (int i = 0; i < b->slices; i++) {
            float ct0 = b->ct[i], st0 = b->st[i], ct1 = b->ct[i + 1], st1 = b->st[i + 1];
            SetColor(Lit(col, b->shade[j][i]));
            BlobPoint(c, r, cp0, sp0, ct0, st0, normals, lumpy);
            BlobPoint(c, r, cp1, sp1, ct0, st0, normals, lumpy);
            BlobPoint(c, r, cp1, sp1, ct1, st1, normals, lumpy);
            BlobPoint(c, r, cp0, sp0, ct1, st1, normals, lumpy);
        }
    }
    VEnd();
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
    VBegin(RL_QUADS);
    for (int i = 0; i < 26; i++) {
        const BevelPoly *p = &bevels[i];
        if (grounded && p->n[1] < 0) continue;
        SetColor(Lit(col, p->shade));
        VNormal((float)p->n[0], (float)p->n[1], (float)p->n[2]);
        for (int k = 0; k < 4; k++) {
            int at = k < p->count ? k : p->count - 1;
            Vector3 v = BevelPoint(p->corner[at], p->axis[at], e, b);
            if (grounded && !(p->corner[at] & 2)) v.y = -e.y;
            VVertex(c.x + v.x, c.y + v.y, c.z + v.z);
        }
    }
    VEnd();
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
