#include "game.h"
#include "rlgl.h"
#include <math.h>

#define MAX_KEEP 96

static Rectangle keep[MAX_KEEP];
static int keepCount;

// Long boxes are cut into sections so torchlight can fall on part of a road or wall.
void AddBox(float x, float y0, float z, float w, float h, float d, Color c, bool solid)
{
    const float maxLen = 8.0f;
    int nx = (int)ceilf(w / maxLen), nz = (int)ceilf(d / maxLen);

    if (nx < 1) nx = 1;
    if (nz < 1) nz = 1;
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < nz; j++) {
            if (G.boxCount >= MAX_BOXES) return;
            float sw = w / (float)nx, sd = d / (float)nz;
            float cx = x - w / 2 + sw * ((float)i + 0.5f), cz = z - d / 2 + sd * ((float)j + 0.5f);
            G.boxes[G.boxCount++] = (Box){ { cx, y0 + h / 2, cz }, { sw, h, sd }, c, solid, false, false, 0.0f, SHAPE_BOX };
        }
    }
}

// Sinks the base into slopes so nothing floats on a hillside.
void AddBoxOn(float x, float z, float w, float h, float d, Color c, bool solid)
{
    float a = TerrainHeight(x - w / 2, z - d / 2), b = TerrainHeight(x + w / 2, z - d / 2);
    float e = TerrainHeight(x - w / 2, z + d / 2), f = TerrainHeight(x + w / 2, z + d / 2);
    float lo = fminf(fminf(a, b), fminf(e, f)), hi = fmaxf(fmaxf(a, b), fmaxf(e, f));
    AddBox(x, lo - 0.15f, z, w, hi - lo + h + 0.15f, d, c, solid);
}

void AddShape(float x, float y0, float z, float w, float h, float d, Color c, bool solid, int shape)
{
    int first = G.boxCount;
    AddBox(x, y0, z, w, h, d, c, solid);
    for (int i = first; i < G.boxCount; i++) G.boxes[i].shape = (unsigned char)shape;
}

void AddGlow(float x, float y0, float z, float w, float h, float d, Color c)
{
    if (G.boxCount >= MAX_BOXES) return;
    G.boxes[G.boxCount++] = (Box){ { x, y0 + h / 2, z }, { w, h, d }, c, false, false, true, 0.0f, SHAPE_BOX };
}

void AddWater(float x0, float z0, float x1, float z1)
{
    if (G.boxCount >= MAX_BOXES) return;
    G.boxes[G.boxCount++] = (Box){ { (x0 + x1) / 2, -0.25f, (z0 + z1) / 2 }, { x1 - x0, 0.66f, z1 - z0 },
                                   COL_WATER, true, true, false, 0.0f, SHAPE_BOX };
}

void AddTree(float x, float z, float s, bool poplar)
{
    float y = TerrainHeight(x, z);

    AddShape(x, y - 0.2f, z, 0.36f * s, 2.6f * s, 0.36f * s, COL_PALM_TRUNK, true, SHAPE_CYL);
    if (poplar) {
        AddShape(x, y + 1.6f * s, z, 1.5f * s, 4.6f * s, 1.5f * s, (Color){ 82, 120, 60, 255 }, false, SHAPE_BLOB);
        AddShape(x, y + 4.8f * s, z, 0.9f * s, 2.2f * s, 0.9f * s, (Color){ 92, 130, 66, 255 }, false, SHAPE_BLOB);
    } else {
        AddShape(x, y + 1.7f * s, z, 3.4f * s, 2.0f * s, 3.2f * s, COL_TREE, false, SHAPE_BLOB);
        AddShape(x + 0.4f * s, y + 2.9f * s, z - 0.3f * s, 2.4f * s, 1.7f * s, 2.3f * s, (Color){ 84, 118, 64, 255 }, false, SHAPE_BLOB);
        AddShape(x - 0.7f * s, y + 2.4f * s, z + 0.6f * s, 1.8f * s, 1.3f * s, 1.7f * s, (Color){ 78, 112, 60, 255 }, false, SHAPE_BLOB);
    }
}

