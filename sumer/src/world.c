#include "game.h"
#include "rlgl.h"
#include <math.h>

#define MAX_KEEP 160
#define MAX_WATER 64
#define MAX_PONDS 4

static Rectangle keep[MAX_KEEP];
static int keepCount;
static int water[MAX_WATER], waterCount;
static Vector4 ponds[MAX_PONDS];
static int pondCount;
static bool building;

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
            G.boxes[G.boxCount++] = (Box){ { cx, y0 + h / 2, cz }, { sw, h, sd }, c, solid, false, false, 0.0f, SHAPE_BOX, building };
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
    G.boxes[G.boxCount++] = (Box){ { x, y0 + h / 2, z }, { w, h, d }, c, false, false, true, 0.0f, SHAPE_BOX, false };
}

static void WaterBox(float cx, float cz, float w, float d, int shape)
{
    if (G.boxCount >= MAX_BOXES) return;
    if (waterCount < MAX_WATER) water[waterCount++] = G.boxCount;
    G.boxes[G.boxCount++] = (Box){ { cx, WATER_LEVEL - 0.33f, cz }, { w, 0.66f, d }, COL_WATER, true, true, false, 0.0f, (unsigned char)shape, false };
}

// Long water is cut into sections so each one is culled by its own distance.
void AddWater(float x0, float z0, float x1, float z1)
{
    int nx = (int)ceilf((x1 - x0) / 48.0f), nz = (int)ceilf((z1 - z0) / 48.0f);
    float sw = (x1 - x0) / (float)nx, sd = (z1 - z0) / (float)nz;

    for (int i = 0; i < nx; i++)
        for (int j = 0; j < nz; j++)
            WaterBox(x0 + sw * ((float)i + 0.5f), z0 + sd * ((float)j + 0.5f), sw, sd, SHAPE_BOX);
}

// An oval of water over a hollow in the ground; the banks hide its edge, so the shore follows the land.
void AddPond(float cx, float cz, float rx, float rz)
{
    WaterBox(cx, cz, 2.0f * rx, 2.0f * rz, SHAPE_CYL);
    KeepOut(cx - rx, cz - rz, cx + rx, cz + rz);
    if (pondCount < MAX_PONDS) ponds[pondCount++] = (Vector4){ cx, cz, rx, rz };
}

void AddTree(float x, float z, float s, bool poplar)
{
    float y = TerrainHeight(x, z);

    AddShape(x, y - 0.2f, z, 0.36f * s, 2.6f * s, 0.36f * s, COL_BARK, true, SHAPE_CYL);
    if (poplar) {
        AddShape(x, y + 1.6f * s, z, 1.5f * s, 4.6f * s, 1.5f * s, (Color){ 82, 120, 60, 253 }, false, SHAPE_BLOB);
        AddShape(x, y + 4.8f * s, z, 0.9f * s, 2.2f * s, 0.9f * s, (Color){ 92, 130, 66, 253 }, false, SHAPE_BLOB);
    } else {
        AddShape(x, y + 1.7f * s, z, 3.4f * s, 2.0f * s, 3.2f * s, COL_TREE, false, SHAPE_BLOB);
        AddShape(x + 0.4f * s, y + 2.9f * s, z - 0.3f * s, 2.4f * s, 1.7f * s, 2.3f * s, (Color){ 84, 118, 64, 253 }, false, SHAPE_BLOB);
        AddShape(x - 0.7f * s, y + 2.4f * s, z + 0.6f * s, 1.8f * s, 1.3f * s, 1.7f * s, (Color){ 78, 112, 60, 253 }, false, SHAPE_BLOB);
    }
}

void KeepOut(float x0, float z0, float x1, float z1)
{
    if (keepCount < MAX_KEEP) keep[keepCount++] = (Rectangle){ x0, z0, x1 - x0, z1 - z0 };
}

// Roads count as kept clear too, so nothing grows or is built on them.
bool InKeepOut(float x, float z, float margin)
{
    for (int i = 0; i < keepCount; i++) {
        Rectangle r = keep[i];
        if (x > r.x - margin && x < r.x + r.width + margin && z > r.y - margin && z < r.y + r.height + margin)
            return true;
    }
    return RoadDistance(x, z) < margin + 0.5f;
}

