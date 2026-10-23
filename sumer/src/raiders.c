#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Four raiders to a camp; the barbarians' war camps have a fifth, a bowman by the lookout.
#define RAIDER_CAMP 4
#define WAR_CAMP    5

static const float HOMES[WAR_CAMP][2] = { { -4.5f, 2.5f }, { 3.5f, 3.0f }, { 5.0f, -3.5f }, { -2.5f, -5.0f }, { 5.0f, -9.5f } };
// Raiders south and north of the city; barbarians, twice as strong, north-west and south-west across the river.
static const float CAMP_POS[CAMPS][2] = { { 34, 210 }, { 30, -240 }, { -360, -230 }, { -380, 250 } };
static const int STASHES[CAMPS] = { STN_STASH, STN_STASH2, STN_STASH3, STN_STASH4 };

// Two bands of bandits roam the eastern hills, two swordsmen and an archer each, far from the city, the
// villages and the camps and away from the edge of the world. They walk from one spot to another at
// random and now and then sit down a while, never on a road. They come after the camps in G.raiders.
#define BAND       3
#define BAND_FIRST (EAST_CAMPS * RAIDER_CAMP + (CAMPS - EAST_CAMPS) * WAR_CAMP)
static const Rectangle BAND_ZONES[BANDS] = { { 200, -480, 280, 340 }, { 180, 150, 300, 330 } };
// Where each band is headed, how long it still sits where it is, how long it has got nowhere.
static Vector3 bandGoal[BANDS];
static float bandRest[BANDS], bandStuck[BANDS];
// Where the leader has walked, a metre apart, newest first: a way round anything the followers meet.
#define TRAIL 16
static Vector3 bandTrail[BANDS][TRAIL];
static int bandTrailN[BANDS];

Vector3 CampCenter(int camp)
{
    float x = CAMP_POS[camp][0], z = CAMP_POS[camp][1];
    return (Vector3){ x, TerrainHeight(x, z), z };
}

int StashCamp(int station)
{
    for (int c = 0; c < CAMPS; c++)
        if (STASHES[c] == station) return c;
    return -1;
}

static bool West(int camp)
{
    return camp >= EAST_CAMPS && camp < CAMPS;
}

static int CampSize(int camp)
{
    return West(camp) ? WAR_CAMP : RAIDER_CAMP;
}

static int CampFirst(int camp)
{
    return camp <= EAST_CAMPS ? camp * RAIDER_CAMP : EAST_CAMPS * RAIDER_CAMP + (camp - EAST_CAMPS) * WAR_CAMP;
}

bool RaiderBandit(int i)
{
    return i >= BAND_FIRST && i < MAX_RAIDERS;
}

bool RaiderBowman(int i)
{
    if (i < 0 || i >= MAX_RAIDERS) return false;
    if (RaiderBandit(i)) return (i - BAND_FIRST) % BAND == BAND - 1;
    int camp = G.raiders[i].camp;
    return West(camp) && i == CampFirst(camp) + WAR_CAMP - 1;
}

static float Ground(float x, float z)
{
    return TerrainHeight(x, z);
}

static void Tent(float x, float z, float w, float d, Color c)
{
    float y = Ground(x, z);
    AddShape(x, y - 0.1f, z, w, 2.6f, d, c, true, SHAPE_CONE);
    AddShape(x, y + 2.3f, z, 0.12f, 0.7f, 0.12f, COL_WOOD, false, SHAPE_CYL);
    AddShape(x + 0.12f, y + 2.3f, z - 0.05f, 0.1f, 0.6f, 0.1f, COL_WOOD, false, SHAPE_CYL);
}