void KeepOut(float x0, float z0, float x1, float z1)
{
    if (keepCount < MAX_KEEP) keep[keepCount++] = (Rectangle){ x0, z0, x1 - x0, z1 - z0 };
}

bool InKeepOut(float x, float z, float margin)
{
    for (int i = 0; i < keepCount; i++) {
        Rectangle r = keep[i];
        if (x > r.x - margin && x < r.x + r.width + margin && z > r.y - margin && z < r.y + r.height + margin)
            return true;
    }
    return false;
}

int AddResource(float x, float z, ResKind kind)
{
    if (G.resCount >= MAX_RES) return -1;
    float y = (kind == RES_FISH) ? 0.0f : TerrainHeight(x, z);

    G.res[G.resCount] = (Resource){ { x, y, z }, kind, 0.0f, RandF(0, 360), false };
    if (kind == RES_PALM) AddShape(x, y - 0.2f, z, 0.46f, 6.7f, 0.46f, COL_PALM_TRUNK, true, SHAPE_CYL);
    else if (kind == RES_TREE) AddShape(x, y - 0.2f, z, 0.5f, 1.0f, 0.5f, COL_PALM_TRUNK, true, SHAPE_CYL);
    else if (kind == RES_ORE) AddBoxOn(x, z, 1.4f, 0.9f, 1.2f, COL_ROCK, true);
    return G.resCount++;
}

static bool Contains(const Box *b, float x, float z, float margin)
{
    return fabsf(x - b->pos.x) <= b->size.x / 2 + margin && fabsf(z - b->pos.z) <= b->size.z / 2 + margin;
}

bool IsBlocking(const Box *b, float y)
{
    if (!b->solid) return false;
    if (b->water) return y < 0.25f;
    float top = b->pos.y + b->size.y / 2, bottom = b->pos.y - b->size.y / 2;
    return top > y + STEP_HEIGHT && bottom < y + PLAYER_HEIGHT;
}

Vector2 PushOut(Vector2 p, float radius, const Box *b)
{
    float x0 = b->pos.x - b->size.x / 2, x1 = b->pos.x + b->size.x / 2;
    float z0 = b->pos.z - b->size.z / 2, z1 = b->pos.z + b->size.z / 2;
    float cx = Clamp(p.x, x0, x1), cz = Clamp(p.y, z0, z1);
    float dx = p.x - cx, dz = p.y - cz;
    float d2 = dx * dx + dz * dz;

    if (d2 >= radius * radius) return p;
    if (d2 > 1e-8f) {
        float d = sqrtf(d2);
        return (Vector2){ cx + dx / d * radius, cz + dz / d * radius };
    }

    float l = p.x - x0, r = x1 - p.x, t = p.y - z0, bt = z1 - p.y;
    float m = fminf(fminf(l, r), fminf(t, bt));
    if (m == l) p.x = x0 - radius;
    else if (m == r) p.x = x1 + radius;
    else if (m == t) p.y = z0 - radius;
    else p.y = z1 + radius;
    return p;
}

float GroundHeight(float x, float z, float y)
{
    float h = TerrainHeight(x, z);

    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (!b->solid || b->water) continue;
        float top = b->pos.y + b->size.y / 2;
        if (top > h && top <= y + STEP_HEIGHT && Contains(b, x, z, 0)) h = top;
    }
    return h;
}

float CeilingHeight(float x, float z, float y)
{
    float c = 1e9f;

    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (!b->solid || b->water) continue;
        float bottom = b->pos.y - b->size.y / 2;
        if (bottom > y + 0.5f && bottom < c && Contains(b, x, z, 0)) c = bottom;
    }
    return c;
}

