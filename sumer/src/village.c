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

// Each player's home: the first one by the farm, the others in a row south of the east-west street,
// their doors toward it. A house is laid out in its own frame with the door on its -x side.
static const float HOUSE_SPOTS[MAX_PLAYERS][3] = { { 20, 22, 0 }, { 14, 50, 1 }, { 28, 50, 1 }, { 42, 50, 1 }, { -14, 50, 1 }, { 56, 50, 1 } };
static const int HOUSE_STATIONS[HOUSE_STN] = { STN_BED, STN_STORAGE, STN_QUERN, STN_TROUGH, STN_BENCH, STN_OVEN };

Color PlayerColor(int slot)
{
    static const Color colors[MAX_PLAYERS] = {
        { 38, 84, 160, 255 }, { 176, 46, 40, 255 }, { 46, 128, 62, 255 }, { 222, 162, 30, 255 }, { 118, 60, 150, 255 }, { 30, 150, 150, 255 },
    };
    return colors[slot >= 0 && slot < MAX_PLAYERS ? slot : 0];
}

const char *PlayerColorName(int slot)
{
    static const char *const names[MAX_PLAYERS] = { "blue", "red", "green", "yellow", "purple", "teal" };
    return L(names[slot >= 0 && slot < MAX_PLAYERS ? slot : 0]);
}

typedef struct { float cx, cz; int rot; } Frame;

// Quarter turns about the house's center: 1 puts the door to the north, 2 east, 3 south.
static Vector2 Turn(const Frame *f, float lx, float lz)
{
    switch (f->rot & 3) {
    case 1:  return (Vector2){ f->cx - lz, f->cz + lx };
    case 2:  return (Vector2){ f->cx - lx, f->cz - lz };
    case 3:  return (Vector2){ f->cx + lz, f->cz - lx };
    default: return (Vector2){ f->cx + lx, f->cz + lz };
    }
}

static void HShape(const Frame *f, float lx, float y, float lz, float w, float h, float d, Color c, bool solid, int shape)
{
    Vector2 p = Turn(f, lx, lz);
    bool across = (f->rot & 1) != 0;
    AddShape(p.x, y, p.y, across ? d : w, h, across ? w : d, c, solid, shape);
}

static void HBox(const Frame *f, float lx, float y, float lz, float w, float h, float d, Color c, bool solid)
{
    HShape(f, lx, y, lz, w, h, d, c, solid, SHAPE_BOX);
}

static Vector3 HPoint(const Frame *f, float lx, float lz)
{
    Vector2 p = Turn(f, lx, lz);
    return (Vector3){ p.x, 0, p.y };
}

