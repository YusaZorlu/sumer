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
#define FERRY_Z (-62.0f)

static const int FARES[][2] = { { IT_BREAD, 1 }, { IT_GRILLED, 1 }, { IT_FISH, 2 }, { IT_POT, 1 }, { IT_WOOD, 3 }, { IT_DATES, 10 }, { IT_REEDS, 6 } };
#define FARE_COUNT ((int)(sizeof(FARES) / sizeof(FARES[0])))
static int fareChoice;

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
    for (int k = 0; k < 3; k++) {
        AddBox(-132.8f + 2.2f * (float)k, -1.8f, FERRY_Z - 1.1f, 0.2f, 2.4f, 0.2f, COL_WOOD, false);
        AddBox(-149.8f + 2.2f * (float)k, -1.8f, FERRY_Z - 1.1f, 0.2f, 2.4f, 0.2f, COL_WOOD, false);
    }
    AddBox(-125.2f, 0, FERRY_Z + 2.2f, 0.16f, 1.8f, 0.16f, COL_WOOD, false);
    AddBox(-125.2f, 1.7f, FERRY_Z + 2.2f, 0.7f, 0.5f, 0.06f, COL_LINEN, false);
    G.stations[STN_FERRY_E] = (Vector3){ -127.0f, 0, FERRY_Z };
    G.stations[STN_FERRY_W] = (Vector3){ -151.0f, 0, FERRY_Z };
    KeepOut(-135, FERRY_Z - 5, -122, FERRY_Z + 5);
}

static bool CanPay(int i)
{
    return BagCount(FARES[i][0]) >= FARES[i][1];
}

// The fare picked with F while it can still be paid, otherwise the first one that can.
static int Fare(void)
{
    if (fareChoice < FARE_COUNT && CanPay(fareChoice)) return fareChoice;
    for (int i = 0; i < FARE_COUNT; i++)
        if (CanPay(i)) return i;
    return -1;
}

static int NextFare(int f)
{
    for (int k = 1; k < FARE_COUNT; k++)
        if (CanPay((f + k) % FARE_COUNT)) return (f + k) % FARE_COUNT;
    return -1;
}

void FerryPrompt(int s, char *e, char *f, int size, bool *okE, bool *okF)
{
    int fare = Fare(), next = fare >= 0 ? NextFare(fare) : -1;

    *okE = fare >= 0;
    *okF = next >= 0;
    if (*okE) Fmt(e, size, s == STN_FERRY_E ? "[E] Ferry to the west bank for %d %s" : "[E] Ferry to the east bank for %d %s", FARES[fare][1], ItemName(FARES[fare][0]));
    else Fmt(e, size, "Ferryman: \"Bread, fish, a pot, wood, reeds or dates pays the crossing.\"");
    if (*okF) Fmt(f, size, "[F] Pay with %d %s instead", FARES[next][1], ItemName(FARES[next][0]));
}

void ChangeFare(void)
{
    int fare = Fare(), next = fare >= 0 ? NextFare(fare) : -1;
    if (next >= 0) fareChoice = next;
}

void TakeFerry(int s)
{
    int f = Fare();
    if (f < 0) return;
    BagRemove(FARES[f][0], FARES[f][1]);
    Player *p = &G.player;
    float x = s == STN_FERRY_E ? -152.5f : -125.5f;
    p->pos = (Vector3){ x, GroundHeight(x, FERRY_Z, 0.5f), FERRY_Z };
    p->facing = s == STN_FERRY_E ? -PI / 2 : PI / 2;
    p->vy = 0;
    // Alone the crossing takes half an hour; in company the world's clock is everyone's and goes on as it is.
    if (!Online()) {
        G.hour += 0.5f;
        if (G.hour >= 24.0f) {
            G.hour -= 24.0f;
            G.day++;
        }
        AdvanceHours(0.5f);
    }
    G.sleeping = G.slept = true;
    G.sleepT = 1.1f;
    Say(s == STN_FERRY_E ? "The ferryman poles you across to the wild west bank. Barbarians camp out there."
                         : "The ferryman brings you back to the city side.");
}