bool InWater(float x, float z)
{
    for (int i = 0; i < G.boxCount; i++)
        if (G.boxes[i].water && Contains(&G.boxes[i], x, z, 0)) return true;
    return false;
}

bool PointInSolid(Vector3 p)
{
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (b->solid && !b->water && Contains(b, p.x, p.z, 0) && fabsf(p.y - b->pos.y) <= b->size.y / 2) return true;
    }
    return false;
}

bool SolidAt(float x, float z, float radius)
{
    float ground = TerrainHeight(x, z);

    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (b->solid && !b->water && b->pos.y + b->size.y / 2 > ground + 0.6f && Contains(b, x, z, radius)) return true;
    }
    return false;
}

bool FreeSpot(float x, float z, float margin)
{
    return !InWater(x, z) && !InWater(x + margin, z) && !InWater(x - margin, z) && !InWater(x, z + margin) &&
           !InWater(x, z - margin) && !SolidAt(x, z, margin) && !InKeepOut(x, z, margin);
}

static void PlacePalm(float x0, float x1, float z0, float z1)
{
    for (int tries = 0; tries < 15; tries++) {
        float x = RandF(x0, x1), z = RandF(z0, z1);
        if (FreeSpot(x, z, 1.5f) && !InsideWalls(x, z, -4.0f)) {
            AddResource(x, z, RES_PALM);
            return;
        }
    }
}

static void Forest(float cx, float cz, float r, int n, int resources, bool poplar)
{
    for (int k = 0, tries = 0; k < n && tries < n * 40; tries++) {
        float a = RandF(0, 2 * PI), d = r * sqrtf(RandF(0, 1));
        float x = cx + cosf(a) * d, z = cz + sinf(a) * d;
        if (fabsf(x) > WORLD_LIMIT - 4 || fabsf(z) > WORLD_LIMIT - 4) continue;
        if (!FreeSpot(x, z, 1.6f) || InsideWalls(x, z, -10.0f)) continue;
        if (k < resources) AddResource(x, z, RES_TREE);
        else AddTree(x, z, RandF(0.8f, 1.3f), poplar);
        k++;
    }
}

