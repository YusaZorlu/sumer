#include "game.h"
#include <math.h>

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

// The hunters butcher game better than your oven and deal in pelts and arrows.
static const Offer HUNTER[] = {
    { IT_GAZELLE, 1, IT_NONE,  0, OUT_ITEM,   IT_ROAST,  4, 0, 0 },
    { IT_BOAR,    1, IT_NONE,  0, OUT_ITEM,   IT_ROAST,  7, 0, 0 },
    { IT_DATES,  12, IT_NONE,  0, OUT_ITEM,   IT_ROAST,  2, 0, 0 },
    { IT_BREAD,   4, IT_NONE,  0, OUT_ITEM,   IT_WOLF,   1, 0, 0 },
    { IT_WOLF,    2, IT_NONE,  0, OUT_ITEM,   IT_ARROWS, 20, 0, 0 },
    { IT_HERB,    4, IT_NONE,  0, OUT_ITEM,   IT_ARROWS, 10, 0, 0 },
    { IT_GOOSE,   1, IT_REEDS, 4, OUT_ITEM,   IT_ARROWS, 12, 0, 0 },
    { IT_PARTRIDGE, 1, IT_NONE, 0, OUT_ITEM,  IT_ARROWS, 4, 0, 0 },
    { IT_WOLF,    2, IT_REEDS, 6, OUT_QUIVER, 0, 1, 0, 0 },
    { IT_ONAGER,  1, IT_WOLF,  2, OUT_BOW,    2, 1, 0, 0 },
};

// The only fleeces and sheep for sale anywhere; plant waste is fodder here.
static const Offer SHEPHERD[] = {
    { IT_WHEAT, 6, IT_NONE, 0, OUT_ITEM, IT_SHEEPSKIN, 1, 0, 0 },
    { IT_EGG,   4, IT_NONE, 0, OUT_ITEM, IT_SHEEPSKIN, 1, 0, 0 },
    { IT_HERB,  3, IT_NONE, 0, OUT_ITEM, IT_SHEEPSKIN, 1, 0, 0 },
    { IT_WOLF,  1, IT_NONE, 0, OUT_ITEM, IT_SHEEPSKIN, 2, 0, 0 },
    { IT_BREAD, 3, IT_NONE, 0, OUT_ITEM, IT_SHEEP,     1, 0, 0 },
    { IT_FISH,  6, IT_NONE, 0, OUT_ITEM, IT_SHEEP,     1, 0, 0 },
    { IT_WASTE, 2, IT_NONE, 0, OUT_ITEM, IT_MILK,      1, 0, 0 },
};

// The fishers sell the only fine net, and deal in fish.
static const Offer FISHER[] = {
    { IT_REEDS,  8, IT_COPPER, 2, OUT_TOOL, TOOL_NET,   1, 0, 0 },
    { IT_DATES,  4, IT_NONE,   0, OUT_ITEM, IT_FISH,    3, 0, 0 },
    { IT_FISH,   3, IT_WOOD,   1, OUT_ITEM, IT_GRILLED, 3, 0, 0 },
    { IT_FISH,   5, IT_NONE,   0, OUT_ITEM, IT_ARROWS,  8, 0, 0 },
    { IT_FISH,   4, IT_NONE,   0, OUT_ITEM, IT_HERB,    2, 0, 0 },
    { IT_FISH,   8, IT_NONE,   0, OUT_ITEM, IT_BRONZE,  1, 0, 0 },
};

static const char *const NAMES[VILLAGES] = { "Hunters' village", "Herders' village", "Fishers' village" };
static const float CENTERS[VILLAGES][2] = { { HUNTERS_X, HUNTERS_Z }, { HERDERS_X, HERDERS_Z }, { FISHERS_X, FISHERS_Z } };

Vector3 VillageCenter(int v)
{
    v = v < 0 || v >= VILLAGES ? 0 : v;
    float x = CENTERS[v][0], z = CENTERS[v][1];
    return (Vector3){ x, TerrainHeight(x, z), z };
}

// The village a place belongs to (within 45 m of its middle), or -1.
int VillageAt(Vector3 p)
{
    for (int v = 0; v < VILLAGES; v++) {
        float dx = p.x - CENTERS[v][0], dz = p.z - CENTERS[v][1];
        if (dx * dx + dz * dz < 45.0f * 45.0f) return v;
    }
    return -1;
}

const char *VillageName(int v)
{
    return L(NAMES[v < 0 || v >= VILLAGES ? 0 : v]);
}

