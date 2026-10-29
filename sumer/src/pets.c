// Cats and dogs. A few cats keep to the city's streets and the harbor, and more round the fishers' village;
// dogs live in the villages, and two lie by the temple gate. They wander near their homes on every machine
// on their own, like the livestock (nothing of it is sent), step aside for people, are solid to the player
// and come to no harm. A cat takes a fish, a dog roast meat: fed, it follows whoever fed it for a while,
// then goes home. Who is followed is the host's to say (RQ_PET, and the snapshot), so every screen agrees.
// Not in a battle royale or a party: no guards and no townsfolk's animals there, so no frames spent on them.
#include "game.h"
#include "rlgl.h"
#include <math.h>

#define FOLLOW_TIME  150.0f   // seconds a fed animal keeps to its friend
#define EAT_TIME     3.0f
#define LEASH        60.0f    // a follower this far from home gives up and goes back
#define PET_RADIUS   0.28f

enum { PET_CAT, PET_DOG };

typedef struct {
    float x, z, range;
    unsigned char kind, coat;
} PetHome;

typedef struct {
    Vector3 pos, home;
    float heading, timer, speed, range, step, eatT, fedT, sit;
    signed char friend;
    unsigned char kind, coat;
    Vitals walk;   // the way found round what is in between, going after a friend or home
} Pet;

// Cats: city streets (the harbor quay, the alehouse, the west and east streets, the main street), then the
// fishers' village. Dogs: the hunters' salukis, the herders' big flock dogs, the fishers' dog, the temple's two.
static const PetHome HOMES[] = {
    { -56.5f, -16.0f, 5, PET_CAT, 0 }, { -62.0f, 24.0f, 5, PET_CAT, 1 }, { -45.0f, 41.5f, 5, PET_CAT, 2 },
    { 37.0f, 11.5f, 5, PET_CAT, 3 }, { 2.5f, 36.0f, 5, PET_CAT, 4 },
    { FISHERS_X + 5, FISHERS_Z + 4, 6, PET_CAT, 3 }, { FISHERS_X - 4, FISHERS_Z + 6, 6, PET_CAT, 0 },
    { FISHERS_X + 2, FISHERS_Z - 5, 6, PET_CAT, 5 },
    { HUNTERS_X + 4, HUNTERS_Z + 3, 7, PET_DOG, 0 }, { HUNTERS_X - 5, HUNTERS_Z - 2, 7, PET_DOG, 0 },
    { HERDERS_X + 1, HERDERS_Z + 9, 7, PET_DOG, 1 }, { HERDERS_X - 4, HERDERS_Z - 3, 7, PET_DOG, 1 },
    { FISHERS_X + 6, FISHERS_Z - 4, 6, PET_DOG, 2 },
    { -5.0f, -6.5f, 2.5f, PET_DOG, 1 }, { 5.0f, -6.5f, 2.5f, PET_DOG, 1 },
};
#define HOME_COUNT ((int)(sizeof(HOMES) / sizeof(HOMES[0])))

static Pet pets[HOME_COUNT];
static int petCount;
static unsigned seed = 12345u;

// The animals' own random numbers, so the world's stay as they were.
static float Rand(float lo, float hi)
{
    seed = seed * 1664525u + 1013904223u;
    return lo + (hi - lo) * (float)(seed >> 8) / 16777216.0f;
}

bool PetsOn(void)
{
    return !Match();
}

int PetCount(void)
{
    return petCount;
}

Vector3 PetPos(int i)
{
    return i >= 0 && i < petCount ? pets[i].pos : (Vector3){ 0 };
}

Vector3 PetHomeAt(int i, float *range)
{
    if (range) *range = i >= 0 && i < petCount ? pets[i].range : 0;
    return i >= 0 && i < petCount ? pets[i].home : (Vector3){ 0 };
}

bool PetIsDog(int i)
{
    return i >= 0 && i < petCount && pets[i].kind == PET_DOG;
}

int PetFriend(int i)
{
    return i >= 0 && i < petCount ? pets[i].friend : -1;
}

bool PetEating(int i)
{
    return i >= 0 && i < petCount && pets[i].eatT > 0;
}

static bool Free(float x, float z)
{
    return !InWater(x, z) && !SolidAt(x, z, 0.4f);
}