static void BuildVegetation(void)
{
    static const float cityPalms[][2] = { { -14, -16 }, { 14, -16 }, { -52, -29 }, { -52, 5 } };

    for (int i = 0; i < 4; i++) AddResource(cityPalms[i][0], cityPalms[i][1], RES_PALM);
    for (int i = 0; i < 30; i++) PlacePalm(-124, -104, -330, 330);
    for (int i = 0; i < 8; i++) PlacePalm(-80, 100, -116, -111);
    for (int i = 0; i < 7; i++) PlacePalm(-9, -5, 140, 340);
    for (int i = 0; i < 7; i++) PlacePalm(5, 9, 140, 340);
    for (int i = 0; i < 8; i++) PlacePalm(104, 117, -90, 80);

    for (float z = -330.0f; z <= 330.0f; z += 12.0f) {
        if (fabsf(z + 10) < 8 || fabsf(z - 30) < 6 || fabsf(z + 62) < 6 || (z > FORD_Z0 - 4 && z < FORD_Z1 + 4)) continue;
        AddResource(-126.4f + RandF(-0.4f, 0.4f), z + RandF(-2, 2), RES_REEDS);
    }
    for (int i = 0; i < 60; i++) {
        float x = RandF(-127.5f, -121.0f), z = RandF(-335, 335);
        if (fabsf(z + 10) < 8 || InKeepOut(x, z, 1.0f)) continue;
        for (int k = 0; k < 3; k++)
            AddBox(x + RandF(-0.5f, 0.5f), 0, z + RandF(-0.5f, 0.5f), 0.09f, RandF(1.3f, 2.4f), 0.09f, COL_REED, false);
    }

    Forest(198, 52, 22, 26, 16, false);
    Forest(242, -105, 24, 18, 8, false);
    Forest(218, 190, 22, 16, 8, false);
    Forest(-110, -200, 18, 16, 8, true);
    Forest(-112, 250, 16, 12, 6, true);
    Forest(300, 180, 28, 22, 8, false);
    Forest(290, -200, 26, 20, 8, false);

    for (int tries = 0; tries < 1500; tries++) {
        float x = RandF(-354, 354), z = RandF(-354, 354);
        float h = TerrainHeight(x, z), f = Fertility(x, z);
        if (InsideWalls(x, z, -3.0f) || InKeepOut(x, z, 0.5f) || InWater(x, z)) continue;
        if (f < 0.35f && !(h > 1.5f && Hash2(tries, 5) < 0.5f)) continue;
        float s = RandF(0.3f, 0.6f), y = TerrainHeight(x, z) - 0.05f;
        Color c = Mix(COL_GRASS, COL_WHEAT_GREEN, Hash2(tries, 3));
        AddShape(x, y, z, s * 0.45f, s * RandF(1.2f, 1.8f), s * 0.45f, c, false, SHAPE_CONE);
        AddShape(x + s * 0.25f, y, z + s * 0.15f, s * 0.35f, s * RandF(0.8f, 1.3f), s * 0.35f, Mix(c, COL_STRAW, 0.2f), false, SHAPE_CONE);
    }
    for (int tries = 0; tries < 380; tries++) {
        float x = RandF(-354, 354), z = RandF(-354, 354);
        if (x > RIVER_X0 - 3 && x < -60) continue;
        if (Fertility(x, z) > 0.35f || InsideWalls(x, z, -6.0f) || InKeepOut(x, z, 1.0f) || SolidAt(x, z, 0.5f)) continue;
        float s = RandF(0.5f, 1.1f);
        AddShape(x, TerrainHeight(x, z) - 0.15f, z, s * 1.2f, s * 0.9f, s * 1.1f, COL_SHRUB, false, SHAPE_BLOB);
    }
    for (int tries = 0; tries < 300; tries++) {
        float x = RandF(145, 354), z = RandF(-354, 354);
        if (TerrainHeight(x, z) < 5.0f || InKeepOut(x, z, 2.0f) || SolidAt(x, z, 1.0f)) continue;
        float s = RandF(0.8f, 2.2f);
        AddShape(x, TerrainHeight(x, z) - 0.35f * s, z, s, s * 0.9f, s * 0.8f, Mix(COL_ROCK, COL_STONE, Hash2(tries, 9)), true, SHAPE_BLOB);
    }
}

void BuildWorld(void)
{
    keepCount = 0;
    BuildCity();
    BuildVillage();
    BuildJobs();
    BuildRaiders();
    BuildCityHouses();
    BuildVegetation();
    BuildTerrain();
    BuildLights();

    for (int i = 0; i < MAX_STARS; i++)
        G.stars[i] = Vector3Normalize((Vector3){ RandF(-1, 1), RandF(0.12f, 1), RandF(-1, 1) });
}

static Vector3 SunDirection(void)
{
    float a = (G.hour - 6.0f) / 12.0f * PI;
    return Vector3Normalize((Vector3){ cosf(a), sinf(a), -0.35f });
}

