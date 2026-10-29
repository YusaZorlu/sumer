#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>

enum { TOOL_DRAW_AXE, TOOL_DRAW_PICK, TOOL_DRAW_HOE, TOOL_DRAW_POLE };

static const Offer SMITH[] = {
    { IT_BRONZE, 2, IT_WOOD,  3, OUT_TOOL, TOOL_AXE,        1, 0, 0 },
    { IT_BRONZE, 2, IT_WOOD,  3, OUT_TOOL, TOOL_PICK,       1, 0, 0 },
    { IT_BRONZE, 3, IT_REEDS, 6, OUT_TOOL, TOOL_ARROWHEADS, 1, 0, 0 },
    { IT_COPPER, 4, IT_NONE,  0, OUT_ITEM, IT_ARROWS,       6, 0, 0 },
    { IT_BRONZE, 5, IT_SHEEPSKIN, 3, OUT_ARMOR, ARMOR_BRONZE, 1, IT_COWHIDE, 3 },
    { IT_BRONZE, 2, IT_COPPER, 3, OUT_MACE, 0, 1, IT_WOOD, 4 },
};

#define EAST 30.0f
#define FERRY_Z (-62.0f)    // the old landing stages, one on each bank
#define DOCK_LAND_X (-126.0f)   // where the fishing dock west of the city meets the bank
#define DOCK_LAND_Z (30.5f)

static void AddWorker(JobKind job, float hx, float hz, float sx, float sz, float lx, float lz)
{
    if (G.workerCount >= MAX_WORKERS) return;
    Worker *w = &G.workers[G.workerCount++];
    *w = (Worker){ job, { hx, TerrainHeight(hx, hz), hz }, { sx, 0, sz }, { sx, 0, sz }, atan2f(lx - sx, lz - sz),
                   RandF(0, 2 * PI), false, false, { .hp = 0 } };
    w->spot.y = (job == JOB_BOAT) ? WATER_LEVEL : TerrainHeight(sx, sz);
}

static void Hut(float x, float z, int door)
{
    float y = TerrainHeight(x, z);
    AddShape(x, y - 0.2f, z, 3.0f, 2.4f, 3.0f, COL_STRAW, true, SHAPE_CYL);
    AddShape(x, y + 2.1f, z, 3.6f, 1.6f, 3.6f, (Color){ 160, 150, 90, 255 }, false, SHAPE_CONE);
    float dx = (door == 0) ? -1.52f : (door == 1) ? 1.52f : 0.0f;
    float dz = (door == 2) ? -1.52f : (door == 3) ? 1.52f : 0.0f;
    AddBox(x + dx, y - 0.1f, z + dz, dx != 0 ? 0.08f : 0.9f, 1.8f, dz != 0 ? 0.08f : 0.9f, COL_DOORWAY, false);
    KeepOut(x - 2.5f, z - 2.5f, x + 2.5f, z + 2.5f);
}