static float Floor(float x, float z, float y)
{
    return GroundHeight(x, z, y + 0.45f);
}

void PetsInit(void)
{
    petCount = 0;
    seed = 12345u;
    if (!PetsOn()) return;
    for (int h = 0; h < HOME_COUNT; h++) {
        const PetHome *d = &HOMES[h];
        float x = d->x, z = d->z;
        // The home as written, or the nearest free ground round it.
        for (int k = 0; k < 48 && !Free(x, z); k++) {
            float a = (float)k * 2.4f, r = 0.5f + 0.15f * (float)k;
            x = d->x + cosf(a) * r;
            z = d->z + sinf(a) * r;
        }
        Pet *p = &pets[petCount++];
        *p = (Pet){ 0 };
        p->home = (Vector3){ x, 0, z };
        p->pos = (Vector3){ x, Floor(x, z, TerrainHeight(x, z) + 0.5f), z };
        p->home.y = p->pos.y;
        p->range = d->range;
        p->kind = d->kind;
        p->coat = d->coat;
        p->heading = Rand(-PI, PI);
        p->timer = Rand(1, 4);
        p->friend = -1;
    }
}

// Someone close to where the animal is about to step: the player here, another player, a townsperson.
static bool Present(int s)
{
    return s == G.self || PlayerIn(s);
}

static bool PeopleAt(float x, float z, float y)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!Present(s)) continue;
        Vector3 q = s == G.self ? G.player.pos : G.peers[s].pos;
        if (fabsf(q.y - y) < 1.5f && fabsf(q.x - x) < 0.75f && fabsf(q.z - z) < 0.75f) return true;
    }
    for (int i = 0; i < G.villagerCount; i++) {
        Vector3 q = VillagerPos(&G.villagers[i]);
        if (fabsf(q.x - x) < 0.7f && fabsf(q.z - z) < 0.7f && fabsf(q.y - y) < 1.5f) return true;
    }
    return false;
}

// One step along the heading; false when the way is blocked (a wall, water, a drop, someone there).
static bool Step(Pet *p, float dist)
{
    float nx = p->pos.x + sinf(p->heading) * dist, nz = p->pos.z + cosf(p->heading) * dist;
    float ahead = 0.35f;
    float ax = nx + sinf(p->heading) * ahead, az = nz + cosf(p->heading) * ahead;
    if (InWater(ax, az) || SolidAt(ax, az, PET_RADIUS)) return false;
    float y = Floor(nx, nz, p->pos.y);
    if (y > p->pos.y + 0.45f || y < p->pos.y - 1.2f) return false;
    if (PeopleAt(ax, az, p->pos.y)) return false;
    p->pos = (Vector3){ nx, y, nz };
    p->step += dist;
    return true;
}

static float Toward(Vector3 a, Vector3 b)
{
    return atan2f(b.x - a.x, b.z - a.z);
}

static void Turn(Pet *p, float want, float dt)
{
    float d = want - p->heading;
    while (d > PI) d -= 2 * PI;
    while (d < -PI) d += 2 * PI;
    float k = fminf(1.0f, dt * 5.0f);
    p->heading += d * k;
}

