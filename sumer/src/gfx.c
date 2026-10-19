// The shader renderer. The world is drawn by the same code as ever, but instead of going to the
// screen its vertices are caught: what never moves (ground, water, roads, buildings, plants) once,
// cut into 60 m chunks kept on the GPU at three levels of detail, and the people, animals and
// everything else that moves afresh every frame, into buffers drawn twice (for the sun's shadow and
// for the eye). The shaders do the light: the sun's (or moon's) shadow, torches at night, brick,
// soil, wood and water worked into the surfaces, the sky, fog, and a glow round bright things.
//
// Settings: High (desktop), Low (phones: smaller frame, one shadow tap, no glow) and Classic, the
// old way of drawing with colors lit on the CPU, which also runs wherever these shaders cannot.
#include "game.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern const char *SH_WORLD_VS, *SH_COMMON, *SH_SKY_VS, *SH_SKY_FS, *SH_WORLD_FS, *SH_DEPTH_VS, *SH_DEPTH_FS;
extern const char *SH_BRIGHT_FS, *SH_BLUR_FS, *SH_POST_FS;

#define GFX_FILE        "sumer.gfx"
#define CHUNK           60.0f
#define CHUNKS          18                  // 2 * WORLD_HALF / CHUNK
#define LODS            3
#define BLOCK_VERTS     65536               // as many as 16-bit indices reach
#define MAX_DYN_BLOCKS  8
#define GRID_CELL       8.0f                // the torch grid: which torches reach each 8 m square
#define GRID_N          135
#define GRID_SLOTS      8
#define TORCH_REACH     10.0f
#define SKY_RADIUS      900.0f
#define TEX_SHADOW      10                  // texture units the world shader reads from
#define TEX_HEIGHT      11
#define TEX_GRID        12
#define TEX_TORCH       13

// 24 bytes: position, normal (0 when the shader is to work it out), color, material.
typedef struct { float x, y, z; signed char n[4]; unsigned char c[4]; unsigned char m[4]; } GVert;
typedef struct { GVert *v; int n, cap; } VList;
typedef struct { unsigned int vao, vbo; int verts; } Block;
typedef struct { Block *blocks; int count; bool built; } GMesh;
typedef struct {
    GMesh ground, things[LODS];
    Vector3 lo, hi;
    int *boxes, boxCount, boxCap, *res, resCount, resCap;
    unsigned int sig;
    int lod;                    // the detail its things are drawn at this frame; -1 none
} Chunk;

typedef struct {
    int mvp, camPos, sunDir, sunGlow, skyZen, skyHor, exposure, quality, lightDir, lightCol, ambSky, ambGround;
    int torchCol, fogDensity, time, worldHalf, night, wet, shadowTexel, shadowRange, groundTile, groundN;
    int heightScale, heightBase, torchSlots, gridN, gridCell, torchRadius, dynCount, dynLights, lightVP;
    int shadowMap, heightMap, torchGrid, torchData, moonDir, cloudy, texel, bloom, bloomAmt, dir;
} Locs;

typedef struct {
    Vector3 lightDir, lightCol, sunDir, sunGlow, moonDir, zen, hor, ambSky, ambGround;
    float exposure, fog, day, wet;
} SkyNow;

#if defined(SUMER_TEST) && defined(__EMSCRIPTEN__)
// The browser suites run in software GL, too slow for the shaders: they test the game, not the
// pictures, so the test build draws the classic way unless the page asks (?gfx=auto, high or low).
static int choice = GFX_CLASSIC;
#else
static int choice = GFX_AUTO;
#endif
static bool inited, failed, on, drawAll, settingLoaded;
static Shader worldSh, skySh, depthSh, brightSh, blurSh, postSh;
static Locs wl, sl, dl, pl, bl;
static int worldGen = 1, builtGen, builtBoxes, builtRes, builtTorches = -1;
static Chunk chunks[CHUNKS * CHUNKS];
static unsigned int quadIndex;
static Block dynBlocks[MAX_DYN_BLOCKS], skyBlock;
static int dynUsed;
static VList dyn, scratch;
static Texture2D heightTex, gridTex, torchTex;
static float heightScale = 1.0f, heightBase;
static RenderTexture2D scene, brightRT, blurRT, shadowRT;
static int sceneW, sceneH, shadowSize, targetTier = -1;
static double frameStart;
static bool firstBuild, stats;     // stats: GFX_STATS=1 in the environment prints mesh sizes and build times

// ---- The vertex stream: to rlgl's batch, or caught into a list ----

static VList *sink;
static int material = MAT_THING, vMode, vInPrim;
static GVert cur = { 0, 0, 0, { 0 }, { 255, 255, 255, 255 }, { 0 } };
static Matrix vm;
static bool vmOn;

bool GfxOn(void) { return on; }
bool DrawingAll(void) { return drawAll; }
void SetMaterial(int m) { material = m; }
int CurrentMaterial(void) { return material; }

static void Push(VList *l, const GVert *v)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 65536;
        l->v = realloc(l->v, (size_t)l->cap * sizeof(GVert));
    }
    l->v[l->n++] = *v;
}

void VBegin(int mode)
{
    vMode = mode;
    vInPrim = 0;
    cur.n[0] = cur.n[1] = cur.n[2] = 0;
    cur.m[0] = (unsigned char)material;
    if (sink) {
        // Inside rlPushMatrix the draw code moves and turns its parts; outside it this is the identity.
        vm = rlGetMatrixTransform();
        vmOn = !(vm.m0 == 1.0f && vm.m5 == 1.0f && vm.m10 == 1.0f && vm.m12 == 0.0f && vm.m13 == 0.0f && vm.m14 == 0.0f &&
                 vm.m1 == 0.0f && vm.m2 == 0.0f && vm.m4 == 0.0f && vm.m6 == 0.0f && vm.m8 == 0.0f && vm.m9 == 0.0f);
        return;
    }
    rlBegin(mode);
    if (on) {
        rlTexCoord2f((float)material, 0);
        rlNormal3f(0, 0, 0);
    }
}

void VEnd(void)
{
    if (!sink) rlEnd();
}

void VColor(Color c)
{
    cur.c[0] = c.r;
    cur.c[1] = c.g;
    cur.c[2] = c.b;
    cur.c[3] = c.a;
    if (!sink) rlColor4ub(c.r, c.g, c.b, c.a);
}