static void BuildCraftQuarter(void)
{
    const Color clayHeap = { 122, 100, 82, 255 };

    G.stations[STN_KILN] = (Vector3){ -52, 0, 18 };
    AddShape(-52, 0, 18, 2.2f, 1.2f, 2.2f, COL_BRICK, true, SHAPE_CYL);
    AddShape(-52, 0.4f, 18, 2.0f, 2.0f, 2.0f, COL_CLAY, true, SHAPE_BLOB);
    AddShape(-52, 2.0f, 18, 0.5f, 0.5f, 0.5f, COL_CLAY_DARK, false, SHAPE_CYL);
    AddGlow(-50.88f, 0.15f, 18, 0.06f, 0.5f, 0.7f, COL_FIRE);

    AddBox(-45.5f, 0, 17, 0.5f, 0.45f, 0.5f, COL_STONE, true);
    AddBox(-45.5f, 0.45f, 17, 0.9f, 0.06f, 0.9f, COL_WOOD, false);
    AddBox(-46, 0, 22.5f, 3.0f, 0.5f, 0.8f, COL_WOOD, true);
    for (int k = 0; k < 5; k++)
        AddShape(-47.2f + 0.6f * (float)k, 0.5f, 22.5f, 0.38f, 0.42f, 0.38f, COL_POT, false, SHAPE_BLOB);
    AddShape(-56, -0.3f, 24.5f, 2.2f, 1.0f, 1.8f, clayHeap, false, SHAPE_BLOB);

    G.stations[STN_SMELTER] = (Vector3){ -52, 0, 28 };
    AddShape(-52, 0, 28, 1.7f, 1.6f, 1.7f, COL_BRICK, true, SHAPE_CYL);
    AddShape(-52, 1.6f, 28, 1.1f, 0.5f, 1.1f, COL_CLAY_DARK, false, SHAPE_CYL);
    AddGlow(-52, 2.1f, 28, 0.6f, 0.05f, 0.6f, COL_FIRE);
    AddGlow(-51.18f, 0.25f, 28, 0.04f, 0.4f, 0.5f, COL_FIRE);
    AddBox(-50.5f, 0, 27.2f, 0.8f, 0.35f, 0.6f, (Color){ 120, 80, 60, 255 }, false);
    AddBox(-48.5f, 0, 29.2f, 0.6f, 0.7f, 0.4f, COL_STONE, true);
    AddBox(-48.5f, 0.7f, 29.2f, 0.3f, 0.08f, 0.15f, COL_BRONZE, false);
    AddBox(-56, 0, 28.5f, 1.8f, 0.8f, 1.0f, COL_WOOD, true);

    G.traders[TR_SMITH] = (Trader){ "Smith", "Copper from the hills, tin from the caravans. Bronze makes good tools.",
                                    { -46, 0, 30.5f }, PI, { 90, 70, 60, 255 }, SMITH, (int)(sizeof(SMITH) / sizeof(Offer)), false, false, { .hp = 0 } };

    AddWorker(JOB_POTTER, -56.5f, 16, -45.5f, 18.0f, -45.5f, 17);
    AddWorker(JOB_KILN, -56.5f, 19, -50.2f, 19.8f, -52, 18);
    AddWorker(JOB_SMITH, -56.5f, 31, -50.6f, 28.8f, -52, 28);
    KeepOut(-58, 14, -40, 33);
}

static void BuildWaterfront(void)
{
    static const float bank[] = { -300, -230, -170, -110, -45, 62, 112, 160, 200, 260, 310 };
    static const float clay[] = { -200, -140, -84, 48, 100, 140, 200 };

    AddBox(-131.5f, 0, 30, 9, 0.3f, 3, COL_WOOD, true);
    for (int k = 0; k < 3; k++) {
        AddBox(-135.8f + 2.8f * (float)k, -1.8f, 28.6f, 0.2f, 2.4f, 0.2f, COL_WOOD, false);
        AddBox(-135.8f + 2.8f * (float)k, -1.8f, 31.4f, 0.2f, 2.4f, 0.2f, COL_WOOD, false);
    }
    AddBox(-131.8f, 0.3f, 28.55f, 8.4f, 0.6f, 0.1f, COL_WOOD, true);
    AddBox(-131.8f, 0.3f, 31.45f, 8.4f, 0.6f, 0.1f, COL_WOOD, true);
    AddBox(-135.95f, 0.3f, 30, 0.1f, 0.6f, 3.0f, COL_WOOD, true);
    KeepOut(-137, 27, -125, 33);
    AddResource(-137.4f, 30, RES_FISH);
    AddResource(-133, 27.2f, RES_FISH);
    for (int i = 0; i < (int)(sizeof(bank) / sizeof(bank[0])); i++) AddResource(-129.5f, bank[i], RES_FISH);
    AddResource(-59.4f, -20, RES_FISH);
    AddResource(-59.4f, -4, RES_FISH);

    for (int i = 0; i < (int)(sizeof(clay) / sizeof(clay[0])); i++) {
        AddResource(-123.5f, clay[i], RES_CLAY);
        KeepOut(-126, clay[i] - 2, -121, clay[i] + 2);
    }

    Hut(-120, 34, 0);
    Hut(-118, -34, 0);
    Hut(-118, 104, 0);
    AddWorker(JOB_DOCK, -121.8f, 34, -135.4f, 31.0f, -140, 31.0f);
    AddWorker(JOB_BOAT, -118, -34, -141, -34, -141, -30);
    AddWorker(JOB_BOAT, -118, 104, -140, 104, -140, 108);

    AddBox(-130.5f, 0, FERRY_Z, 6.0f, 0.3f, 2.4f, COL_WOOD, true);
    AddBox(-147.5f, 0, FERRY_Z, 6.0f, 0.3f, 2.4f, COL_WOOD, true);
    // Posts under both edges of each deck, down into the river bed.
    for (int k = 0; k < 3; k++)
        for (int e = -1; e <= 1; e += 2)
            for (int w = 0; w < 2; w++) {
                float x = (w ? -149.8f : -132.8f) + 2.2f * (float)k, z = FERRY_Z + 1.1f * (float)e, bed = TerrainHeight(x, z) - 0.3f;
                AddBox(x, bed, z, 0.2f, 0.3f - bed, 0.2f, COL_WOOD, false);
            }
    G.stations[STN_FERRY_E] = (Vector3){ -127.0f, 0, FERRY_Z };
    G.stations[STN_FERRY_W] = (Vector3){ -151.0f, 0, FERRY_Z };
    KeepOut(-135, FERRY_Z - 5, -122, FERRY_Z + 5);
}