// The barbarians' chests hold about twice as much as the raiders'.
static void Restock(int camp)
{
    static const int pool[][3] = {
        { IT_BRONZE, 1, 3 }, { IT_TIN, 1, 2 }, { IT_COPPER, 3, 6 }, { IT_ARROWS, 6, 10 },
        { IT_WOLF, 1, 2 }, { IT_BREAD, 2, 4 }, { IT_COWHIDE, 1, 2 }, { IT_WOOD, 4, 8 },
        { IT_ROAST, 2, 3 }, { IT_HERB, 3, 5 },
    };
    int kinds = West(camp) ? 10 : 8, more = West(camp) ? 2 : 1;
    for (int k = 0; k < 4; k++) {
        int pick = (int)(Hash2(G.day * 5 + k, 77 + camp) * ((float)kinds - 0.01f));
        if (k == 0) pick = Hash2(G.day, 3 + camp) < 0.5f ? 0 : 1;
        int n = pool[pick][1] + (int)(Hash2(G.day, k + 11 + camp) * (float)(pool[pick][2] - pool[pick][1] + 1) * 0.999f);
        n *= more;
        if (n > ITEMS[pool[pick][0]].stack) n = ITEMS[pool[pick][0]].stack;
        G.stash[camp][k] = (Slot){ pool[pick][0], n };
    }
}

// The barbarians' camps are bigger: two more tents, a lookout platform, a war banner and skulls on poles.
static void BuildWarCamp(float cx, float cz)
{
    const Color dark = { 84, 60, 46, 255 }, banner = { 150, 40, 34, 255 }, bone = { 226, 220, 200, 255 };

    Tent(cx - 7.0f, cz + 7.5f, 3.0f, 3.2f, dark);
    Tent(cx + 7.5f, cz + 6.5f, 3.2f, 3.0f, dark);

    float lx = cx + 7.5f, lz = cz - 6.5f, ly = Ground(lx, lz);
    for (int k = 0; k < 4; k++)
        AddShape(lx + ((k & 1) ? 1.0f : -1.0f), Ground(lx, lz) - 0.1f, lz + ((k & 2) ? 1.0f : -1.0f), 0.18f, 3.3f, 0.18f, COL_PALM_TRUNK, true, SHAPE_CYL);
    AddBox(lx, ly + 3.1f, lz, 2.5f, 0.15f, 2.5f, COL_WOOD, false);
    AddBox(lx, ly + 3.25f, lz - 1.2f, 2.5f, 0.7f, 0.1f, COL_WOOD, false);
    AddBox(lx, ly + 3.25f, lz + 1.2f, 2.5f, 0.7f, 0.1f, COL_WOOD, false);
    AddBox(lx - 1.2f, ly + 3.25f, lz, 0.1f, 0.7f, 2.5f, COL_WOOD, false);
    AddBox(lx + 1.2f, ly + 3.25f, lz, 0.1f, 0.7f, 2.5f, COL_WOOD, false);

    float bx = cx - 2.5f, bz = cz + 2.0f, by = Ground(bx, bz);
    AddShape(bx, by - 0.1f, bz, 0.12f, 4.6f, 0.12f, COL_WOOD, true, SHAPE_CYL);
    AddBox(bx + 0.55f, by + 3.6f, bz, 1.0f, 0.9f, 0.05f, banner, false);
    AddBox(bx + 0.55f, by + 3.65f, bz, 0.4f, 0.4f, 0.07f, COL_LINEN, false);

    for (int s = -1; s <= 1; s += 2)
        for (int e = -1; e <= 1; e += 2) {
            float px = cx + 1.6f * (float)s, pz = cz + 11.6f * (float)e;
            AddShape(px, Ground(px, pz) - 0.1f, pz, 0.1f, 2.1f, 0.1f, COL_WOOD, true, SHAPE_CYL);
            AddShape(px, Ground(px, pz) + 2.05f, pz, 0.32f, 0.36f, 0.34f, bone, false, SHAPE_BLOB);
        }
}