void VNormal(float x, float y, float z)
{
    if (!sink) {
        if (on) rlNormal3f(x, y, z);
        return;
    }
    if (vmOn) {
        float tx = vm.m0 * x + vm.m4 * y + vm.m8 * z, ty = vm.m1 * x + vm.m5 * y + vm.m9 * z, tz = vm.m2 * x + vm.m6 * y + vm.m10 * z;
        x = tx;
        y = ty;
        z = tz;
    }
    float l = sqrtf(x * x + y * y + z * z);
    float k = l > 1e-6f ? 127.0f / l : 0.0f;
    cur.n[0] = (signed char)lrintf(x * k);
    cur.n[1] = (signed char)lrintf(y * k);
    cur.n[2] = (signed char)lrintf(z * k);
}

void VVertex(float x, float y, float z)
{
    if (!sink) {
        rlVertex3f(x, y, z);
        return;
    }
    if (vmOn) {
        cur.x = vm.m0 * x + vm.m4 * y + vm.m8 * z + vm.m12;
        cur.y = vm.m1 * x + vm.m5 * y + vm.m9 * z + vm.m13;
        cur.z = vm.m2 * x + vm.m6 * y + vm.m10 * z + vm.m14;
    } else {
        cur.x = x;
        cur.y = y;
        cur.z = z;
    }
    Push(sink, &cur);
    // Triangles become quads with their last corner twice, so everything shares the quad indices.
    if (vMode == RL_TRIANGLES && ++vInPrim == 3) {
        Push(sink, &cur);
        vInPrim = 0;
    }
}

static void Catch(VList *l)
{
    l->n = 0;
    sink = l;
}

static void Release(void)
{
    sink = NULL;
    material = MAT_THING;
}

// ---- GPU buffers ----

static Block NewBlock(const GVert *v, int n, bool dynamic, int capacity)
{
    Block b = { 0 };
    b.vao = rlLoadVertexArray();
    rlEnableVertexArray(b.vao);
    b.vbo = rlLoadVertexBuffer(v, capacity * (int)sizeof(GVert), dynamic);
    if (!quadIndex) {
        int quads = BLOCK_VERTS / 4;
        unsigned short *ix = malloc((size_t)quads * 6 * sizeof(unsigned short));
        for (int q = 0; q < quads; q++) {
            unsigned short a = (unsigned short)(q * 4);
            unsigned short *o = ix + q * 6;
            o[0] = a; o[1] = (unsigned short)(a + 1); o[2] = (unsigned short)(a + 2);
            o[3] = a; o[4] = (unsigned short)(a + 2); o[5] = (unsigned short)(a + 3);
        }
        quadIndex = rlLoadVertexBufferElement(ix, quads * 6 * (int)sizeof(unsigned short), false);
        free(ix);
    } else {
        rlEnableVertexBufferElement(quadIndex);
    }
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, (int)sizeof(GVert), 0);
    rlEnableVertexAttribute(0);
    rlSetVertexAttribute(2, 3, 0x1400 /* GL_BYTE */, true, (int)sizeof(GVert), 12);
    rlEnableVertexAttribute(2);
    rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, true, (int)sizeof(GVert), 16);
    rlEnableVertexAttribute(3);
    rlSetVertexAttribute(1, 2, RL_UNSIGNED_BYTE, false, (int)sizeof(GVert), 20);
    rlEnableVertexAttribute(1);
    rlDisableVertexArray();
    b.verts = n;
    return b;
}

static void FreeBlock(Block *b)
{
    if (b->vao) rlUnloadVertexArray(b->vao);
    if (b->vbo) rlUnloadVertexBuffer(b->vbo);
    *b = (Block){ 0 };
}

static void FreeMesh(GMesh *m)
{
    for (int i = 0; i < m->count; i++) FreeBlock(&m->blocks[i]);
    free(m->blocks);
    *m = (GMesh){ 0 };
}

static void MeshFrom(GMesh *m, const GVert *v, int n)
{
    FreeMesh(m);
    m->built = true;
    n -= n % 4;
    m->count = (n + BLOCK_VERTS - 1) / BLOCK_VERTS;
    if (!m->count) return;
    m->blocks = calloc((size_t)m->count, sizeof(Block));
    for (int b = 0; b < m->count; b++) {
        int k = n - b * BLOCK_VERTS < BLOCK_VERTS ? n - b * BLOCK_VERTS : BLOCK_VERTS;
        m->blocks[b] = NewBlock(v + b * BLOCK_VERTS, k, false, k);
    }
}

static void DrawBlock(const Block *b)
{
    if (b->verts < 4) return;
    rlEnableVertexArray(b->vao);
    rlDrawVertexArrayElements(0, b->verts / 4 * 6, 0);
}

static void DrawMeshBlocks(const GMesh *m)
{
    for (int i = 0; i < m->count; i++) DrawBlock(&m->blocks[i]);
}

// ---- Shaders ----

static char *Join(const char *a, const char *b, const char *c)
{
    size_t la = strlen(a), lb = b ? strlen(b) : 0, lc = strlen(c);
    char *s = malloc(la + lb + lc + 1);
    memcpy(s, a, la);
    if (b) memcpy(s + la, b, lb);
    memcpy(s + la + lb, c, lc + 1);
    return s;
}

static Shader Compile(const char *vs, const char *common, const char *fs)
{
#if defined(__EMSCRIPTEN__)
    const char *head = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n";
#else
    const char *head = "#version 330\n";
#endif
    char *v = vs ? Join(head, NULL, vs) : NULL, *f = Join(head, common, fs);
    Shader s = LoadShaderFromMemory(v, f);
    free(v);
    free(f);
    if (s.id == 0 || s.id == rlGetShaderIdDefault()) failed = true;
    return s;
}

static Locs Find(Shader s)
{
    Locs l;
#define L_(name) l.name = GetShaderLocation(s, #name)
    L_(mvp); L_(camPos); L_(sunDir); L_(sunGlow); L_(skyZen); L_(skyHor); L_(exposure); L_(quality); L_(lightDir);
    L_(lightCol); L_(ambSky); L_(ambGround); L_(torchCol); L_(fogDensity); L_(time); L_(worldHalf); L_(night); L_(wet);
    L_(shadowTexel); L_(shadowRange); L_(groundTile); L_(groundN); L_(heightScale); L_(heightBase); L_(torchSlots);
    L_(gridN); L_(gridCell); L_(torchRadius); L_(dynCount); L_(dynLights); L_(lightVP); L_(shadowMap); L_(heightMap);
    L_(torchGrid); L_(torchData); L_(moonDir); L_(cloudy); L_(texel); L_(bloom); L_(bloomAmt); L_(dir);
#undef L_
    return l;
}