// Something set on the ground at (x, z), standing 'up' above it.
static void OnGround(float x, float z, float up, float w, float h, float d, Color c, bool solid, int shape)
{
    AddShape(x, TerrainHeight(x, z) + up, z, w, h, d, c, solid, shape);
}

// A copper mine as they were worked in the third millennium BC (Timna and Feinan, and Magan, today's Oman,
// whence Sumer had its copper): a rocky ridge with green malachite seams, an adit timbered at its mouth, a
// shaft with a notched log for a ladder and a frame to haul baskets up; the ore broken with stone hammers on
// anvil stones and sorted by hand under a reed shade, rich green from grey waste; bowl furnaces of clay blown
// through clay pipes by pot bellows, charcoal, the black slag thrown on a heap, and the copper cast in buns.
static void BuildMine(void)
{
    const float E = EAST;
    float y = TerrainHeight(149 + E, -60);
    static const float veins[][2] = { { 144, -53 }, { 141, -62 }, { 145, -70 }, { 152, -52 }, { 153, -69 }, { 138, -57 } };
    const Color rock2 = { 134, 124, 110, 255 }, rock3 = { 116, 110, 100, 255 }, malachite = { 52, 150, 108, 255 };
    const Color waste = { 138, 132, 120, 255 }, slag = { 46, 42, 46, 255 }, coal = { 34, 30, 28, 255 }, copper = { 196, 114, 66, 255 };
    const Color reed = { 176, 156, 96, 255 }, leather = { 122, 86, 56, 255 }, furnace = { 150, 92, 62, 255 };

    // The ridge: the face with the adit, a higher crest behind, a spur either side.
    AddBoxOn(151.5f + E, -60, 5.0f, 4.5f, 6.0f, COL_ROCK, true);
    OnGround(155.0f + E, -60, -1.0f, 7.0f, 8.0f, 12.5f, rock3, true, SHAPE_BLOB);
    OnGround(152.6f + E, -55.0f, -0.6f, 3.6f, 4.0f, 3.4f, rock2, true, SHAPE_BLOB);
    OnGround(152.6f + E, -65.0f, -0.6f, 3.6f, 4.4f, 3.4f, rock2, true, SHAPE_BLOB);
    OnGround(156.0f + E, -57.0f, 5.4f, 3.0f, 1.8f, 2.8f, rock2, false, SHAPE_BLOB);
    OnGround(155.0f + E, -63.5f, 5.6f, 2.4f, 1.4f, 2.2f, COL_ROCK, false, SHAPE_BLOB);
    // Malachite seams across the faces, where the miners follow them in.
    AddBox(148.97f + E, y + 1.2f, -62.4f, 0.05f, 0.22f, 0.9f, malachite, false);
    AddBox(148.97f + E, y + 3.0f, -58.0f, 0.05f, 0.18f, 1.4f, malachite, false);
    AddBox(148.97f + E, y + 0.5f, -57.6f, 0.05f, 0.3f, 0.5f, COL_ORE, false);
    AddBox(151.5f + E, y + 2.0f, -56.97f, 1.8f, 0.2f, 0.05f, malachite, false);
    AddBox(151.0f + E, y + 1.0f, -63.03f, 1.2f, 0.18f, 0.05f, COL_ORE, false);

    // The adit: a dark mouth in the face, two props and a cap piece, a rough sill, baskets of ore at hand.
    AddBox(148.95f + E, y - 0.1f, -60, 0.1f, 2.3f, 1.8f, COL_DOORWAY, false);
    AddBox(148.7f + E, y - 0.2f, -61.1f, 0.25f, 2.7f, 0.25f, COL_WOOD, true);
    AddBox(148.7f + E, y - 0.2f, -58.9f, 0.25f, 2.7f, 0.25f, COL_WOOD, true);
    AddBox(148.7f + E, y + 2.3f, -60, 0.3f, 0.3f, 2.6f, COL_WOOD, false);
    AddBoxOn(148.3f + E, -60, 0.6f, 0.12f, 1.8f, rock3, false);
    for (int k = 0; k < 2; k++) {
        float bz = k ? -61.8f : -58.3f;
        OnGround(147.6f + E, bz, 0, 0.5f, 0.38f, 0.5f, reed, false, SHAPE_CYL);
        OnGround(147.6f + E, bz, 0.34f, 0.42f, 0.16f, 0.42f, malachite, false, SHAPE_BLOB);
    }

    // The miners at the outcrops by the face, as before.
    for (int i = 0; i < 2; i++) {
        float z = i ? -56.5f : -63.5f;
        AddBoxOn(147.5f + E, z, 1.4f, 0.9f, 1.2f, COL_ROCK, true);
        AddBox(146.79f + E, TerrainHeight(147.5f + E, z) + 0.4f, z, 0.04f, 0.15f, 0.6f, COL_ORE, false);
        AddWorker(JOB_MINE, 136 + E, -45.2f, 146.0f + E, z, 147.5f + E, z);
    }

    // The shaft: a collar of stones round the dark hole, a notched log standing in it, the hauling frame
    // with its rope and a basket.
    const float sx = 140.5f + E, sz = -65.0f, sy = TerrainHeight(sx, sz);
    AddBox(sx, sy - 0.1f, sz + 0.7f, 1.8f, 0.65f, 0.4f, COL_STONE, true);
    AddBox(sx, sy - 0.1f, sz - 0.7f, 1.8f, 0.65f, 0.4f, COL_STONE, true);
    AddBox(sx - 0.7f, sy - 0.1f, sz, 0.4f, 0.65f, 1.0f, rock2, true);
    AddBox(sx + 0.7f, sy - 0.1f, sz, 0.4f, 0.65f, 1.0f, rock2, true);
    AddBox(sx, sy - 0.1f, sz, 1.0f, 0.58f, 1.0f, (Color){ 20, 16, 14, 255 }, true);
    AddShape(sx - 0.2f, sy + 0.4f, sz - 0.15f, 0.2f, 1.7f, 0.2f, COL_WOOD, false, SHAPE_CYL);
    for (int k = 0; k < 4; k++) AddBox(sx - 0.1f, sy + 0.6f + 0.35f * (float)k, sz - 0.15f, 0.04f, 0.06f, 0.16f, COL_DOORWAY, false);
    for (int e = -1; e <= 1; e += 2) AddBox(sx + 1.15f * (float)e, sy - 0.1f, sz, 0.14f, 2.6f, 0.14f, COL_WOOD, true);
    AddBox(sx, sy + 2.4f, sz, 2.5f, 0.14f, 0.14f, COL_WOOD, false);
    AddBox(sx + 0.25f, sy + 0.9f, sz, 0.03f, 1.5f, 0.03f, COL_STRAW, false);
    AddShape(sx + 0.25f, sy + 0.62f, sz, 0.36f, 0.3f, 0.36f, reed, false, SHAPE_CYL);
    // What came up and was no use: the spoil heap.
    OnGround(141.5f + E, -70.5f, -0.3f, 4.2f, 1.5f, 3.0f, waste, false, SHAPE_BLOB);
    OnGround(139.8f + E, -69.0f, -0.2f, 1.6f, 0.8f, 1.4f, rock3, false, SHAPE_BLOB);

    // Dressing the ore under a reed shade: anvil stones with their hammerstones, the rich green ore kept,
    // the grey thrown aside; one worker breaking it.
    const float dx = 142.0f + E, dz = -57.5f;
    for (int k = 0; k < 4; k++) {
        float px = dx + ((k & 1) ? 1.8f : -1.8f), pz = dz + ((k & 2) ? 1.5f : -1.5f);
        AddBox(px, TerrainHeight(px, pz) - 0.1f, pz, 0.14f, 2.45f, 0.14f, COL_WOOD, true);
    }
    AddBox(dx, TerrainHeight(dx, dz) + 2.3f, dz, 4.2f, 0.08f, 3.5f, reed, false);
    AddBoxOn(dx - 0.8f, dz - 0.3f, 0.8f, 0.35f, 0.6f, COL_STONE, true);
    AddBoxOn(dx + 0.9f, dz + 0.5f, 0.7f, 0.32f, 0.6f, rock2, true);
    OnGround(dx - 0.65f, dz - 0.25f, 0.35f, 0.16f, 0.12f, 0.2f, rock3, false, SHAPE_BLOB);
    OnGround(dx + 1.0f, dz + 0.45f, 0.32f, 0.16f, 0.12f, 0.2f, COL_STONE, false, SHAPE_BLOB);
    OnGround(dx - 0.6f, dz + 1.0f, -0.05f, 0.7f, 0.34f, 0.6f, malachite, false, SHAPE_BLOB);
    OnGround(dx + 1.1f, dz - 0.9f, -0.05f, 0.8f, 0.3f, 0.7f, waste, false, SHAPE_BLOB);
    OnGround(dx + 0.1f, dz + 1.15f, 0, 0.45f, 0.36f, 0.45f, reed, false, SHAPE_CYL);
    AddWorker(JOB_MINE, 136 + E, -45.2f, dx - 0.8f, dz - 1.4f, dx - 0.8f, dz - 0.3f);

    // Smelting: two bowl furnaces of clay (one alight), clay pipes from the pot bellows, charcoal and wood
    // for it, the slag heap, and the copper cast in buns cooling on a stone.
    const float fx = 136.5f + E, fz = -63.8f;
    for (int k = 0; k < 2; k++) {
        float z = fz - 2.2f * (float)k, g = TerrainHeight(fx, z);
        AddShape(fx, g - 0.05f, z, 0.85f, 0.75f, 0.85f, furnace, true, SHAPE_CYL);
        AddShape(fx, g + 0.69f, z, 0.6f, 0.03f, 0.6f, k ? coal : (Color){ 236, 120, 40, 255 }, false, SHAPE_CYL);
        AddBox(fx + 0.6f, g + 0.12f, z, 0.6f, 0.1f, 0.1f, COL_CLAY_DARK, false);
        for (int e = -1; e <= 1; e += 2) {
            AddShape(fx + 1.05f, g, z + 0.28f * (float)e, 0.4f, 0.28f, 0.4f, COL_POT, false, SHAPE_CYL);
            AddShape(fx + 1.05f, g + 0.24f, z + 0.28f * (float)e, 0.42f, 0.12f, 0.42f, leather, false, SHAPE_BLOB);
        }
    }
    AddFireLight(fx, TerrainHeight(fx, fz) + 0.95f, fz);
    OnGround(134.5f + E, -61.0f, -0.1f, 1.5f, 0.6f, 1.2f, coal, false, SHAPE_BLOB);
    AddBoxOn(136.2f + E, -59.4f, 1.4f, 0.5f, 0.7f, COL_WOOD, true);
    OnGround(134.3f + E, -67.6f, -0.2f, 2.3f, 0.8f, 1.9f, slag, false, SHAPE_BLOB);
    for (int k = 0; k < 3; k++)
        OnGround(135.6f + E + 0.6f * (float)k, -69.0f + 0.3f * (float)k, 0, 0.5f, 0.08f, 0.45f, slag, false, SHAPE_CYL);
    AddBoxOn(138.2f + E, -66.4f, 0.9f, 0.3f, 0.6f, COL_STONE, true);
    for (int k = 0; k < 2; k++) OnGround(137.95f + E + 0.45f * (float)k, -66.4f, 0.3f, 0.32f, 0.09f, 0.3f, copper, false, SHAPE_BLOB);
    OnGround(138.5f + E, -65.3f, 0, 0.36f, 0.5f, 0.36f, COL_POT, false, SHAPE_CYL);

    Hut(136 + E, -47, 3);
    for (int i = 0; i < 6; i++) AddResource(veins[i][0] + E, veins[i][1], RES_ORE);
    KeepOut(133 + E, -73, 157 + E, -48);
}