void UpdatePets(float dt)
{
    for (int i = 0; i < petCount; i++) {
        Pet *p = &pets[i];
        if (p->eatT > 0) p->eatT -= dt;
        if (p->fedT > 0) {
            p->fedT -= dt;
            if (p->fedT <= 0 || XZDist(p->pos, p->home) > LEASH) {
                p->fedT = 0;
                if (!IsClient()) p->friend = -1;
            }
        }
        // Far from this machine's eyes nothing moves (the host's own screen included); a follower always does.
        bool following = p->friend >= 0 && p->fedT > 0 && Present(p->friend);
        if (!following && (Dedicated() || XZDist(p->pos, G.cam.position) > 120.0f)) continue;
        if (p->eatT > 0) {
            p->speed = 0;
            p->sit = fminf(1.0f, p->sit + dt * 2);
            continue;
        }
        float walk = p->kind == PET_DOG ? 1.1f : 0.7f;
        // After a friend, or back home from too far: the way round what is in between, as people find it.
        Vector3 goal = p->home;
        float stop = p->range * 0.5f, pace = walk * 1.5f;
        bool going = false;
        if (following) {
            goal = p->friend == G.self ? G.player.pos : G.peers[p->friend].pos;
            float d = XZDist(p->pos, goal);
            going = d > 2.0f;
            stop = 1.6f;
            pace = d > 6.0f ? walk * 4.0f : walk * 2.2f;
            if (!going) p->speed = 0;
        } else if (XZDist(p->pos, p->home) > p->range) {
            going = true;
        }
        if (going) {
            Vector3 was = p->pos;
            p->walk.at = p->pos;
            WalkToward(&p->walk, goal, pace, dt, stop);
            p->pos = p->walk.at;
            float moved = XZDist(was, p->pos);
            if (moved > 1e-4f) Turn(p, Toward(was, p->pos), dt);
            p->step += moved;
            p->speed = moved / fmaxf(dt, 1e-4f);
            p->sit = fmaxf(0, p->sit - dt * 3);
            continue;
        }
        if (!following) {
            p->timer -= dt;
            if (p->timer <= 0) {
                p->timer = Rand(2, 6);
                p->heading += Rand(-1.6f, 1.6f);
                // Cats sit about more than they walk; dogs nose round.
                p->speed = Rand(0, 1) < (p->kind == PET_CAT ? 0.35f : 0.55f) ? walk : 0;
            }
        }
        if (p->speed > 0) {
            if (!Step(p, p->speed * dt)) {
                p->heading += (i & 1 ? 1.0f : -1.0f) * Rand(0.8f, 1.8f);
                if (!following) p->timer = Rand(0.5f, 2);
            }
            p->sit = fmaxf(0, p->sit - dt * 3);
        } else {
            p->sit = fminf(1.0f, p->sit + dt * 1.5f);
        }
    }
}

// The player bumps into them, never through.
Vector2 PushOutPets(Vector2 q, float r)
{
    for (int i = 0; i < petCount; i++) {
        const Pet *p = &pets[i];
        if (fabsf(p->pos.x - q.x) > 2.0f || fabsf(p->pos.z - q.y) > 2.0f || fabsf(p->pos.y - G.player.pos.y) > 1.2f) continue;
        float len = p->kind == PET_DOG ? 0.32f : 0.18f;
        for (int k = -1; k <= 1; k += 2) {
            float cx = p->pos.x + sinf(p->heading) * len * (float)k, cz = p->pos.z + cosf(p->heading) * len * (float)k;
            float dx = q.x - cx, dz = q.y - cz, d = sqrtf(dx * dx + dz * dz), min = r + (p->kind == PET_DOG ? 0.24f : 0.16f);
            if (d >= min) continue;
            if (d < 1e-4f) {
                dx = 1;
                dz = 0;
                d = 1;
            }
            q = (Vector2){ cx + dx / d * min, cz + dz / d * min };
        }
    }
    return q;
}

// The food each takes, the first of them in the baskets; IT_NONE when there is none.
int PetFood(int i)
{
    if (PetIsDog(i)) return BagCount(IT_ROAST) > 0 ? IT_ROAST : IT_NONE;
    return BagCount(IT_FISH) > 0 ? IT_FISH : BagCount(IT_GRILLED) > 0 ? IT_GRILLED : IT_NONE;
}

// The host: slot fed animal i, which now follows them.
bool PetServe(int slot, int i)
{
    if (i < 0 || i >= petCount || !Present(slot)) return false;
    Pet *p = &pets[i];
    p->friend = (signed char)slot;
    p->fedT = FOLLOW_TIME;
    p->eatT = EAT_TIME;
    return true;
}

// The player who fed it: the food goes from the baskets.
void PetReply(int i, int item, bool ok)
{
    if (!ok || i < 0 || i >= petCount) return;
    if (BagRemove(item, 1) < 1) return;
    Pet *p = &pets[i];
    p->eatT = EAT_TIME;
    p->fedT = FOLLOW_TIME;
    p->friend = (signed char)G.self;
    Say(p->kind == PET_DOG ? "The dog wolfs down the meat and follows you about." : "The cat eats the fish and follows you about.");
}

void WritePets(Writer *w)
{
    W8(w, (unsigned)petCount);
    for (int i = 0; i < petCount; i++) {
        W8(w, (unsigned char)(pets[i].friend + 1));
        W8(w, (unsigned)fminf(255.0f, fmaxf(0.0f, pets[i].fedT)));
    }
}