int AddResource(float x, float z, ResKind kind)
{
    if (G.resCount >= MAX_RES) return -1;
    float y = (kind == RES_FISH) ? WATER_LEVEL : TerrainHeight(x, z);

    G.res[G.resCount] = (Resource){ { x, y, z }, kind, 0.0f, RandF(0, 360), false };
    if (kind == RES_PALM) AddShape(x, y - 0.2f, z, 0.46f, 6.7f, 0.46f, COL_BARK, true, SHAPE_CYL);
    else if (kind == RES_TREE) AddShape(x, y - 0.2f, z, 0.5f, 1.0f, 0.5f, COL_BARK, true, SHAPE_CYL);
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

// A pond is wet only inside its oval and below its banks.
bool InWater(float x, float z)
{
    for (int k = 0; k < waterCount; k++) {
        const Box *b = &G.boxes[water[k]];
        if (!Contains(b, x, z, 0)) continue;
        if (b->shape != SHAPE_CYL) return true;
        float dx = (x - b->pos.x) / (b->size.x / 2), dz = (z - b->pos.z) / (b->size.z / 2);
        if (dx * dx + dz * dz < 1.0f && TerrainHeight(x, z) < WATER_LEVEL + 0.05f) return true;
    }
    return false;
}

// How fast someone moves wading here: a canal's water slows them by 60%, the river's deep current by 85%.
float WadeSpeed(float x, float z)
{
    if (!InWater(x, z)) return 1.0f;
    return x > RIVER_X0 - 1.0f && x < RIVER_X1 + 1.0f ? 0.15f : 0.4f;
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

static void Tuft(float x, float z, float s, Color c)
{
    float y = TerrainHeight(x, z) - 0.05f;
    AddShape(x, y, z, s * 0.45f, s * RandF(1.2f, 1.8f), s * 0.45f, c, false, SHAPE_CONE);
    AddShape(x + s * 0.25f, y, z + s * 0.15f, s * 0.35f, s * RandF(0.8f, 1.3f), s * 0.35f, Mix(c, COL_STRAW, 0.2f), false, SHAPE_CONE);
}

// Small things of the open land, kept to what grew on the plain: feathery tamarisk by the water, low
// camelthorn on the dry steppe, fallen limbs and stumps of dead poplar, flat slabs and scatters of
// stones on the rises. None of it blocks the way.
static bool Open(float x, float z, float margin)
{
    return fabsf(x) < WORLD_LIMIT - 4 && fabsf(z) < WORLD_LIMIT - 4 && !InWater(x, z) &&
           !InsideWalls(x, z, -6.0f) && !InKeepOut(x, z, margin) && !SolidAt(x, z, margin);
}

static void BuildWilds(void)
{
    const float L = WORLD_LIMIT - 4;
    const Color tamarisk = { 112, 128, 100, 253 }, bloom = { 196, 150, 160, 253 }, thorn = { 132, 128, 78, 253 };
    const Color deadwood = { 140, 128, 110, 254 }, pale = { 164, 154, 136, 254 };

    for (int k = 0, tries = 0; k < 170 && tries < 3000; tries++) {
        float x = RandF(-L, L), z = RandF(-L, L), f = Fertility(x, z);
        if (f < 0.25f || f > 0.75f || !Open(x, z, 1.5f)) continue;
        float s = RandF(0.8f, 1.5f), y = TerrainHeight(x, z) - 0.1f;
        Color c = Hash2(tries, 41) < 0.25f ? Mix(tamarisk, bloom, 0.45f) : tamarisk;
        AddShape(x, y, z, 1.3f * s, 1.5f * s, 1.2f * s, c, false, SHAPE_BLOB);
        AddShape(x + 0.6f * s, y, z - 0.3f * s, 0.6f * s, 1.6f * s, 0.6f * s, Mix(c, COL_SHRUB, 0.3f), false, SHAPE_CONE);
        AddShape(x - 0.5f * s, y, z + 0.5f * s, 1.0f * s, 0.9f * s, 0.9f * s, c, false, SHAPE_BLOB);
        k++;
    }
    for (int k = 0, tries = 0; k < 260 && tries < 3000; tries++) {
        float x = RandF(-L, L), z = RandF(-L, L);
        if (Fertility(x, z) > 0.3f || !Open(x, z, 1.0f)) continue;
        float s = RandF(0.4f, 0.8f), y = TerrainHeight(x, z) - 0.08f;
        AddShape(x, y, z, 1.3f * s, 0.6f * s, 1.2f * s, thorn, false, SHAPE_BLOB);
        AddShape(x + 0.3f * s, y, z + 0.2f * s, 0.5f * s, 0.9f * s, 0.5f * s, Mix(thorn, COL_STRAW, 0.4f), false, SHAPE_CONE);
        k++;
    }
    for (int k = 0, tries = 0; k < 90 && tries < 2000; tries++) {
        float x = RandF(-L, L), z = RandF(-L, L);
        if (!Open(x, z, 2.5f)) continue;
        float y = TerrainHeight(x, z), len = RandF(2.0f, 4.0f), r = RandF(0.09f, 0.16f);
        bool alongX = Hash2(tries, 43) < 0.5f;
        Color c = Hash2(tries, 47) < 0.5f ? deadwood : pale;
        if (Hash2(tries, 53) < 0.3f) {   // a broken stump with its top fallen beside it
            AddShape(x, y - 0.1f, z, 2.2f * r, RandF(0.5f, 1.1f), 2.2f * r, c, false, SHAPE_CYL);
            x += 1.2f;
        }
        AddBox(x, y - 0.05f, z, alongX ? len : 2 * r, 2 * r, alongX ? 2 * r : len, c, false);
        AddBox(x + (alongX ? len * 0.2f : 0.5f), y, z + (alongX ? 0.5f : len * 0.2f), alongX ? 0.9f : 0.1f, 0.1f, alongX ? 0.1f : 0.9f, c, false);
        k++;
    }
    for (int k = 0, tries = 0; k < 140 && tries < 3000; tries++) {
        float x = RandF(-L, L), z = RandF(-L, L), h = TerrainHeight(x, z);
        if ((h < 1.0f && Fertility(x, z) > 0.3f) || !Open(x, z, 1.5f)) continue;
        Color c = Mix(COL_ROCK, (Color){ 150, 136, 112, 255 }, Hash2(tries, 59));
        if (Hash2(tries, 61) < 0.4f) {   // a flat slab, half sunk
            float s = RandF(1.0f, 2.2f);
            AddBox(x, h - 0.12f, z, s, RandF(0.18f, 0.35f), s * RandF(0.5f, 0.9f), c, false);
        } else {   // a scatter of stones
            for (int j = 0; j < 5; j++) {
                float s = RandF(0.15f, 0.45f);
                AddShape(x + RandF(-1.2f, 1.2f), h - 0.3f * s, z + RandF(-1.2f, 1.2f), s * 1.2f, s, s, Mix(c, COL_STONE, Hash2(tries, j)), false, SHAPE_BLOB);
            }
        }
        k++;
    }
}

// Herbs grow where the ground is green: river meadows, the pastures, the hunters' glade, the west bank.
static void PlaceHerbs(float x0, float x1, float z0, float z1, int n)
{
    for (int k = 0, tries = 0; k < n && tries < n * 30; tries++) {
        float x = RandF(x0, x1), z = RandF(z0, z1);
        if (Fertility(x, z) < 0.5f || InsideWalls(x, z, -8.0f) || !FreeSpot(x, z, 1.2f)) continue;
        AddResource(x, z, RES_HERB);
        k++;
    }
}

// Across the river: woods, flowering meadows, scattered trees and a reed-fringed pond.
static void BuildWestBank(void)
{
    static const Color flowers[4] = { { 236, 200, 60, 255 }, { 240, 236, 226, 255 }, { 160, 96, 196, 255 }, { 210, 60, 50, 255 } };
    const float x0 = -WORLD_LIMIT + 4, x1 = RIVER_X0 - 8, z0 = -WORLD_LIMIT + 4, z1 = WORLD_LIMIT - 4;

    AddPond(-333, 174, 31, 27);
    AddResource(-336, 168, RES_FISH);
    AddResource(-324, 182, RES_FISH);
    for (int k = 0; k < 48; k++) {
        float a = (float)k * 2.0f * PI / 48.0f + RandF(-0.05f, 0.05f), r = RandF(0.55f, 0.95f);
        float x = -333 + cosf(a) * 31 * r, z = 174 + sinf(a) * 27 * r;
        if (InWater(x, z) || TerrainHeight(x, z) > 1.2f) continue;
        if (k % 6 == 0) AddResource(x, z, RES_REEDS);
        else
            for (int j = 0; j < 3; j++)
                AddBox(x + RandF(-0.6f, 0.6f), TerrainHeight(x, z) - 0.1f, z + RandF(-0.6f, 0.6f), 0.09f, RandF(1.3f, 2.4f), 0.09f, COL_REED, false);
    }

    Forest(-250, -300, 30, 24, 10, false);
    Forest(-460, -180, 34, 26, 10, true);
    Forest(-225, 30, 24, 18, 8, false);
    Forest(-420, 300, 34, 26, 10, false);
    Forest(-500, -390, 28, 18, 6, true);
    Forest(-300, 455, 28, 20, 8, false);
    Forest(-480, 75, 26, 18, 6, true);
    Forest(-370, -40, 22, 16, 6, false);

    for (int k = 0, tries = 0; k < 120 && tries < 900; tries++) {
        float x = RandF(x0, x1), z = RandF(z0, z1);
        if (!FreeSpot(x, z, 2.5f)) continue;
        AddTree(x, z, RandF(0.8f, 1.4f), Hash2(tries, 17) < 0.35f);
        k++;
    }
    for (int tries = 0; tries < 420; tries++) {
        float x = RandF(x0, x1), z = RandF(z0, z1);
        if (InWater(x, z) || InKeepOut(x, z, 1.0f) || SolidAt(x, z, 0.5f)) continue;
        float s = RandF(0.5f, 1.2f);
        Color c = Mix((Color){ 76, 118, 52, 255 }, (Color){ 102, 140, 60, 255 }, Hash2(tries, 23));
        AddShape(x, TerrainHeight(x, z) - 0.15f, z, s * 1.2f, s * 0.9f, s * 1.1f, c, false, SHAPE_BLOB);
    }
    for (int tries = 0; tries < 320; tries++) {
        float x = RandF(x0, x1), z = RandF(z0, z1);
        if (InWater(x, z) || InKeepOut(x, z, 0.5f)) continue;
        Color c = flowers[(int)(Hash2(tries, 31) * 3.99f)];
        float y = TerrainHeight(x, z);
        for (int j = 0; j < 4; j++) {
            float fx = x + RandF(-0.7f, 0.7f), fz = z + RandF(-0.7f, 0.7f);
            AddShape(fx, y - 0.05f, fz, 0.06f, 0.42f, 0.06f, COL_GRASS, false, SHAPE_CYL);
            AddShape(fx, y + 0.34f, fz, 0.2f, 0.14f, 0.2f, c, false, SHAPE_BLOB);
        }
    }
    for (int tries = 0; tries < 1700; tries++) {
        float x = RandF(x0, x1), z = RandF(z0, z1);
        if (InKeepOut(x, z, 0.5f) || InWater(x, z)) continue;
        Tuft(x, z, RandF(0.35f, 0.7f), Mix(COL_GRASS, (Color){ 70, 126, 46, 255 }, Hash2(tries, 37)));
    }
    for (int tries = 0; tries < 160; tries++) {
        float x = RandF(x0, x1), z = RandF(z0, z1);
        if (TerrainHeight(x, z) < 13.0f || InKeepOut(x, z, 2.0f) || SolidAt(x, z, 1.0f)) continue;
        float s = RandF(0.8f, 2.0f);
        AddShape(x, TerrainHeight(x, z) - 0.35f * s, z, s, s * 0.9f, s * 0.8f, Mix(COL_ROCK, COL_STONE, Hash2(tries, 41)), true, SHAPE_BLOB);
    }
}

static void BuildVegetation(void)
{
    static const float cityPalms[][2] = { { -14, -16 }, { 14, -16 }, { -52, -29 }, { -52, 5 } };
    const float L = WORLD_LIMIT;

    for (int i = 0; i < 4; i++) AddResource(cityPalms[i][0], cityPalms[i][1], RES_PALM);
    for (int i = 0; i < 46; i++) PlacePalm(-124, -104, -510, 510);
    for (int i = 0; i < 8; i++) PlacePalm(-80, 100, -116, -111);
    for (int i = 0; i < 11; i++) PlacePalm(-9, -5, 140, 520);
    for (int i = 0; i < 11; i++) PlacePalm(5, 9, 140, 520);
    for (int i = 0; i < 8; i++) PlacePalm(104, 117, -90, 80);

    for (float z = -510.0f; z <= 510.0f; z += 12.0f) {
        if (fabsf(z + 10) < 8 || fabsf(z - 30) < 6 || fabsf(z + 62) < 6 || (z > FORD_Z0 - 4 && z < FORD_Z1 + 4)) continue;
        AddResource(-126.4f + RandF(-0.4f, 0.4f), z + RandF(-2, 2), RES_REEDS);
    }
    for (int i = 0; i < 92; i++) {
        float x = RandF(-127.5f, -121.0f), z = RandF(-515, 515);
        if (fabsf(z + 10) < 8 || InKeepOut(x, z, 1.0f)) continue;
        for (int k = 0; k < 3; k++) {
            float rx = x + RandF(-0.5f, 0.5f), rz = z + RandF(-0.5f, 0.5f);
            AddBox(rx, TerrainHeight(rx, rz) - 0.1f, rz, 0.09f, RandF(1.3f, 2.4f), 0.09f, COL_REED, false);
        }
    }

    Forest(198, 52, 22, 26, 16, false);
    Forest(242, -105, 24, 18, 8, false);
    Forest(218, 190, 22, 16, 8, false);
    Forest(-110, -200, 18, 16, 8, true);
    Forest(-112, 250, 16, 12, 6, true);
    Forest(300, 180, 28, 22, 8, false);
    Forest(290, -200, 26, 20, 8, false);
    Forest(475, -45, 30, 24, 10, false);
    Forest(395, 95, 26, 18, 8, false);
    Forest(470, 150, 22, 14, 6, false);
    Forest(390, -300, 30, 20, 6, false);
    Forest(250, 430, 26, 16, 6, false);
    Forest(-108, -400, 18, 12, 6, true);

    BuildWestBank();

    for (int tries = 0; tries < 2400; tries++) {
        float x = RandF(RIVER_X1, L), z = RandF(-L, L);
        float h = TerrainHeight(x, z), f = Fertility(x, z);
        if (InsideWalls(x, z, -3.0f) || InKeepOut(x, z, 0.5f) || InWater(x, z)) continue;
        if (f < 0.35f && !(h > 1.5f && Hash2(tries, 5) < 0.5f)) continue;
        Tuft(x, z, RandF(0.3f, 0.6f), Mix(COL_GRASS, COL_WHEAT_GREEN, Hash2(tries, 3)));
    }
    for (int tries = 0; tries < 700; tries++) {
        float x = RandF(RIVER_X1, L), z = RandF(-L, L);
        if (x < -60) continue;
        if (Fertility(x, z) > 0.35f || InsideWalls(x, z, -6.0f) || InKeepOut(x, z, 1.0f) || SolidAt(x, z, 0.5f)) continue;
        float s = RandF(0.5f, 1.1f);
        AddShape(x, TerrainHeight(x, z) - 0.15f, z, s * 1.2f, s * 0.9f, s * 1.1f, COL_SHRUB, false, SHAPE_BLOB);
    }
    for (int tries = 0; tries < 700; tries++) {
        float x = RandF(145, L), z = RandF(-L, L);
        if (TerrainHeight(x, z) < 5.0f || InKeepOut(x, z, 2.0f) || SolidAt(x, z, 1.0f)) continue;
        float s = RandF(0.8f, 2.2f);
        AddShape(x, TerrainHeight(x, z) - 0.35f * s, z, s, s * 0.9f, s * 0.8f, Mix(COL_ROCK, COL_STONE, Hash2(tries, 9)), true, SHAPE_BLOB);
    }

    BuildWilds();
    PlaceHerbs(-124, -100, -500, 500, 12);
    PlaceHerbs(-125, -88, -235, -165, 3);
    PlaceHerbs(-125, -92, 225, 275, 3);
    PlaceHerbs(-120, -15, 330, 510, 6);
    PlaceHerbs(370, 510, -70, 110, 6);
    PlaceHerbs(-530, -160, -530, 530, 48);
}

// Things are placed on the true lie of the land, but the ground is drawn as 6 m triangles, which sag
// below it over hilltops and the world's edge. Once the ground is built, whatever was set on the land
// (its bottom no more than a hand above it) reaches down to the lowest drawn ground under it: walls,
// posts and trunks grow down, round things (rocks, bushes, huts, tents) sink. Things on floors and
// platforms stand higher and are left alone.
static void SettleOnGround(void)
{
    for (int i = 0; i < G.boxCount; i++) {
        Box *b = &G.boxes[i];
        if (b->water || b->glow) continue;
        float bottom = b->pos.y - b->size.y / 2, lo = 1e9f;
        if (bottom > TerrainHeight(b->pos.x, b->pos.z) + 0.25f) continue;
        for (int u = 0; u <= 2; u++)
            for (int v = 0; v <= 2; v++)
                lo = fminf(lo, GroundSurface(b->pos.x + b->size.x * ((float)u / 2 - 0.5f), b->pos.z + b->size.z * ((float)v / 2 - 0.5f)));
        float gap = bottom - (lo - 0.05f);
        if (gap <= 0) continue;
        if (b->shape == SHAPE_BLOB || b->shape == SHAPE_CONE) {
            b->pos.y -= gap;
        } else {
            b->size.y += gap;
            b->pos.y -= gap / 2;
        }
    }
}

void BuildWorld(void)
{
    keepCount = 0;
    waterCount = 0;
    pondCount = 0;
    building = true;
    BuildCity();
    BuildVillage();
    BuildJobs();
    BuildRaiders();
    BuildVillages();
    BuildCityHouses();
    building = false;
    BuildVegetation();
    BuildTerrain();
    SettleOnGround();
    BuildRoads();
    BuildLights();

    for (int i = 0; i < MAX_STARS; i++)
        G.stars[i] = Vector3Normalize((Vector3){ RandF(-1, 1), RandF(0.12f, 1), RandF(-1, 1) });
    GfxWorldChanged();
}

Vector3 SunDirection(void)
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

// Distance inside a rectangle to its nearest edge; 0 outside.
static float EdgeDepth(Rectangle r, float x, float z)
{
    return fmaxf(0.0f, fminf(fminf(x - r.x, r.x + r.width - x), fminf(z - r.y, r.y + r.height - z)));
}

// Darker where deep, with a slow shimmer; torchlight and night fog as on the ground.
static void WaterVertex(float x, float z, float deep, float t)
{
    float shade = 0.98f - 0.15f * deep + 0.035f * sinf(t * 1.1f + x * 0.31f) * cosf(t * 0.8f + z * 0.27f);
    G.drawTorch = GroundTorch(x, z);
    G.drawFog = FogAt((Vector3){ x, WATER_LEVEL, z }) * (1.0f - 0.6f * G.drawTorch);
    Color c = Lit(COL_WATER, shade);
    VColor(c);
    VVertex(x, WATER_LEVEL, z);
}

// Water lies flat a little below the land. Its surface reaches under the banks, so the cut ground,
// not the surface's edge, decides where the shore runs.
void DrawWater(void)
{
    const float cell = 8.0f, t = (float)GetTime();
    Vector3 eye = G.cam.position, fwd = Vector3Normalize(Vector3Subtract(G.cam.target, eye));
    bool all = DrawingAll();

    for (int c = 0; c < ChannelCount(); c++) {
        bool wet;
        Rectangle r, w = ChannelRect(c, &wet, &r);
        float x0 = fmaxf(r.x, -WORLD_HALF), z0 = fmaxf(r.y, -WORLD_HALF);
        float x1 = fminf(r.x + r.width, WORLD_HALF), z1 = fminf(r.y + r.height, WORLD_HALF);
        int nx = (int)ceilf((x1 - x0) / cell), nz = (int)ceilf((z1 - z0) / cell);
        float sx = (x1 - x0) / (float)nx, sz = (z1 - z0) / (float)nz, deep = wet ? 1.0f / 7.0f : 0.0f;
        for (int i = 0; i < nx; i++) {
            rlCheckRenderBatchLimit(4 * nz);
            VBegin(RL_QUADS);
            VNormal(0, 1, 0);
            for (int j = 0; j < nz; j++) {
                float xa = x0 + sx * (float)i, za = z0 + sz * (float)j, xb = xa + sx, zb = za + sz;
                Vector3 mid = { (xa + xb) / 2 - eye.x, WATER_LEVEL - eye.y, (za + zb) / 2 - eye.z };
                if (!all && (mid.x * mid.x + mid.z * mid.z > 420.0f * 420.0f || Vector3DotProduct(mid, fwd) < -12.0f)) continue;
                WaterVertex(xa, za, fminf(1.0f, EdgeDepth(w, xa, za) * deep), t);
                WaterVertex(xa, zb, fminf(1.0f, EdgeDepth(w, xa, zb) * deep), t);
                WaterVertex(xb, zb, fminf(1.0f, EdgeDepth(w, xb, zb) * deep), t);
                WaterVertex(xb, za, fminf(1.0f, EdgeDepth(w, xb, za) * deep), t);
            }
            VEnd();
        }
    }

    for (int p = 0; p < pondCount; p++) {
        const int segs = 40;
        Vector4 o = ponds[p];
        if (!all && XZDist((Vector3){ o.x, 0, o.y }, eye) > 420.0f) continue;
        rlCheckRenderBatchLimit(4 * segs);
        VBegin(RL_QUADS);
        VNormal(0, 1, 0);
        for (int k = 0; k < segs; k++) {
            float a0 = 2.0f * PI * (float)k / segs, a1 = 2.0f * PI * (float)(k + 1) / segs;
            float rx = o.z + 2.0f, rz = o.w + 2.0f;
            WaterVertex(o.x, o.y, 1.0f, t);
            WaterVertex(o.x + cosf(a1) * rx, o.y + sinf(a1) * rz, 0.0f, t);
            WaterVertex(o.x + cosf(a0) * rx, o.y + sinf(a0) * rz, 0.0f, t);
            WaterVertex(o.x + cosf(a0) * rx, o.y + sinf(a0) * rz, 0.0f, t);
        }
        VEnd();
    }

    // The river runs on past the edge of the world, between low banks.
    const float F = 1150.0f, A = RIVER_X0 - 3.0f, B = RIVER_X1 + 3.0f, L = WATER_LEVEL;
    G.drawTorch = 0;
    G.drawFog = G.night * 0.8f;
    Color c = Lit(COL_WATER, 0.9f), bank = Lit((Color){ 104, 88, 62, 255 }, 0.8f);
    rlCheckRenderBatchLimit(24);
    VBegin(RL_QUADS);
    for (int s = -1; s <= 1; s += 2) {
        float za = s < 0 ? -F : WORLD_HALF, zb = s < 0 ? -WORLD_HALF : F;
        VColor(c);
        VNormal(0, 1, 0);
        VVertex(A, L, za); VVertex(A, L, zb); VVertex(B, L, zb); VVertex(B, L, za);
        int mat = CurrentMaterial();
        SetMaterial(MAT_GROUND);
        VColor(bank);
        VNormal(1, 0, 0);
        VVertex(A, L, zb); VVertex(A, L, za); VVertex(A, 0.0f, za); VVertex(A, 0.0f, zb);
        VNormal(-1, 0, 0);
        VVertex(B, L, za); VVertex(B, L, zb); VVertex(B, 0.0f, zb); VVertex(B, 0.0f, za);
        SetMaterial(mat);
    }
    VEnd();
    G.drawTorch = G.drawFog = 0;
}

// One box at the detail fitting its squared distance d2 from the eye: buildings up close get twice
// the facets and softened edges.
void DrawBoxAt(int i, float d2)
{
    const Box *b = &G.boxes[i];
    if (b->glow) {
        DrawGlowBox(b->pos, b->size, b->color);
        return;
    }
    Vector3 base = { b->pos.x, b->pos.y - b->size.y / 2, b->pos.z };
    bool close = b->built && d2 < 45.0f * 45.0f;
    int sides = close ? 16 : d2 > 60.0f * 60.0f ? 5 : 8;
    float small = fminf(fminf(b->size.x, b->size.y), b->size.z);
    switch (b->shape) {
    case SHAPE_CYL:  DrawShadedCylinder(base, b->size.x / 2, b->size.z / 2, b->size.y, 1.0f, sides, b->color); break;
    case SHAPE_CONE: DrawShadedCylinder(base, b->size.x / 2, b->size.z / 2, b->size.y, 0.0f, sides, b->color); break;
    case SHAPE_BLOB:
        if (close) DrawShadedBlobFine(b->pos, Vector3Scale(b->size, 0.5f), b->color);
        else if (d2 > 70.0f * 70.0f) DrawShadedBlobFar(b->pos, Vector3Scale(b->size, 0.5f), b->color);
        else DrawShadedBlob(b->pos, Vector3Scale(b->size, 0.5f), b->color);
        break;
    default:
        if (close && b->solid && small >= 0.4f) DrawBeveledBox(b->pos, b->size, b->color, fminf(0.12f, 0.14f * small));
        else DrawShadedBox(b->pos, b->size, b->color);
        break;
    }
}

void DrawTerrain(void)
{
    Vector3 eye = G.cam.position;
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, eye));

    DrawGround();
    DrawWater();
    DrawRoads();
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (b->water) continue;
        Vector3 v = Vector3Subtract(b->pos, eye);
        float r = (b->size.x + b->size.y + b->size.z) * 0.5f;
        float d2 = Vector3DotProduct(v, v);
        if (Vector3DotProduct(v, fwd) < -r || d2 > 340.0f * 340.0f || (r < 1.0f && d2 > 150.0f * 150.0f)) continue;
        if (!b->glow) {
            G.drawTorch = BoxLight(i);
            G.drawFog = FogAt(b->pos) * (1.0f - 0.6f * G.drawTorch);
        }
        DrawBoxAt(i, d2);
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
    DrawShadedBlob((Vector3){ r->pos.x, r->pos.y + 6.4f, r->pos.z }, (Vector3){ 0.4f, 0.3f, 0.4f }, COL_BARK);
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
    bool poplar = WestBank(x) ? Hash2(i, 29) < 0.35f : x < -60.0f;

    DrawShadedCylinder((Vector3){ x, y + 0.7f, z }, 0.2f * s, 0.2f * s, 2.3f * s, 0.75f, 7, COL_BARK);
    if (poplar) {
        DrawShadedBlob((Vector3){ x, y + 4.0f * s, z }, (Vector3){ 0.75f * s, 2.4f * s, 0.75f * s }, (Color){ 82, 120, 60, 253 });
        DrawShadedBlob((Vector3){ x, y + 6.4f * s, z }, (Vector3){ 0.45f * s, 1.1f * s, 0.45f * s }, (Color){ 92, 130, 66, 253 });
    } else {
        DrawShadedBlob((Vector3){ x, y + 3.0f * s, z }, (Vector3){ 1.7f * s, 1.05f * s, 1.6f * s }, COL_TREE);
        DrawShadedBlob((Vector3){ x + 0.4f * s, y + 3.9f * s, z - 0.3f * s }, (Vector3){ 1.2f * s, 0.9f * s, 1.15f * s }, (Color){ 84, 118, 64, 253 });
        DrawShadedBlob((Vector3){ x - 0.7f * s, y + 3.5f * s, z + 0.6f * s }, (Vector3){ 0.9f * s, 0.7f * s, 0.85f * s }, (Color){ 78, 112, 60, 253 });
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
    float s = 0.2f + 0.7f * ph;

    // A ring spreading on the water: short pale dashes round a circle, fading as it grows.
    Color c = Fade((Color){ 200, 222, 228, 255 }, 0.7f * (1.0f - ph));
    for (int k = 0; k < 14; k++) {
        float a = (float)k * 2.0f * PI / 14.0f;
        rlPushMatrix();
        rlTranslatef(r->pos.x + cosf(a) * s, r->pos.y + 0.06f, r->pos.z + sinf(a) * s);
        rlRotatef(-a * RAD2DEG, 0, 1, 0);
        DrawShadedBox((Vector3){ 0, 0, 0 }, (Vector3){ 0.04f, 0.01f, s * 0.42f }, c);
        rlPopMatrix();
    }
    if (ph < 0.25f) {
        float a = ph / 0.25f;
        Part(r->pos.x + 0.6f * a - 0.3f, r->pos.y + 0.03f + 0.5f * sinf(a * PI), r->pos.z, 0.3f, 0.08f, 0.1f, (Color){ 170, 180, 186, 255 });
    }
}

// A low sage-green clump; while it can be picked it carries bright flowers, so it stands out from grass.
static void DrawHerb(int i, const Resource *r)
{
    static const Color bloom[3] = { { 172, 104, 206, 255 }, { 242, 208, 64, 255 }, { 246, 244, 236, 255 } };
    const Color leaf = { 92, 140, 84, 255 }, pale = { 128, 164, 104, 255 };
    float x = r->pos.x, y = r->pos.y, z = r->pos.z, s = r->regrow > 0 ? 0.45f : 1.0f;

    DrawShadedBlob((Vector3){ x, y + 0.16f * s, z }, (Vector3){ 0.42f * s, 0.26f * s, 0.42f * s }, leaf);
    DrawShadedBlob((Vector3){ x + 0.22f * s, y + 0.12f * s, z - 0.16f * s }, (Vector3){ 0.26f * s, 0.2f * s, 0.26f * s }, pale);
    if (r->regrow > 0) return;
    for (int k = 0; k < 6; k++) {
        float a = (float)k * 1.047f + (float)i, d = 0.14f + 0.12f * (float)(k & 1);
        Part(x + cosf(a) * d, y + 0.44f + 0.06f * (float)(k % 3), z + sinf(a) * d, 0.1f, 0.1f, 0.1f, bloom[i % 3]);
    }
}

void DrawResource(int i)
{
    const Resource *r = &G.res[i];
    switch (r->kind) {
    case RES_PALM:  DrawPalm(r); break;
    case RES_REEDS: DrawReeds(i, r); break;
    case RES_TREE:  DrawTreeRes(i, r); break;
    case RES_CLAY:  DrawClay(r); break;
    case RES_ORE:   DrawOre(r); break;
    case RES_FISH:  DrawFish(i, r); break;
    case RES_HERB:  DrawHerb(i, r); break;
    default: break;
    }
}

// Fish rings are the only resources that move; the shaders' meshes hold the rest (DrawResourcesOf).
void DrawResourcesOf(bool still, bool moving)
{
    for (int i = 0; i < G.resCount; i++) {
        const Resource *r = &G.res[i];
        if (r->kind == RES_FISH ? !moving : !still) continue;
        float d = XZDist(r->pos, G.cam.position);
        if (d > 260.0f || (r->kind == RES_HERB && d > 130.0f)) continue;
        if (G.night > 0) LightFor(r->pos);
        DrawResource(i);
    }
    G.drawTorch = G.drawFog = 0;
}

void DrawResources(void)
{
    DrawResourcesOf(true, true);
}