static void BuildForestCamp(void)
{
    const float E = EAST;
    Hut(147 + E, 40, 1);
    AddBoxOn(150.5f + E, 37.5f, 2.0f, 0.9f, 1.0f, COL_WOOD, true);
    AddShape(150.5f + E, TerrainHeight(150.5f + E, 41.2f) - 0.1f, 41.2f, 0.6f, 0.6f, 0.6f, COL_PALM_TRUNK, true, SHAPE_CYL);
    AddTree(154 + E, 47, 1.1f, false);
    AddTree(143.5f + E, 52.5f, 1.0f, false);
    AddWorker(JOB_WOOD, 148.8f + E, 40, 152.6f + E, 45.6f, 154 + E, 47);
    AddWorker(JOB_WOOD, 148.8f + E, 40, 144.8f + E, 51.2f, 143.5f + E, 52.5f);
    KeepOut(140 + E, 33, 157 + E, 56);
}

static void BuildFarmhands(void)
{
    Hut(-6.5f, 108, 0);
    Hut(6.5f, 120, 1);
    AddWorker(JOB_FARM, -8.4f, 108, -15, 112, -20, 112);
    AddWorker(JOB_FARM, -8.4f, 108, -22, 124, -22, 128);
    AddWorker(JOB_FARM, 8.4f, 120, 16, 116, 20, 116);
}

