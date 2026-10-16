#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const Offer BOWYER[] = {
    { IT_REEDS,   6, IT_NONE,  0, OUT_ITEM,   IT_ARROWS, 5, 0, 0 },
    { IT_EGG,     3, IT_NONE,  0, OUT_ITEM,   IT_ARROWS, 5, 0, 0 },
    { IT_GAZELLE, 1, IT_NONE,  0, OUT_ITEM,   IT_ARROWS, 10, 0, 0 },
    { IT_BOAR,    1, IT_REEDS, 6, OUT_QUIVER, 0, 1, 0, 0 },
    { IT_BOAR,    2, IT_REEDS, 10, OUT_BOW,   1, 1, 0, 0 },
    { IT_ONAGER,  2, IT_BREAD, 6, OUT_BOW,    2, 1, 0, 0 },
    { IT_WOLF,    1, IT_NONE,  0, OUT_ITEM,   IT_ARROWS, 8, 0, 0 },
    { IT_WOOD,    6, IT_NONE,  0, OUT_ITEM,   IT_ARROWS, 6, 0, 0 },
};

static const Offer FARMER[] = {
    { IT_DATES,  16, IT_NONE, 0, OUT_ITEM, IT_SEEDS, 2, 0, 0 },
    { IT_EGG,     3, IT_NONE, 0, OUT_ITEM, IT_SEEDS, 8, 0, 0 },
    { IT_GOOSE,   1, IT_NONE, 0, OUT_ITEM, IT_SEEDS, 10, 0, 0 },
    { IT_GAZELLE, 1, IT_NONE, 0, OUT_HEN,  0, 1, 0, 0 },
    { IT_WHEAT,   8, IT_NONE, 0, OUT_HEN,  0, 1, 0, 0 },
    { IT_BREAD,   2, IT_NONE, 0, OUT_ITEM, IT_SEEDS, 6, 0, 0 },
    { IT_FISH,    4, IT_NONE, 0, OUT_ITEM, IT_SEEDS, 6, 0, 0 },
    { IT_HERB,    4, IT_NONE, 0, OUT_ITEM, IT_SEEDS, 6, 0, 0 },
};

static const Offer WEAVER[] = {
    { IT_REEDS, 8, IT_NONE,  0, OUT_BASKET, 0, 1, 0, 0 },
    { IT_GOOSE, 1, IT_REEDS, 4, OUT_BASKET, 0, 1, 0, 0 },
    { IT_DATES, 15, IT_NONE, 0, OUT_ITEM,   IT_REEDS, 2, 0, 0 },
    { IT_FLOUR, 2, IT_NONE,  0, OUT_ITEM,   IT_REEDS, 10, 0, 0 },
    { IT_POT,   2, IT_NONE,  0, OUT_ITEM,   IT_REEDS, 12, 0, 0 },
    { IT_REEDS, 6, IT_NONE,  0, OUT_ITEM,   IT_BUCKET, 1, 0, 0 },
};

// The cattle herder deals only in milk and cowhide; fleeces come from the herders' village.
static const Offer HERDER[] = {
    { IT_DATES, 30, IT_NONE, 0, OUT_ITEM, IT_MILK, 1, 0, 0 },
    { IT_EGG,    2, IT_NONE, 0, OUT_ITEM, IT_MILK, 1, 0, 0 },
    { IT_BREAD,  1, IT_NONE, 0, OUT_ITEM, IT_MILK, 2, 0, 0 },
    { IT_ONAGER, 1, IT_NONE, 0, OUT_ITEM, IT_MILK, 4, 0, 0 },
    { IT_WOLF,   1, IT_NONE, 0, OUT_ITEM, IT_MILK, 4, 0, 0 },
    { IT_FISH,   3, IT_NONE, 0, OUT_ITEM, IT_MILK, 1, 0, 0 },
    { IT_SEEDS,  6, IT_NONE, 0, OUT_ITEM, IT_MILK, 1, 0, 0 },
    { IT_BREAD,  2, IT_NONE, 0, OUT_ITEM, IT_COWHIDE, 1, 0, 0 },
    { IT_ROAST,  2, IT_NONE, 0, OUT_ITEM, IT_COWHIDE, 1, 0, 0 },
    { IT_WHEAT,  6, IT_NONE, 0, OUT_ITEM, IT_COWHIDE, 1, 0, 0 },
};