static void U1(int loc, float v) { rlSetUniform(loc, &v, RL_SHADER_UNIFORM_FLOAT, 1); }
static void UI(int loc, int v) { rlSetUniform(loc, &v, RL_SHADER_UNIFORM_INT, 1); }
static void U3(int loc, Vector3 v) { rlSetUniform(loc, &v, RL_SHADER_UNIFORM_VEC3, 1); }

// A ball of 24 x 12 quads round the camera, drawn from inside.
static void MakeSky(void)
{
    const int slices = 24, stacks = 12;
    VList l = { 0 };
    Catch(&l);
    VBegin(RL_QUADS);
    VColor(WHITE);
    for (int j = 0; j < stacks; j++) {
        float p0 = -PI / 2 + PI * (float)j / (float)stacks, p1 = -PI / 2 + PI * (float)(j + 1) / (float)stacks;
        for (int i = 0; i < slices; i++) {
            float t0 = 2 * PI * (float)i / (float)slices, t1 = 2 * PI * (float)(i + 1) / (float)slices;
            VVertex(cosf(p0) * cosf(t0), sinf(p0), cosf(p0) * sinf(t0));
            VVertex(cosf(p1) * cosf(t0), sinf(p1), cosf(p1) * sinf(t0));
            VVertex(cosf(p1) * cosf(t1), sinf(p1), cosf(p1) * sinf(t1));
            VVertex(cosf(p0) * cosf(t1), sinf(p0), cosf(p0) * sinf(t1));
        }
    }
    VEnd();
    Release();
    skyBlock = NewBlock(l.v, l.n, false, l.n);
    free(l.v);
}

static void Init(void)
{
    inited = true;
    if (!settingLoaded) GfxLoadSetting();
    stats = getenv("GFX_STATS") != NULL;
    int gl = rlGetVersion();
    if (gl != RL_OPENGL_33 && gl != RL_OPENGL_43 && gl != RL_OPENGL_ES_30) {
        failed = true;
        return;
    }
    worldSh = Compile(SH_WORLD_VS, SH_COMMON, SH_WORLD_FS);
    skySh = Compile(SH_SKY_VS, SH_COMMON, SH_SKY_FS);
    depthSh = Compile(SH_DEPTH_VS, NULL, SH_DEPTH_FS);
    brightSh = Compile(NULL, NULL, SH_BRIGHT_FS);
    blurSh = Compile(NULL, NULL, SH_BLUR_FS);
    postSh = Compile(NULL, NULL, SH_POST_FS);
    if (failed) {
        TraceLog(LOG_WARNING, "GFX: the shaders did not build; drawing the classic way");
        return;
    }
    wl = Find(worldSh);
    sl = Find(skySh);
    dl = Find(depthSh);
    pl = Find(postSh);
    bl = Find(blurSh);
    MakeSky();
}

// ---- The setting ----

static bool Phone(void)
{
    static int phone = -1;
    if (phone < 0) phone = OnWeb() && OnPhone();
    return phone != 0;
}

static bool autoLow;    // Auto found High too slow here

int GfxTier(void)
{
    if (failed || choice == GFX_CLASSIC) return GFX_CLASSIC;
    if (choice == GFX_AUTO) return Phone() || autoLow ? GFX_LOW : GFX_HIGH;
    return choice;
}

// Auto steps down to Low for the rest of the session when High cannot keep up: most of six seconds of
// play spent in frames slower than 1/24 s. A long single frame (a pause, a hidden tab) does not count.
static void WatchSpeed(void)
{
    static float watched, slow;
    float dt = GetFrameTime();
    if (choice != GFX_AUTO || GfxTier() != GFX_HIGH || dt > 0.5f) return;
    watched += dt;
    if (dt > 1.0f / 24.0f) slow += dt;
    if (watched < 6.0f) return;
    if (slow > 0.7f * watched) {
        autoLow = true;
        Say("Graphics: %s.", GfxLabel());
    }
    watched = slow = 0;
}

int GfxChoice(void)
{
    return choice;
}

const char *GfxChoiceName(int c)
{
    static const char *names[GFX_CHOICES] = { "Auto", "High", "Low", "Classic" };
    return names[c >= 0 && c < GFX_CHOICES ? c : 0];
}

// What the menus show: the setting, and for Auto what it picked here.
const char *GfxLabel(void)
{
    if (choice != GFX_AUTO) return L(GfxChoiceName(choice));
    return TextFormat(L("Auto (%s)"), L(GfxChoiceName(GfxTier())));
}

void GfxSetChoice(int c)
{
    choice = (c % GFX_CHOICES + GFX_CHOICES) % GFX_CHOICES;
    FILE *f = fopen(DataPath(GFX_FILE), "w");
    if (!f) return;
    fprintf(f, "%s\n", GfxChoiceName(choice));
    fclose(f);
    SyncStorage();
}

void GfxLoadSetting(void)
{
    char buf[16] = "";
    settingLoaded = true;
    // For tests and screenshots, SUMER_GFX=High|Low|Classic (or ?gfx=low in the page's address)
    // overrides the saved setting for this run.
    const char *env = OnWeb() ? PageParam("gfx") : getenv("SUMER_GFX");
    if (env && env[0]) {
        for (int c = 0; c < GFX_CHOICES; c++)
            if (strcasecmp(env, GfxChoiceName(c)) == 0) choice = c;
        return;
    }
    FILE *f = fopen(DataPath(GFX_FILE), "r");
    if (!f) return;
    if (fgets(buf, sizeof(buf), f))
        for (int c = 0; c < GFX_CHOICES; c++)
            if (strncmp(buf, GfxChoiceName(c), strlen(GfxChoiceName(c))) == 0) choice = c;
    fclose(f);
}

void GfxWorldChanged(void)
{
    worldGen++;
}

// ---- The still world in chunks ----

static int ChunkAt(float x, float z)
{
    int i = (int)floorf((x + WORLD_HALF) / CHUNK), j = (int)floorf((z + WORLD_HALF) / CHUNK);
    i = i < 0 ? 0 : i >= CHUNKS ? CHUNKS - 1 : i;
    j = j < 0 ? 0 : j >= CHUNKS ? CHUNKS - 1 : j;
    return j * CHUNKS + i;
}