// Walls tinted toward the owner's color, a band of it under the roofline and over the door.
static void BuildHouse(int h)
{
    const float W = 10, D = 10, H = 3, t = 0.5f, gap = 1.8f;
    const float seg = (D - 2 * t - gap) / 2;
    Frame f = { HOUSE_SPOTS[h][0], HOUSE_SPOTS[h][1], (int)HOUSE_SPOTS[h][2] };
    House *home = &G.houses[h];
    Color own = PlayerColor(h), wall = Mix(COL_MUD_PALE, own, 0.12f), band = Mix(own, COL_MUD_PALE, 0.25f);
    Vector3 st[HOUSE_STN];

    home->center = (Vector3){ f.cx, 0, f.cz };
    HBox(&f, 0, 0, -D / 2 + t / 2, W, H, t, wall, true);
    HBox(&f, 0, 0, D / 2 - t / 2, W, H, t, wall, true);
    HBox(&f, W / 2 - t / 2, 0, 0, t, H, D - 2 * t, wall, true);
    HBox(&f, -W / 2 + t / 2, 0, -gap / 2 - seg / 2, t, H, seg, wall, true);
    HBox(&f, -W / 2 + t / 2, 0, gap / 2 + seg / 2, t, H, seg, wall, true);
    HBox(&f, -W / 2 + t / 2, H - 0.5f, 0, t + 0.1f, 0.5f, gap, own, false);
    HBox(&f, 0, 0, 0, W - 2 * t, 0.04f, D - 2 * t, COL_BRICK, false);
    HBox(&f, 0, H - 0.42f, -D / 2 + t / 2, W + 0.08f, 0.24f, t + 0.08f, band, false);
    HBox(&f, 0, H - 0.42f, D / 2 - t / 2, W + 0.08f, 0.24f, t + 0.08f, band, false);
    HBox(&f, W / 2 - t / 2, H - 0.42f, 0, t + 0.08f, 0.24f, D - 2 * t, band, false);

    st[0] = HPoint(&f, 3.0f, -3.0f);
    HBox(&f, 3.0f, 0, -3.0f, 1.3f, 0.45f, 2.3f, COL_WOOD, true);
    HBox(&f, 3.0f, 0.45f, -3.8f, 1.0f, 0.15f, 0.5f, COL_LINEN, false);
    HBox(&f, 3.0f, 0.45f, -2.6f, 1.2f, 0.06f, 1.3f, h == 0 ? COL_BLANKET : own, false);

    st[1] = HPoint(&f, -1.6f, -3.4f);
    for (int k = 0; k < 3; k++) {
        float x = -2.7f + 1.1f * (float)k;
        HShape(&f, x, -0.1f, -4.05f, 0.8f, 1.2f, 0.8f, COL_CLAY, true, SHAPE_BLOB);
        HShape(&f, x, 0.95f, -4.05f, 0.42f, 0.25f, 0.42f, COL_CLAY_DARK, false, SHAPE_CYL);
    }

    st[2] = HPoint(&f, -3.4f, 3.6f);
    HBox(&f, -3.4f, 0, 3.6f, 0.9f, 0.45f, 0.7f, COL_STONE, true);
    HBox(&f, -3.4f, 0.45f, 3.6f, 0.55f, 0.12f, 0.45f, (Color){ 100, 96, 90, 255 }, false);

    st[3] = HPoint(&f, 0, 3.9f);
    HBox(&f, 0, 0, 3.9f, 1.4f, 0.55f, 0.6f, COL_WOOD, true);
    HBox(&f, 0, 0.55f, 3.9f, 1.1f, 0.03f, 0.35f, (Color){ 226, 204, 160, 255 }, false);

    // The workbench with a hide and a bundle of wool on it.
    st[4] = HPoint(&f, 2.9f, 0.6f);
    HBox(&f, 4.0f, 0, 0.6f, 0.8f, 0.75f, 1.6f, COL_WOOD, true);
    HBox(&f, 4.0f, 0.75f, 0.3f, 0.5f, 0.04f, 0.6f, (Color){ 130, 92, 60, 255 }, false);
    HBox(&f, 4.0f, 0.75f, 1.1f, 0.4f, 0.1f, 0.3f, (Color){ 222, 214, 196, 255 }, false);

    st[5] = HPoint(&f, 3.4f, 3.4f);
    HShape(&f, 3.4f, -0.4f, 3.4f, 1.2f, 1.6f, 1.2f, COL_CLAY, true, SHAPE_BLOB);
    HShape(&f, 3.4f, 0.85f, 3.4f, 0.5f, 0.2f, 0.5f, COL_CLAY_DARK, false, SHAPE_CYL);
    Vector2 glow = Turn(&f, 3.4f, 3.4f);
    AddGlow(glow.x, 1.05f, glow.y, 0.32f, 0.03f, 0.32f, COL_FIRE);

    for (int k = 0; k < HOUSE_STN; k++) home->stations[k] = st[k];
    home->door = HPoint(&f, -W / 2 - 0.8f, 0);
    home->bedFacing = -(float)f.rot * PI / 2;
    KeepOut(f.cx - W / 2 - 1, f.cz - D / 2 - 1, f.cx + W / 2 + 1, f.cz + D / 2 + 1);
}

// The stations of the house this player lives in become theirs to use.
void SetHome(int slot)
{
    const House *home = &G.houses[slot >= 0 && slot < MAX_PLAYERS ? slot : 0];
    G.home = home->center;
    for (int k = 0; k < HOUSE_STN; k++) G.stations[HOUSE_STATIONS[k]] = home->stations[k];
}