static void FerryBoat(float x)
{
    float bob = WATER_LEVEL + 0.04f * sinf((float)GetTime() * 1.3f + x);
    DrawShadedBox((Vector3){ x, bob, FERRY_Z + 2.0f }, (Vector3){ 3.6f, 0.3f, 1.0f }, COL_STRAW);
    DrawShadedBox((Vector3){ x - 1.75f, bob + 0.2f, FERRY_Z + 2.0f }, (Vector3){ 0.35f, 0.5f, 0.7f }, COL_STRAW);
    DrawShadedBox((Vector3){ x + 1.75f, bob + 0.2f, FERRY_Z + 2.0f }, (Vector3){ 0.35f, 0.5f, 0.7f }, COL_STRAW);
}

void DrawFerries(void)
{
    Look a = PersonLook(LOOK_WORKER, 900u, (Color){ 100, 120, 130, 255 }), b = PersonLook(LOOK_WORKER, 901u, (Color){ 100, 120, 130, 255 });
    if (XZDist(G.stations[STN_FERRY_E], G.cam.position) > 220.0f) return;
    LightFor(G.stations[STN_FERRY_E]);
    FerryBoat(-131.0f);
    FerryBoat(-147.0f);
    DrawHuman((Vector3){ -128.6f, 0.3f, FERRY_Z - 0.6f }, PI / 2, 0, a, false, false, 0);
    DrawHuman((Vector3){ -149.4f, 0.3f, FERRY_Z - 0.6f }, -PI / 2, 0, b, false, false, 0);
    G.drawTorch = G.drawFog = 0;
}

static void BuildMine(void)
{
    const float E = EAST;
    float y = TerrainHeight(149 + E, -60);
    static const float veins[][2] = { { 144, -53 }, { 141, -62 }, { 145, -70 }, { 152, -52 }, { 153, -69 }, { 138, -57 } };

    AddBoxOn(151.5f + E, -60, 5.0f, 4.5f, 6.0f, COL_ROCK, true);
    AddBox(148.95f + E, y - 0.1f, -60, 0.1f, 2.3f, 1.8f, COL_DOORWAY, false);
    AddBox(148.7f + E, y - 0.2f, -61.1f, 0.25f, 2.7f, 0.25f, COL_WOOD, true);
    AddBox(148.7f + E, y - 0.2f, -58.9f, 0.25f, 2.7f, 0.25f, COL_WOOD, true);
    AddBox(148.7f + E, y + 2.3f, -60, 0.3f, 0.3f, 2.6f, COL_WOOD, false);
    AddBoxOn(142 + E, -67, 3.0f, 1.0f, 2.5f, (Color){ 110, 104, 96, 255 }, false);
    AddBoxOn(146.5f + E, -60, 0.6f, 0.5f, 0.6f, COL_STRAW, false);

    for (int i = 0; i < 2; i++) {
        float z = i ? -56.5f : -63.5f;
        AddBoxOn(147.5f + E, z, 1.4f, 0.9f, 1.2f, COL_ROCK, true);
        AddBox(146.79f + E, TerrainHeight(147.5f + E, z) + 0.4f, z, 0.04f, 0.15f, 0.6f, COL_ORE, false);
        AddWorker(JOB_MINE, 136 + E, -45.2f, 146.0f + E, z, 147.5f + E, z);
    }
    Hut(136 + E, -47, 3);
    for (int i = 0; i < 6; i++) AddResource(veins[i][0] + E, veins[i][1], RES_ORE);
    KeepOut(135 + E, -73, 157 + E, -48);
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
    if (w->job != JOB_BOAT) p.y = GroundHeight(p.x, p.z, TerrainHeight(p.x, p.z) + 0.4f);
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
        if (!w->visible || XZDist(w->pos, G.cam.position) > 220.0f) continue;

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