void UpdateSky(void)
{
    const Color skyNight = { 12, 16, 38, 255 }, skyDay = { 140, 186, 222, 255 }, skyDusk = { 232, 140, 96, 255 };
    float sunH = sinf((G.hour - 6.0f) / 12.0f * PI);
    float warm = Clamp(1.0f - fabsf(sunH - 0.05f) * 3.5f, 0.0f, 1.0f);
    Vector3 night = { 0.30f, 0.34f, 0.52f }, day = { 1.0f, 0.98f, 0.94f }, dusk = { 1.0f, 0.66f, 0.48f };

    static const Vector3 seasonTint[4] = { { 1.0f, 1.0f, 0.97f }, { 1.05f, 1.0f, 0.88f }, { 1.03f, 0.95f, 0.86f }, { 0.9f, 0.95f, 1.04f } };
    static float wet;

    wet += ((G.raining ? 1.0f : 0.0f) - wet) * fminf(1.0f, GetFrameTime() * 0.4f);
    G.daylight = Clamp(sunH * 2.0f + 0.35f, 0.0f, 1.0f);
    G.light = Vector3Lerp(Vector3Lerp(night, day, G.daylight), dusk, warm * 0.6f);
    G.light = Vector3Multiply(G.light, seasonTint[Season()]);
    G.light = Vector3Scale(G.light, 1.0f - 0.38f * wet);
    G.sky = Mix(Mix(skyNight, skyDay, G.daylight), skyDusk, warm * 0.75f);
    G.sky = Mix(G.sky, Mix((Color){ 18, 20, 28, 255 }, (Color){ 112, 118, 126, 255 }, G.daylight), 0.85f * wet);

    // Braziers light the temple rooms whatever the hour.
    if (InTempleInterior(G.player.pos)) G.light = Vector3Lerp(G.light, (Vector3){ 0.78f, 0.62f, 0.46f }, 0.75f);
}

void DrawSky(void)
{
    const Color sunLow = { 255, 150, 80, 255 }, sunHigh = { 255, 236, 180, 255 }, moon = { 222, 226, 240, 255 };
    Vector3 eye = G.cam.position, sun = SunDirection();
    float stars = Clamp(1.0f - G.daylight / 0.5f, 0.0f, 1.0f);

    if (stars > 0) {
        Color c = Mix(G.sky, (Color){ 235, 235, 255, 255 }, stars);
        for (int i = 0; i < MAX_STARS; i++)
            DrawSphereEx(Vector3Add(eye, Vector3Scale(G.stars[i], 450)), 1.3f, 3, 4, c);
    }
    if (sun.y > -0.2f)
        DrawSphereEx(Vector3Add(eye, Vector3Scale(sun, 400)), 14.0f, 8, 12, Mix(sunLow, sunHigh, Clamp(sun.y * 2.5f, 0.0f, 1.0f)));
    if (sun.y < 0.2f)
        DrawSphereEx(Vector3Add(eye, Vector3Scale(sun, -400)), 9.0f, 8, 12, moon);
}

void DrawTerrain(void)
{
    Vector3 eye = G.cam.position;
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, eye));

    DrawGround();
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        Vector3 v = Vector3Subtract(b->pos, eye);
        float r = (b->size.x + b->size.y + b->size.z) * 0.5f;
        float d2 = Vector3DotProduct(v, v);
        if (Vector3DotProduct(v, fwd) < -r || d2 > 340.0f * 340.0f || (r < 1.0f && d2 > 150.0f * 150.0f)) continue;
        if (b->glow) {
            DrawGlowBox(b->pos, b->size, b->color);
            continue;
        }
        G.drawTorch = BoxLight(i);
        G.drawFog = FogAt(b->pos) * (1.0f - 0.6f * G.drawTorch);
        Vector3 base = { b->pos.x, b->pos.y - b->size.y / 2, b->pos.z };
        int sides = d2 > 60.0f * 60.0f ? 5 : 8;
        switch (b->shape) {
        case SHAPE_CYL:  DrawShadedCylinder(base, b->size.x / 2, b->size.z / 2, b->size.y, 1.0f, sides, b->color); break;
        case SHAPE_CONE: DrawShadedCylinder(base, b->size.x / 2, b->size.z / 2, b->size.y, 0.0f, sides, b->color); break;
        case SHAPE_BLOB: DrawShadedBlob(b->pos, Vector3Scale(b->size, 0.5f), b->color); break;
        default:         DrawShadedBox(b->pos, b->size, b->color); break;
        }
    }
    G.drawTorch = G.drawFog = 0;
    DrawTorches();
}

static void Part(float x, float y, float z, float w, float h, float d, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, d }, c);
}