static const Offer QUERN  = { IT_WHEAT, 2, IT_NONE, 0, OUT_ITEM, IT_FLOUR, 1, 0, 0 };
static const Offer TROUGH = { IT_FLOUR, 1, IT_MILK, 1, OUT_ITEM, IT_DOUGH, 2, 0, 0 };
static const Offer KILN    = { IT_CLAY, 3, IT_WOOD, 1, OUT_ITEM, IT_POT, 2, 0, 0 };
static const Offer SMELTER = { IT_COPPER, 2, IT_TIN, 1, OUT_ITEM, IT_BRONZE, 1, IT_WOOD, 2 };
static const Offer FORGE[2] = {
    { IT_COPPER, 3, IT_WOOD, 2, OUT_WEAPON, 1, 1, 0, 0 },
    { IT_BRONZE, 3, IT_WOOD, 2, OUT_WEAPON, 2, 1, 0, 0 },
};
static const Offer LEATHER = { IT_COWHIDE, 2, IT_SHEEPSKIN, 2, OUT_ARMOR, ARMOR_LEATHER, 1, 0, 0 };
static const Offer CLOAK   = { IT_WOLF, 3, IT_NONE, 0, OUT_ARMOR, ARMOR_WOLF, 1, 0, 0 };
static const Offer OVEN[] = {
    { IT_DOUGH,   1, IT_NONE, 0, OUT_ITEM, IT_BREAD, 2, 0, 0 },
    { IT_FISH,    2, IT_NONE, 0, OUT_ITEM, IT_GRILLED, 2, 0, 0 },
    { IT_GOOSE,   1, IT_NONE, 0, OUT_ITEM, IT_ROAST, 2, 0, 0 },
    { IT_GAZELLE, 1, IT_NONE, 0, OUT_ITEM, IT_ROAST, 3, 0, 0 },
    { IT_ONAGER,  1, IT_NONE, 0, OUT_ITEM, IT_ROAST, 4, 0, 0 },
    { IT_BOAR,    1, IT_NONE, 0, OUT_ITEM, IT_ROAST, 5, 0, 0 },
    { IT_SHEEP,   1, IT_NONE, 0, OUT_ITEM, IT_ROAST, 4, 0, 0 },
    { IT_HEN,     1, IT_NONE, 0, OUT_ITEM, IT_ROAST, 2, 0, 0 },
};

#define BLESS_HOURS 24.0f

static void Fence(float x0, float z0, float x1, float z1)
{
    const Color wattle = { 150, 130, 82, 255 };
    float w = fabsf(x1 - x0), d = fabsf(z1 - z0), len = fmaxf(w, d);

    AddBox((x0 + x1) / 2, 0, (z0 + z1) / 2, fmaxf(w, 0.12f), 0.9f, fmaxf(d, 0.12f), wattle, true);
    for (float s = 0; s <= len + 0.01f; s += 2.0f) {
        float t = (len > 0) ? s / len : 0;
        AddShape(Lerp(x0, x1, t), 0, Lerp(z0, z1, t), 0.17f, 1.15f, 0.17f, COL_WOOD, false, SHAPE_CYL);
    }
}

static void BuildHome(void)
{
    const float hx = 20, hz = 22, W = 10, D = 10, H = 3, t = 0.5f, gap = 1.8f;
    const float seg = (D - 2 * t - gap) / 2;

    G.home = (Vector3){ hx, 0, hz };
    AddBox(hx, 0, hz - D / 2 + t / 2, W, H, t, COL_MUD_PALE, true);
    AddBox(hx, 0, hz + D / 2 - t / 2, W, H, t, COL_MUD_PALE, true);
    AddBox(hx + W / 2 - t / 2, 0, hz, t, H, D - 2 * t, COL_MUD_PALE, true);
    AddBox(hx - W / 2 + t / 2, 0, hz - gap / 2 - seg / 2, t, H, seg, COL_MUD_PALE, true);
    AddBox(hx - W / 2 + t / 2, 0, hz + gap / 2 + seg / 2, t, H, seg, COL_MUD_PALE, true);
    AddBox(hx - W / 2 + t / 2, H - 0.5f, hz, t + 0.1f, 0.5f, gap, COL_LAPIS, false);
    AddBox(hx, 0, hz, W - 2 * t, 0.04f, D - 2 * t, COL_BRICK, false);

    Vector3 bed = { hx + 3.0f, 0, hz - 3.0f };
    G.stations[STN_BED] = bed;
    AddBox(bed.x, 0, bed.z, 1.3f, 0.45f, 2.3f, COL_WOOD, true);
    AddBox(bed.x, 0.45f, bed.z - 0.8f, 1.0f, 0.15f, 0.5f, COL_LINEN, false);
    AddBox(bed.x, 0.45f, bed.z + 0.4f, 1.2f, 0.06f, 1.3f, COL_BLANKET, false);

    G.stations[STN_STORAGE] = (Vector3){ hx - 1.6f, 0, hz - 3.4f };
    for (int k = 0; k < 3; k++) {
        float x = hx - 2.7f + 1.1f * k;
        AddShape(x, -0.1f, hz - 4.05f, 0.8f, 1.2f, 0.8f, COL_CLAY, true, SHAPE_BLOB);
        AddShape(x, 0.95f, hz - 4.05f, 0.42f, 0.25f, 0.42f, COL_CLAY_DARK, false, SHAPE_CYL);
    }

    G.stations[STN_QUERN] = (Vector3){ hx - 3.4f, 0, hz + 3.6f };
    AddBox(hx - 3.4f, 0, hz + 3.6f, 0.9f, 0.45f, 0.7f, COL_STONE, true);
    AddBox(hx - 3.4f, 0.45f, hz + 3.6f, 0.55f, 0.12f, 0.45f, (Color){ 100, 96, 90, 255 }, false);

    G.stations[STN_TROUGH] = (Vector3){ hx, 0, hz + 3.9f };
    AddBox(hx, 0, hz + 3.9f, 1.4f, 0.55f, 0.6f, COL_WOOD, true);
    AddBox(hx, 0.55f, hz + 3.9f, 1.1f, 0.03f, 0.35f, (Color){ 226, 204, 160, 255 }, false);

    G.stations[STN_BENCH] = (Vector3){ hx + 2.9f, 0, hz + 0.6f };
    AddBox(hx + 4.0f, 0, hz + 0.6f, 0.8f, 0.75f, 1.6f, COL_WOOD, true);
    AddBox(hx + 4.0f, 0.75f, hz + 0.3f, 0.5f, 0.04f, 0.6f, (Color){ 130, 92, 60, 255 }, false);
    AddBox(hx + 4.0f, 0.75f, hz + 1.1f, 0.4f, 0.1f, 0.3f, (Color){ 222, 214, 196, 255 }, false);
    AddBox(hx + 4.42f, 1.0f, hz + 0.6f, 0.06f, 1.2f, 1.4f, (Color){ 112, 84, 58, 255 }, false);

    G.stations[STN_OVEN] = (Vector3){ hx + 3.4f, 0, hz + 3.4f };
    AddShape(hx + 3.4f, -0.4f, hz + 3.4f, 1.2f, 1.6f, 1.2f, COL_CLAY, true, SHAPE_BLOB);
    AddShape(hx + 3.4f, 0.85f, hz + 3.4f, 0.5f, 0.2f, 0.5f, COL_CLAY_DARK, false, SHAPE_CYL);
    AddGlow(hx + 3.4f, 1.05f, hz + 3.4f, 0.32f, 0.03f, 0.32f, COL_FIRE);

    KeepOut(hx - W / 2 - 1, hz - D / 2 - 1, hx + W / 2 + 1, hz + D / 2 + 1);
}