// Each village keeps a guard with a bow, who shoots at anyone the village has turned on.
static void Warden(float x, float z, float face, Color robe)
{
    int i = AddNpc(NPC_WARDEN, (Vector3){ x, TerrainHeight(x, z), z }, (Vector3){ x, TerrainHeight(x, z), z }, 0, robe, face);
    if (i >= 0) G.villagers[i].bow = 1;
}

// The sheepfold, where the herders' flock wanders.
Rectangle FoldArea(void)
{
    return (Rectangle){ HERDERS_X + 3.0f, HERDERS_Z - 6.0f, 14.0f, 12.0f };
}

static float Ground(float x, float z)
{
    return TerrainHeight(x, z);
}

static void Post(float x, float z, float h, Color c)
{
    AddShape(x, Ground(x, z) - 0.1f, z, 0.14f, h + 0.1f, 0.14f, c, true, SHAPE_CYL);
}

// A wattle fence that follows the slope, one short panel at a time.
static void FenceOn(float x0, float z0, float x1, float z1)
{
    const Color wattle = { 150, 130, 82, 255 };
    float len = Vector2Distance((Vector2){ x0, z0 }, (Vector2){ x1, z1 });
    int n = (int)ceilf(len / 2.0f);

    for (int k = 0; k < n; k++) {
        float ta = (float)k / (float)n, tb = (float)(k + 1) / (float)n;
        float ax = Lerp(x0, x1, ta), az = Lerp(z0, z1, ta), bx = Lerp(x0, x1, tb), bz = Lerp(z0, z1, tb);
        float w = fmaxf(fabsf(bx - ax), 0.12f), d = fmaxf(fabsf(bz - az), 0.12f);
        AddBoxOn((ax + bx) / 2, (az + bz) / 2, w, 0.9f, d, wattle, true);
        AddShape(ax, Ground(ax, az) - 0.1f, az, 0.17f, 1.25f, 0.17f, COL_WOOD, false, SHAPE_CYL);
    }
    AddShape(x1, Ground(x1, z1) - 0.1f, z1, 0.17f, 1.25f, 0.17f, COL_WOOD, false, SHAPE_CYL);
}

static void FirePit(float x, float z)
{
    float y = Ground(x, z);
    for (int k = 0; k < 8; k++) {
        float a = (float)k * 2.0f * PI / 8.0f;
        AddShape(x + cosf(a) * 0.85f, y - 0.05f, z + sinf(a) * 0.85f, 0.36f, 0.3f, 0.32f, COL_STONE, false, SHAPE_BLOB);
    }
    AddBox(x, y, z, 1.0f, 0.12f, 0.2f, COL_WOOD, false);
    AddBox(x, y, z, 0.2f, 0.12f, 1.0f, COL_WOOD, false);
    AddGlow(x, y + 0.12f, z, 0.55f, 0.35f, 0.55f, COL_FIRE);
    AddGlow(x, y + 0.3f, z, 0.3f, 0.45f, 0.3f, (Color){ 255, 214, 120, 255 });
    AddFireLight(x, y + 0.5f, z);
}

// A conical hide tent with its door toward (fx, fz).
static void HideTent(float x, float z, float r, Color hide, float fx, float fz)
{
    float y = Ground(x, z);
    Vector2 to = Vector2Normalize((Vector2){ fx - x, fz - z });

    AddShape(x, y - 0.2f, z, 2.0f * r, 3.4f, 2.0f * r, hide, true, SHAPE_CONE);
    AddShape(x, y + 0.9f, z, 2.0f * r * 0.74f, 0.12f, 2.0f * r * 0.74f, Mix(hide, COL_MUD_DARK, 0.4f), false, SHAPE_CYL);
    for (int k = 0; k < 3; k++) {
        float a = (float)k * 2.1f;
        AddShape(x + cosf(a) * 0.14f, y + 2.8f, z + sinf(a) * 0.14f, 0.08f, 1.0f, 0.08f, COL_WOOD, false, SHAPE_CYL);
    }
    AddBox(x + to.x * r * 0.62f, y - 0.05f, z + to.y * r * 0.62f, 0.9f, 1.45f, 0.9f, COL_DOORWAY, false);
}

