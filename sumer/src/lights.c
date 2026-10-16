#include "game.h"
#include "rlgl.h"
#include <math.h>

#define TORCH_RADIUS  10.0f
#define HAND_RADIUS   10.0f
#define LIGHT_MAX     0.95f

static int fireCount;
static Vector3 fires[24];
static float flicker = 1.0f;

void AddTorch(float x, float y, float z, bool pole)
{
    if (G.torchCount == 0) fireCount = 0;
    if (G.torchCount >= MAX_TORCHES) return;
    if (pole) {
        AddShape(x, y, z, 0.14f, 1.9f, 0.14f, COL_WOOD, true, SHAPE_CYL);
        AddShape(x, y + 1.82f, z, 0.32f, 0.2f, 0.32f, COL_CLAY_DARK, false, SHAPE_CYL);
        G.torches[G.torchCount++] = (Vector3){ x, y + 2.15f, z };
    } else {
        AddBox(x, y - 0.3f, z, 0.16f, 0.3f, 0.16f, COL_CLAY_DARK, false);
        G.torches[G.torchCount++] = (Vector3){ x, y, z };
    }
}

// A fire lights its surroundings like a torch but draws no flame of its own.
void AddFireLight(float x, float y, float z)
{
    if (G.torchCount == 0) fireCount = 0;
    if (G.torchCount >= MAX_TORCHES) return;
    G.torches[G.torchCount++] = (Vector3){ x, y, z };
    if (fireCount < (int)(sizeof(fires) / sizeof(fires[0]))) fires[fireCount++] = (Vector3){ x, y, z };
}

static void AddFire(float x, float y, float z)
{
    AddFireLight(x, y, z);
}

static void PlaceTorches(void)
{
    static const float poles[][2] = {
        { -4.2f, 0 }, { 4.2f, 0 }, { -4.2f, 18 }, { 4.2f, 18 }, { -4.2f, 34 }, { 4.2f, 34 }, { -4.2f, 50 }, { 4.2f, 50 },
        { -66, 6.3f }, { -48, 11.7f }, { -26, 6.3f }, { 26, 11.7f }, { 48, 6.3f }, { 66, 11.7f }, { -84, 11.7f }, { 84, 6.3f },
        { -4.2f, 64 }, { 4.2f, 64 }, { -4.2f, 76 }, { 4.2f, 76 }, { 94, 5 }, { 94, 13 }, { -94, 23.5f }, { -94, 28.5f },
        { -39.5f, -84 }, { -34.5f, -84 }, { -39.5f, -60 }, { -34.5f, -60 }, { 80, 37.4f }, { -80, 40.6f },
        { -12, -8.6f }, { 12, -8.6f }, { -2.8f, -13.4f }, { 2.8f, -13.4f }, { -25.6f, -12 }, { 25.6f, -12 },
        { -56.6f, -22 }, { -56.6f, -2 }, { 48, 21 }, { 13.8f, 20.2f }, { -42, 15 }, { -42, 31 },
        { -30, 37.4f }, { -60, 40.6f }, { 20, 40.6f }, { 60, 37.4f },
    };
    const float cz = ZIG_Z;

    for (int i = 0; i < (int)(sizeof(poles) / sizeof(poles[0])); i++) AddTorch(poles[i][0], 0, poles[i][1], true);
    for (int s = -1; s <= 1; s += 2) {
        float f = (float)s;
        AddTorch(12 * f, 7, -24, true);
        AddTorch(3.2f * f, 9.4f, -21.85f, false);
        AddTorch(5.0f * f, 9.2f, -29.85f, false);
        AddTorch(1.4f * f, 19.2f, -41.85f, false);
        AddTorch(6.5f * f, 16.5f, -48.5f, true);
        AddFire(4.3f * f, 8.1f, -48.6f);
    }
    for (int c = 0; c < CAMPS; c++) {
        Vector3 camp = CampCenter(c);
        AddFire(camp.x, camp.y + 0.4f, camp.z);
    }

    AddFire(-3, 1.15f, cz + 8);
    AddFire(3, 1.15f, cz + 8);
    AddFire(-50.8f, 0.45f, 18);
    AddFire(-52, 2.2f, 28);
    AddFire(G.stations[STN_OVEN].x, 1.2f, G.stations[STN_OVEN].z);
}

static float Falloff(float d, float r)
{
    if (d >= r) return 0.0f;
    float k = 1.0f - d / r;
    return k * k;
}