static void Grow(Chunk *c, Vector3 lo, Vector3 hi)
{
    c->lo = Vector3Min(c->lo, lo);
    c->hi = Vector3Max(c->hi, hi);
}

static void AddIndex(int **list, int *count, int *cap, int i)
{
    if (*count == *cap) {
        *cap = *cap ? *cap * 2 : 64;
        *list = realloc(*list, (size_t)*cap * sizeof(int));
    }
    (*list)[(*count)++] = i;
}

static void DropThings(Chunk *c)
{
    for (int l = 0; l < LODS; l++) FreeMesh(&c->things[l]);
}

static unsigned int ResSig(const Chunk *c)
{
    unsigned int h = 2166136261u;
    for (int k = 0; k < c->resCount; k++) h = (h ^ (unsigned int)(G.res[c->res[k]].regrow > 0)) * 16777619u;
    return h;
}

// Boxes and resources made since the last look (a palm planted, a game loaded) join their chunks.
static void TakeNew(void)
{
    for (int i = builtBoxes; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (b->water || FieldBox(i)) continue;
        Chunk *c = &chunks[ChunkAt(b->pos.x, b->pos.z)];
        AddIndex(&c->boxes, &c->boxCount, &c->boxCap, i);
        Vector3 h = Vector3Scale(b->size, 0.5f);
        Grow(c, Vector3Subtract(b->pos, h), Vector3Add(b->pos, h));
        DropThings(c);
    }
    for (int i = builtRes; i < G.resCount; i++) {
        const Resource *r = &G.res[i];
        if (r->kind == RES_FISH) continue;
        Chunk *c = &chunks[ChunkAt(r->pos.x, r->pos.z)];
        AddIndex(&c->res, &c->resCount, &c->resCap, i);
        Grow(c, Vector3Add(r->pos, (Vector3){ -3.5f, -0.5f, -3.5f }), Vector3Add(r->pos, (Vector3){ 3.5f, 9.5f, 3.5f }));
        DropThings(c);
    }
    builtBoxes = G.boxCount;
    builtRes = G.resCount;
}

static unsigned short Half(float f)
{
    unsigned int x;
    memcpy(&x, &f, 4);
    unsigned int sign = (x >> 16) & 0x8000u, e = (x >> 23) & 0xffu, m = x & 0x7fffffu;
    int ex = (int)e - 127 + 15;
    if (ex <= 0) return (unsigned short)sign;
    if (ex >= 31) return (unsigned short)(sign | 0x7c00u);
    return (unsigned short)(sign | ((unsigned int)ex << 10) | (m >> 13));
}

// The ground's height for the shaders (their ambient shade near the ground, the depth of water):
// half floats where the GPU takes them, else bytes over the height range.
static void MakeHeights(void)
{
    const int n = GROUND_TILES + 1;
    if (heightTex.id) UnloadTexture(heightTex);
    unsigned short *h = malloc((size_t)(n * n) * sizeof(unsigned short));
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) h[j * n + i] = Half(G.heights[i][j]);
    Image im = { h, n, n, 1, PIXELFORMAT_UNCOMPRESSED_R16 };
    heightTex = LoadTextureFromImage(im);
    free(h);
    heightScale = 1.0f;
    heightBase = 0.0f;
    if (!heightTex.id) {
        float lo = 1e9f, hi = -1e9f;
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) {
                lo = fminf(lo, G.heights[i][j]);
                hi = fmaxf(hi, G.heights[i][j]);
            }
        unsigned char *b = malloc((size_t)(n * n));
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) b[j * n + i] = (unsigned char)lrintf((G.heights[i][j] - lo) / fmaxf(hi - lo, 0.01f) * 255.0f);
        Image bi = { b, n, n, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE };
        heightTex = LoadTextureFromImage(bi);
        free(b);
        heightScale = fmaxf(hi - lo, 0.01f);
        heightBase = lo;
    }
    SetTextureFilter(heightTex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(heightTex, TEXTURE_WRAP_CLAMP);
}