static void BuildCamp(int camp)
{
    const float cx = CAMP_POS[camp][0], cz = CAMP_POS[camp][1];
    const Color hide = { 128, 98, 66, 255 }, dark = { 96, 74, 52, 255 };

    Tent(cx - 6.5f, cz - 1.0f, 3.0f, 3.6f, hide);
    Tent(cx + 6.0f, cz - 2.0f, 3.0f, 3.2f, dark);
    Tent(cx + 0.5f, cz + 7.0f, 3.6f, 3.0f, hide);

    float y = Ground(cx, cz);
    for (int k = 0; k < 7; k++) {
        float a = (float)k * 2.0f * PI / 7.0f;
        AddShape(cx + cosf(a) * 0.75f, y - 0.05f, cz + sinf(a) * 0.75f, 0.34f, 0.28f, 0.3f, COL_STONE, false, SHAPE_BLOB);
    }
    AddBox(cx, y, cz, 0.9f, 0.12f, 0.2f, COL_WOOD, false);
    AddBox(cx, y, cz, 0.2f, 0.12f, 0.9f, COL_WOOD, false);
    AddGlow(cx, y + 0.12f, cz, 0.55f, 0.35f, 0.55f, COL_FIRE);
    AddGlow(cx, y + 0.3f, cz, 0.3f, 0.4f, 0.3f, (Color){ 255, 214, 120, 255 });

    for (int k = 0; k < 4; k++) {
        float a = (float)k * PI / 2 + 0.4f;
        AddBox(cx + cosf(a) * 2.4f, Ground(cx + cosf(a) * 2.4f, cz + sinf(a) * 2.4f), cz + sinf(a) * 2.4f, 0.9f, 0.4f, 0.4f, COL_PALM_TRUNK, false);
    }

    float sx = cx + 2.4f, sz = cz - 7.0f, sy = Ground(sx, sz);
    G.stations[STASHES[camp]] = (Vector3){ sx, sy, sz + 1.1f };
    AddBox(sx, sy, sz, 1.4f, 0.7f, 0.9f, COL_WOOD, true);
    AddBox(sx, sy + 0.7f, sz, 1.5f, 0.12f, 1.0f, COL_PALM_TRUNK, false);
    AddBox(sx, sy + 0.25f, sz + 0.46f, 0.2f, 0.2f, 0.04f, COL_BRONZE, false);

    float rx = cx - 3.0f, rz = cz - 6.5f, ry = Ground(rx, rz);
    AddBox(rx - 1.0f, ry, rz, 0.1f, 1.8f, 0.1f, COL_WOOD, false);
    AddBox(rx + 1.0f, ry, rz, 0.1f, 1.8f, 0.1f, COL_WOOD, false);
    AddBox(rx, ry + 1.7f, rz, 2.2f, 0.08f, 0.08f, COL_WOOD, false);
    AddBox(rx - 0.4f, ry + 0.7f, rz, 0.7f, 1.0f, 0.04f, (Color){ 128, 122, 112, 255 }, false);
    AddBox(rx + 0.45f, ry + 0.8f, rz, 0.6f, 0.9f, 0.04f, (Color){ 130, 92, 60, 255 }, false);

    for (int k = 0; k < 10; k++) {
        float a = (float)k * 2.0f * PI / 10.0f + 0.3f;
        if (k == 2 || k == 7) continue;
        float px = cx + cosf(a) * 11.0f, pz = cz + sinf(a) * 11.0f;
        AddShape(px, Ground(px, pz) - 0.1f, pz, 0.24f, 1.5f + 0.3f * Hash2(k, 4), 0.24f, COL_PALM_TRUNK, true, SHAPE_CYL);
        AddShape(px, Ground(px, pz) + 1.35f + 0.3f * Hash2(k, 4), pz, 0.24f, 0.3f, 0.24f, COL_PALM_TRUNK, false, SHAPE_CONE);
    }
    if (West(camp)) BuildWarCamp(cx, cz);
    KeepOut(cx - 12, cz - 12, cx + 12, cz + 12);

    for (int k = 0; k < CampSize(camp); k++) {
        Raider *r = &G.raiders[CampFirst(camp) + k];
        r->camp = camp;
        r->home = (Vector3){ cx + HOMES[k][0], 0, cz + HOMES[k][1] };
        r->home.y = Ground(r->home.x, r->home.z);
        r->pos = r->home;
        r->face = RandF(-PI, PI);
        r->timer = RandF(1, 5);
        r->speed = 1.1f;
        r->respawnDay = 0;
        r->moving = false;
        r->vit = (Vitals){ .used = false };
    }
}

// Somewhere a band may go: well away from the city, the villages, the camps and the world's edge, on
// dry open ground off the roads.
static bool BandSpot(float x, float z)
{
    if (fabsf(x) > WORLD_LIMIT - 70 || fabsf(z) > WORLD_LIMIT - 70 || hypotf(x, z - CITY_Z) < 220.0f) return false;
    for (int v = 0; v < VILLAGES; v++)
        if (XZDist((Vector3){ x, 0, z }, VillageCenter(v)) < 120.0f) return false;
    for (int c = 0; c < CAMPS; c++)
        if (hypotf(x - CAMP_POS[c][0], z - CAMP_POS[c][1]) < 70.0f) return false;
    return !InWater(x, z) && !SolidAt(x, z, 1.5f) && RoadDistance(x, z) > 14.0f;
}