void ReadPets(Reader *r)
{
    int n = (int)R8(r);
    for (int i = 0; i < n; i++) {
        int friend = (int)R8(r) - 1;
        float fed = (float)R8(r);
        if (i >= petCount) continue;
        Pet *p = &pets[i];
        if (friend >= 0 && friend != p->friend) p->eatT = EAT_TIME;   // someone fed it just now: it eats first
        p->friend = (signed char)friend;
        if (friend < 0 || fabsf(fed - p->fedT) > 2.0f) p->fedT = friend < 0 ? 0 : fed;
    }
}

// ---- Drawing ----

static const Color CAT_COATS[6] = {
    { 150, 120, 88, 255 }, { 206, 128, 62, 255 }, { 120, 116, 110, 255 }, { 44, 40, 38, 255 }, { 200, 176, 136, 255 }, { 226, 220, 206, 255 },
};
static const Color DOG_COATS[3] = { { 212, 184, 142, 255 }, { 96, 70, 48, 255 }, { 176, 130, 80, 255 } };

static void Leg(Vector3 at, float len, float r, float swing, Color c)
{
    DrawShadedCylinder((Vector3){ at.x, at.y - len, at.z + swing }, r, r, len, 1.25f, 5, c);
}

static void DrawCat(const Pet *p, float t)
{
    Color c = CAT_COATS[p->coat % 6], dark = Mix(c, BLACK, 0.45f), pink = { 210, 150, 140, 255 };
    float gait = sinf(p->step * 9.0f) * 0.06f, sit = p->sit, eat = p->eatT > 0 ? 1.0f : 0.0f;
    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y, p->pos.z);
    rlRotatef(p->heading * RAD2DEG, 0, 1, 0);
    float hip = 0.2f - sit * 0.1f;
    Leg((Vector3){ -0.05f, 0.2f, 0.13f }, 0.2f, 0.022f, gait, c);
    Leg((Vector3){ 0.05f, 0.2f, 0.13f }, 0.2f, 0.022f, -gait, c);
    Leg((Vector3){ -0.05f, hip, -0.13f }, hip, 0.025f, -gait * (1 - sit), c);
    Leg((Vector3){ 0.05f, hip, -0.13f }, hip, 0.025f, gait * (1 - sit), c);
    // Body (the rear lower when it sits), head, ears, tail curling up.
    rlPushMatrix();
    rlTranslatef(0, 0.24f - sit * 0.05f, 0);
    rlRotatef(-sit * 22.0f, 1, 0, 0);
    DrawShadedBlob((Vector3){ 0, 0, 0 }, (Vector3){ 0.09f, 0.08f, 0.2f }, c);
    if (p->coat == 0 || p->coat == 1)
        for (int k = -1; k <= 1; k++) DrawShadedBox((Vector3){ 0, 0.075f, (float)k * 0.07f }, (Vector3){ 0.1f, 0.012f, 0.025f }, dark);
    rlPopMatrix();
    float hy = 0.34f - eat * 0.17f, hz = 0.2f + eat * 0.05f;
    DrawShadedBlob((Vector3){ 0, hy, hz }, (Vector3){ 0.065f, 0.06f, 0.065f }, c);
    for (int s = -1; s <= 1; s += 2) DrawShadedBox((Vector3){ (float)s * 0.035f, hy + 0.06f, hz - 0.01f }, (Vector3){ 0.025f, 0.05f, 0.015f }, c);
    DrawShadedBox((Vector3){ 0, hy - 0.01f, hz + 0.06f }, (Vector3){ 0.02f, 0.015f, 0.01f }, pink);
    float wave = sinf(t * 2.0f + (float)p->coat) * 0.25f;
    for (int k = 0; k < 3; k++) {
        float a = (float)k * 0.35f + 0.4f + sit * 0.3f;
        DrawShadedBlob((Vector3){ sinf(wave * (float)k) * 0.04f, 0.24f + sinf(a) * 0.07f * (float)k, -0.2f - cosf(a) * 0.07f * (float)k },
                       (Vector3){ 0.018f, 0.018f, 0.05f }, k == 2 ? dark : c);
    }
    rlPopMatrix();
}