// Where a player starts in a PvP game (and comes to after being brought down): anywhere inside the city
// walls on open ground, as far from the other players as a handful of tries finds. Alone, or if nothing
// open turns up, it is the player's own door.
Vector3 CitySpawn(int slot)
{
    Vector3 best = G.houses[slot >= 0 && slot < MAX_PLAYERS ? slot : 0].door;
    float bestAway = -1.0f;
    int found = 0;

    for (int tries = 0; tries < 400 && found < 24; tries++) {
        float x = RandF(-CITY_A, CITY_A), z = RandF(CITY_Z - CITY_B, CITY_Z + CITY_B);
        if (!InsideWalls(x, z, 10.0f) || InWater(x, z) || SolidAt(x, z, 1.3f)) continue;
        float ground = TerrainHeight(x, z);
        if (GroundHeight(x, z, ground + 0.5f) > ground + 0.3f) continue;
        found++;
        float away = 1e9f;
        for (int s = 0; s < MAX_PLAYERS; s++)
            if (s != slot && PlayerIn(s)) away = fminf(away, XZDist(G.peers[s].pos, (Vector3){ x, 0, z }));
        if (away > bestAway) {
            bestAway = away;
            best = (Vector3){ x, ground, z };
        }
    }
    return best;
}

// In PvP the guards on the ziggurat's stairs and terrace are gone, so anyone can climb to the priest.
// Host and guests both call this once the rules are known, so their lists of people stay the same.
void RemoveZigguratGuards(void)
{
    int kept = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (v->kind == NPC_GUARD && v->a.y >= 5.0f && v->b.y >= 5.0f) continue;
        G.villagers[kept++] = G.villagers[i];
    }
    G.villagerCount = kept;
}

static void BuildHome(void)
{
    for (int h = 0; h < MAX_PLAYERS; h++) BuildHouse(h);
    SetHome(0);
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
    G.villagers[G.villagerCount] = (Villager){ a, b, speed > 0 ? RandF(0, 1) : 0.0f, 1.0f, speed, robe, kind, face, 0.0f, false, false,
                                               { .hp = 0 }, -1, 0.0f, 0.0f, -1, -1, 0.0f, 0.0f, 0 };
    return G.villagerCount++;
}

// The city's streets as crossings and the stretches between them: the main street from the temple
// gate to the south gate, the east-west street from gate to gate, the street south of it, the street
// past the west side of the temple precinct, the little street in the west and the harbor quay.
static const float STREET_NODES[][2] = {
    { 0, -3 }, { 0, 9 }, { 0, 39 }, { 0, 72 }, { -37, 9 }, { -54, 9 }, { -86, 9 }, { 35, 9 }, { 65, 9 }, { 86, 9 },
    { -37, -35 }, { -37, -74 }, { -45, 39 }, { -76, 39 }, { 45, 39 }, { 76, 39 }, { -60, 26 }, { -84, 26 }, { -54.5f, -18 },
};
static const int STREET_EDGES[][2] = {
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 1, 4 }, { 4, 5 }, { 5, 6 }, { 1, 7 }, { 7, 8 }, { 8, 9 }, { 4, 10 }, { 10, 11 },
    { 2, 12 }, { 12, 13 }, { 2, 14 }, { 14, 15 }, { 16, 17 }, { 5, 18 }, { 16, 5 },
};
#define STREET_NODE_COUNT COUNT(STREET_NODES)
#define STREET_EDGE_COUNT COUNT(STREET_EDGES)

int StreetNodeCount(void)
{
    return STREET_NODE_COUNT;
}

Vector3 StreetNode(int n)
{
    if (n < 0 || n >= STREET_NODE_COUNT) n = 0;
    float x = STREET_NODES[n][0], z = STREET_NODES[n][1];
    return (Vector3){ x, GroundHeight(x, z, 1.0f), z };
}

// Puts a townsperson on the stretch from crossing 'from' to crossing 'to'.
void SetStreet(Villager *v, int from, int to)
{
    v->from = from;
    v->to = to;
    v->a = StreetNode(from);
    v->b = StreetNode(to);
}

static int Neighbours(int n, int *out)
{
    int k = 0;
    for (int e = 0; e < STREET_EDGE_COUNT; e++) {
        if (STREET_EDGES[e][0] == n) out[k++] = STREET_EDGES[e][1];
        else if (STREET_EDGES[e][1] == n) out[k++] = STREET_EDGES[e][0];
    }
    return k;
}