// The band's next stop, 25 to 150 m on in its hills. While the world is built the pick comes from the
// seed, not the shared random numbers, so the rest of the world comes out as before.
static Vector3 BandPick(int b, Vector3 from, bool build)
{
    Rectangle z = BAND_ZONES[b];
    for (int t = 0; t < 80; t++) {
        float u = build ? Hash2(t, 901 + b) : RandF(0, 1), w = build ? Hash2(t, 911 + b) : RandF(0, 1);
        float x = z.x + u * z.width, y = z.y + w * z.height, d = hypotf(x - from.x, y - from.z);
        if ((build || (d > 25.0f && d < 150.0f)) && BandSpot(x, y)) return (Vector3){ x, Ground(x, y), y };
    }
    float x = z.x + z.width / 2, y = z.y + z.height / 2;
    return (Vector3){ x, Ground(x, y), y };
}

void BuildRaiders(void)
{
    for (int c = 0; c < CAMPS; c++) BuildCamp(c);
    G.stashDay = 0;
    for (int b = 0; b < BANDS; b++) {
        Vector3 at = BandPick(b, (Vector3){ 0, 0, 0 }, true);
        bandGoal[b] = at;
        bandRest[b] = 10.0f;
        bandStuck[b] = 0;
        bandTrailN[b] = 0;
        for (int k = 0; k < BAND; k++) {
            Raider *r = &G.raiders[BAND_FIRST + b * BAND + k];
            *r = (Raider){ .camp = CAMPS + b, .speed = 1.3f, .face = (float)k * 2.1f };
            r->home = (Vector3){ at.x + (float)(k - 1) * 1.6f, 0, at.z + (k == 1 ? 1.4f : 0.0f) };
            r->home.y = Ground(r->home.x, r->home.z);
            r->pos = r->home;
        }
    }
}

static bool Free(Vector3 at, float x, float z, int i)
{
    return !InWater(x, z) && !SolidAt(x, z, 0.35f) && !BodyAt((Vector3){ x, at.y, z }, 0.38f, OWNER_RAIDER + i);
}

// A step toward 'to', round a tree or a rock by ever wider side steps, all to the one side it keeps to
// first, so it does not slide back and forth along a wide rock; false if every way is blocked.
static signed char sideOf[MAX_RAIDERS];

static bool Walk(Raider *r, int i, Vector3 to, float speed, float dt)
{
    static const float tries[9] = { 0.0f, 0.6f, 1.2f, 1.8f, 2.4f, -0.6f, -1.2f, -1.8f, -2.4f };
    float dx = to.x - r->pos.x, dz = to.z - r->pos.z, d = sqrtf(dx * dx + dz * dz);
    if (d < 0.05f) return true;
    if (!sideOf[i]) sideOf[i] = (i & 1) ? 1 : -1;
    float base = atan2f(dx, dz), step = fminf(speed * dt, d);
    for (int k = 0; k < 9; k++) {
        float a = base + tries[k] * (float)sideOf[i];
        float nx = r->pos.x + sinf(a) * step, nz = r->pos.z + cosf(a) * step;
        if (!Free(r->pos, nx, nz, i)) continue;
        r->pos = (Vector3){ nx, Ground(nx, nz), nz };
        r->face = a;
        return true;
    }
    return false;
}