static void DryingRack(float x, float z, bool alongX)
{
    static const Color hides[3] = { { 128, 122, 112, 255 }, { 150, 108, 70, 255 }, { 192, 142, 84, 255 } };
    float y = Ground(x, z), dx = alongX ? 1.5f : 0.0f, dz = alongX ? 0.0f : 1.5f;

    Post(x - dx, z - dz, 2.2f, COL_WOOD);
    Post(x + dx, z + dz, 2.2f, COL_WOOD);
    AddBox(x, y + 2.0f, z, alongX ? 3.2f : 0.1f, 0.1f, alongX ? 0.1f : 3.2f, COL_WOOD, false);
    for (int k = 0; k < 3; k++) {
        float o = ((float)k - 1.0f) * 0.95f;
        AddBox(x + (alongX ? o : 0.0f), y + 1.05f, z + (alongX ? 0.0f : o), alongX ? 0.8f : 0.05f, 0.95f, alongX ? 0.05f : 0.8f, hides[k], false);
    }
}

// Strips of meat over a smouldering bed.
static void SmokeRack(float x, float z)
{
    float y = Ground(x, z);
    for (int k = 0; k < 4; k++) Post(x + ((k & 1) ? 1.1f : -1.1f), z + ((k & 2) ? 0.6f : -0.6f), 1.2f, COL_WOOD);
    AddBox(x, y + 1.1f, z - 0.6f, 2.4f, 0.08f, 0.08f, COL_WOOD, false);
    AddBox(x, y + 1.1f, z + 0.6f, 2.4f, 0.08f, 0.08f, COL_WOOD, false);
    for (int k = 0; k < 5; k++) AddBox(x - 0.9f + 0.45f * (float)k, y + 0.85f, z, 0.14f, 0.45f, 1.2f, (Color){ 140, 62, 44, 255 }, false);
    AddGlow(x, y + 0.02f, z, 1.6f, 0.08f, 0.8f, (Color){ 200, 80, 30, 255 });
}

// A straw butt with painted rings, facing east.
static void ArcheryButt(float x, float z)
{
    float y = Ground(x, z);
    AddBoxOn(x, z, 0.5f, 1.5f, 1.4f, COL_STRAW, true);
    AddBox(x + 0.26f, y + 0.45f, z, 0.03f, 0.9f, 0.9f, COL_LINEN, false);
    AddBox(x + 0.28f, y + 0.65f, z, 0.03f, 0.5f, 0.5f, COL_BLANKET, false);
    AddBox(x + 0.3f, y + 0.8f, z, 0.03f, 0.2f, 0.2f, COL_LINEN, false);
}

static void Woodpile(float x, float z)
{
    float y = Ground(x, z);
    for (int k = 0; k < 3; k++) AddBox(x, y + 0.22f * (float)k, z + 0.1f * (float)k, 2.0f - 0.3f * (float)k, 0.22f, 0.9f, COL_PALM_TRUNK, true);
}

// A beehive bread oven of clay (a tannur), warm at its mouth.
static void Tannur(float x, float z)
{
    float y = Ground(x, z);
    AddShape(x, y - 0.1f, z, 1.1f, 1.2f, 1.1f, COL_CLAY, true, SHAPE_BLOB);
    AddGlow(x + 0.45f, y + 0.2f, z, 0.08f, 0.3f, 0.35f, COL_FIRE);
}

// A domed granary of mud brick on a low base, its hatch up top.
static void Granary(float x, float z)
{
    float y = Ground(x, z);
    AddShape(x, y - 0.1f, z, 2.0f, 0.5f, 2.0f, COL_MUD_DARK, true, SHAPE_CYL);
    AddShape(x, y + 0.3f, z, 1.8f, 3.0f, 1.8f, COL_MUD_PALE, true, SHAPE_BLOB);
    AddShape(x, y + 1.4f, z, 1.5f, 1.9f, 1.5f, COL_MUD_PALE, false, SHAPE_CONE);
    AddBox(x, y + 3.2f, z, 0.4f, 0.12f, 0.4f, COL_WOOD, false);
}

// A low mud wall round a yard, open where the gap is.
static void YardWall(float x0, float z0, float x1, float z1, Color c)
{
    AddBoxOn((x0 + x1) / 2, (z0 + z1) / 2, fmaxf(fabsf(x1 - x0), 0.35f), 1.1f, fmaxf(fabsf(z1 - z0), 0.35f), c, true);
}