// How far a walker may stray from the middle of each street and stay clear of walls, houses, stalls
// and water: [0] to the right of the street as listed (walking from its first crossing to its
// second), [1] to the left. Measured once the whole city stands; the player houses go up last.
static float streetRoom[STREET_EDGE_COUNT][2];
static bool roomMeasured;

// Whether the stretch a-b passes within m of the box, seen from above.
static bool Passes(float ax, float az, float bx, float bz, const Box *box, float m)
{
    float o[2] = { ax, az }, d[2] = { bx - ax, bz - az }, lo = 0.0f, hi = 1.0f;
    float mn[2] = { box->pos.x - box->size.x / 2 - m, box->pos.z - box->size.z / 2 - m };
    float mx[2] = { box->pos.x + box->size.x / 2 + m, box->pos.z + box->size.z / 2 + m };
    for (int k = 0; k < 2; k++) {
        if (fabsf(d[k]) < 1e-6f) {
            if (o[k] < mn[k] || o[k] > mx[k]) return false;
            continue;
        }
        float t0 = (mn[k] - o[k]) / d[k], t1 = (mx[k] - o[k]) / d[k];
        lo = fmaxf(lo, fminf(t0, t1));
        hi = fminf(hi, fmaxf(t0, t1));
    }
    return lo <= hi;
}

static void MeasureStreets(void)
{
    static int near[512];
    for (int e = 0; e < STREET_EDGE_COUNT; e++) {
        float ax = STREET_NODES[STREET_EDGES[e][0]][0], az = STREET_NODES[STREET_EDGES[e][0]][1];
        float bx = STREET_NODES[STREET_EDGES[e][1]][0], bz = STREET_NODES[STREET_EDGES[e][1]][1];
        float dx = bx - ax, dz = bz - az, len = sqrtf(dx * dx + dz * dz);
        int n = 0, steps = (int)ceilf(len / 0.5f);
        for (int i = 0; i < G.boxCount && n < COUNT(near); i++) {
            const Box *b = &G.boxes[i];
            if (b->solid && !b->water && Passes(ax, az, bx, bz, b, 3.0f)) near[n++] = i;
        }
        for (int s = 0; s < 2; s++) {
            float sign = s ? 1.0f : -1.0f, room = 0.0f;
            for (int o = 1; o <= 8; o++) {
                bool clear = true;
                for (int j = 0; j <= steps && clear; j++) {
                    float x = ax + dx * (float)j / (float)steps + dz / len * 0.25f * (float)o * sign;
                    float z = az + dz * (float)j / (float)steps - dx / len * 0.25f * (float)o * sign;
                    float ground = TerrainHeight(x, z);
                    clear = !InWater(x, z);
                    for (int k = 0; k < n && clear; k++) {
                        const Box *b = &G.boxes[near[k]];
                        clear = !(fabsf(x - b->pos.x) <= b->size.x / 2 + 0.35f && fabsf(z - b->pos.z) <= b->size.z / 2 + 0.35f &&
                                  b->pos.y + b->size.y / 2 > ground + 0.6f);
                    }
                }
                if (!clear) break;
                room = 0.25f * (float)o;
            }
            streetRoom[e][s] = room;
        }
    }
    roomMeasured = true;
}

// The room on a walker's right and on its left along its stretch of street.
static void RoomOf(const Villager *v, float *right, float *left)
{
    if (!roomMeasured) MeasureStreets();
    *right = *left = 0.0f;
    for (int e = 0; e < STREET_EDGE_COUNT; e++) {
        bool along = STREET_EDGES[e][0] == v->from && STREET_EDGES[e][1] == v->to;
        if (!along && !(STREET_EDGES[e][1] == v->from && STREET_EDGES[e][0] == v->to)) continue;
        *right = streetRoom[e][along ? 0 : 1];
        *left = streetRoom[e][along ? 1 : 0];
        return;
    }
}