// The first of the band still on its feet leads; the others keep a step behind on either side. While
// one of them fights the rest stand by, and they all sit down where they stopped when they rest.
static void Roam(int b, float dt)
{
    Raider *lead = NULL;
    int li = -1;
    bool fighting = false;
    for (int k = 0; k < BAND; k++) {
        Raider *r = &G.raiders[BAND_FIRST + b * BAND + k];
        if (r->vit.dead) continue;
        fighting = fighting || r->vit.override;
        if (!lead) {
            lead = r;
            li = BAND_FIRST + b * BAND + k;
        }
    }
    if (!lead) return;
    for (int k = 0; k < BAND; k++) {
        Raider *r = &G.raiders[BAND_FIRST + b * BAND + k];
        r->home = lead->pos;   // the fallen come back where the band is
        r->moving = false;
    }
    if (fighting) return;
    if (bandRest[b] > 0) {
        bandRest[b] -= dt;
        return;
    }
    if (XZDist(lead->pos, bandGoal[b]) < 1.5f || bandStuck[b] > 5.0f) {
        if (bandStuck[b] > 5.0f) sideOf[li] = (signed char)-sideOf[li];
        if (bandStuck[b] <= 5.0f && RandF(0, 1) < (G.night > 0.5f ? 0.8f : 0.5f)) bandRest[b] = RandF(20.0f, 75.0f);
        bandStuck[b] = 0;
        bandGoal[b] = BandPick(b, lead->pos, false);
        return;
    }
    Vector3 was = lead->pos;
    lead->moving = true;
    Walk(lead, li, bandGoal[b], lead->speed, dt);
    if (XZDist(was, lead->pos) < lead->speed * dt * 0.3f) bandStuck[b] += dt;
    else bandStuck[b] = fmaxf(0.0f, bandStuck[b] - dt);
    if (bandTrailN[b] == 0 || XZDist(lead->pos, bandTrail[b][0]) > 1.0f) {
        memmove(&bandTrail[b][1], &bandTrail[b][0], sizeof(Vector3) * (TRAIL - 1));
        bandTrail[b][0] = lead->pos;
        if (bandTrailN[b] < TRAIL) bandTrailN[b]++;
    }

    float fx = sinf(lead->face), fz = cosf(lead->face), side = -1.0f;
    for (int k = 0; k < BAND; k++) {
        int i = BAND_FIRST + b * BAND + k;
        Raider *r = &G.raiders[i];
        if (r == lead || r->vit.dead) continue;
        Vector3 at = { lead->pos.x - fx * 1.8f + fz * 1.4f * side, 0, lead->pos.z - fz * 1.8f - fx * 1.4f * side };
        side = -side;
        float d = XZDist(r->pos, at);
        r->moving = true;
        if (d > 3.0f && bandTrailN[b] > 1) {
            // Fallen behind: along the leader's own way, from the nearest point of it to the next.
            int k = 0;
            for (int t = 1; t < bandTrailN[b]; t++)
                if (XZDist(r->pos, bandTrail[b][t]) < XZDist(r->pos, bandTrail[b][k])) k = t;
            Walk(r, i, bandTrail[b][k > 0 ? k - 1 : 0], lead->speed * 1.6f, dt);
        } else if (d > 0.3f) {
            Walk(r, i, at, lead->speed * 1.15f, dt);
        } else {
            r->face = lead->face;
        }
        // Left behind a while: round the other way. Far behind with no one about to see: back in its place.
        r->timer = d > 4.0f ? r->timer + dt : 0.0f;
        if (r->timer > 2.0f) {
            sideOf[i] = (signed char)-sideOf[i];
            r->timer = 0;
        }
        if (d > 20.0f && NearestAnyone(r->pos) > 100.0f && Free(r->pos, at.x, at.z, i)) r->pos = (Vector3){ at.x, Ground(at.x, at.z), at.z };
    }
}

static void Idle(Raider *r, int i, float dt)
{
    bool night = G.night > 0.5f;
    float cx = CAMP_POS[r->camp][0], cz = CAMP_POS[r->camp][1];
    Vector3 seat = { cx + cosf((float)i * PI / 2 + 0.4f) * 2.4f, 0, cz + sinf((float)i * PI / 2 + 0.4f) * 2.4f };

    r->timer -= dt;
    if (night) {
        Vector3 to = Vector3Subtract(seat, r->pos);
        to.y = 0;
        float d = Vector3Length(to);
        r->moving = d > 0.6f;
        if (r->moving) {
            r->face = atan2f(to.x, to.z);
            r->pos.x += to.x / d * r->speed * dt;
            r->pos.z += to.z / d * r->speed * dt;
        } else {
            r->face = atan2f(cx - r->pos.x, cz - r->pos.z);
        }
    } else {
        if (r->timer <= 0) {
            r->timer = RandF(2.5f, 7.0f);
            r->moving = GetRandomValue(0, 2) > 0;
            r->face = XZDist(r->pos, r->home) > 6.0f ? atan2f(r->home.x - r->pos.x, r->home.z - r->pos.z) : RandF(-PI, PI);
        }
        if (r->moving) {
            float nx = r->pos.x + sinf(r->face) * r->speed * dt, nz = r->pos.z + cosf(r->face) * r->speed * dt;
            if (SolidAt(nx, nz, 0.35f) || BodyAt((Vector3){ nx, r->pos.y, nz }, 0.38f, OWNER_RAIDER + i)) {
                r->face += PI * 0.7f;
            } else {
                r->pos.x = nx;
                r->pos.z = nz;
            }
        }
    }
    r->pos.y = Ground(r->pos.x, r->pos.z);
}