void BuildJobs(void)
{
    G.workerCount = 0;
    BuildCraftQuarter();
    BuildWaterfront();
    BuildMine();
    BuildForestCamp();
    BuildFarmhands();
}

static float ShiftT(const Worker *w)
{
    float h = G.hour, t = 1.0f;
    if (w->job != JOB_BOAT) {
        if (h < 6.4f) t = (h - 6.0f) / 0.4f;
        else if (h > 18.1f) t = (18.5f - h) / 0.4f;
    }
    return Clamp(t, 0.0f, 1.0f);
}

Vector3 WorkerScriptPos(const Worker *w)
{
    Vector3 p = Vector3Lerp(w->hut, w->spot, ShiftT(w));
    if (w->job == JOB_DOCK) {   // to the deck's land end first, then out along it: not over the water beside it
        Vector3 end = { DOCK_LAND_X, 0, DOCK_LAND_Z };
        float a = XZDist(w->hut, end), b = XZDist(end, w->spot), d = ShiftT(w) * (a + b);
        p = d < a ? Vector3Lerp(w->hut, end, d / a) : Vector3Lerp(end, w->spot, (d - a) / b);
    }
    // Looked for from just over the ground, so a worker at a table stands beside it, not on it; the dock's
    // fisherman from over its deck (0.3 m), which stands over the river bed: else he would stand under it.
    float from = TerrainHeight(p.x, p.z);
    if (w->job == JOB_DOCK) from = fmaxf(from, 0.3f);
    if (w->job != JOB_BOAT) p.y = GroundHeight(p.x, p.z, from + 0.4f);
    return p;
}