static void DrawPalm(const Resource *r)
{
    const Color spine = { 96, 110, 52, 255 };
    for (int k = 0; k < 9; k++) {
        float droop = 18.0f + 12.0f * (float)(k % 3);
        rlPushMatrix();
        rlTranslatef(r->pos.x, r->pos.y + 6.45f, r->pos.z);
        rlRotatef(r->spin + (float)k * 40.0f, 0, 1, 0);
        rlRotatef(-droop + 30.0f, 0, 0, 1);
        Part(0.8f, 0, 0, 1.6f, 0.06f, 0.55f, COL_PALM_LEAF);
        Part(0.8f, 0.03f, 0, 1.6f, 0.04f, 0.06f, spine);
        rlTranslatef(1.55f, 0, 0);
        rlRotatef(-droop - 10.0f, 0, 0, 1);
        Part(0.75f, 0, 0, 1.5f, 0.06f, 0.45f, Mix(COL_PALM_LEAF, COL_WHEAT_GREEN, 0.25f));
        rlPopMatrix();
    }
    DrawShadedBlob((Vector3){ r->pos.x, r->pos.y + 6.4f, r->pos.z }, (Vector3){ 0.4f, 0.3f, 0.4f }, COL_PALM_TRUNK);
    if (r->regrow <= 0) {
        DrawShadedBlob((Vector3){ r->pos.x + 0.38f, r->pos.y + 5.95f, r->pos.z }, (Vector3){ 0.2f, 0.3f, 0.2f }, COL_DATES);
        DrawShadedBlob((Vector3){ r->pos.x - 0.3f, r->pos.y + 6.0f, r->pos.z + 0.25f }, (Vector3){ 0.18f, 0.28f, 0.18f }, COL_DATES);
        DrawShadedBlob((Vector3){ r->pos.x - 0.1f, r->pos.y + 5.95f, r->pos.z - 0.36f }, (Vector3){ 0.17f, 0.26f, 0.17f }, COL_DATES);
    }
}

static void DrawReeds(int i, const Resource *r)
{
    float grown = (r->regrow > 0) ? 0.25f : 1.0f;
    for (int k = 0; k < 7; k++) {
        float ox = (Hash2(i, k) - 0.5f) * 1.3f, oz = (Hash2(k, i + 7) - 0.5f) * 1.3f;
        float h = (1.7f + Hash2(i + 3, k) * 1.0f) * grown;
        Part(r->pos.x + ox, r->pos.y + h / 2, r->pos.z + oz, 0.1f, h, 0.1f, COL_REED);
        if (grown > 0.5f) Part(r->pos.x + ox, r->pos.y + h + 0.12f, r->pos.z + oz, 0.14f, 0.28f, 0.14f, COL_WOOD);
    }
}

static void DrawTreeRes(int i, const Resource *r)
{
    if (r->regrow > 0) return;
    float s = 0.8f + 0.5f * Hash2(i, 11), x = r->pos.x, y = r->pos.y, z = r->pos.z;
    bool poplar = x < -60.0f;

    DrawShadedCylinder((Vector3){ x, y + 0.7f, z }, 0.2f * s, 0.2f * s, 2.3f * s, 0.75f, 7, COL_PALM_TRUNK);
    if (poplar) {
        DrawShadedBlob((Vector3){ x, y + 4.0f * s, z }, (Vector3){ 0.75f * s, 2.4f * s, 0.75f * s }, (Color){ 82, 120, 60, 255 });
        DrawShadedBlob((Vector3){ x, y + 6.4f * s, z }, (Vector3){ 0.45f * s, 1.1f * s, 0.45f * s }, (Color){ 92, 130, 66, 255 });
    } else {
        DrawShadedBlob((Vector3){ x, y + 3.0f * s, z }, (Vector3){ 1.7f * s, 1.05f * s, 1.6f * s }, COL_TREE);
        DrawShadedBlob((Vector3){ x + 0.4f * s, y + 3.9f * s, z - 0.3f * s }, (Vector3){ 1.2f * s, 0.9f * s, 1.15f * s }, (Color){ 84, 118, 64, 255 });
        DrawShadedBlob((Vector3){ x - 0.7f * s, y + 3.5f * s, z + 0.6f * s }, (Vector3){ 0.9f * s, 0.7f * s, 0.85f * s }, (Color){ 78, 112, 60, 255 });
    }
}