// A woven reed screen against wind and eyes.
static void ReedScreen(float x0, float z0, float x1, float z1)
{
    const Color reed = { 188, 170, 108, 255 };
    AddBoxOn((x0 + x1) / 2, (z0 + z1) / 2, fmaxf(fabsf(x1 - x0), 0.1f), 1.7f, fmaxf(fabsf(z1 - z0), 0.1f), reed, true);
}

// A net hung to dry between two poles.
static void NetPoles(float x, float z, bool alongX)
{
    float y = Ground(x, z), dx = alongX ? 1.6f : 0.0f, dz = alongX ? 0.0f : 1.6f;
    Post(x - dx, z - dz, 2.1f, COL_WOOD);
    Post(x + dx, z + dz, 2.1f, COL_WOOD);
    AddBox(x, y + 1.25f, z, alongX ? 3.1f : 0.03f, 1.6f, alongX ? 0.03f : 3.1f, (Color){ 110, 100, 80, 255 }, false);
}

// A game animal hung by its hind legs from a tripod to be skinned.
static void GameTripod(float x, float z)
{
    float y = Ground(x, z);
    for (int k = 0; k < 3; k++) {
        float a = (float)k * 2.094f;
        AddShape(x + cosf(a) * 0.7f, y - 0.1f, z + sinf(a) * 0.7f, 0.1f, 2.6f, 0.1f, COL_WOOD, false, SHAPE_CYL);
    }
    AddShape(x, y + 1.0f, z, 0.45f, 1.1f, 0.4f, (Color){ 178, 132, 84, 255 }, false, SHAPE_BLOB);
    AddShape(x, y + 0.6f, z, 0.2f, 0.35f, 0.3f, (Color){ 160, 118, 76, 255 }, false, SHAPE_BLOB);
}

// Baskets and jars stood about a doorway.
static void Clutter(float x, float z, int n, int seed)
{
    for (int k = 0; k < n; k++) {
        float a = Hash2(seed, k) * 6.28f, r = 0.4f + Hash2(k, seed) * 0.8f;
        float px = x + cosf(a) * r, pz = z + sinf(a) * r;
        if (k & 1) AddShape(px, Ground(px, pz) - 0.05f, pz, 0.45f, 0.55f, 0.45f, COL_POT, true, SHAPE_BLOB);
        else AddShape(px, Ground(px, pz), pz, 0.55f, 0.4f, 0.55f, COL_STRAW, true, SHAPE_CYL);
    }
}

// Someone at work in one place: grinding grain, mending nets, minding a fire.
static void AtWork(float x, float z, float face, Color robe, bool crouch)
{
    int i = AddNpc(NPC_VILLAGER, (Vector3){ x, Ground(x, z), z }, (Vector3){ x, Ground(x, z), z }, 0, robe, face);
    if (i >= 0) G.villagers[i].crouch = crouch;
}