static void BuildPen(void)
{
    G.pen = (Vector3){ 55.0f, 0, 31.0f };
    Fence(50, 26, 60, 26);
    Fence(50, 36, 60, 36);
    Fence(60, 26, 60, 36);
    Fence(50, 26, 50, 30);
    Fence(50, 32, 50, 36);
    AddBox(58.5f, 0, 27.2f, 2.4f, 0.4f, 0.8f, COL_WOOD, true);
    AddBox(58.5f, 0.4f, 27.2f, 2.0f, 0.02f, 0.5f, COL_WATER, false);
    for (int i = 0; i < MAX_COWS; i++)
        G.cows[i] = (Wanderer){ { RandF(52, 58), 0, RandF(29, 34) }, RandF(-PI, PI), RandF(1, 3), 0 };
    for (int i = 0; i < MAX_SHEEP; i++)
        G.sheep[i] = (Wanderer){ { RandF(52, 58), 0, RandF(28, 34) }, RandF(-PI, PI), RandF(1, 3), 0 };
    KeepOut(48, 25, 61, 37);
}

static void BuildMarket(void)
{
    const float cz = ZIG_Z;
    Trader *t = G.traders;

    t[TR_BOWYER] = (Trader){ "Bowyer", "Reed shafts and horn bows. What do you bring?",
                             { -8.3f, 0, cz }, PI / 2, { 120, 60, 40, 255 }, BOWYER, COUNT(BOWYER), false, false, { .hp = 0 } };
    t[TR_FARMER] = (Trader){ "Farmer", "Seed for your field, hens for your yard.",
                             { 8.3f, 0, cz }, -PI / 2, { 90, 110, 60, 255 }, FARMER, COUNT(FARMER), false, false, { .hp = 0 } };
    t[TR_WEAVER] = (Trader){ "Basket weaver", "A good basket carries a whole harvest.",
                             { 0, 0, cz - 8.3f }, 0, { 160, 130, 70, 255 }, WEAVER, COUNT(WEAVER), false, false, { .hp = 0 } };
    t[TR_HERDER] = (Trader){ "Cattle herder", "Fresh milk from my cows, for something in trade.",
                             { 48.6f, 0, 31.0f }, -PI / 2, { 110, 90, 70, 255 }, HERDER, COUNT(HERDER), false, false, { .hp = 0 } };

    AddBox(-6.9f, 0, cz, 0.9f, 0.9f, 3.0f, COL_WOOD, true);
    AddBox(6.9f, 0, cz, 0.9f, 0.9f, 3.0f, COL_WOOD, true);
    AddBox(0, 0, cz - 6.9f, 3.0f, 0.9f, 0.9f, COL_WOOD, true);

    AddBox(-6.9f, 0.9f, cz - 0.7f, 0.08f, 0.06f, 1.3f, COL_WOOD, false);
    AddBox(-6.75f, 0.9f, cz - 0.5f, 0.08f, 0.06f, 1.3f, COL_CLAY_DARK, false);
    AddBox(-6.9f, 0.9f, cz + 0.8f, 0.4f, 0.14f, 0.8f, COL_REED, false);
    AddBox(6.9f, 0.9f, cz - 0.9f, 0.5f, 0.45f, 0.5f, COL_LINEN, false);
    AddBox(6.9f, 0.9f, cz - 0.2f, 0.45f, 0.4f, 0.45f, (Color){ 200, 180, 140, 255 }, false);
    AddBox(6.9f, 0.9f, cz + 0.8f, 0.3f, 0.6f, 0.3f, COL_WHEAT_RIPE, false);
    AddBox(-0.9f, 0.9f, cz - 6.9f, 0.55f, 0.4f, 0.55f, COL_REED, false);
    AddBox(0.2f, 0.9f, cz - 6.9f, 0.6f, 0.35f, 0.6f, (Color){ 176, 160, 90, 255 }, false);
    AddBox(0.9f, 0.9f, cz - 6.9f, 0.45f, 0.5f, 0.45f, COL_REED, false);

    AddBox(-9.95f, 1.0f, cz, 0.05f, 1.8f, 2.6f, COL_BLANKET, false);
    AddBox(9.95f, 1.0f, cz, 0.05f, 1.8f, 2.6f, COL_LAPIS, false);
    AddBox(0, 1.0f, cz - 9.95f, 2.6f, 1.8f, 0.05f, (Color){ 176, 140, 60, 255 }, false);
    AddShape(47.4f, 0, 33.0f, 0.5f, 0.65f, 0.5f, COL_CLAY, true, SHAPE_BLOB);
}