void UpdateWorkers(void)
{
    for (int i = 0; i < G.workerCount; i++) {
        Worker *w = &G.workers[i];
        bool onShift = G.hour >= 6.0f && G.hour < 18.5f;

        w->visible = onShift || w->vit.override;
        w->walking = onShift && ShiftT(w) < 1.0f;
        w->pos = w->vit.override ? w->vit.at : WorkerScriptPos(w);
    }
}

bool PeopleNear(Vector3 p, float r)
{
    for (int i = 0; i < G.workerCount; i++)
        if (G.workers[i].visible && !G.workers[i].vit.dead && XZDist(G.workers[i].pos, p) < r) return true;
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (!v->vit.dead && XZDist(VillagerPos(v), p) < r) return true;
    }
    for (int i = 0; i < TR_COUNT; i++) {
        const Trader *t = &G.traders[i];
        if (!t->roaming && !t->vit.dead && XZDist(TraderPos(t), p) < r) return true;
    }
    return CaravanNear(p, r);
}

static void Part(float x, float y, float z, float w, float h, float d, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, d }, c);
}

static void DrawTool(Vector3 pos, float facing, float shoulder, float angle, int kind)
{
    float len = (kind == TOOL_DRAW_POLE) ? 2.6f : 0.95f;

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + shoulder, pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    rlMultMatrixf(MatrixToFloat(BlowFrame(angle)));
    GripHere(HAND_WEAPON, 0, 0, 0.04f);
    Part(0, 0, len / 2, 0.05f, 0.05f, len, COL_WOOD);
    if (kind == TOOL_DRAW_AXE) Part(0, 0.1f, len - 0.05f, 0.04f, 0.26f, 0.16f, COL_BRONZE);
    else if (kind == TOOL_DRAW_PICK) Part(0, 0, len, 0.05f, 0.5f, 0.06f, COL_STONE);
    else if (kind == TOOL_DRAW_HOE) Part(0, -0.15f, len, 0.04f, 0.3f, 0.12f, COL_BRONZE);
    rlPopMatrix();
}