static void BuildHunters(void)
{
    const float cx = HUNTERS_X, cz = HUNTERS_Z;
    static const float tents[5][3] = { { -10, -9, 2.0f }, { 1, -14, 2.2f }, { 12, -8, 1.9f }, { 12, 9, 2.1f }, { 0, 14, 2.0f } };
    static const Color hide[2] = { { 150, 116, 80, 255 }, { 120, 92, 64, 255 } };

    FirePit(cx, cz);
    for (int k = 0; k < 5; k++) HideTent(cx + tents[k][0], cz + tents[k][1], tents[k][2], hide[k & 1], cx, cz);
    HideTent(cx - 15, cz + 4, 1.7f, hide[0], cx, cz);
    GameTripod(cx + 9, cz - 1);
    ReedScreen(cx + 15, cz - 6, cx + 15, cz + 6);
    for (int k = 0; k < 5; k++) {
        float a = (float)k * 1.26f + 0.3f, r = 2.2f;
        AddBoxOn(cx + cosf(a) * r, cz + sinf(a) * r, 0.7f, 0.32f, 0.35f, COL_PALM_TRUNK, true);
    }
    Clutter(cx + tents[1][0] + 1.5f, cz + tents[1][1] + 2.5f, 3, 11);
    Clutter(cx + tents[3][0] - 2.2f, cz + tents[3][1] - 1.0f, 3, 12);
    DryingRack(cx - 7, cz + 8, true);
    DryingRack(cx + 6, cz + 3, false);
    SmokeRack(cx + 5, cz - 5);
    Woodpile(cx + 16, cz + 1);
    ArcheryButt(cx - 22, cz - 12);
    for (int k = 0; k < 3; k++) {
        float x = cx - 13 + 1.1f * (float)k, z = cz + 13 - 0.4f * (float)k;
        AddShape(x, Ground(x, z) - 0.05f, z, 0.5f, 0.55f, 0.5f, COL_POT, true, SHAPE_BLOB);
    }

    // The hunter's stall faces the road: a bench of pelts and a pole hung with antlers.
    float sx = cx - 7, sz = cz - 2, sy = Ground(sx, sz);
    AddBoxOn(sx, sz, 0.9f, 0.75f, 2.2f, COL_WOOD, true);
    AddBox(sx, sy + 0.75f, sz - 0.5f, 0.8f, 0.08f, 0.9f, (Color){ 128, 122, 112, 255 }, false);
    AddBox(sx, sy + 0.75f, sz + 0.5f, 0.7f, 0.1f, 0.8f, (Color){ 150, 108, 70, 255 }, false);
    Post(sx, sz + 1.7f, 2.0f, COL_WOOD);
    AddBox(sx, sy + 2.0f, sz + 1.7f, 0.06f, 0.06f, 0.9f, COL_LINEN, false);
    AddBox(sx, sy + 2.2f, sz + 1.7f, 0.7f, 0.4f, 0.06f, COL_LINEN, false);

    G.traders[TR_HUNTER] = (Trader){ "Hunter", "Pelts, meat and arrows. Bring me game and I'll treat you fairly.",
                                     { sx + 1.1f, Ground(sx + 1.1f, sz), sz }, -PI / 2, { 112, 84, 52, 255 }, HUNTER, COUNT(HUNTER), false, false, { .hp = 0 } };

    AddTorch(cx - 15, Ground(cx - 15, cz + 7), cz + 7, true);
    AddTorch(cx + 7, Ground(cx + 7, cz - 15), cz - 15, true);
    AddTorch(cx + 6, Ground(cx + 6, cz + 15), cz + 15, true);
    KeepOut(cx - 24, cz - 18, cx + 19, cz + 18);

    const Color tan = { 150, 116, 76, 255 }, green = { 96, 104, 62, 255 };
    AddNpc(NPC_VILLAGER, (Vector3){ cx - 4, Ground(cx - 4, cz + 5), cz + 5 }, (Vector3){ cx + 9, Ground(cx + 9, cz + 5), cz + 5 }, 0.6f, tan, 0);
    AddNpc(NPC_VILLAGER, (Vector3){ cx - 3, Ground(cx - 3, cz - 8), cz - 8 }, (Vector3){ cx - 3, Ground(cx - 3, cz + 10), cz + 10 }, 0.5f, green, 0);
    float ax = cx - 13, az = cz - 12;
    int archer = AddNpc(NPC_VILLAGER, (Vector3){ ax, Ground(ax, az), az }, (Vector3){ ax, Ground(ax, az), az }, 0, green, -PI / 2);
    if (archer >= 0) G.villagers[archer].bow = 1;   // the one at the butt shoots; the others work with their hands
    Warden(cx - 12, cz + 3, -PI / 2, (Color){ 104, 82, 56, 255 });
    AtWork(cx + 1.6f, cz + 1.4f, -2.3f, tan, true);
    AtWork(cx + 8.2f, cz - 1.6f, -1.0f, green, false);
}

// A low mud-brick house with a flat roof and its door toward (fx, fz).
static void MudHouse(float x, float z, float w, float d, Color c, float fx, float fz)
{
    float y = Ground(x, z), h = 2.7f;
    AddBoxOn(x, z, w, h, d, c, true);
    AddBox(x, y + h - 0.05f, z, w + 0.2f, 0.2f, d + 0.2f, Mix(c, COL_MUD_DARK, 0.35f), false);
    bool side = fabsf(fx - x) > fabsf(fz - z);
    float dx = side ? (fx > x ? w / 2 : -w / 2) : 0.0f, dz = side ? 0.0f : (fz > z ? d / 2 : -d / 2);
    AddBox(x + dx, y - 0.1f, z + dz, side ? 0.12f : 1.0f, 2.0f, side ? 1.0f : 0.12f, COL_DOORWAY, false);
    AddBox(x + dx * 1.02f, y + 1.9f, z + dz * 1.02f, side ? 0.18f : 1.3f, 0.16f, side ? 1.3f : 0.18f, COL_WOOD, false);
}