static void DrawDog(const Pet *p, float t)
{
    Color c = DOG_COATS[p->coat % 3], dark = Mix(c, BLACK, 0.5f);
    bool big = p->coat == 1, slim = p->coat == 0;
    float k = big ? 1.2f : 1.0f, leg = (slim ? 0.42f : 0.36f) * k, sit = p->sit, eat = p->eatT > 0 ? 1.0f : 0.0f;
    float gait = sinf(p->step * 6.0f) * 0.09f;
    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y, p->pos.z);
    rlRotatef(p->heading * RAD2DEG, 0, 1, 0);
    float hip = leg * (1 - sit * 0.55f);
    Leg((Vector3){ -0.08f * k, leg, 0.24f * k }, leg, 0.035f * k, gait, c);
    Leg((Vector3){ 0.08f * k, leg, 0.24f * k }, leg, 0.035f * k, -gait, c);
    Leg((Vector3){ -0.08f * k, hip, -0.24f * k }, hip, 0.04f * k, -gait * (1 - sit), c);
    Leg((Vector3){ 0.08f * k, hip, -0.24f * k }, hip, 0.04f * k, gait * (1 - sit), c);
    rlPushMatrix();
    rlTranslatef(0, leg + 0.06f * k - sit * 0.08f, 0);
    rlRotatef(-sit * 20.0f, 1, 0, 0);
    DrawShadedBlob((Vector3){ 0, 0, 0 }, (Vector3){ (slim ? 0.1f : 0.14f) * k, (slim ? 0.11f : 0.14f) * k, 0.34f * k }, c);
    rlPopMatrix();
    // Head on its neck, a muzzle, ears (a saluki's long and hanging, the others' up), the tail (wagging for a friend).
    float hy = leg + 0.22f * k - eat * 0.3f * k, hz = 0.38f * k + eat * 0.04f;
    DrawShadedBlob((Vector3){ 0, hy - 0.08f * k, hz - 0.08f * k }, (Vector3){ 0.06f * k, 0.1f * k, 0.07f * k }, c);
    DrawShadedBlob((Vector3){ 0, hy, hz }, (Vector3){ 0.08f * k, 0.08f * k, 0.09f * k }, c);
    DrawShadedBlob((Vector3){ 0, hy - 0.02f * k, hz + 0.1f * k }, (Vector3){ 0.045f * k, 0.045f * k, (slim ? 0.09f : 0.07f) * k }, c);
    DrawShadedBox((Vector3){ 0, hy, hz + (slim ? 0.19f : 0.17f) * k }, (Vector3){ 0.03f, 0.025f, 0.02f }, (Color){ 30, 26, 24, 255 });
    for (int s = -1; s <= 1; s += 2) {
        if (slim) DrawShadedBlob((Vector3){ (float)s * 0.07f * k, hy - 0.04f * k, hz - 0.03f }, (Vector3){ 0.02f, 0.08f * k, 0.04f }, dark);
        else DrawShadedBox((Vector3){ (float)s * 0.05f * k, hy + 0.08f * k, hz - 0.02f }, (Vector3){ 0.035f, 0.07f * k, 0.02f }, dark);
    }
    float wag = p->friend >= 0 ? sinf(t * 14.0f) * 0.5f : sinf(t * 1.5f) * 0.1f;
    rlPushMatrix();
    rlTranslatef(0, leg + 0.1f * k - sit * 0.1f, -0.33f * k);
    rlRotatef(wag * RAD2DEG, 0, 1, 0);
    rlRotatef(slim ? 30.0f : -35.0f, 1, 0, 0);
    DrawShadedCylinder((Vector3){ 0, 0, 0 }, 0.022f * k, 0.022f * k, 0.28f * k, 0.45f, 5, c);
    rlPopMatrix();
    rlPopMatrix();
}

void DrawPets(void)
{
    float t = (float)GetTime();
    for (int i = 0; i < petCount; i++) {
        const Pet *p = &pets[i];
        if (XZDist(p->pos, G.cam.position) > 70.0f || !InView(p->pos, 1.0f)) continue;
        LightFor(p->pos);
        if (p->kind == PET_DOG) DrawDog(p, t + (float)i);
        else DrawCat(p, t + (float)i);
    }
}