void UpdateRaiders(float dt)
{
    if (G.hour >= 6.0f && G.stashDay != G.day) {
        G.stashDay = G.day;
        for (int c = 0; c < CAMPS; c++) Restock(c);
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        Raider *r = &G.raiders[i];
        if (r->vit.dead) {
            if (r->respawnDay == 0) r->respawnDay = (float)G.day;
            bool far = NearestAnyone(r->home) > 70.0f;
            if ((float)G.day > r->respawnDay && G.hour >= 6.0f && far) {
                r->vit = (Vitals){ .used = false };
                r->pos = r->home;
                r->respawnDay = 0;
            }
            continue;
        }
        if (!r->vit.override && !RaiderBandit(i)) Idle(r, i, dt);
    }
    for (int b = 0; b < BANDS; b++) Roam(b, dt);
}

void DrawRaiders(void)
{
    float time = (float)GetTime();

    for (int i = 0; i < MAX_RAIDERS; i++) {
        const Raider *r = &G.raiders[i];
        Vector3 p = r->vit.override ? r->vit.at : r->pos;
        float f = r->vit.override ? r->vit.face : r->face;
        if (XZDist(p, G.cam.position) > 220.0f || !InView(p, 2.0f)) continue;

        // Barbarians from the west wear dark red, wolf pelts and horned helmets and swing a bronze axe; the
        // raiders from the east, leather, a stone-grey cap and a stone axe.
        // Bandits: dark cloth and leather, a bronze sickle-sword or a bow.
        bool west = West(r->camp), bandit = RaiderBandit(i);
        Color cloth = bandit ? (Color){ 54, 50, 58, 255 } : west ? (Color){ 112, 40, 34, 255 } : (Color){ 96, 72, 48, 255 };
        Look look = PersonLook(west ? LOOK_BARBARIAN : LOOK_RAIDER, 800u + (unsigned)i, cloth);
        look.trim = bandit ? (Color){ 92, 64, 44, 255 } : (i & 1) ? (Color){ 150, 102, 70, 255 } : (Color){ 58, 44, 38, 255 };
        LightFor(p);
        if (r->vit.dead) {
            DrawPersonBody(p, f, look);
            continue;
        }
        bool sitting = !r->vit.override && !r->moving && (G.night > 0.5f || bandit);
        float stride = (r->vit.override || r->moving) && !r->vit.drawing ? time * (r->vit.override ? 8.0f : 4.0f) : 0.0f;
        if (RaiderBowman(i)) {
            // The bowman: a war bow in hand, no axe.
            float pull = r->vit.drawing ? Clamp(1.0f - r->vit.windup / BOW_DRAW_TIME, 0.0f, 1.0f) : 0.0f;
            if (!r->vit.drawing) HoldBow(p, f, sitting);
            DrawHuman(p, f, stride, look, sitting, r->vit.drawing, pull);
            continue;
        }
        float angle = BlowAngle(r->vit.windup, r->vit.swing, r->vit.override ? -40.0f : 30.0f);
        rlPushMatrix();
        rlTranslatef(p.x, p.y + (sitting ? 1.04f : 1.42f), p.z);
        rlRotatef(f * RAD2DEG, 0, 1, 0);
        rlMultMatrixf(MatrixToFloat(BlowFrame(angle)));
        GripHere(HAND_WEAPON, 0, 0, 0.04f);
        if (bandit) {
            DrawShadedBox((Vector3){ 0, 0, 0.12f }, (Vector3){ 0.05f, 0.05f, 0.26f }, COL_WOOD);
            DrawShadedBox((Vector3){ 0, 0, 0.52f }, (Vector3){ 0.03f, 0.07f, 0.56f }, COL_BRONZE);
            DrawShadedBox((Vector3){ 0, 0.08f, 0.82f }, (Vector3){ 0.03f, 0.18f, 0.12f }, COL_BRONZE);
        } else if (west) {
            DrawShadedBox((Vector3){ 0, 0, 0.5f }, (Vector3){ 0.06f, 0.06f, 1.0f }, COL_WOOD);
            DrawShadedBox((Vector3){ 0, 0.14f, 0.92f }, (Vector3){ 0.04f, 0.42f, 0.28f }, COL_BRONZE);
        } else {
            DrawShadedBox((Vector3){ 0, 0, 0.42f }, (Vector3){ 0.05f, 0.05f, 0.85f }, COL_WOOD);
            DrawShadedBox((Vector3){ 0, 0.1f, 0.8f }, (Vector3){ 0.04f, 0.3f, 0.2f }, COL_STONE);
        }
        rlPopMatrix();
        DrawHuman(p, f, stride, look, sitting, false, 0);
    }
    G.drawTorch = G.drawFog = 0;
}