int AddNpc(NpcKind kind, Vector3 a, Vector3 b, float speed, Color robe, float face)
{
    if (G.villagerCount >= MAX_VILLAGERS) return -1;
    G.villagers[G.villagerCount] = (Villager){ a, b, speed > 0 ? RandF(0, 1) : 0.0f, 1.0f, speed, robe, kind, face, 0.0f, false, false, { .hp = 0 }, -1 };
    return G.villagerCount++;
}

static void AddVillager(float ax, float az, float bx, float bz, float speed, Color robe)
{
    AddNpc(NPC_CIVILIAN, (Vector3){ ax, 0, az }, (Vector3){ bx, 0, bz }, speed, robe, 0);
}

static void BuildTempleStaff(void)
{
    const Color guard = { 150, 128, 96, 255 }, cleric = { 238, 234, 224, 255 };
    const float cz = ZIG_Z;

    AddNpc(NPC_GUARD, (Vector3){ -7.2f, 0, -4.2f }, (Vector3){ -7.2f, 0, -4.2f }, 0, guard, 0);
    AddNpc(NPC_GUARD, (Vector3){ 7.2f, 0, -4.2f }, (Vector3){ 7.2f, 0, -4.2f }, 0, guard, 0);
    AddNpc(NPC_GUARD, (Vector3){ -4.8f, 0, 70.5f }, (Vector3){ -4.8f, 0, 70.5f }, 0, guard, 0);
    AddNpc(NPC_GUARD, (Vector3){ 4.8f, 0, 70.5f }, (Vector3){ 4.8f, 0, 70.5f }, 0, guard, 0);
    AddNpc(NPC_GUARD, (Vector3){ 90, 0, 9 }, (Vector3){ 50, 0, 9 }, 0.9f, guard, 0);
    AddNpc(NPC_GUARD, (Vector3){ -90, 0, 39 }, (Vector3){ -40, 0, 39 }, 0.9f, guard, 0);
    AddNpc(NPC_GUARD, (Vector3){ -24, 0, -10 }, (Vector3){ -24, 0, -60 }, 1.0f, guard, 0);
    AddNpc(NPC_GUARD, (Vector3){ -17, 7, -28.5f }, (Vector3){ -8, 7, -28.5f }, 0.8f, guard, 0);
    for (int s = -1; s <= 1; s += 2) {
        AddNpc(NPC_GUARD, (Vector3){ 0.55f * (float)s, 7, -25.9f }, (Vector3){ 0.55f * (float)s, 7, -25.9f }, 0, guard, 0);
        G.villagers[G.villagerCount - 1].templePost = true;
    }

    AddNpc(NPC_CLERIC, (Vector3){ 0, 0.5f, -11.6f }, (Vector3){ 0, 7, -22.0f }, 0.5f, cleric, 0);
    AddNpc(NPC_CLERIC, (Vector3){ -6, 16.5f, -40.5f }, (Vector3){ 6, 16.5f, -40.5f }, 0.4f, cleric, 0);
    AddNpc(NPC_CLERIC, (Vector3){ 24, 0, -28 }, (Vector3){ 24, 0, -58 }, 0.6f, cleric, 0);
    AddNpc(NPC_CLERIC, (Vector3){ 9, 0, -15.5f }, (Vector3){ 14, 0, -15.5f }, 0.4f, cleric, 0);
    AddNpc(NPC_CLERIC, (Vector3){ -9, 7, -41 }, (Vector3){ -3, 7, -41 }, 0.4f, cleric, 0);
    (void)cz;
}

Vector3 VillagerPos(const Villager *v)
{
    return v->vit.override ? v->vit.at : VillagerLanePos(v);
}

Vector3 TraderPos(const Trader *t)
{
    return t->vit.override ? t->vit.at : t->pos;
}

static float LaneLength(const Villager *v)
{
    return v->road >= 0 ? RoadLength(v->road) : fmaxf(Vector3Distance(v->a, v->b), 0.1f);
}

Vector3 VillagerLanePos(const Villager *v)
{
    if (v->road >= 0) {
        Vector3 dir, p = RoadPoint(v->road, v->t * RoadLength(v->road), &dir);
        return (Vector3){ p.x + dir.z * v->side, p.y, p.z - dir.x * v->side };
    }
    Vector3 p = Vector3Lerp(v->a, v->b, v->t);
    float dx = v->b.x - v->a.x, dz = v->b.z - v->a.z, len = sqrtf(dx * dx + dz * dz);
    if (len < 0.01f || v->side == 0) return p;
    return (Vector3){ p.x + dz / len * v->side, p.y, p.z - dx / len * v->side };
}