static void DrawBoat(Vector3 p, float bob)
{
    Part(p.x, p.y + bob, p.z, 1.0f, 0.3f, 3.6f, COL_STRAW);
    Part(p.x, p.y + bob + 0.2f, p.z - 1.75f, 0.7f, 0.5f, 0.35f, COL_STRAW);
    Part(p.x, p.y + bob + 0.2f, p.z + 1.75f, 0.7f, 0.5f, 0.35f, COL_STRAW);
}

static Look WorkerLook(JobKind job, int i)
{
    static const Color cloth[] = {
        [JOB_WOOD] = { 110, 90, 60, 255 }, [JOB_MINE] = { 92, 82, 72, 255 }, [JOB_POTTER] = { 150, 120, 90, 255 },
        [JOB_KILN] = { 170, 140, 100, 255 }, [JOB_SMITH] = { 80, 60, 50, 255 }, [JOB_BOAT] = { 120, 140, 150, 255 },
        [JOB_DOCK] = { 130, 150, 160, 255 }, [JOB_FARM] = { 204, 194, 164, 255 },
    };
    return PersonLook(LOOK_WORKER, 500u + (unsigned)i, cloth[job]);
}

void DrawWorkers(void)
{
    float time = (float)GetTime();

    for (int i = 0; i < G.workerCount; i++) {
        const Worker *w = &G.workers[i];
        float bob = 0.05f * sinf(time * 1.3f + w->phase);
        if (w->job == JOB_BOAT) DrawBoat(w->spot, bob);
        if (!w->visible || XZDist(w->pos, G.cam.position) > 220.0f || !InView(w->pos, 2.0f)) continue;

        Look look = WorkerLook(w->job, i);
        Vector3 pos = w->pos;
        int tool = w->job == JOB_WOOD ? TOOL_DRAW_AXE : w->job == JOB_MINE ? TOOL_DRAW_PICK : w->job == JOB_FARM ? TOOL_DRAW_HOE : -1;
        LightFor(pos);
        if (w->vit.dead) {
            DrawPersonBody(pos, w->vit.face, look);
            continue;
        }
        if (w->vit.override) {
            float angle = BlowAngle(w->vit.windup, w->vit.swing, -20.0f);
            if (tool >= 0) DrawTool(pos, w->vit.face, 1.42f, angle, tool);
            DrawHuman(pos, w->vit.face, time * 8.0f, look, false, false, 0);
            continue;
        }
        if (w->walking) {
            Vector3 dir = (G.hour > 12.0f) ? Vector3Subtract(w->hut, w->spot) : Vector3Subtract(w->spot, w->hut);
            DrawHuman(pos, atan2f(dir.x, dir.z), time * 7.0f, look, false, false, 0);
            continue;
        }

        float s = 0.5f + 0.5f * sinf(time * 3.0f + w->phase);
        float f = w->facing;
        switch (w->job) {
        case JOB_WOOD:
        case JOB_MINE:
            DrawTool(pos, f, 1.42f, -75.0f + 115.0f * powf(s, 2.5f), tool);
            DrawHuman(pos, f, 0, look, false, false, 0);
            break;
        case JOB_FARM: {
            bool bend = sinf(time * 0.7f + w->phase) > -0.3f;
            DrawTool(pos, f, bend ? 1.05f : 1.42f, bend ? 10.0f + 30.0f * s : -20.0f, TOOL_DRAW_HOE);
            DrawHuman(pos, f, 0, look, bend, false, 0);
            break;
        }
        case JOB_POTTER: {
            Vector3 wheel = { pos.x + sinf(f), pos.y + 0.51f, pos.z + cosf(f) };
            DrawHuman(pos, f, 0, look, true, false, 0);
            rlPushMatrix();
            rlTranslatef(wheel.x, wheel.y, wheel.z);
            rlRotatef(time * 300.0f, 0, 1, 0);
            Part(0, 0.14f, 0, 0.3f, 0.28f, 0.3f, COL_CLAY);
            Part(0.12f, 0.2f, 0, 0.08f, 0.1f, 0.08f, COL_CLAY_DARK);
            rlPopMatrix();
            break;
        }
        case JOB_KILN:
            DrawHuman(pos, f, 0, look, sinf(time * 0.5f + w->phase) > 0.6f, false, 0);
            break;
        case JOB_SMITH:
            pos.y += 0.05f * sinf(time * 6.0f);
            DrawHuman(pos, f, 0, look, true, false, 0);
            break;
        case JOB_BOAT:
            pos.y = w->spot.y + bob + 0.15f;
            DrawTool(pos, f, 1.42f, -35.0f + 12.0f * s, TOOL_DRAW_POLE);
            DrawHuman(pos, f, 0, look, false, false, 0);
            break;
        case JOB_DOCK: {
            DrawTool(pos, f, 1.05f, -30.0f, TOOL_DRAW_POLE);
            DrawHuman(pos, f, 0, look, true, false, 0);
            float tipY = pos.y + 1.05f + 2.6f * sinf(30.0f * DEG2RAD);
            float reach = 0.15f + 2.6f * cosf(30.0f * DEG2RAD);
            float tx = pos.x + sinf(f) * reach + cosf(f) * 0.22f, tz = pos.z + cosf(f) * reach - sinf(f) * 0.22f;
            Part(tx, (tipY + 0.1f) / 2, tz, 0.015f, tipY - 0.1f, 0.015f, COL_LINEN);
            break;
        }
        }
    }
    G.drawTorch = G.drawFog = 0;
}