static int StashItems(int camp)
{
    int n = 0;
    for (int k = 0; k < 4; k++) n += G.stash[camp][k].count;
    return n;
}

void StashPrompt(int camp, char *buf, int size, bool *ok)
{
    int n = StashItems(camp);
    *ok = n > 0;
    if (*ok) Fmt(buf, size, "[E] Take the raiders' loot (%d things)", n);
    else Fmt(buf, size, "The raiders' chest is empty. They bring back more at dawn.");
}

// Asks for as much of each of the chest's heaps as fits in this player's quiver and baskets.
void TakeStash(int camp)
{
    int args[5] = { camp, 0, 0, 0, 0 }, quiver = G.quiverMax - G.quiver;
    Slot bag[BAG_MAX];
    bool any = false;

    memcpy(bag, G.bag, sizeof(bag));
    for (int k = 0; k < 4; k++) {
        const Slot *s = &G.stash[camp][k];
        if (s->item == IT_NONE) continue;
        int n = s->count, q = 0;
        if (s->item == IT_ARROWS) {
            q = n < quiver ? n : quiver;
            quiver -= q;
        }
        args[1 + k] = q + SlotsAdd(bag, G.bagSlots, s->item, n - q);
        any = any || args[1 + k] > 0;
    }
    if (any) Act(RQ_STASH, args, 5);
    else if (StashItems(camp) > 0) Say("No room in your baskets.");
}

// The chest's half: what was asked for, as far as it still holds it. res: item and count for each
// heap, then how many things are left.
bool ClaimStash(int camp, const int *want, int *res, Vector3 by)
{
    int got = 0;
    if (camp < 0 || camp >= CAMPS || XZDist(G.stations[STASHES[camp]], by) > 8.0f) return false;
    for (int k = 0; k < 4; k++) {
        Slot *s = &G.stash[camp][k];
        int n = want[k] < s->count ? want[k] : s->count;
        if (n < 0) n = 0;
        res[k * 2] = s->item;
        res[k * 2 + 1] = n;
        s->count -= n;
        if (s->count <= 0) *s = (Slot){ IT_NONE, 0 };
        got += n;
    }
    res[8] = StashItems(camp);
    return got > 0;
}

void StashApply(bool ok, const int *res)
{
    char log[256] = "";

    if (!ok) {
        Say("Someone else emptied the chest first.");
        return;
    }
    for (int k = 0; k < 4; k++) {
        int item = res[k * 2], n = res[k * 2 + 1];
        if (item <= IT_NONE || item >= IT_COUNT || n <= 0) continue;
        int got = item == IT_ARROWS ? GiveArrows(n) : BagAdd(item, n);
        if (got > 0) snprintf(log + strlen(log), sizeof(log) - strlen(log), "%s%d %s", log[0] ? ", " : "", got, ItemName(item));
    }
    if (!log[0]) Say("No room in your baskets.");
    else Say(res[8] > 0 ? "Took %s. Your baskets are full." : "Took %s.", log);
}