// Walks the lane; blocked by someone ahead it waits and steps aside, then drifts back.
static void WalkVillager(Villager *v, int i, float dt)
{
    const int self = OWNER_VILLAGER + i;
    float len = LaneLength(v);
    Villager next = *v;

    v->moving = false;
    if (v->speed <= 0 || v->vit.override) return;
    next.t += v->dir * v->speed * dt / len;
    if (next.t > 1) { next.t = 1; next.dir = -1; }
    if (next.t < 0) { next.t = 0; next.dir = 1; }

    Vector3 cur = VillagerPos(v), to = VillagerPos(&next);
    if (BodyAhead(cur, to, 0.35f, self)) {
        Villager aside = *v;
        float room = v->road >= 0 ? 2.2f : 1.3f;
        aside.side = Clamp(v->side - v->dir * 1.2f * dt, -room, room);
        Vector3 sp = VillagerPos(&aside);
        if (!SolidAt(sp.x, sp.z, 0.3f) && !BodyAt(sp, 0.35f, self)) v->side = aside.side;
        return;
    }
    *v = next;
    v->moving = true;
    if (to.x != cur.x || to.z != cur.z) v->face = atan2f(to.x - cur.x, to.z - cur.z);
    // On a road the way back is to the right-hand side of the direction of travel.
    float home = v->road >= 0 ? -1.0f * v->dir : 0.0f;
    if (v->side != home) {
        Villager back = *v;
        float step = 0.6f * dt;
        back.side = fabsf(v->side - home) <= step ? home : v->side - copysignf(step, v->side - home);
        if (!BodyAt(VillagerPos(&back), 0.35f, self)) v->side = back.side;
    }
}

void BuildVillage(void)
{
    BuildHome();
    BuildFarm();
    BuildPen();
    BuildMarket();

    G.villagerCount = 0;
    AddVillager(-3.5f, -11, -3.5f, 74, 1.1f, (Color){ 200, 190, 160, 255 });
    AddVillager(3.5f, 74, 3.5f, -11, 1.3f, (Color){ 150, 110, 80, 255 });
    AddVillager(-92, 8, 92, 8, 1.2f, (Color){ 180, 170, 140, 255 });
    AddVillager(60, 10, -60, 10, 1.0f, (Color){ 160, 120, 90, 255 });
    AddVillager(-3, ZIG_Z + 2, 3, ZIG_Z + 2, 0.7f, (Color){ 230, 225, 210, 255 });
    AddVillager(-92, 26, -59, 26, 0.9f, (Color){ 170, 150, 110, 255 });
    AddVillager(-70, 39, -8, 39, 1.0f, (Color){ 190, 175, 150, 255 });
    AddVillager(10, 39, 92, 39, 1.0f, (Color){ 168, 148, 118, 255 });
    AddVillager(-14, -9, 14, -9, 0.6f, (Color){ 236, 232, 222, 255 });
    AddVillager(-56, -22, -56, 1, 0.8f, (Color){ 120, 140, 150, 255 });
    AddVillager(-37, -82, -37, 2, 1.1f, (Color){ 176, 160, 120, 255 });
    for (int i = 0; i < CityFarmCount(); i++) {
        Vector3 c = CityFarmSpot(i);
        AddVillager(c.x - 3.5f, c.z, c.x + 7.5f, c.z, 0.45f, (Color){ 204, 194, 164, 255 });
    }
    BuildTempleStaff();
}

static void WanderIn(Wanderer *w, Rectangle area, float speed, float dt)
{
    w->timer -= dt;
    if (w->timer <= 0) {
        w->timer = RandF(1, 4);
        w->heading += RandF(-2, 2);
        w->speed = GetRandomValue(0, 1) ? speed : 0.0f;
    }
    float nx = w->pos.x + sinf(w->heading) * w->speed * dt;
    float nz = w->pos.z + cosf(w->heading) * w->speed * dt;
    if (nx < area.x || nx > area.x + area.width || nz < area.y || nz > area.y + area.height) {
        w->heading += PI;
        return;
    }
    w->pos.x = nx;
    w->pos.z = nz;
}

void UpdateVillage(float dt)
{
    for (int i = 0; i < G.villagerCount; i++) {
        Villager *v = &G.villagers[i];
        if (v->templePost) {
            float s = v->a.x < 0 ? -1.0f : 1.0f;
            Vector3 post = G.templeAccess ? (Vector3){ 1.75f * s, 7, -24.4f } : (Vector3){ 0.55f * s, 7, -25.9f };
            v->a = v->b = post;
        }
        WalkVillager(v, i, dt);
    }
    for (int i = 0; i < G.henCount; i++) WanderIn(&G.hens[i], (Rectangle){ 15.5f, 31.0f, 5.0f, 2.5f }, 0.6f, dt);
    for (int i = 0; i < MAX_COWS; i++) WanderIn(&G.cows[i], (Rectangle){ 51.5f, 27.8f, 7.0f, 7.0f }, 0.5f, dt);
    for (int i = 0; i < MAX_SHEEP; i++) WanderIn(&G.sheep[i], (Rectangle){ 51.0f, 27.0f, 8.0f, 8.0f }, 0.45f, dt);
    Rectangle fold = FoldArea();
    fold = (Rectangle){ fold.x + 0.8f, fold.y + 0.8f, fold.width - 1.6f, fold.height - 1.6f };
    for (int i = 0; i < MAX_FLOCK; i++) {
        if (XZDist(G.flock[i].pos, G.cam.position) > 220.0f) continue;
        WanderIn(&G.flock[i], fold, 0.45f, dt);
        G.flock[i].pos.y = TerrainHeight(G.flock[i].pos.x, G.flock[i].pos.z);
    }
}

// Date palms fruit once a day, never in winter; the farm palm also needs the farm watered.
void AdvanceHours(float h)
{
    bool winter = Season() == WINTER;
    for (int i = 0; i < G.resCount; i++) {
        Resource *r = &G.res[i];
        if (r->regrow <= 0) continue;
        if (r->kind == RES_PALM && (winter || (r->farm && G.farmWater <= 0))) continue;
        r->regrow -= h;
    }
    // Long sleeps run the farm an hour at a time so water runs out at the right moment.
    for (float left = h; left > 0.0f; left -= 1.0f) FarmHours(fminf(left, 1.0f));
    if (G.blessT > 0) G.blessT = fmaxf(0.0f, G.blessT - h);
}