// Which torches reach each square of the torch grid, nearest first, and where each torch is.
static void MakeTorches(void)
{
    static unsigned char grid[GRID_N][GRID_N * 2][4];
    float *data = calloc((size_t)(MAX_TORCHES > 1 ? MAX_TORCHES : 1) * 4, sizeof(float));
    memset(grid, 0, sizeof(grid));
    for (int t = 0; t < G.torchCount; t++) {
        data[t * 4 + 0] = G.torches[t].x;
        data[t * 4 + 1] = G.torches[t].y;
        data[t * 4 + 2] = G.torches[t].z;
        data[t * 4 + 3] = (float)t;
    }
    for (int gj = 0; gj < GRID_N; gj++) {
        for (int gi = 0; gi < GRID_N; gi++) {
            float x0 = -WORLD_HALF + (float)gi * GRID_CELL, z0 = -WORLD_HALF + (float)gj * GRID_CELL;
            int idx[GRID_SLOTS], n = 0;
            float dist[GRID_SLOTS];
            for (int t = 0; t < G.torchCount && t < 255; t++) {
                Vector3 p = G.torches[t];
                float dx = fmaxf(fmaxf(x0 - p.x, p.x - (x0 + GRID_CELL)), 0.0f), dz = fmaxf(fmaxf(z0 - p.z, p.z - (z0 + GRID_CELL)), 0.0f);
                if (dx * dx + dz * dz > TORCH_REACH * TORCH_REACH) continue;
                float cx = x0 + GRID_CELL / 2 - p.x, cz = z0 + GRID_CELL / 2 - p.z, d = cx * cx + cz * cz;
                int k = n < GRID_SLOTS ? n++ : GRID_SLOTS;
                if (k == GRID_SLOTS) {
                    if (d >= dist[GRID_SLOTS - 1]) continue;
                    k = GRID_SLOTS - 1;
                }
                for (; k > 0 && dist[k - 1] > d; k--) {
                    dist[k] = dist[k - 1];
                    idx[k] = idx[k - 1];
                }
                dist[k] = d;
                idx[k] = t;
            }
            for (int s = 0; s < n; s++) grid[gj][gi * 2 + s / 4][s % 4] = (unsigned char)(idx[s] + 1);
        }
    }
    if (gridTex.id) UnloadTexture(gridTex);
    if (torchTex.id) UnloadTexture(torchTex);
    Image gi = { grid, GRID_N * 2, GRID_N, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    gridTex = LoadTextureFromImage(gi);
    Image ti = { data, MAX_TORCHES, 1, 1, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32 };
    torchTex = LoadTextureFromImage(ti);
    free(data);
    builtTorches = G.torchCount;
}

static void FreeChunks(void)
{
    for (int c = 0; c < CHUNKS * CHUNKS; c++) {
        Chunk *k = &chunks[c];
        FreeMesh(&k->ground);
        DropThings(k);
        free(k->boxes);
        free(k->res);
        *k = (Chunk){ 0 };
        k->lo = (Vector3){ 1e9f, 1e9f, 1e9f };
        k->hi = (Vector3){ -1e9f, -1e9f, -1e9f };
    }
}

// The ground, water and roads of the whole map, caught once and cut up by where each quad lies.
static void BuildStill(void)
{
    static VList bins[CHUNKS * CHUNKS];
    VList all = { 0 };

    FreeChunks();
    Catch(&all);
    drawAll = true;
    SetMaterial(MAT_GROUND);
    DrawGround();
    SetMaterial(MAT_WATER);
    DrawWater();
    SetMaterial(MAT_GROUND);
    DrawRoads();
    drawAll = false;
    Release();
    for (int q = 0; q + 3 < all.n; q += 4) {
        const GVert *v = &all.v[q];
        int c = ChunkAt((v[0].x + v[1].x + v[2].x + v[3].x) / 4, (v[0].z + v[1].z + v[2].z + v[3].z) / 4);
        for (int k = 0; k < 4; k++) {
            Push(&bins[c], &v[k]);
            Grow(&chunks[c], (Vector3){ v[k].x, v[k].y, v[k].z }, (Vector3){ v[k].x, v[k].y, v[k].z });
        }
    }
    free(all.v);
    for (int c = 0; c < CHUNKS * CHUNKS; c++) {
        MeshFrom(&chunks[c].ground, bins[c].v, bins[c].n);
        free(bins[c].v);
        bins[c] = (VList){ 0 };
    }
    builtBoxes = builtRes = 0;
    TakeNew();
    MakeHeights();
    MakeTorches();
    builtGen = worldGen;
    firstBuild = true;
}

static const float LOD_D2[LODS] = { 0.0f, 50.0f * 50.0f, 100.0f * 100.0f };

// A chunk's buildings, plants and rocks at one level of detail: the finest has bevels and round
// columns, the farthest leaves out everything small.
static void BuildThings(Chunk *c, int lod)
{
    Catch(&scratch);
    for (int k = 0; k < c->boxCount; k++) {
        int i = c->boxes[k];
        const Box *b = &G.boxes[i];
        if (lod == LODS - 1 && (b->size.x + b->size.y + b->size.z) * 0.5f < 1.0f) continue;
        SetMaterial(b->built ? MAT_BUILT : MAT_THING);
        DrawBoxAt(i, LOD_D2[lod]);
    }
    SetMaterial(MAT_THING);
    for (int k = 0; k < c->resCount; k++) {
        if (lod == LODS - 1 && G.res[c->res[k]].kind == RES_HERB) continue;
        DrawResource(c->res[k]);
    }
    Release();
    MeshFrom(&c->things[lod], scratch.v, scratch.n);
    c->sig = ResSig(c);
}

static bool TimeLeft(void)
{
    return firstBuild || GetTime() - frameStart < 0.006;
}

static float BoxDistance(Vector3 p, Vector3 lo, Vector3 hi)
{
    float dx = fmaxf(fmaxf(lo.x - p.x, p.x - hi.x), 0.0f), dy = fmaxf(fmaxf(lo.y - p.y, p.y - hi.y), 0.0f);
    float dz = fmaxf(fmaxf(lo.z - p.z, p.z - hi.z), 0.0f);
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

typedef struct { Vector4 p[6]; } Frustum;

static Frustum FrustumOf(Matrix m)
{
    Frustum f;
    Vector4 r0 = { m.m0, m.m4, m.m8, m.m12 }, r1 = { m.m1, m.m5, m.m9, m.m13 }, r2 = { m.m2, m.m6, m.m10, m.m14 }, r3 = { m.m3, m.m7, m.m11, m.m15 };
    f.p[0] = (Vector4){ r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w };
    f.p[1] = (Vector4){ r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w };
    f.p[2] = (Vector4){ r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w };
    f.p[3] = (Vector4){ r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w };
    f.p[4] = (Vector4){ r3.x + r2.x, r3.y + r2.y, r3.z + r2.z, r3.w + r2.w };
    f.p[5] = (Vector4){ r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w };
    return f;
}

static bool InFrustum(const Frustum *f, Vector3 lo, Vector3 hi)
{
    for (int i = 0; i < 6; i++) {
        Vector4 p = f->p[i];
        float x = p.x > 0 ? hi.x : lo.x, y = p.y > 0 ? hi.y : lo.y, z = p.z > 0 ? hi.z : lo.z;
        if (p.x * x + p.y * y + p.z * z + p.w < 0) return false;
    }
    return true;
}

// Picks each chunk's detail for this frame and makes what is missing, a few milliseconds' worth a
// frame; until a finer mesh is ready a coarser one stands in.
static void ChooseLods(void)
{
    Vector3 eye = G.cam.position;
    float reach = GfxTier() == GFX_LOW ? 600.0f : 1e9f;
    for (int ci = 0; ci < CHUNKS * CHUNKS; ci++) {
        Chunk *c = &chunks[ci];
        c->lod = -1;
        if (c->lo.x > c->hi.x || (!c->boxCount && !c->resCount)) continue;
        if (c->resCount && c->sig != ResSig(c)) DropThings(c);
        float d = BoxDistance(eye, c->lo, c->hi);
        if (d > reach) continue;
        int want = d < 40.0f ? 0 : d < 150.0f ? 1 : 2;
        if (!c->things[want].built && (TimeLeft() || (!c->things[0].built && !c->things[1].built && !c->things[2].built)))
            BuildThings(c, want);
        for (int k = 0; k < LODS && !c->things[want].built; k++)
            if (c->things[(want + k) % LODS].built) want = (want + k) % LODS;
        c->lod = c->things[want].built ? want : -1;
    }
}

static void DrawChunks(const Frustum *f)
{
    for (int ci = 0; ci < CHUNKS * CHUNKS; ci++) {
        const Chunk *c = &chunks[ci];
        if (c->lo.x > c->hi.x || !InFrustum(f, c->lo, c->hi)) continue;
        DrawMeshBlocks(&c->ground);
        if (c->lod >= 0) DrawMeshBlocks(&c->things[c->lod]);
    }
}

// ---- What moves, caught afresh each frame ----

static void CatchMoving(void)
{
    Catch(&dyn);
    SetMaterial(MAT_FIGURE);
    DrawVillage();
    DrawWorkers();
    DrawFerries();
    DrawCaravans();
    DrawRaiders();
    DrawWildlife();
    DrawPeers();
    DrawPlayer();
    SetMaterial(MAT_THING);
    DrawArrows();
    DrawNetArrows();
    DrawFields();
    DrawResourcesOf(false, true);
    DrawTorches();
    Release();

    int n = dyn.n - dyn.n % 4;
    dynUsed = (n + BLOCK_VERTS - 1) / BLOCK_VERTS;
    if (dynUsed > MAX_DYN_BLOCKS) dynUsed = MAX_DYN_BLOCKS;
    for (int b = 0; b < dynUsed; b++) {
        int k = n - b * BLOCK_VERTS < BLOCK_VERTS ? n - b * BLOCK_VERTS : BLOCK_VERTS;
        if (!dynBlocks[b].vao) dynBlocks[b] = NewBlock(NULL, 0, true, BLOCK_VERTS);
        rlUpdateVertexBuffer(dynBlocks[b].vbo, dyn.v + b * BLOCK_VERTS, k * (int)sizeof(GVert), 0);
        dynBlocks[b].verts = k;
    }
}

static void DrawMoving(void)
{
    for (int b = 0; b < dynUsed; b++) DrawBlock(&dynBlocks[b]);
}

// ---- Light and sky ----

static float Smooth(float e0, float e1, float x)
{
    float t = Clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static Vector3 V3(float x, float y, float z)
{
    return (Vector3){ x, y, z };
}

// Sun by day, moon by night: their light, the sky's colors and how much light comes from all round.
static SkyNow Sky(void)
{
    static const Vector3 seasonTint[4] = { { 1.0f, 1.0f, 0.97f }, { 1.05f, 1.0f, 0.88f }, { 1.03f, 0.95f, 0.86f }, { 0.9f, 0.95f, 1.04f } };
    static float wet = -1.0f;
    SkyNow s;
    // The rain comes and goes slowly, but a game that starts in it starts wet.
    if (wet < 0.0f) wet = G.raining ? 1.0f : 0.0f;
    wet += ((G.raining ? 1.0f : 0.0f) - wet) * fminf(1.0f, GetFrameTime() * 0.4f);
    Vector3 sun = SunDirection();
    float elev = sinf((G.hour - 6.0f) / 12.0f * PI);
    float day = Smooth(-0.12f, 0.10f, elev), low = Clamp(1.0f - elev * 1.6f, 0.0f, 1.0f);
    Vector3 tint = seasonTint[Season() & 3];

    Vector3 sunCol = Vector3Multiply(Vector3Scale(Vector3Lerp(V3(2.35f, 2.2f, 1.95f), V3(2.4f, 1.55f, 0.85f), low), Smooth(-0.03f, 0.10f, elev) * (1.0f - 0.7f * wet)), tint);
    Vector3 moonCol = Vector3Scale(V3(0.16f, 0.20f, 0.32f), Smooth(0.0f, 0.2f, -elev) * (1.0f - 0.6f * wet));
    s.sunDir = sun;
    s.moonDir = Vector3Negate(sun);
    s.lightDir = elev > -0.03f ? sun : s.moonDir;
    s.lightCol = elev > -0.03f ? sunCol : moonCol;
    // A light from under the horizon would come up through the ground.
    if (s.lightDir.y < 0.08f) {
        s.lightDir.y = 0.08f;
        s.lightDir = Vector3Normalize(s.lightDir);
    }
    s.zen = Vector3Lerp(V3(0.004f, 0.007f, 0.022f), Vector3Lerp(V3(0.05f, 0.19f, 0.56f), V3(0.08f, 0.16f, 0.42f), low), day);
    s.hor = Vector3Lerp(V3(0.016f, 0.022f, 0.045f), Vector3Lerp(V3(0.50f, 0.62f, 0.78f), V3(0.82f, 0.58f, 0.40f), low), day);
    Vector3 grey = Vector3Scale(V3(0.30f, 0.32f, 0.35f), 0.15f + 0.85f * day);
    s.zen = Vector3Lerp(s.zen, Vector3Scale(grey, 0.8f), wet * 0.8f);
    s.hor = Vector3Lerp(s.hor, grey, wet * 0.8f);
    s.sunGlow = Vector3Scale(Vector3Lerp(V3(2.35f, 2.2f, 1.95f), V3(2.4f, 1.3f, 0.6f), low), Smooth(-0.18f, 0.02f, elev) * (1.0f - 0.8f * wet));
    Vector3 skyAmb = Vector3Add(Vector3Scale(Vector3Lerp(s.hor, s.zen, 0.35f), 0.72f), V3(0.06f, 0.04f, 0.0f));
    s.ambSky = Vector3Lerp(V3(0.035f, 0.045f, 0.085f), skyAmb, day);
    s.ambGround = Vector3Scale(Vector3Lerp(V3(0.018f, 0.02f, 0.03f), V3(0.38f, 0.29f, 0.19f), day), 1.0f - 0.4f * wet);
    // Braziers light the temple rooms whatever the hour.
    if (InTempleInterior(G.player.pos)) {
        s.ambSky = Vector3Add(s.ambSky, V3(0.45f, 0.32f, 0.2f));
        s.ambGround = Vector3Add(s.ambGround, V3(0.3f, 0.2f, 0.12f));
    }
    s.exposure = Lerp(1.25f, 0.78f, day);
    float fog = GfxTier() == GFX_LOW ? 0.0019f : 0.0016f;
    s.fog = Lerp(0.0045f, fog, day) + wet * 0.004f;
    s.day = day;
    s.wet = wet;
    return s;
}

static void SkyUniforms(const Locs *l, const SkyNow *s, int quality)
{
    U3(l->camPos, G.cam.position);
    U3(l->sunDir, s->sunDir);
    U3(l->sunGlow, s->sunGlow);
    U3(l->skyZen, s->zen);
    U3(l->skyHor, s->hor);
    U1(l->exposure, s->exposure);
    UI(l->quality, quality);
}

// ---- Render targets ----

static void Targets(void)
{
    int tier = GfxTier(), sw = GetRenderWidth(), sh = GetRenderHeight();
    float budget = tier == GFX_HIGH ? 3.7e6f : 0.62e6f;
    float k = fminf(1.0f, sqrtf(budget / (float)(sw * sh > 0 ? sw * sh : 1)));
    int w = (int)((float)sw * k), h = (int)((float)sh * k), sm = tier == GFX_HIGH ? 2048 : 1024;
    if (w < 16) w = 16;
    if (h < 16) h = 16;
    if (w == sceneW && h == sceneH && sm == shadowSize && tier == targetTier) return;
    if (scene.id) UnloadRenderTexture(scene);
    if (brightRT.id) UnloadRenderTexture(brightRT);
    if (blurRT.id) UnloadRenderTexture(blurRT);
    scene = LoadRenderTexture(w, h);
    brightRT = LoadRenderTexture(w / 2 > 0 ? w / 2 : 1, h / 2 > 0 ? h / 2 : 1);
    blurRT = LoadRenderTexture(w / 2 > 0 ? w / 2 : 1, h / 2 > 0 ? h / 2 : 1);
    SetTextureFilter(scene.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(brightRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(blurRT.texture, TEXTURE_FILTER_BILINEAR);
    if (sm != shadowSize) {
        if (shadowRT.id) UnloadRenderTexture(shadowRT);
        shadowRT = LoadRenderTexture(sm, sm);
        SetTextureFilter(shadowRT.texture, TEXTURE_FILTER_POINT);
        SetTextureWrap(shadowRT.texture, TEXTURE_WRAP_CLAMP);
    }
    sceneW = w;
    sceneH = h;
    shadowSize = sm;
    targetTier = tier;
}

// ---- The frame ----

static float shadowRange = 400.0f;

// The sun's view of a square ahead of the camera, its edges stepped by whole shadow texels so the
// shadows do not crawl as the camera moves.
static Matrix ShadowPass(Vector3 lightDir)
{
    bool high = GfxTier() == GFX_HIGH;
    float size = high ? 130.0f : 90.0f, ahead = high ? 42.0f : 30.0f;
    Vector3 eye = G.cam.position, look = Vector3Subtract(G.cam.target, eye);
    Vector3 flat = Vector3Normalize((Vector3){ look.x, 0, look.z });
    if (fabsf(look.x) + fabsf(look.z) < 1e-4f) flat = (Vector3){ 0, 0, 1 };
    Vector3 center = Vector3Add(G.cam.target, Vector3Scale(flat, ahead));
    Vector3 z = lightDir, x = Vector3Normalize(Vector3CrossProduct((Vector3){ 0, 1, 0 }, z)), y = Vector3CrossProduct(z, x);
    float texel = size / (float)shadowSize;
    float cx = floorf(Vector3DotProduct(center, x) / texel) * texel, cy = floorf(Vector3DotProduct(center, y) / texel) * texel;
    center = Vector3Add(Vector3Add(Vector3Scale(x, cx), Vector3Scale(y, cy)), Vector3Scale(z, Vector3DotProduct(center, z)));
    Camera3D lc = { Vector3Add(center, Vector3Scale(lightDir, 300.0f)), center, { 0, 1, 0 }, size, CAMERA_ORTHOGRAPHIC };

    rlSetClipPlanes(100.0, 100.0 + shadowRange);
    BeginTextureMode(shadowRT);
    ClearBackground(WHITE);
    BeginMode3D(lc);
    Matrix lvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    Frustum f = FrustumOf(lvp);
    rlEnableShader(depthSh.id);
    rlSetUniformMatrix(dl.mvp, lvp);
    rlDisableBackfaceCulling();
    DrawChunks(&f);
    DrawMoving();
    rlDisableVertexArray();
    rlEnableBackfaceCulling();
    rlDisableShader();
    EndMode3D();
    EndTextureMode();
    rlSetClipPlanes(0.1, 1200.0);
    return lvp;
}

static void BindTexture(int unit, unsigned int id, int loc)
{
    rlActiveTextureSlot(unit);
    rlEnableTexture(id);
    rlSetUniform(loc, &unit, RL_SHADER_UNIFORM_INT, 1);
}

static void WorldUniforms(const SkyNow *s, Matrix lightVP, int quality)
{
    const Locs *l = &wl;
    SkyUniforms(l, s, quality);
    U3(l->lightDir, s->lightDir);
    U3(l->lightCol, s->lightCol);
    U3(l->ambSky, s->ambSky);
    U3(l->ambGround, s->ambGround);
    U3(l->torchCol, V3(1.3f, 0.7f, 0.28f));
    U1(l->fogDensity, s->fog);
    U1(l->time, (float)fmod(GetTime(), 3600.0));
    U1(l->worldHalf, WORLD_HALF);
    U1(l->night, G.night);
    U1(l->wet, s->wet);
    U1(l->shadowTexel, 1.0f / (float)shadowSize);
    U1(l->shadowRange, shadowRange);
    U1(l->groundTile, TILE_SIZE);
    U1(l->groundN, (float)(GROUND_TILES + 1));
    U1(l->heightScale, heightScale);
    U1(l->heightBase, heightBase);
    UI(l->torchSlots, torchTex.id ? (quality > 0 ? 8 : 4) : 0);
    UI(l->gridN, GRID_N);
    U1(l->gridCell, GRID_CELL);
    U1(l->torchRadius, TORCH_REACH);
    Vector4 dl8[8];
    int n = 0;
    for (int i = 0; i < G.dynCount && n < (quality > 0 ? 8 : 4); i++) dl8[n++] = (Vector4){ G.dynLights[i].x, G.dynLights[i].y, G.dynLights[i].z, 1.0f };
    UI(l->dynCount, n);
    if (n) rlSetUniform(l->dynLights, dl8, RL_SHADER_UNIFORM_VEC4, n);
    rlSetUniformMatrix(l->lightVP, lightVP);
    BindTexture(TEX_SHADOW, shadowRT.texture.id, l->shadowMap);
    BindTexture(TEX_HEIGHT, heightTex.id, l->heightMap);
    BindTexture(TEX_GRID, gridTex.id, l->torchGrid);
    BindTexture(TEX_TORCH, torchTex.id, l->torchData);
    rlActiveTextureSlot(0);
}

static void ScenePass(const SkyNow *s, Matrix lightVP, int quality)
{
    BeginTextureMode(scene);
    ClearBackground(BLACK);
    BeginMode3D(G.cam);
    Matrix vp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

    // The sky first, behind everything.
    rlEnableShader(skySh.id);
    SkyUniforms(&sl, s, quality);
    U3(sl.moonDir, s->moonDir);
    U1(sl.night, 1.0f - s->day);
    U1(sl.time, (float)fmod(GetTime(), 3600.0));
    U1(sl.cloudy, s->wet);
    Matrix model = MatrixMultiply(MatrixScale(SKY_RADIUS, SKY_RADIUS, SKY_RADIUS), MatrixTranslate(G.cam.position.x, G.cam.position.y, G.cam.position.z));
    rlSetUniformMatrix(sl.mvp, MatrixMultiply(model, vp));
    rlDisableBackfaceCulling();
    rlDisableDepthTest();
    rlDisableDepthMask();
    DrawBlock(&skyBlock);
    rlDisableVertexArray();
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();

    rlEnableShader(worldSh.id);
    WorldUniforms(s, lightVP, quality);
    rlSetUniformMatrix(wl.mvp, vp);
    Frustum f = FrustumOf(vp);
    DrawChunks(&f);
    DrawMoving();
    rlDisableVertexArray();
    rlDisableShader();

    // Sparks, trails and rain go through rlgl's batch with the same shader, after everything solid.
    BeginShaderMode(worldSh);
    SetMaterial(MAT_UNLIT);
    rlTexCoord2f((float)MAT_UNLIT, 0);
    rlNormal3f(0, 0, 0);
    DrawEffects();
    rlTexCoord2f((float)MAT_UNLIT, 0);
    DrawRain();
    rlDrawRenderBatchActive();
    EndShaderMode();
    SetMaterial(MAT_THING);
    EndMode3D();
    EndTextureMode();
}

static void PostPass(bool high)
{
    int w = sceneW, h = sceneH, hw = w / 2 > 0 ? w / 2 : 1, hh = h / 2 > 0 ? h / 2 : 1;
    Rectangle full = { 0, 0, (float)w, -(float)h }, half = { 0, 0, (float)hw, -(float)hh }, halfDst = { 0, 0, (float)hw, (float)hh };
    if (high) {
        BeginTextureMode(brightRT);
        BeginShaderMode(brightSh);
        DrawTexturePro(scene.texture, full, halfDst, (Vector2){ 0 }, 0, WHITE);
        EndShaderMode();
        EndTextureMode();
        for (int pass = 0; pass < 2; pass++) {
            Vector2 dh = { 2.0f / (float)hw, 0 }, dv = { 0, 2.0f / (float)hh };
            BeginTextureMode(blurRT);
            BeginShaderMode(blurSh);
            SetShaderValue(blurSh, bl.dir, &dh, SHADER_UNIFORM_VEC2);
            DrawTexturePro(brightRT.texture, half, halfDst, (Vector2){ 0 }, 0, WHITE);
            EndShaderMode();
            EndTextureMode();
            BeginTextureMode(brightRT);
            BeginShaderMode(blurSh);
            SetShaderValue(blurSh, bl.dir, &dv, SHADER_UNIFORM_VEC2);
            DrawTexturePro(blurRT.texture, half, halfDst, (Vector2){ 0 }, 0, WHITE);
            EndShaderMode();
            EndTextureMode();
        }
    }
    ClearBackground(BLACK);
    BeginShaderMode(postSh);
    Vector2 texel = { 1.0f / (float)w, 1.0f / (float)h };
    float amt = high ? 0.55f : 0.0f;
    SetShaderValue(postSh, pl.texel, &texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(postSh, pl.bloomAmt, &amt, SHADER_UNIFORM_FLOAT);
    SetShaderValueTexture(postSh, pl.bloom, brightRT.texture);
    DrawTexturePro(scene.texture, full, (Rectangle){ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, (Vector2){ 0 }, 0, WHITE);
    EndShaderMode();
}

// GFX_STATS=1: how big the still meshes are and how long they took, once they are first made, and
// how much moves each frame.
static long MeshVerts(const GMesh *m)
{
    long n = 0;
    for (int b = 0; b < m->count; b++) n += m->blocks[b].verts;
    return n;
}

static void PrintStats(double still, double things)
{
    long ground = 0, v[LODS] = { 0 }, n[LODS] = { 0 };
    for (int c = 0; c < CHUNKS * CHUNKS; c++) {
        ground += MeshVerts(&chunks[c].ground);
        for (int l = 0; l < LODS; l++)
            if (chunks[c].things[l].built) {
                n[l]++;
                v[l] += MeshVerts(&chunks[c].things[l]);
            }
    }
    fprintf(stderr, "gfx: ground %ld vertices (%.0f ms); things fine %ld (%ld chunks), mid %ld (%ld), far %ld (%ld) (%.0f ms)\n",
            ground, still * 1000, v[0], n[0], v[1], n[1], v[2], n[2], things * 1000);
}

bool GfxDrawWorld(void)
{
    if (!inited) Init();
    if (failed || GfxTier() == GFX_CLASSIC) return false;
    frameStart = GetTime();
    WatchSpeed();
    int quality = GfxTier() == GFX_HIGH ? 1 : 0;
    on = true;
    if (builtGen != worldGen || G.boxCount < builtBoxes || G.resCount < builtRes) BuildStill();
    if (G.boxCount != builtBoxes || G.resCount != builtRes) TakeNew();
    if (G.torchCount != builtTorches) MakeTorches();
    Targets();
    double t0 = GetTime();
    ChooseLods();
    if (stats && firstBuild) PrintStats(t0 - frameStart, GetTime() - t0);
    firstBuild = false;
    CatchMoving();
    if (stats) fprintf(stderr, "gfx: moving %d verts\n", dyn.n);
    SkyNow s = Sky();
    Matrix lightVP = ShadowPass(s.lightDir);
    ScenePass(&s, lightVP, quality);
    on = false;
    PostPass(quality > 0);
    return true;
}