static void BuildHerders(void)
{
    const float cx = HERDERS_X, cz = HERDERS_Z;
    Rectangle f = FoldArea();
    const Color wool = { 236, 230, 214, 255 };

    MudHouse(cx - 13, cz - 7, 5.0f, 4.4f, COL_HOUSE, cx, cz);
    MudHouse(cx - 14, cz + 6, 4.6f, 5.0f, COL_MUD_PALE, cx, cz);
    MudHouse(cx - 3, cz + 12, 5.2f, 4.2f, COL_HOUSE_DARK, cx, cz);
    MudHouse(cx + 7, cz - 12, 4.4f, 4.4f, COL_WALL, cx, cz);
    // A walled yard behind the first house, with the bread oven and the granary.
    YardWall(cx - 19, cz - 11, cx - 19, cz - 2, COL_MUD_PALE);
    YardWall(cx - 19, cz - 11, cx - 15.5f, cz - 11, COL_MUD_PALE);
    Tannur(cx - 17, cz - 3.5f);
    Granary(cx - 6, cz - 13);
    Granary(cx - 2, cz - 14.5f);
    Clutter(cx - 11.5f, cz - 3.5f, 4, 21);
    Clutter(cx - 3, cz + 9.5f, 3, 22);
    // Dung cakes drying on the yard wall, the fuel of a treeless plain.
    for (int k = 0; k < 4; k++) AddBox(cx - 19.2f, Ground(cx - 19.2f, cz - 9 + 1.6f * (float)k) + 0.75f, cz - 9 + 1.6f * (float)k, 0.06f, 0.35f, 0.35f, (Color){ 96, 78, 56, 255 }, false);

    // The fold, open on the side toward the houses.
    FenceOn(f.x, f.y, f.x + f.width, f.y);
    FenceOn(f.x, f.y + f.height, f.x + f.width, f.y + f.height);
    FenceOn(f.x + f.width, f.y, f.x + f.width, f.y + f.height);
    FenceOn(f.x, f.y, f.x, f.y + 4.5f);
    FenceOn(f.x, f.y + f.height - 4.5f, f.x, f.y + f.height);
    float tx = f.x + f.width - 2.0f, tz = f.y + 1.6f;
    AddBoxOn(tx, tz, 2.4f, 0.4f, 0.8f, COL_WOOD, true);
    AddBox(tx, Ground(tx, tz) + 0.4f, tz, 2.0f, 0.02f, 0.5f, COL_WATER, false);
    float hx = f.x + f.width - 2.2f, hz = f.y + f.height - 2.2f;
    AddShape(hx, Ground(hx, hz) - 0.3f, hz, 2.2f, 1.6f, 2.0f, COL_STRAW, false, SHAPE_BLOB);

    // Bales of wool and a shearing bench by the gate.
    for (int k = 0; k < 4; k++) {
        float x = cx - 1.5f + 0.9f * (float)(k % 2), z = cz + 3.0f + 0.9f * (float)(k / 2);
        AddBoxOn(x, z, 0.8f, 0.6f, 0.8f, wool, true);
    }
    AddBoxOn(cx - 1, cz - 4, 1.8f, 0.5f, 0.7f, COL_WOOD, true);
    FirePit(cx - 7, cz);

    G.traders[TR_SHEPHERD] = (Trader){ "Shepherd", "The only fleeces this side of the hills. Wolves are the bane of my flock.",
                                       { cx + 0.5f, Ground(cx + 0.5f, cz - 3), cz - 3 }, PI, { 168, 150, 110, 255 }, SHEPHERD, COUNT(SHEPHERD), false, false, { .hp = 0 } };

    AddTorch(cx - 9, Ground(cx - 9, cz - 10), cz - 10, true);
    AddTorch(cx - 9, Ground(cx - 9, cz + 10), cz + 10, true);
    AddTorch(f.x - 0.8f, Ground(f.x - 0.8f, cz + 2), cz + 2, true);
    KeepOut(cx - 18, cz - 16, f.x + f.width + 2, cz + 16);

    for (int i = 0; i < MAX_FLOCK; i++) {
        float x = RandF(f.x + 1, f.x + f.width - 1), z = RandF(f.y + 1, f.y + f.height - 1);
        G.flock[i] = (Wanderer){ { x, Ground(x, z), z }, RandF(-PI, PI), RandF(1, 3), 0 };
    }

    const Color brown = { 140, 110, 80, 255 }, pale = { 200, 186, 150, 255 };
    AddNpc(NPC_VILLAGER, (Vector3){ f.x - 1.5f, Ground(f.x - 1.5f, f.y), f.y }, (Vector3){ f.x - 1.5f, Ground(f.x - 1.5f, f.y + f.height), f.y + f.height }, 0.5f, brown, 0);
    AddNpc(NPC_VILLAGER, (Vector3){ cx - 10, Ground(cx - 10, cz - 3), cz - 3 }, (Vector3){ cx - 10, Ground(cx - 10, cz + 9), cz + 9 }, 0.45f, pale, 0);
    Warden(cx + 2, cz - 9, PI, (Color){ 120, 96, 70, 255 });
    AtWork(cx - 15.6f, cz - 3.5f, PI / 2, pale, true);
    AtWork(cx - 0.4f, cz + 4.1f, PI, brown, true);
}