static const Offer *OvenRecipe(void)
{
    const char *why;
    for (int i = 0; i < COUNT(OVEN); i++)
        if (OfferCheck(&OVEN[i], &why)) return &OVEN[i];
    return NULL;
}

void StationPrompt(int s, char *buf, int size, bool *ok)
{
    const char *why = "";

    switch (s) {
    case STN_BED:
        *ok = true;
        Fmt(buf, size, (G.hour >= 19.0f || G.hour < 5.0f) ? "[E] Sleep until dawn" : "[E] Rest until dusk");
        break;
    case STN_STORAGE:
        *ok = true;
        Fmt(buf, size, "[E] Open the storeroom");
        break;
    case STN_QUERN:
        *ok = OfferCheck(&QUERN, &why);
        if (*ok) Fmt(buf, size, "[E] Grind 2 wheat into flour (-1.5 food)");
        else Fmt(buf, size, "Quern: %s", why);
        break;
    case STN_TROUGH:
        *ok = OfferCheck(&TROUGH, &why);
        if (*ok) Fmt(buf, size, "[E] Knead flour + milk into 2 dough");
        else Fmt(buf, size, "Kneading trough: %s", why);
        break;
    case STN_OVEN: {
        const Offer *o = OvenRecipe();
        *ok = o != NULL;
        if (!o) Fmt(buf, size, "Oven: bring dough, fish or a carcass");
        else if (o->in1 == IT_DOUGH) Fmt(buf, size, "[E] Bake dough into 2 bread");
        else if (o->in1 == IT_FISH) Fmt(buf, size, "[E] Grill 2 fish");
        else Fmt(buf, size, "[E] Roast the %s into %d meat", ItemName(o->in1), o->nOut);
        break;
    }
    case STN_KILN:
        *ok = OfferCheck(&KILN, &why);
        if (*ok) Fmt(buf, size, "[E] Fire 3 clay + 1 wood into 2 pots");
        else Fmt(buf, size, "Kiln: %s (3 clay + 1 wood)", why);
        break;
    case STN_SMELTER:
        *ok = OfferCheck(&SMELTER, &why);
        if (*ok) Fmt(buf, size, "[E] Smelt 2 copper ore + 1 tin + 2 wood into bronze (-2 food)");
        else Fmt(buf, size, "Smelter: %s (2 copper ore + 1 tin + 2 wood)", why);
        break;
    case STN_BENCH:
        *ok = OfferCheck(&LEATHER, &why);
        if (*ok) Fmt(buf, size, "[E] Sew leather armor (2 cowhide + 2 sheepskin)");
        else Fmt(buf, size, "Workbench, leather armor: %s", why);
        break;
    case STN_ALTAR:
        BlessPrompt(buf, size, ok);
        break;
    case STN_WELL:
        *ok = BagCount(IT_BUCKET) > 0;
        if (*ok) Fmt(buf, size, "[E] Draw water into a bucket");
        else Fmt(buf, size, BagCount(IT_WATER) > 0 ? "Your bucket is full" : "Well: bring a bucket (one is in your storeroom)");
        break;
    case STN_DITCH:
        DitchPrompt(buf, size, ok);
        break;
    default:
        buf[0] = '\0';
        *ok = false;
        break;
    }
}

void BlessPrompt(char *buf, int size, bool *ok)
{
    *ok = G.prayDay != G.day;
    if (*ok) Fmt(buf, size, "[E] Ask the priest for Nanna's blessing (a day of +20%% speed)");
    else if (G.blessT > 0) Fmt(buf, size, "Blessed: %d hours left. Come back tomorrow", (int)ceilf(G.blessT));
    else Fmt(buf, size, "You have been blessed today");
}

void CloakPrompt(char *buf, int size, bool *ok)
{
    const char *why;
    *ok = OfferCheck(&CLOAK, &why);
    if (*ok) Fmt(buf, size, "[F] Sew a wolf-pelt cloak (3 wolf pelts)");
    else Fmt(buf, size, "Wolf-pelt cloak: %s", why);
}

void SewCloak(void)
{
    if (!OfferApply(&CLOAK)) return;
    Exert(1.0f);
    Say("Sewed a wolf-pelt cloak. Wolves keep their distance unless you attack them.");
}