// At a crossing: on along another street, seldom back the way it came. A dead end (a gate, the
// quay, the end of a lane) is a place to stand a while before turning back.
static void NextStreet(Villager *v)
{
    Vector3 at = VillagerLanePos(v);
    int around[8], n = Neighbours(v->to, around), ways[8], m = 0;
    for (int k = 0; k < n; k++)
        if (around[k] != v->from) ways[m++] = around[k];
    int next = m > 0 && GetRandomValue(0, 9) > 0 ? ways[GetRandomValue(0, m - 1)] : v->from;
    SetStreet(v, v->to, next);
    // It goes on from where it stands, measured along and across the new street, so turning a
    // corner never moves it in one jump.
    float dx = v->b.x - v->a.x, dz = v->b.z - v->a.z, len = fmaxf(sqrtf(dx * dx + dz * dz), 0.1f);
    float ox = at.x - v->a.x, oz = at.z - v->a.z;
    v->t = (ox * dx + oz * dz) / (len * len);
    v->side = (ox * dz - oz * dx) / len;
    if (m == 0) v->wait = RandF(4.0f, 12.0f);
    else if (GetRandomValue(0, 3) == 0) v->wait = RandF(2.0f, 7.0f);
}

static void AddWalker(int from, int to, float speed, Color robe)
{
    int i = AddNpc(NPC_CIVILIAN, StreetNode(from), StreetNode(to), speed, robe, 0);
    if (i >= 0) SetStreet(&G.villagers[i], from, to);
}

static void AddVillager(float ax, float az, float bx, float bz, float speed, Color robe)
{
    AddNpc(NPC_CIVILIAN, (Vector3){ ax, 0, az }, (Vector3){ bx, 0, bz }, speed, robe, 0);
}