// A reed house in the marsh style: a long barrel vault of bundled reeds, its door toward (fx, fz).
static void ReedHouse(float x, float z, float len, bool alongX)
{
    const Color reed = { 196, 176, 112, 255 }, dark = { 150, 132, 80, 255 };
    float y = Ground(x, z), w = 3.6f;
    float sx = alongX ? len : w, sz = alongX ? w : len;
    AddBox(x, y, z, sx, 1.4f, sz, reed, true);
    AddShape(x, y + 1.0f, z, sx * 0.98f, 1.7f, sz * 0.98f, reed, true, SHAPE_BLOB);
    for (int k = -1; k <= 1; k += 2) {
        float ex = alongX ? x + (len / 2) * (float)k : x, ez = alongX ? z : z + (len / 2) * (float)k;
        AddShape(ex, y - 0.1f, ez, 0.36f, 2.9f, 0.36f, dark, true, SHAPE_CYL);
    }
    if (alongX) AddBox(x + len / 2 + 0.02f, y, z, 0.06f, 1.5f, 1.0f, COL_DOORWAY, false);
    else AddBox(x, y, z + len / 2 + 0.02f, 1.0f, 1.5f, 0.06f, COL_DOORWAY, false);
}

// Fish split and hung to dry in the sun.
static void FishRack(float x, float z)
{
    const Color fish = { 170, 176, 168, 255 };
    float y = Ground(x, z);
    Post(x - 1.4f, z, 1.6f, COL_WOOD);
    Post(x + 1.4f, z, 1.6f, COL_WOOD);
    AddBox(x, y + 1.5f, z, 3.0f, 0.06f, 0.06f, COL_WOOD, false);
    for (int k = 0; k < 6; k++) AddBox(x - 1.1f + 0.44f * (float)k, y + 1.15f, z, 0.16f, 0.6f, 0.05f, fish, false);
}

static void BeachedBoat(float x, float z)
{
    float y = Ground(x, z);
    AddBox(x, y + 0.1f, z, 1.0f, 0.36f, 3.6f, COL_STRAW, false);
    AddBox(x, y + 0.3f, z - 1.75f, 0.7f, 0.5f, 0.35f, COL_STRAW, false);
    AddBox(x, y + 0.3f, z + 1.75f, 0.7f, 0.5f, 0.35f, COL_STRAW, false);
}