void UseStation(int s)
{
    switch (s) {
    case STN_BED:
        StartSleep();
        break;
    case STN_STORAGE:
        SetPanel(PANEL_STORAGE);
        break;
    case STN_QUERN:
        if (!OfferApply(&QUERN)) break;
        Exert(1.5f);
        Say("Ground wheat into flour. You have %d flour.", BagCount(IT_FLOUR));
        break;
    case STN_TROUGH:
        if (!OfferApply(&TROUGH)) break;
        Exert(0.5f);
        Say("Kneaded 2 dough.");
        break;
    case STN_OVEN: {
        const Offer *o = OvenRecipe();
        if (!o) Say("The oven needs dough, fish, a carcass or a hen.");
        else if (!OfferApply(o)) break;
        else if (o->in1 == IT_DOUGH) Say("Baked 2 bread.");
        else if (o->in1 == IT_FISH) Say("Grilled 2 fish.");
        else Say("Roasted %d meat.", o->nOut);
        if (o) Exert(0.5f);
        break;
    }
    case STN_KILN:
        if (!OfferApply(&KILN)) break;
        Exert(1.0f);
        Say("Fired 2 clay pots. Caravans trade well for pots.");
        break;
    case STN_SMELTER:
        if (!OfferApply(&SMELTER)) break;
        Exert(2.0f);
        Say("Poured a bronze ingot. You have %d bronze.", BagCount(IT_BRONZE));
        break;
    case STN_BENCH:
        if (!OfferApply(&LEATHER)) break;
        Exert(1.0f);
        Say("Sewed leather armor and put it on. Blows hurt a quarter less.");
        break;
    case STN_ALTAR:
        if (G.prayDay == G.day) break;
        G.prayDay = G.day;
        G.blessT = BLESS_HOURS;
        G.health = HEALTH_MAX;
        Say("The priest blesses you in Nanna's name: for a day you run and strike a fifth faster.");
        break;
    case STN_WELL:
        FillBucket();
        break;
    case STN_DITCH:
        PourDitch();
        break;
    default:
        break;
    }
}

void ForgePrompt(char *buf, int size, bool *ok)
{
    const char *why;
    *ok = false;
    if (G.weapon >= 2) {
        Fmt(buf, size, "Your %s is the best weapon the forge can make", WeaponName(G.weapon));
        return;
    }
    const Offer *o = &FORGE[G.weapon];
    *ok = OfferCheck(o, &why);
    if (*ok) Fmt(buf, size, "[F] Forge a %s (%d %s + %d wood)", WeaponName(o->out), o->n1, ItemName(o->in1), o->n2);
    else Fmt(buf, size, "Forge %s: %s", WeaponName(o->out), why);
}

void UseForge(void)
{
    if (G.weapon >= 2) return;
    if (!OfferApply(&FORGE[G.weapon])) return;
    Exert(3.0f);
    Say("Forged a %s. Press 2 to take it in hand.", WeaponName(G.weapon));
}

static void Part(float x, float y, float z, float w, float h, float l, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, l }, c);
}

// A walking staff, a bundle on the back and a wide straw hat.
static void DrawTravelGear(Vector3 pos, float face, Color robe)
{
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(face * RAD2DEG, 0, 1, 0);
    Part(0.36f, 1.0f, 0.24f, 0.05f, 2.0f, 0.05f, COL_WOOD);
    DrawShadedBlob((Vector3){ 0, 1.3f, -0.3f }, (Vector3){ 0.22f, 0.26f, 0.16f }, Mix(robe, COL_STRAW, 0.5f));
    Part(0, 1.95f, -0.02f, 0.46f, 0.05f, 0.46f, COL_STRAW);
    rlPopMatrix();
}

static void DrawSheep(const Wanderer *w, int i)
{
    const Color wool = { 226, 220, 204, 255 }, face = { 52, 44, 40, 255 };

    rlPushMatrix();
    rlTranslatef(w->pos.x, w->pos.y, w->pos.z);
    rlRotatef(w->heading * RAD2DEG, 0, 1, 0);
    for (int k = 0; k < 4; k++)
        DrawShadedCylinder((Vector3){ (k & 1) ? 0.16f : -0.16f, 0, (k & 2) ? 0.32f : -0.32f }, 0.04f, 0.04f, 0.5f, 0.8f, 5, face);
    DrawShadedBlob((Vector3){ 0, 0.72f, 0 }, (Vector3){ 0.34f, 0.3f, 0.52f }, wool);
    DrawShadedBlob((Vector3){ 0, 0.82f, 0.05f }, (Vector3){ 0.28f, 0.26f, 0.42f }, Mix(wool, COL_LINEN, 0.5f));
    DrawShadedBlob((Vector3){ 0, 0.86f, 0.58f }, (Vector3){ 0.12f, 0.15f, 0.17f }, face);
    Part(-0.15f, 0.92f, 0.52f, 0.12f, 0.05f, 0.08f, face);
    Part(0.15f, 0.92f, 0.52f, 0.12f, 0.05f, 0.08f, face);
    Part(0, 0.98f, 0.5f, 0.26f, 0.1f, 0.14f, wool);
    (void)i;
    rlPopMatrix();
}

static void DrawCow(const Wanderer *w, int i)
{
    Color coat = (i == 1) ? (Color){ 128, 86, 54, 255 } : (Color){ 226, 220, 206, 255 };
    const Color pink = { 200, 150, 140, 255 }, dark = { 60, 48, 40, 255 };

    rlPushMatrix();
    rlTranslatef(w->pos.x, 0, w->pos.z);
    rlRotatef(w->heading * RAD2DEG, 0, 1, 0);
    for (int k = 0; k < 4; k++)
        DrawShadedCylinder((Vector3){ (k & 1) ? 0.26f : -0.26f, 0, (k & 2) ? 0.68f : -0.68f }, 0.08f, 0.08f, 0.9f, 0.75f, 6, coat);
    DrawShadedBlob((Vector3){ 0, 1.17f, 0 }, (Vector3){ 0.42f, 0.44f, 1.0f }, coat);
    if (i != 1) DrawShadedBlob((Vector3){ 0.12f, 1.3f, -0.3f }, (Vector3){ 0.36f, 0.3f, 0.36f }, dark);
    DrawShadedBlob((Vector3){ 0, 1.35f, 1.1f }, (Vector3){ 0.21f, 0.23f, 0.3f }, coat);
    Part(0, 1.22f, 1.38f, 0.32f, 0.22f, 0.12f, pink);
    Part(0, 1.6f, 1.0f, 0.6f, 0.06f, 0.06f, COL_LINEN);
    Part(0, 0.74f, -0.4f, 0.3f, 0.16f, 0.3f, pink);
    Part(0, 1.1f, -0.92f, 0.06f, 0.6f, 0.06f, dark);
    rlPopMatrix();
}