static void BuildTempleStaff(void)
{
    const Color guard = { 150, 128, 96, 255 }, cleric = { 238, 234, 224, 255 };
    const float cz = ZIG_Z;

    // The gate guards carry composite bows as well as spears: two at the temple yard's gate and two just
    // inside each of the city's gates (south, north, east, west, and on the quays of the harbor's water gate).
    static const float gates[][2] = {
        { -7.2f, -4.2f }, { 7.2f, -4.2f }, { -4.8f, 70.5f }, { 4.8f, 70.5f }, { -39.5f, -79.5f }, { -33.5f, -79.5f },
        { 91.0f, 4.8f }, { 91.0f, 13.2f }, { -85.0f, 21.8f }, { -85.0f, 30.2f }, { -94.0f, -17.5f }, { -94.0f, -2.5f },
    };
    for (int k = 0; k < (int)(sizeof(gates) / sizeof(gates[0])); k++) {
        int g = AddNpc(NPC_GUARD, (Vector3){ gates[k][0], 0, gates[k][1] }, (Vector3){ gates[k][0], 0, gates[k][1] }, 0, guard, 0);
        if (g >= 0) G.villagers[g].bow = 2;
    }
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

// Turns toward a heading over a moment instead of all at once.
static float TurnToward(float face, float want, float k)
{
    return face + atan2f(sinf(want - face), cosf(want - face)) * fminf(1.0f, k);
}

// Walks the lane or the streets. Someone ahead: it steps aside to get past, and if the way stays
// blocked it turns back. On a road or a street it keeps to the right, so people pass each other,
// and on a street it keeps within the room the street leaves between its walls.
static void WalkVillager(Villager *v, int i, float dt)
{
    const int self = OWNER_VILLAGER + i;
    bool street = v->from >= 0;
    float len = LaneLength(v), right = 0.0f, left = 0.0f;
    Villager next = *v;

    v->moving = false;
    if (v->speed <= 0 || v->vit.override) return;
    if (v->wait > 0) {
        v->wait -= dt;
        return;
    }
    if (street) RoomOf(v, &right, &left);
    next.t += v->dir * v->speed * dt / len;
    if (street && next.t >= 1.0f) {
        NextStreet(v);
        return;
    }
    if (!street && next.t > 1) { next.t = 1; next.dir = -1; }
    if (!street && next.t < 0) { next.t = 0; next.dir = 1; }

    Vector3 cur = VillagerPos(v), to = VillagerPos(&next);
    if (BodyAhead(cur, to, 0.35f, self)) {
        // A step to one side, or else to the other.
        float lo = street ? -right : v->road >= 0 ? -2.2f : -1.3f, hi = street ? left : -lo;
        float way = street ? -1.0f : -v->dir;
        for (int k = 0; k < 2; k++, way = -way) {
            Villager aside = *v;
            aside.side = Clamp(v->side + way * 1.4f * dt, lo, hi);
            Vector3 sp = VillagerPos(&aside);
            if (aside.side == v->side || BodyAt(sp, 0.35f, self) || (!street && SolidAt(sp.x, sp.z, 0.3f))) continue;
            v->side = aside.side;
            break;
        }
        v->stuck += dt;
        if (v->stuck > 2.5f) {
            // Blocked too long: turn round and go another way.
            v->stuck = 0;
            if (street) {
                SetStreet(v, v->to, v->from);
                v->t = 1.0f - v->t;
                v->side = -v->side;
            } else {
                v->dir = -v->dir;
            }
        }
        return;
    }
    v->stuck = 0;
    *v = next;
    v->moving = true;
    if (to.x != cur.x || to.z != cur.z) v->face = TurnToward(v->face, atan2f(to.x - cur.x, to.z - cur.z), dt * 8.0f);
    // On a road or a street the way back is to the right-hand side of the direction of travel, as
    // near the middle as a narrow street needs. Out of a street's room (just round a corner) it
    // steps back in briskly.
    float home = v->road >= 0 ? -1.0f * v->dir : street ? -fminf(1.0f, right) : 0.0f;
    if (v->side != home) {
        Villager back = *v;
        float step = (street && (v->side < -right || v->side > left) ? 1.5f : 0.6f) * dt;
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

    // Townsfolk wander the streets; one sweeps the temple's inner court, one its outer court.
    static const struct { int from, to; float speed; Color robe; } walkers[] = {
        { 1, 2, 1.1f, { 200, 190, 160, 255 } }, { 2, 1, 1.3f, { 150, 110, 80, 255 } },  { 4, 1, 1.2f, { 180, 170, 140, 255 } },
        { 7, 8, 1.0f, { 160, 120, 90, 255 } },  { 16, 17, 0.9f, { 170, 150, 110, 255 } }, { 12, 13, 1.0f, { 190, 175, 150, 255 } },
        { 14, 15, 1.0f, { 168, 148, 118, 255 } }, { 5, 18, 0.8f, { 120, 140, 150, 255 } }, { 10, 11, 1.1f, { 176, 160, 120, 255 } },
        { 3, 2, 1.0f, { 186, 150, 120, 255 } }, { 9, 8, 1.15f, { 140, 120, 100, 255 } }, { 6, 5, 0.95f, { 210, 196, 170, 255 } },
    };
    G.villagerCount = 0;
    roomMeasured = false;
    for (int k = 0; k < COUNT(walkers); k++) AddWalker(walkers[k].from, walkers[k].to, walkers[k].speed, walkers[k].robe);
    AddVillager(-3, ZIG_Z + 2, 3, ZIG_Z + 2, 0.7f, (Color){ 230, 225, 210, 255 });
    AddVillager(-14, -9, 14, -9, 0.6f, (Color){ 236, 232, 222, 255 });
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

// Townsfolk walk where the host says; the gate guards step aside for whoever on this machine made
// an offering, and the livestock wander on every machine on their own.
void UpdateVillage(float dt)
{
    for (int i = 0; i < G.villagerCount; i++) {
        Villager *v = &G.villagers[i];
        if (v->templePost) {
            float s = v->a.x < 0 ? -1.0f : 1.0f;
            Vector3 post = G.templeAccess ? (Vector3){ 1.75f * s, 7, -24.4f } : (Vector3){ 0.55f * s, 7, -25.9f };
            v->a = v->b = post;
        }
        if (!IsClient()) WalkVillager(v, i, dt);
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
        Look look = PersonLook(LOOK_MERCHANT, 300u + (unsigned)i, tr->robe);
        Vector3 tp = TraderPos(tr);
        if (XZDist(tp, G.cam.position) > 220.0f) continue;
        LightFor(tp);
        if (tr->vit.dead) DrawPersonBody(tp, tr->vit.face, look);
        else DrawHuman(tp, tr->vit.override ? tr->vit.face : tr->facing, tr->vit.override ? (float)GetTime() * 8.0f : 0.0f, look, false, false, 0);
    }
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        Vector3 pos = VillagerPos(v);
        // The legs swing with the time walked, so a new stretch of street does not jolt them.
        float stride = v->moving ? (float)GetTime() * v->speed * 3.0f + (float)i : 0.0f;
        float face = v->vit.override ? v->vit.face : v->face;
        if (XZDist(pos, G.cam.position) > 200.0f) continue;
        if (v->vit.override && v->vit.hostile > 0) stride = (float)GetTime() * 8.0f;
        // An archer stands still while it draws.
        float pull = v->vit.drawing ? Clamp(1.0f - v->vit.windup / BOW_DRAW_TIME, 0.0f, 1.0f) : 0.0f;
        if (v->vit.drawing) stride = 0;
        LightFor(pos);
        static const int roles[] = { [NPC_CIVILIAN] = LOOK_TOWN, [NPC_GUARD] = LOOK_GUARD, [NPC_CLERIC] = LOOK_PRIEST,
                                     [NPC_VILLAGER] = LOOK_VILLAGER, [NPC_TRAVELLER] = LOOK_TRAVELLER, [NPC_WARDEN] = LOOK_WARDEN };
        Look look = PersonLook(roles[v->kind], (unsigned)i, v->robe);
        if (v->vit.dead) {
            DrawPersonBody(pos, face, look);
            continue;
        }
        bool torch = G.night >= 0.3f && (v->kind == NPC_TRAVELLER || v->kind == NPC_WARDEN || v->kind == NPC_GUARD);
        if (torch) GripAt(HAND_OTHER, -0.36f, 1.36f, 0.28f);
        if (v->kind == NPC_CIVILIAN || v->kind == NPC_VILLAGER || v->kind == NPC_TRAVELLER || v->kind == NPC_CLERIC) {
            // A villager standing still at the hunters' butt keeps drawing a bow.
            bool archer = v->kind == NPC_VILLAGER && v->speed <= 0 && !v->vit.override;
            float pull = archer ? fmaxf(0.0f, sinf((float)GetTime() * 0.9f + (float)i)) : 0.0f;
            if (v->kind == NPC_TRAVELLER) {
                DrawTravelGear(pos, face, v->robe);
                GripAt(HAND_WEAPON, 0.36f, 1.12f, 0.24f);
            }
            DrawHuman(pos, face, stride, look, false, archer, pull);
        } else if (v->kind == NPC_WARDEN) {
            // A village guard: a leather jerkin and cap, and a bow always in hand.
            DrawHuman(pos, face, stride, look, false, true, pull);
        } else {
            // A city guard: a bronze helmet and studded cloak, a spear, and a shield by day.
            rlPushMatrix();
            rlTranslatef(pos.x, pos.y, pos.z);
            rlRotatef(face * RAD2DEG, 0, 1, 0);
            if (v->vit.drawing) {
                // The spear slung on the back while the bow is drawn.
                Part(0.1f, 1.3f, -0.25f, 0.05f, 2.4f, 0.05f, COL_WOOD);
            } else if (v->vit.swing > 0 || v->vit.windup > 0) {
                float z = SpearThrust(v->vit.windup, v->vit.swing), y = v->vit.windup > 0.1f ? 1.45f : 1.35f;
                GripHere(HAND_WEAPON, 0.3f, y, z - 0.2f);
                Part(0.3f, y, z, 0.05f, 0.05f, 2.4f, COL_WOOD);
                Part(0.3f, y, z + 1.3f, 0.08f, 0.08f, 0.26f, COL_BRONZE);
            } else {
                GripHere(HAND_WEAPON, 0.36f, 1.18f, 0.12f);
                Part(0.36f, 1.2f, 0.12f, 0.05f, 2.4f, 0.05f, COL_WOOD);
                Part(0.36f, 2.5f, 0.12f, 0.08f, 0.26f, 0.08f, COL_BRONZE);
            }
            if (G.night < 0.3f && !v->vit.drawing) {
                GripHere(HAND_OTHER, -0.31f, 1.2f, 0.14f);
                Part(-0.38f, 1.2f, 0.12f, 0.08f, 0.62f, 0.5f, (Color){ 130, 90, 56, 255 });
            }
            rlPopMatrix();
            DrawHuman(pos, face, stride, look, false, v->vit.drawing, pull);
        }
        if (torch) DrawHandTorch(pos, face);
    }
    G.drawTorch = G.drawFog = 0;
}