// Across the river from the city, beside the old landing stage: reed houses, fish drying, boats on the
// bank, and the river full of fish below.
static void BuildFishers(void)
{
    const float cx = FISHERS_X, cz = FISHERS_Z;
    const Color blue = { 70, 96, 120, 255 }, sand = { 186, 168, 128, 255 };

    ReedHouse(cx - 10, cz - 10, 7.0f, true);
    ReedHouse(cx - 11, cz + 9, 6.0f, true);
    ReedHouse(cx + 1, cz - 15, 6.5f, false);
    ReedHouse(cx + 2, cz + 15, 6.0f, false);
    ReedHouse(cx - 2, cz - 25, 5.0f, false);
    ReedScreen(cx - 15, cz - 4, cx - 15, cz + 4);
    NetPoles(cx + 13, cz - 14, false);
    NetPoles(cx + 13, cz + 14, false);
    // A pile of cut reeds, the marsh's timber.
    for (int k = 0; k < 3; k++) AddBoxOn(cx - 7 + 0.3f * (float)k, cz + 15 + 0.2f * (float)k, 0.9f, 0.3f + 0.25f * (float)k, 3.2f, COL_REED, true);
    Clutter(cx - 6, cz - 6.5f, 3, 31);
    FirePit(cx - 1, cz);
    FishRack(cx + 7, cz - 6);
    FishRack(cx + 7, cz + 6);
    FishRack(cx - 6, cz - 1);
    for (int k = 0; k < 3; k++) BeachedBoat(cx + 14 + 1.6f * (float)k, cz - 24 + 0.6f * (float)k);
    for (int k = 0; k < 3; k++) {
        float x = cx - 4 + 1.0f * (float)k, z = cz + 6.5f;
        AddShape(x, Ground(x, z) - 0.05f, z, 0.5f, 0.55f, 0.5f, COL_POT, true, SHAPE_BLOB);
    }

    // The fisher's stall faces the road: baskets of fish and a net hung on poles.
    float sx = cx + 12, sz = cz + 4, sy = Ground(sx, sz);
    AddBoxOn(sx, sz, 0.9f, 0.75f, 2.2f, COL_WOOD, true);
    AddBox(sx, sy + 0.8f, sz - 0.5f, 0.6f, 0.2f, 0.6f, COL_REED, false);
    AddBox(sx, sy + 0.8f, sz + 0.5f, 0.5f, 0.15f, 0.7f, (Color){ 170, 176, 168, 255 }, false);
    Post(sx - 0.2f, sz - 1.8f, 2.0f, COL_WOOD);
    Post(sx - 0.2f, sz + 1.8f, 2.0f, COL_WOOD);
    AddBox(sx - 0.2f, sy + 1.4f, sz, 0.04f, 1.0f, 3.4f, (Color){ 110, 100, 80, 255 }, false);
    G.traders[TR_FISHER] = (Trader){ "Fisher", "The river feeds us. A fine net catches twice the fish, and I have the only ones.",
                                     { sx + 1.1f, Ground(sx + 1.1f, sz), sz }, PI / 2, blue, FISHER, COUNT(FISHER), false, false, { .hp = 0 } };

    // Fish to catch in the river below the village, upstream and downstream of the landing.
    static const float spots[4] = { -86, -76, -46, -38 };
    for (int k = 0; k < 4; k++) AddResource(-147.5f, spots[k], RES_FISH);

    AddTorch(cx + 10, Ground(cx + 10, cz - 9), cz - 9, true);
    AddTorch(cx + 10, Ground(cx + 10, cz + 10), cz + 10, true);
    AddTorch(cx - 15, Ground(cx - 15, cz), cz, true);
    KeepOut(cx - 16, cz - 20, cx + 16, cz + 20);

    AddNpc(NPC_VILLAGER, (Vector3){ cx - 4, Ground(cx - 4, cz - 5), cz - 5 }, (Vector3){ cx + 9, Ground(cx + 9, cz - 5), cz - 5 }, 0.5f, blue, 0);
    AddNpc(NPC_VILLAGER, (Vector3){ cx + 5, Ground(cx + 5, cz + 10), cz + 10 }, (Vector3){ cx + 5, Ground(cx + 5, cz - 10), cz - 10 }, 0.45f, sand, 0);
    Warden(cx + 11, cz - 2, PI / 2, (Color){ 88, 100, 110, 255 });
    AtWork(cx + 12.4f, cz - 12.4f, PI / 2, sand, true);
    AtWork(cx - 2.6f, cz + 1.2f, 2.0f, blue, true);
}

// Travellers walk whole roads back and forth, day and night, carrying torches after dark.
static void Traveller(int road, float t, float dir, float speed, Color robe)
{
    int i = AddNpc(NPC_TRAVELLER, (Vector3){ 0 }, (Vector3){ 0 }, speed, robe, 0);
    if (i < 0) return;
    G.villagers[i].road = road;
    G.villagers[i].t = t;
    G.villagers[i].dir = dir;
    G.villagers[i].side = -dir;
}

void BuildVillages(void)
{
    BuildHunters();
    BuildHerders();
    BuildFishers();
    Traveller(ROAD_EAST, 0.2f, 1.0f, 1.2f, (Color){ 150, 120, 84, 255 });
    Traveller(ROAD_EAST, 0.75f, -1.0f, 1.0f, (Color){ 120, 96, 120, 255 });
    Traveller(ROAD_HERDERS, 0.5f, 1.0f, 1.1f, (Color){ 176, 160, 120, 255 });
    Traveller(ROAD_SOUTH, 0.35f, 1.0f, 1.15f, (Color){ 110, 120, 140, 255 });
    Traveller(ROAD_NORTH, 0.3f, 1.0f, 1.0f, (Color){ 160, 110, 80, 255 });
}