static float BoxDistance(const Box *b, Vector3 p)
{
    float dx = fmaxf(fabsf(p.x - b->pos.x) - b->size.x / 2, 0.0f);
    float dy = fmaxf(fabsf(p.y - b->pos.y) - b->size.y / 2, 0.0f);
    float dz = fmaxf(fabsf(p.z - b->pos.z) - b->size.z / 2, 0.0f);
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// Static torches never move, so their light on boxes and ground is summed once.
void BuildLights(void)
{
    PlaceTorches();
    for (int i = 0; i < G.boxCount; i++) {
        Box *b = &G.boxes[i];
        float sum = 0;
        for (int t = 0; t < G.torchCount; t++) sum += Falloff(BoxDistance(b, G.torches[t]), TORCH_RADIUS);
        b->torch = fminf(sum, LIGHT_MAX);
    }
    for (int i = 0; i < GROUND_TILES; i++) {
        for (int j = 0; j < GROUND_TILES; j++) {
            float x = -WORLD_HALF + ((float)i + 0.5f) * TILE_SIZE, z = -WORLD_HALF + ((float)j + 0.5f) * TILE_SIZE;
            Vector3 c = { x, TerrainHeight(x, z), z };
            float sum = 0;
            for (int t = 0; t < G.torchCount; t++) sum += Falloff(Vector3Distance(c, G.torches[t]), TORCH_RADIUS);
            G.tileTorch[i][j] = fminf(sum, LIGHT_MAX);
        }
    }
}

static Vector3 HandPos(Vector3 pos, float f)
{
    const float x = -0.36f, y = 1.62f, z = 0.28f;
    return (Vector3){ pos.x + x * cosf(f) + z * sinf(f), pos.y + y, pos.z - x * sinf(f) + z * cosf(f) };
}

static void AddDyn(Vector3 p)
{
    if (G.dynCount < MAX_DYN_LIGHTS) G.dynLights[G.dynCount++] = p;
}

// Hand torches far from the camera light nothing anyone could see.
static void AddDynNear(Vector3 pos, float face)
{
    if (XZDist(pos, G.cam.position) < 220.0f) AddDyn(HandPos(pos, face));
}

void UpdateLights(void)
{
    float t = (float)GetTime();
    flicker = 0.92f + 0.08f * sinf(t * 9.0f) * sinf(t * 5.3f + 1.0f);
    G.night = Clamp((0.75f - G.daylight) / 0.6f, 0.0f, 1.0f);
    G.dynCount = 0;
    if (G.night < 0.3f) return;

    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if ((v->kind == NPC_GUARD || v->kind == NPC_TRAVELLER) && !v->vit.dead) AddDynNear(VillagerPos(v), v->vit.override ? v->vit.face : v->face);
    }
    for (int c = 0; c < MAX_CARAVANS; c++) {
        for (int k = 0; k < 3 + CARAVAN_GUARDS; k++) {
            Vector3 p;
            float f;
            if ((k == 0 || k >= 3) && CaravanMember(c, k, &p, &f)) AddDynNear(p, f);
        }
    }
}

static float Flicker(void)
{
    return flicker;
}

static float Dynamic(Vector3 p, float radius)
{
    float sum = 0;
    for (int i = 0; i < G.dynCount; i++) sum += Falloff(fmaxf(Vector3Distance(p, G.dynLights[i]) - radius, 0.0f), HAND_RADIUS);
    return sum;
}

float TorchLightAt(Vector3 p)
{
    if (G.night <= 0) return 0.0f;
    float sum = 0;
    for (int t = 0; t < G.torchCount; t++) sum += Falloff(Vector3Distance(p, G.torches[t]), TORCH_RADIUS);
    sum = sum * Flicker() + Dynamic(p, 0.0f);
    return fminf(sum * 0.8f, LIGHT_MAX) * G.night;
}

float FogAt(Vector3 p)
{
    if (G.night <= 0) return 0.0f;
    float k = Clamp((Vector3Distance(p, G.cam.position) - 38.0f) / 95.0f, 0.0f, 1.0f);
    return G.night * 0.8f * k * k * (3.0f - 2.0f * k);
}

void LightFor(Vector3 p)
{
    Vector3 body = { p.x, p.y + 1.0f, p.z };
    G.drawTorch = TorchLightAt(body);
    G.drawFog = FogAt(body) * (1.0f - 0.6f * G.drawTorch);
}

float BoxLight(int i)
{
    if (G.night <= 0) return 0.0f;
    const Box *b = &G.boxes[i];
    float sum = b->torch * Flicker();
    if (G.dynCount > 0) sum += Dynamic(b->pos, (b->size.x + b->size.y + b->size.z) / 6.0f);
    return fminf(sum * 0.8f, LIGHT_MAX) * G.night;
}

// Hand torches only reach tiles near the camera; farther out the night fog hides them anyway.
float TileLight(int i, int j)
{
    if (G.night <= 0) return 0.0f;
    float sum = G.tileTorch[i][j] * Flicker();
    if (G.dynCount > 0) {
        float x = -WORLD_HALF + ((float)i + 0.5f) * TILE_SIZE, z = -WORLD_HALF + ((float)j + 0.5f) * TILE_SIZE;
        float dx = x - G.cam.position.x, dz = z - G.cam.position.z;
        if (dx * dx + dz * dz < 170.0f * 170.0f) sum += Dynamic((Vector3){ x, G.heights[i][j], z }, 2.0f);
    }
    return fminf(sum * 0.8f, LIGHT_MAX) * G.night;
}

static void Flame(Vector3 p, float t, float seed)
{
    float k = 0.85f + 0.15f * sinf(t * 13.0f + seed) * sinf(t * 7.0f + seed * 2.0f);
    DrawGlowBox((Vector3){ p.x, p.y + 0.1f * k, p.z }, (Vector3){ 0.24f * k, 0.36f * k, 0.24f * k }, (Color){ 255, 130, 40, 255 });
    DrawGlowBox((Vector3){ p.x, p.y + 0.05f, p.z }, (Vector3){ 0.13f, 0.22f * k, 0.13f }, (Color){ 255, 220, 120, 255 });
}

void DrawTorches(void)
{
    if (G.night < 0.05f) return;
    float t = (float)GetTime();
    for (int i = 0; i < G.torchCount; i++) {
        bool fire = false;
        for (int k = 0; k < fireCount; k++)
            if (fires[k].x == G.torches[i].x && fires[k].z == G.torches[i].z) fire = true;
        if (!fire && XZDist(G.torches[i], G.cam.position) < 180.0f) Flame(G.torches[i], t, (float)i);
    }
}

void DrawHandTorch(Vector3 pos, float facing)
{
    Vector3 h = HandPos(pos, facing);
    DrawShadedBox((Vector3){ h.x, h.y - 0.2f, h.z }, (Vector3){ 0.06f, 0.6f, 0.06f }, COL_WOOD);
    Flame((Vector3){ h.x, h.y + 0.18f, h.z }, (float)GetTime(), pos.x);
}