void DrawVillage(void)
{
    DrawFarm();
    LightFor(G.pen);
    for (int i = 0; i < MAX_COWS; i++) DrawCow(&G.cows[i], i);
    for (int i = 0; i < MAX_SHEEP; i++) DrawSheep(&G.sheep[i], i);
    for (int i = 0; i < MAX_FLOCK; i++) {
        if (XZDist(G.flock[i].pos, G.cam.position) > 200.0f) continue;
        LightFor(G.flock[i].pos);
        DrawSheep(&G.flock[i], i);
    }

    for (int i = 0; i < TR_COUNT; i++) {
        const Trader *tr = &G.traders[i];
        if (tr->roaming) continue;
        Look look = { COL_SKIN, tr->robe, tr->robe, COL_HAIR };
        Vector3 tp = TraderPos(tr);
        if (XZDist(tp, G.cam.position) > 220.0f) continue;
        LightFor(tp);
        if (tr->vit.dead) DrawPersonBody(tp, tr->vit.face, look);
        else DrawHuman(tp, tr->vit.override ? tr->vit.face : tr->facing, tr->vit.override ? (float)GetTime() * 8.0f : 0.0f, look, false, false, 0);
    }
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        Vector3 pos = VillagerPos(v);
        float stride = v->moving ? v->t * LaneLength(v) * 3.0f : 0.0f;
        float face = v->vit.override ? v->vit.face : v->face;
        if (XZDist(pos, G.cam.position) > 200.0f) continue;
        if (v->vit.override && v->vit.hostile > 0) stride = (float)GetTime() * 8.0f;
        LightFor(pos);
        if (v->vit.dead) {
            Look look = { COL_SKIN, v->robe, v->kind == NPC_GUARD ? (Color){ 120, 96, 66, 255 } : v->robe, v->kind == NPC_GUARD ? COL_BRONZE : COL_HAIR };
            DrawPersonBody(pos, face, look);
            continue;
        }
        if (v->kind == NPC_CIVILIAN || v->kind == NPC_VILLAGER || v->kind == NPC_TRAVELLER) {
            Look look = { COL_SKIN, v->robe, (i % 2) ? COL_SKIN : v->robe, COL_HAIR };
            // A villager standing still at the hunters' butt keeps drawing a bow.
            bool archer = v->kind == NPC_VILLAGER && v->speed <= 0 && !v->vit.override;
            float pull = archer ? fmaxf(0.0f, sinf((float)GetTime() * 0.9f + (float)i)) : 0.0f;
            DrawHuman(pos, face, stride, look, false, archer, pull);
            if (v->kind == NPC_TRAVELLER) DrawTravelGear(pos, face, v->robe);
            if (v->kind == NPC_TRAVELLER && G.night >= 0.3f) DrawHandTorch(pos, face);
        } else if (v->kind == NPC_GUARD) {
            Look look = { COL_SKIN, v->robe, (Color){ 120, 96, 66, 255 }, COL_BRONZE };
            DrawHuman(pos, face, stride, look, false, false, 0);
            rlPushMatrix();
            rlTranslatef(pos.x, pos.y, pos.z);
            rlRotatef(face * RAD2DEG, 0, 1, 0);
            Part(0, 1.97f, -0.02f, 0.36f, 0.12f, 0.36f, COL_BRONZE);
            if (v->vit.swing > 0) {
                float k = v->vit.swing / 0.35f;
                Part(0.3f, 1.35f, 0.9f - 0.5f * k, 0.05f, 0.05f, 2.4f, COL_WOOD);
                Part(0.3f, 1.35f, 2.2f - 0.5f * k, 0.08f, 0.08f, 0.26f, COL_BRONZE);
            } else if (v->vit.windup > 0) {
                Part(0.3f, 1.45f, 0.35f, 0.05f, 0.05f, 2.4f, COL_WOOD);
                Part(0.3f, 1.45f, 1.65f, 0.08f, 0.08f, 0.26f, COL_BRONZE);
            } else {
                Part(0.36f, 1.2f, 0.12f, 0.05f, 2.4f, 0.05f, COL_WOOD);
                Part(0.36f, 2.5f, 0.12f, 0.08f, 0.26f, 0.08f, COL_BRONZE);
            }
            if (G.night < 0.3f) Part(-0.38f, 1.2f, 0.1f, 0.08f, 0.62f, 0.5f, (Color){ 130, 90, 56, 255 });
            rlPopMatrix();
            if (G.night >= 0.3f) DrawHandTorch(pos, face);
        } else {
            Look look = { COL_SKIN, v->robe, v->robe, COL_SKIN };
            DrawHuman(pos, face, stride, look, false, false, 0);
            rlPushMatrix();
            rlTranslatef(pos.x, pos.y, pos.z);
            rlRotatef(face * RAD2DEG, 0, 1, 0);
            Part(0, 0.5f, 0, 0.52f, 0.6f, 0.34f, v->robe);
            Part(0, 2.06f, -0.01f, 0.24f, 0.26f, 0.24f, v->robe);
            rlPopMatrix();
        }
    }
    G.drawTorch = G.drawFog = 0;
}