static void DrawClay(const Resource *r)
{
    const Color clay = { 122, 100, 82, 255 }, wet = { 92, 72, 58, 255 };
    DrawShadedBlob((Vector3){ r->pos.x, r->pos.y + 0.02f, r->pos.z }, (Vector3){ 1.15f, 0.16f, 0.95f }, r->regrow > 0 ? wet : clay);
    if (r->regrow <= 0) {
        DrawShadedBlob((Vector3){ r->pos.x - 0.3f, r->pos.y + 0.2f, r->pos.z + 0.2f }, (Vector3){ 0.4f, 0.2f, 0.35f }, clay);
        DrawShadedBlob((Vector3){ r->pos.x + 0.4f, r->pos.y + 0.16f, r->pos.z - 0.3f }, (Vector3){ 0.3f, 0.16f, 0.3f }, wet);
    }
}

static void DrawOre(const Resource *r)
{
    if (r->regrow > 0) return;
    float y = r->pos.y + 0.8f;
    Part(r->pos.x - 0.3f, y, r->pos.z + 0.61f, 0.5f, 0.12f, 0.04f, COL_ORE);
    Part(r->pos.x + 0.2f, y - 0.25f, r->pos.z + 0.61f, 0.4f, 0.1f, 0.04f, COL_ORE);
    Part(r->pos.x + 0.71f, y - 0.1f, r->pos.z, 0.04f, 0.1f, 0.5f, COL_ORE);
    Part(r->pos.x - 0.1f, y + 0.12f, r->pos.z - 0.1f, 0.4f, 0.04f, 0.3f, COL_ORE);
}

static void DrawFish(int i, const Resource *r)
{
    if (r->regrow > 0) return;
    float t = (float)GetTime() * 0.6f + (float)i * 0.37f;
    float ph = t - floorf(t);
    float s = 0.4f + 1.4f * ph;

    rlPushMatrix();
    rlTranslatef(r->pos.x, 0.1f, r->pos.z);
    rlRotatef(45, 0, 1, 0);
    DrawShadedBox((Vector3){ 0, 0, 0 }, (Vector3){ s, 0.01f, s }, Fade((Color){ 170, 205, 215, 255 }, 1.0f - ph));
    DrawShadedBox((Vector3){ 0, 0.005f, 0 }, (Vector3){ s * 0.8f, 0.01f, s * 0.8f }, COL_WATER);
    rlPopMatrix();
    if (ph < 0.25f) {
        float a = ph / 0.25f;
        Part(r->pos.x + 0.6f * a - 0.3f, 0.1f + 0.5f * sinf(a * PI), r->pos.z, 0.3f, 0.08f, 0.1f, (Color){ 170, 180, 186, 255 });
    }
}

void DrawResources(void)
{
    for (int i = 0; i < G.resCount; i++) {
        const Resource *r = &G.res[i];
        if (XZDist(r->pos, G.cam.position) > 260.0f) continue;
        if (G.night > 0) LightFor(r->pos);
        switch (r->kind) {
        case RES_PALM:  DrawPalm(r); break;
        case RES_REEDS: DrawReeds(i, r); break;
        case RES_TREE:  DrawTreeRes(i, r); break;
        case RES_CLAY:  DrawClay(r); break;
        case RES_ORE:   DrawOre(r); break;
        case RES_FISH:  DrawFish(i, r); break;
        default: break;
        }
    }
    G.drawTorch = G.drawFog = 0;
}
