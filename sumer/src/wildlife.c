#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define ARROW_GRAVITY  5.0f
#define ALERT_LEVEL    0.45f
#define CALM_LEVEL     0.25f
#define WOLF_BITE      6.0f
#define BITE_COOLDOWN  2.0f
#define BOAR_GORE      10.0f
#define GORE_COOLDOWN  2.2f
#define BOAR_RAGE      40.0f
#define BOAR_SPENT     0.15f
#define SNAKE_BITE     8.0f
#define SNAKE_COOLDOWN 2.4f
#define SNAKE_HISS     0.8f
#define CLOSE_LANDING  6.0f
#define CLOSE_FLYBY    3.0f
// Twice a club's weight of bronze: it cannot parry, but it hits everything in front hard.
#define MACE_DAMAGE    60
#define MACE_REACH     2.9f
#define MACE_ARC       0.2f

typedef struct {
    const char *name;
    Vector3 body;
    float legH;
    Color coat, dark;
    float walk, run, sense, wariness, nightSense;
    int hp;
    ItemId carcass;
    bool swims, flies;
} AnimalDef;

typedef struct { AnimalType type; int count; Rectangle zone; } Herd;

static const AnimalDef DEFS[AN_TYPES] = {
    [AN_GAZELLE] = { "Gazelle", { 0.42f, 0.42f, 1.1f }, 0.66f, { 192, 142, 84, 255 }, { 120, 86, 52, 255 },
                     1.4f, 8.2f, 28.0f, 1.3f, 0.5f, 20, IT_GAZELLE, false, false },
    [AN_ONAGER]  = { "Onager", { 0.55f, 0.62f, 1.6f }, 0.85f, { 172, 150, 126, 255 }, { 84, 70, 58, 255 },
                     1.6f, 8.8f, 32.0f, 1.0f, 0.55f, 40, IT_ONAGER, false, false },
    [AN_BOAR]    = { "Boar", { 0.55f, 0.55f, 1.2f }, 0.36f, { 82, 66, 56, 255 }, { 46, 38, 32, 255 },
                     1.0f, 6.0f, 18.0f, 0.8f, 1.0f, 135, IT_BOAR, false, false },
    [AN_GOOSE]   = { "Goose", { 0.3f, 0.3f, 0.55f }, 0.24f, { 204, 200, 190, 255 }, { 90, 90, 92, 255 },
                     0.8f, 5.5f, 15.0f, 1.6f, 0.45f, 12, IT_GOOSE, true, true },
    [AN_WOLF]    = { "Wolf", { 0.36f, 0.46f, 1.1f }, 0.55f, { 128, 122, 112, 255 }, { 72, 68, 62, 255 },
                     1.8f, 7.0f, 22.0f, 1.0f, 1.0f, 60, IT_WOLF, false, false },
    [AN_SNAKE]   = { "Snake", { 0.14f, 0.1f, 1.2f }, 0.0f, { 112, 118, 64, 255 }, { 62, 68, 40, 255 },
                     0.7f, 3.2f, 3.0f, 1.0f, 1.0f, 16, IT_NONE, false, false },
};

// City side: herds near and far, five packs of four (one denning by the far-north ford).
// Across the river: two packs of six and one of five. A few snakes lie in the long grass on both banks.
static const Herd HERDS[] = {
    { AN_GAZELLE, 6, { 40, 150, 260, 180 } },
    { AN_ONAGER,  4, { 60, -330, 240, 170 } },
    { AN_BOAR,    4, { -126, 20, 24, 300 } },
    { AN_GOOSE,   6, { -147, -500, 16, 1000 } },
    { AN_WOLF,    4, { -125, -530, 655, 1060 } },
    { AN_WOLF,    4, { -125, -530, 655, 1060 } },
    { AN_WOLF,    4, { -125, -534, 110, 70 } },
    { AN_GAZELLE, 5, { 300, 120, 220, 220 } },
    { AN_ONAGER,  4, { 150, -520, 330, 160 } },
    { AN_BOAR,    3, { 380, -140, 140, 90 } },
    { AN_SNAKE,   1, { -124, 60, 20, 70 } },
    { AN_SNAKE,   1, { -124, -300, 22, 80 } },
    { AN_SNAKE,   1, { -118, 300, 40, 60 } },
    { AN_GAZELLE, 6, { -520, -160, 330, 330 } },
    { AN_ONAGER,  4, { -520, -520, 330, 260 } },
    { AN_BOAR,    4, { -520, 200, 330, 300 } },
    { AN_WOLF,    6, { -534, -534, 375, 1068 } },
    { AN_WOLF,    6, { -534, -534, 375, 1068 } },
    { AN_SNAKE,   1, { -380, 120, 110, 110 } },
    { AN_SNAKE,   1, { -300, -200, 100, 120 } },
    // More wolves roam the open country: the east plain on either side of the hunters' road, and the
    // west bank's hills.
    { AN_WOLF,    4, { 160, -480, 340, 380 } },
    { AN_WOLF,    4, { 160, 120, 340, 380 } },
    { AN_WOLF,    5, { -534, -150, 300, 400 } },
};
#define HERD_COUNT ((int)(sizeof(HERDS) / sizeof(HERDS[0])))

static const float BOW_SPEED[3] = { 40.0f, 52.0f, 66.0f };
static const int BOW_DAMAGE[3] = { 20, 28, 40 };
static const int MELEE_DAMAGE[3] = { 14, 24, 38 };
static const char *const WEAPONS[3] = { "Wooden club", "Copper mace", "Bronze sickle-sword" };

const char *WeaponName(int tier)
{
    return L(WEAPONS[tier < 0 ? 0 : tier > 2 ? 2 : tier]);
}

const char *MaceName(void)
{
    return L("War mace");
}

int AnimalMaxHp(int type)
{
    return DEFS[type].hp;
}

const char *AnimalName(int type)
{
    return L(DEFS[type].name);
}

int CarcassItem(int type)
{
    return DEFS[type].carcass;
}

float BowSpeed(void)
{
    return BOW_SPEED[UseBow()];
}

static Vector3 Center(const Animal *a)
{
    const AnimalDef *d = &DEFS[a->type];
    return (Vector3){ a->pos.x, a->pos.y + d->legH + d->body.y / 2, a->pos.z };
}

static float Radius(const Animal *a)
{
    const AnimalDef *d = &DEFS[a->type];
    return 0.45f * fmaxf(d->body.z, d->body.y) + 0.12f;
}

float AnimalTop(const Animal *a)
{
    const AnimalDef *d = &DEFS[a->type];
    return a->pos.y + d->legH + d->body.y + 0.7f;
}

// Snakes lie up only in green grass, away from roads, fields, camps and villages.
static bool GoodSpot(const AnimalDef *d, float x, float z)
{
    if (!d->swims && InWater(x, z)) return false;
    if (InsideWalls(x, z, d == &DEFS[AN_WOLF] ? -20.0f : -6.0f)) return false;
    if (d == &DEFS[AN_SNAKE] && (Fertility(x, z) < 0.45f || InKeepOut(x, z, 6.0f))) return false;
    return !SolidAt(x, z, 0.8f);
}

// Geese rest on the water, or on the ground where there is none.
static float RestHeight(float x, float z)
{
    return InWater(x, z) ? WATER_LEVEL : TerrainHeight(x, z);
}

// Comes back out of sight of every player, beside a herd mate when one is far enough away.
static void Respawn(Animal *a)
{
    const AnimalDef *d = &DEFS[a->type];
    Rectangle zone = HERDS[a->pack].zone;
    const Animal *mate = NULL;

    for (int i = 0; i < G.animalCount; i++) {
        const Animal *m = &G.animals[i];
        if (m != a && m->active && m->type == a->type && m->pack == a->pack && m->state != ST_DEAD && NearestAnyone(m->pos) > 45.0f) {
            mate = m;
            break;
        }
    }

    bool found = false;
    for (int tries = 0; tries < 120 && !found; tries++) {
        float x, z;
        if (mate && tries < 20) {
            x = mate->pos.x + RandF(-6, 6);
            z = mate->pos.z + RandF(-6, 6);
        } else {
            x = RandF(zone.x, zone.x + zone.width);
            z = RandF(zone.y, zone.y + zone.height);
        }
        if (!GoodSpot(d, x, z) || (tries < 80 && NearestAnyone((Vector3){ x, 0, z }) < 45.0f)) continue;
        a->pos = (Vector3){ x, 0, z };
        found = true;
    }
    if (!found) {
        a->active = false;
        a->respawn = 10.0f;
        return;
    }
    a->pos.y = d->flies ? RestHeight(a->pos.x, a->pos.z) : TerrainHeight(a->pos.x, a->pos.z);
    a->lair = a->pos;
    a->state = ST_CALM;
    a->heading = RandF(-PI, PI);
    a->speed = 0;
    a->timer = RandF(1, 4);
    a->awareness = 0;
    a->hp = d->hp;
    a->arrowsIn = 0;
    a->bite = 0;
    a->target = -1;
    a->active = true;
}

void InitWildlife(void)
{
    G.animalCount = 0;
    for (int h = 0; h < HERD_COUNT; h++) {
        for (int k = 0; k < HERDS[h].count && G.animalCount < MAX_ANIMALS; k++) {
            Animal *a = &G.animals[G.animalCount++];
            *a = (Animal){ .type = HERDS[h].type, .pack = h };
            Respawn(a);
        }
    }
}

// Killed animals stay gone until the next spring.
void SpringRespawn(void)
{
    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        bool carcass = a->active && a->state == ST_DEAD && NearestAnyone(a->pos) > 80.0f;
        if ((!a->active && a->respawn < 0) || carcass) Respawn(a);
    }
}

// One bit per animal: set for those killed and waiting for spring.
void AnimalsAwaitingSpring(unsigned char *mask, int bytes)
{
    memset(mask, 0, (size_t)bytes);
    for (int i = 0; i < G.animalCount && i < bytes * 8; i++)
        if ((!G.animals[i].active && G.animals[i].respawn < 0) || (G.animals[i].active && G.animals[i].state == ST_DEAD))
            mask[i >> 3] |= (unsigned char)(1u << (i & 7));
}

void HoldAnimalsForSpring(const unsigned char *mask, int bytes)
{
    for (int i = 0; i < G.animalCount && i < bytes * 8; i++) {
        if (!(mask[i >> 3] >> (i & 7) & 1)) continue;
        G.animals[i].active = false;
        G.animals[i].respawn = -1.0f;
    }
}

// A wolf-pelt cloak keeps the pack off a player who has not attacked it.
static bool Intimidated(int s)
{
    return PlayerIn(s) && G.peers[s].armor == ARMOR_WOLF && G.peers[s].aggro <= 0;
}

static bool Wounded(const Animal *a)
{
    return (float)a->hp < 0.45f * (float)DEFS[a->type].hp;
}

static void StartleMates(const Animal *a);

// Prey bolts; a charging boar keeps charging and a snake slips away into other grass.
static void Startle(Animal *a, bool herd)
{
    if (a->state == ST_DEAD || a->type == AN_WOLF || (a->type == AN_BOAR && a->state == ST_HUNT)) return;
    a->state = ST_FLEE;
    a->timer = a->type == AN_SNAKE ? 3.0f : 5.0f;
    a->awareness = 1.2f;
    if (herd) StartleMates(a);
}

static void StartleMates(const Animal *a)
{
    for (int i = 0; i < G.animalCount; i++) {
        Animal *m = &G.animals[i];
        if (m != a && m->active && m->type == a->type && m->state != ST_FLEE && Vector3Distance(m->pos, a->pos) < 15.0f)
            Startle(m, false);
    }
}

static void Wander(Animal *a, const AnimalDef *d, float dt)
{
    a->timer -= dt;
    if (a->timer > 0) return;

    Rectangle zone = HERDS[a->pack].zone;
    float cx = zone.x + zone.width / 2, cz = zone.y + zone.height / 2;
    bool outside = a->pos.x < zone.x || a->pos.x > zone.x + zone.width ||
                   a->pos.z < zone.y || a->pos.z > zone.y + zone.height;

    a->timer = RandF(1.5f, 4.5f);
    if (outside) a->heading = atan2f(cx - a->pos.x, cz - a->pos.z);
    else a->heading += RandF(-1.4f, 1.4f);
    a->speed = (GetRandomValue(0, 2) == 0) ? 0.0f : d->walk;
}

static void Step(Animal *a, const AnimalDef *d, float dt)
{
    if (a->speed <= 0) return;

    float nx = a->pos.x + sinf(a->heading) * a->speed * dt;
    float nz = a->pos.z + cosf(a->heading) * a->speed * dt;
    bool airborne = d->flies && a->pos.y > RestHeight(a->pos.x, a->pos.z) + 0.8f;
    bool blocked = !airborne && ((!d->swims && InWater(nx, nz)) || SolidAt(nx, nz, 0.4f));
    if (InsideWalls(nx, nz, a->type == AN_WOLF ? -8.0f : -4.0f)) blocked = true;

    if (blocked || fabsf(nx) > WORLD_LIMIT || fabsf(nz) > WORLD_LIMIT) {
        a->heading += (a->state == ST_FLEE) ? 1.2f : PI * 0.6f;
        return;
    }
    a->pos.x = nx;
    a->pos.z = nz;
    if (!d->flies) a->pos.y = TerrainHeight(nx, nz);
}

// A pack or a sounder keeps together: one that strays too far from the first of its group goes back to it.
static void KeepTogether(Animal *a, const AnimalDef *d, float gap)
{
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *lead = &G.animals[i];
        if (lead->type == a->type && lead->pack == a->pack && lead->active && lead->state != ST_DEAD) {
            if (lead != a && Vector3Distance(lead->pos, a->pos) > gap) {
                a->heading = atan2f(lead->pos.x - a->pos.x, lead->pos.z - a->pos.z);
                a->speed = d->walk * 1.5f;
            }
            return;
        }
    }
}

static void HuntPack(const Animal *leader, int s)
{
    for (int i = 0; i < G.animalCount; i++) {
        Animal *m = &G.animals[i];
        if (m->active && m->type == AN_WOLF && m->pack == leader->pack && m->state != ST_DEAD && !Wounded(m) &&
            Vector3Distance(m->pos, leader->pos) < 45.0f) {
            m->state = ST_HUNT;
            m->timer = 0;
            m->target = s;
        }
    }
    if (Warn(s, WARN_WOLF, 30.0f)) SayTo(s, "Wolves! They hunt people who are alone. Run to the city or to other people, or fight.");
}

// Which way is away from the nearest player; the way it already faces when everyone sleeps.
static Vector3 AwayFromPlayers(const Animal *a)
{
    float dist;
    int s = NearestPlayer(a->pos, &dist);
    if (s < 0) return (Vector3){ sinf(a->heading), 0, cosf(a->heading) };
    return (Vector3){ a->pos.x - G.peers[s].pos.x, 0, a->pos.z - G.peers[s].pos.z };
}

// How close a player may come before the pack notices: nearer by night, much nearer crouching.
static float WolfRange(int s, bool night)
{
    int f = G.peers[s].flags;
    return (night ? 38.0f : 22.0f) * ((f & PF_CROUCH) ? 0.6f : (f & PF_RUN) ? 1.3f : 1.0f);
}

// Hunts a player who is alone outside the walls, or one who shot at the pack; bites, circles, bites again.
static void UpdateWolf(Animal *a, const AnimalDef *d, float dt)
{
    bool night = G.daylight < 0.45f;

    a->bite -= dt;
    switch (a->state) {
    case ST_CALM:
    case ST_ALERT:
        Wander(a, d, dt);
        KeepTogether(a, d, 12.0f);
        for (int s = 0; s < MAX_PLAYERS; s++) {
            if (!PlayerAwake(s)) continue;
            const Peer *p = &G.peers[s];
            float dist = XZDist(p->pos, a->pos), range = WolfRange(s, night);
            if (p->aggro > 0 ? dist < range * 1.5f : (p->alone && dist < range && !Intimidated(s))) {
                HuntPack(a, s);
                break;
            }
        }
        break;
    case ST_HUNT: {
        int s = a->target;
        const Peer *p = PlayerIn(s) ? &G.peers[s] : NULL;
        Vector3 to = p ? Vector3Subtract(p->pos, a->pos) : (Vector3){ 0, 0, 0 };
        to.y = 0;
        float dist = Vector3Length(to);
        if (!p || !PlayerAwake(s) || (!p->alone && p->aggro <= 0) || dist > 80.0f || Intimidated(s) || Wounded(a)) {
            a->state = ST_FLEE;
            a->timer = Wounded(a) ? 12.0f : 6.0f;
            if (p && Intimidated(s) && dist < 30.0f && Warn(s, WARN_WOLF, 30.0f)) SayTo(s, "The wolves shy away from your wolf-pelt cloak.");
            break;
        }
        a->heading = atan2f(to.x, to.z);
        if (a->timer > 0) {
            a->timer -= dt;
            a->heading += 1.9f;
            a->speed = d->walk * 2.2f;
        } else if (dist > 1.4f) {
            a->speed = d->run * (night ? 1.0f : 0.92f);
        } else {
            a->speed = 0;
            if (a->bite <= 0) {
                HurtSlot(s, WOLF_BITE, BY_WOLF);
                a->bite = BITE_COOLDOWN;
                a->timer = 1.2f;
            }
        }
        break;
    }
    case ST_FLEE: {
        Vector3 away = AwayFromPlayers(a);
        a->timer -= dt;
        a->heading = atan2f(away.x, away.z);
        a->speed = d->run;
        if (a->timer <= 0) {
            a->state = ST_CALM;
            a->speed = 0;
        }
        break;
    }
    case ST_DEAD:
        break;
    }
    Step(a, d, dt);
}

// A boar that was hurt charges whoever hurt it: it gores, wheels away and charges again, and keeps at
// it until its rage cools, it is nearly dead, or the one it hunts gets away (far off or into the city).
static void UpdateCharge(Animal *a, const AnimalDef *d, float dt)
{
    int s = a->target;
    Vector3 at = PlayerAt(s), to = Vector3Subtract(at, a->pos);
    to.y = 0;
    float dist = Vector3Length(to);

    a->timer -= dt;
    a->bite -= dt;
    if (a->timer <= 0 || dist > 80.0f || !PlayerAwake(s) || InsideWalls(at.x, at.z, -2.0f) || (float)a->hp < BOAR_SPENT * (float)d->hp) {
        a->state = ST_FLEE;
        a->timer = 6.0f;
        a->awareness = 1.2f;
        return;
    }
    a->heading = atan2f(to.x, to.z);
    if (a->bite > GORE_COOLDOWN - 0.8f) {
        // Just gored: it runs on past and wheels round for the next charge.
        a->heading += 2.2f;
        a->speed = d->run * 0.6f;
    } else if (dist > 1.5f) {
        a->speed = d->run * 1.12f;
    } else {
        a->speed = 0;
        if (a->bite <= 0 && fabsf(at.y - a->pos.y) < 1.5f) {
            HurtSlot(s, BOAR_GORE, BY_BOAR);
            a->bite = GORE_COOLDOWN;
        }
    }
    Step(a, d, dt);
}

// Like a wolf pack, the whole sounder takes up a boar's fight: every boar of it nearby that is not
// nearly dead charges the one who hurt it. Returns how many charge.
static int RallySounder(const Animal *a, int s)
{
    int n = 0;
    for (int i = 0; i < G.animalCount; i++) {
        Animal *m = &G.animals[i];
        if (!m->active || m->type != AN_BOAR || m->pack != a->pack || m->state == ST_DEAD || Vector3Distance(m->pos, a->pos) > 40.0f ||
            (float)m->hp < BOAR_SPENT * (float)DEFS[AN_BOAR].hp)
            continue;
        if (m->state != ST_HUNT) m->bite = fmaxf(m->bite, 0.5f);
        m->state = ST_HUNT;
        m->target = s;
        m->timer = BOAR_RAGE;
        n++;
    }
    return n;
}

// Lies hidden in its clump of grass. Someone too close makes it rear up and hiss, then strike.
// Crouching lets you creep much nearer. It never strays more than a lunge from its grass.
static void UpdateSnake(Animal *a, const AnimalDef *d, float dt)
{
    bool roused = a->state == ST_ALERT || a->state == ST_HUNT;
    int s = roused && PlayerAwake(a->target) ? a->target : NearestPlayer(a->pos, NULL);
    const Peer *p = s >= 0 ? &G.peers[s] : NULL;
    Vector3 to = p ? Vector3Subtract(p->pos, a->pos) : (Vector3){ 0, 0, 0 };
    to.y = 0;
    float dist = p ? Vector3Length(to) : 1e9f;
    bool level = p && fabsf(p->pos.y - a->pos.y) < 1.6f && !(p->flags & PF_CLIMB);
    float notice = !p ? 0.0f : (p->flags & PF_CROUCH) ? 1.6f : (p->flags & PF_RUN) ? 3.4f : 2.8f;

    a->bite -= dt;
    if (!p && a->state != ST_FLEE) a->state = ST_CALM;
    switch (a->state) {
    case ST_CALM: {
        Vector3 home = Vector3Subtract(a->lair, a->pos);
        home.y = 0;
        a->speed = Vector3Length(home) > 0.3f ? d->walk : 0.0f;
        if (a->speed > 0) a->heading = atan2f(home.x, home.z);
        if (level && dist < notice) {
            a->state = ST_ALERT;
            a->timer = SNAKE_HISS;
            a->speed = 0;
            a->target = s;
            if (Warn(s, WARN_SNAKE, 25.0f)) SayTo(s, "A snake rears up in the grass, hissing! Back away.");
        }
        break;
    }
    case ST_ALERT:
        a->speed = 0;
        a->heading = atan2f(to.x, to.z);
        a->timer -= dt;
        if (!level || dist > notice + 2.0f) a->state = ST_CALM;
        else if (a->timer <= 0) {
            a->state = ST_HUNT;
            a->timer = 6.0f;
        }
        break;
    case ST_HUNT:
        a->heading = atan2f(to.x, to.z);
        a->timer -= dt;
        a->speed = (dist > 1.1f && XZDist(a->pos, a->lair) < 3.0f) ? d->run : 0.0f;
        if (dist < 1.7f && level && a->bite <= 0) {
            HurtSlot(s, SNAKE_BITE, BY_SNAKE);
            a->bite = SNAKE_COOLDOWN;
            SayTo(s, "A snake bit you!");
        }
        if (a->timer <= 0 || dist > 7.0f || !level) a->state = ST_CALM;
        break;
    case ST_FLEE: {
        Vector3 away = AwayFromPlayers(a);
        a->timer -= dt;
        a->heading = atan2f(away.x, away.z);
        a->speed = d->run;
        if (a->timer <= 0) {
            a->state = ST_CALM;
            a->speed = 0;
            a->lair = a->pos;
        }
        break;
    }
    default:
        break;
    }
    Step(a, d, dt);
}

// Wolves after this machine's player.
bool AnyWolfHunting(void)
{
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (a->active && a->type == AN_WOLF && a->state == ST_HUNT && a->target == G.self) return true;
    }
    return false;
}

// A boar charging or a snake striking at this machine's player.
bool AnyBeastAttacking(void)
{
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (a->active && a->type != AN_WOLF && a->state == ST_HUNT && a->target == G.self && XZDist(a->pos, G.player.pos) < 40.0f)
            return true;
    }
    return false;
}

// A snake lying still in its grass shows nothing: no bar, no mark, no dot on the map.
bool AnimalHidden(const Animal *a)
{
    return a->type == AN_SNAKE && a->state == ST_CALM && a->hp >= DEFS[AN_SNAKE].hp;
}

// Everything hunting this player (every player for -1) gives up and slinks off.
void CalmBeasts(int slot)
{
    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (a->active && a->state == ST_HUNT && (slot < 0 || a->target == slot)) {
            a->state = a->type == AN_SNAKE ? ST_CALM : ST_FLEE;
            a->timer = 20.0f;
        }
    }
}

void UpdateWildlife(float dt)
{
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (G.peers[s].aggro > 0) G.peers[s].aggro -= dt;

    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        const AnimalDef *d = &DEFS[a->type];

        if (!a->active) {
            if (a->respawn > 0) {
                a->respawn -= dt;
                if (a->respawn <= 0) Respawn(a);
            }
            continue;
        }
        if (a->state == ST_DEAD) {
            a->pos.y = fmaxf(TerrainHeight(a->pos.x, a->pos.z), a->pos.y - 8.0f * dt);
            a->timer -= dt;
            if (a->timer <= 0) {
                a->active = false;
                a->respawn = -1.0f;
            }
            continue;
        }
        if (a->type == AN_WOLF) {
            UpdateWolf(a, d, dt);
            continue;
        }
        if (a->type == AN_SNAKE) {
            UpdateSnake(a, d, dt);
            continue;
        }
        if (a->state == ST_HUNT) {
            UpdateCharge(a, d, dt);
            continue;
        }

        // Prey watches the nearest player who is up and about.
        float dist;
        int s = NearestPlayer(a->pos, &dist);
        Vector3 away = AwayFromPlayers(a);
        float range = d->sense * (s >= 0 ? PlayerStance(s) : 1.0f) * Lerp(1.0f, d->nightSense, G.night);

        if (dist < range) a->awareness += d->wariness * (1.0f - dist / range) * 1.8f * dt;
        else a->awareness -= 0.15f * dt;
        if (dist < 3.0f) a->awareness += 2.0f * dt;
        a->awareness = Clamp(a->awareness, 0.0f, 1.2f);

        switch (a->state) {
        case ST_CALM:
            Wander(a, d, dt);
            if (a->type == AN_BOAR) KeepTogether(a, d, 10.0f);
            if (a->awareness >= ALERT_LEVEL) {
                a->state = ST_ALERT;
                a->speed = 0;
            }
            break;
        case ST_ALERT:
            a->speed = 0;
            a->heading = atan2f(-away.x, -away.z);
            if (a->awareness >= 1.0f) Startle(a, true);
            else if (a->awareness < CALM_LEVEL) a->state = ST_CALM;
            break;
        case ST_FLEE:
            a->timer -= dt;
            a->heading = atan2f(away.x, away.z) + 0.35f * sinf(a->timer * 2.0f + (float)i);
            a->speed = d->run;
            if (a->timer <= 0 && dist > range * 1.3f + 10.0f) {
                a->state = ST_CALM;
                a->awareness = 0.5f;
                a->speed = 0;
                a->timer = 2.0f;
            }
            break;
        default:
            break;
        }

        Step(a, d, dt);
        if (d->flies) {
            float target = RestHeight(a->pos.x, a->pos.z) + (a->state == ST_FLEE ? 3.0f : 0.0f);
            a->pos.y += (target - a->pos.y) * fminf(1.0f, dt * 1.5f);
        }
    }
}

static void Kill(Animal *a)
{
    a->state = ST_DEAD;
    a->speed = 0;
    a->timer = a->type == AN_SNAKE ? 60.0f : 240.0f;
    a->awareness = 0;
}

// An arrow that hisses past or lands close by: wolves come for the archer, everything else runs.
static void React(Animal *a)
{
    int s = G.actor;
    if (!a->active || a->state == ST_DEAD) return;
    if (a->type == AN_WOLF) {
        if (a->state == ST_HUNT || !PlayerIn(s)) return;
        G.peers[s].aggro = fmaxf(G.peers[s].aggro, 60.0f);
        if (Warn(s, WARN_WOLF, 30.0f)) SayTo(s, "Your arrow flew among the wolves. The pack comes for you!");
        HuntPack(a, s);
        return;
    }
    if (a->state != ST_FLEE) Startle(a, true);
}

static void ReactAround(Vector3 at, float r)
{
    for (int i = 0; i < G.animalCount; i++)
        if (G.animals[i].active && Vector3Distance(Center(&G.animals[i]), at) < r) React(&G.animals[i]);
}

static float SegDist(Vector3 a, Vector3 b, Vector3 c)
{
    Vector3 ab = Vector3Subtract(b, a);
    float len2 = Vector3DotProduct(ab, ab);
    float t = len2 > 0 ? Clamp(Vector3DotProduct(Vector3Subtract(c, a), ab) / len2, 0.0f, 1.0f) : 0.0f;
    return Vector3Distance(Vector3Add(a, Vector3Scale(ab, t)), c);
}

static Animal *ArrowHit(Vector3 from, Vector3 to)
{
    Vector3 seg = Vector3Subtract(to, from);
    float len2 = Vector3DotProduct(seg, seg);

    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (!a->active || a->state == ST_DEAD) continue;
        Vector3 c = Center(a);
        float t = (len2 > 0) ? Clamp(Vector3DotProduct(Vector3Subtract(c, from), seg) / len2, 0.0f, 1.0f) : 0.0f;
        Vector3 closest = Vector3Add(from, Vector3Scale(seg, t));
        if (Vector3Distance(closest, c) < Radius(a)) return a;
    }
    return NULL;
}

static void StickArrow(Arrow *ar)
{
    ar->flying = false;
    ar->life = 90.0f;
    ar->fade = 0.6f;
    ReactAround(ar->pos, CLOSE_LANDING);
}

static int SpawnArrow(int owner, Vector3 from, Vector3 vel, int damage)
{
    Arrow *slot = NULL;

    for (int i = 0; i < MAX_ARROWS && !slot; i++)
        if (!G.arrows[i].active) slot = &G.arrows[i];
    for (int i = 0; i < MAX_ARROWS && !slot; i++)
        if (!G.arrows[i].flying) slot = &G.arrows[i];
    if (!slot) return -1;
    *slot = (Arrow){ .pos = from, .vel = vel, .life = 4.0f, .fade = 1.0f, .flying = true, .active = true, .owner = owner, .damage = damage };
    return (int)(slot - G.arrows);
}

// This machine's player looses an arrow. In someone else's game it flies here only to be watched:
// the host flies the real one and says what it hit.
int ShootArrow(Vector3 from, Vector3 to, float power)
{
    // The archer aims above the mark by as much as the arrow will drop on its way there, so it
    // strikes where the crosshair points.
    float speed = BOW_SPEED[UseBow()] * (0.35f + 0.65f * power);
    Vector3 aim = to;
    for (int k = 0; k < 3; k++) {
        float t = Vector3Distance(from, aim) / speed;
        aim.y = to.y + 0.5f * ARROW_GRAVITY * t * t;
    }
    Vector3 vel = Vector3Scale(Vector3Normalize(Vector3Subtract(aim, from)), speed);
    int damage = BOW_DAMAGE[UseBow()] + (UseArrowheads() ? 10 : 0);
    if (IsClient()) NetShoot(from, vel, damage);
    return SpawnArrow(G.self, from, vel, damage);
}

// Another archer's arrow (a guard's, a village guard's, a barbarian's), aimed above the mark by as
// much as it will drop. It strikes players only.
void LooseFor(Vector3 from, Vector3 to, float speed, int damage, int cause)
{
    Vector3 aim = to;
    for (int k = 0; k < 3; k++) {
        float t = Vector3Distance(from, aim) / speed;
        aim.y = to.y + 0.5f * ARROW_GRAVITY * t * t;
    }
    int i = SpawnArrow(-1, from, Vector3Scale(Vector3Normalize(Vector3Subtract(aim, from)), speed), damage);
    if (i >= 0) G.arrows[i].cause = cause;
}

static void Spend(Arrow *ar);

static void FlyNpcArrow(Arrow *ar, float dt)
{
    if (!ar->flying) {
        ar->fade = fmaxf(0.0f, ar->fade - dt);
        ar->life -= dt;
        if (ar->life <= 0 || (ar->spent && ar->fade <= 0)) ar->active = false;
        return;
    }
    Vector3 prev = ar->pos;
    ar->vel.y -= ARROW_GRAVITY * dt;
    ar->pos = Vector3Add(ar->pos, Vector3Scale(ar->vel, dt));
    ar->life -= dt;
    ar->trailT -= dt;
    if (ar->trailT <= 0) {
        ar->trailT = 0.02f;
        for (int k = TRAIL_POINTS - 1; k > 0; k--) ar->trail[k] = ar->trail[k - 1];
        ar->trail[0] = prev;
        if (ar->trailCount < TRAIL_POINTS) ar->trailCount++;
    }
    int s = PlayerOnSegment(prev, ar->pos, -1);
    if (s >= 0) {
        Vector3 p = G.peers[s].pos;
        SpawnBurst((Vector3){ p.x, p.y + 1.2f, p.z }, (Color){ 170, 30, 30, 255 }, 10, 2.2f);
        HurtSlot(s, (float)ar->damage, ar->cause);
        Spend(ar);
    } else if (ar->pos.y <= TerrainHeight(ar->pos.x, ar->pos.z) || (ar->pos.y <= WATER_LEVEL && InWater(ar->pos.x, ar->pos.z)) || PointInSolid(ar->pos)) {
        if (PointInSolid(ar->pos)) ar->pos = prev;
        else ar->pos.y = fmaxf(TerrainHeight(ar->pos.x, ar->pos.z), WATER_LEVEL) + 0.05f;
        ar->flying = false;
        ar->life = 20.0f;
        ar->fade = 0.6f;
    } else if (ar->life <= 0) {
        ar->active = false;
    }
}

// A guest's arrow, flown by the host. Nothing faster than the best bow or harder than its best arrow.
void ShootFor(int slot, Vector3 from, Vector3 vel, int damage)
{
    float speed = Vector3Length(vel);
    if (!PlayerIn(slot) || Vector3Distance(from, G.peers[slot].pos) > 6.0f || speed < 1.0f) return;
    if (speed > BOW_SPEED[2]) vel = Vector3Scale(vel, BOW_SPEED[2] / speed);
    SpawnArrow(slot, from, vel, damage < 0 ? 0 : damage > BOW_DAMAGE[2] + 10 ? BOW_DAMAGE[2] + 10 : damage);
}

// The acting player (G.actor) hurt it: a boar or a snake turns on them, the pack comes for them.
static void HitAnimal(Animal *a, int damage)
{
    int s = G.actor;
    a->hp -= damage;
    HitMark();
    SpawnBurst(Center(a), (Color){ 170, 30, 30, 255 }, 12, 2.4f);
    if (a->hp <= 0) {
        Kill(a);
        if (a->type == AN_SNAKE) Say("You killed the snake.");
        else Say("%s down! Walk up to it and press E to take the carcass.", AnimalName(a->type));
    } else if (a->type == AN_BOAR && (float)a->hp >= BOAR_SPENT * (float)DEFS[AN_BOAR].hp) {
        bool fresh = a->state != ST_HUNT || a->target != s;
        int n = RallySounder(a, s);
        if (fresh) Say(n > 1 ? "The boars turn on you and charge!" : "The boar turns on you and charges!");
    } else if (a->type == AN_SNAKE) {
        bool near = XZDist(a->pos, PlayerAt(s)) < 3.0f;
        a->state = near ? ST_HUNT : ST_FLEE;
        a->target = s;
        a->timer = near ? 6.0f : 3.0f;
        Say(near ? "The snake strikes back!" : "The snake slithers away through the grass.");
    } else if (a->type == AN_WOLF) {
        if (PlayerIn(s)) G.peers[s].aggro = 90.0f;
        if (Wounded(a)) {
            a->state = ST_FLEE;
            a->timer = 12.0f;
            Say("Wolf badly wounded. It limps away.");
        } else {
            HuntPack(a, s);
            Say("The wolf turns on you, and its pack with it!");
        }
    } else {
        Startle(a, true);
        Say("%s wounded. It bolts!", AnimalName(a->type));
    }
}

// An arrow on the ground goes back into the quiver of the archer who walks over it. A guest gathers
// its own copy; the host's copy of a guest's arrow just goes.
static void Recover(Arrow *ar)
{
    if (ar->owner == G.self) {
        Vector3 d = Vector3Subtract(ar->pos, G.player.pos);
        d.y -= 0.9f;
        if (Vector3Length(d) < 1.4f && G.quiver < G.quiverMax) {
            ar->active = false;
            G.quiver++;
            Say("Arrow recovered (%d/%d).", G.quiver, G.quiverMax);
        }
    } else if (PlayerIn(ar->owner) && XZDist(ar->pos, G.peers[ar->owner].pos) < 1.4f && fabsf(ar->pos.y - G.peers[ar->owner].pos.y) < 2.0f) {
        ar->active = false;
    }
}

static void Spend(Arrow *ar)
{
    ar->flying = false;
    ar->spent = true;
    ar->fade = 0.6f;
}

// A guest's own arrow, flown for the eye only: where it strikes is shown at once, the host decides the rest.
static void WatchArrow(Arrow *ar, Vector3 prev)
{
    Animal *hit = ArrowHit(prev, ar->pos);
    int person = PersonOnSegment(prev, ar->pos), player = G.pvp ? PlayerOnSegment(prev, ar->pos, ar->owner) : -1;
    if (player >= 0) {
        Vector3 pp = G.peers[player].pos;
        ShowImpact((Vector3){ pp.x, pp.y + 1.2f, pp.z });
        Spend(ar);
    } else if (person >= 0 || hit) {
        Vector3 pos = hit ? Center(hit) : (Vector3){ 0 };
        float hp, maxHp;
        const char *name;
        bool hostile, dead;
        if (!hit && FighterInfo(person, &pos, &hp, &maxHp, &name, &hostile, &dead)) pos.y += 1.2f;
        ShowImpact(pos);
        Spend(ar);
    } else if (ar->pos.y <= WATER_LEVEL && InWater(ar->pos.x, ar->pos.z)) {
        ShowImpact((Vector3){ ar->pos.x, WATER_LEVEL + 0.05f, ar->pos.z });
        Spend(ar);
    } else if (ar->pos.y <= TerrainHeight(ar->pos.x, ar->pos.z)) {
        ar->pos.y = TerrainHeight(ar->pos.x, ar->pos.z) + 0.05f;
        ShowImpact(ar->pos);
        ar->flying = false;
        ar->life = 90.0f;
        ar->fade = 0.6f;
    } else if (PointInSolid(ar->pos)) {
        ar->pos = prev;
        ShowImpact(prev);
        ar->flying = false;
        ar->life = 90.0f;
        ar->fade = 0.6f;
    } else if (ar->life <= 0) {
        ar->active = false;
    }
}

// Each arrow acts for whoever shot it: what it hits answers to them, and they hear about it.
void UpdateArrows(float dt)
{
    int actor = G.actor;

    for (int i = 0; i < MAX_ARROWS; i++) {
        Arrow *ar = &G.arrows[i];
        if (!ar->active) continue;
        if (ar->owner < 0) {
            FlyNpcArrow(ar, dt);
            continue;
        }
        G.actor = ar->owner;

        if (!ar->flying) {
            ar->fade = fmaxf(0.0f, ar->fade - dt);
            if (ar->spent) {
                if (ar->fade <= 0) ar->active = false;
                continue;
            }
            ar->life -= dt;
            if (ar->life <= 0) ar->active = false;
            else Recover(ar);
            continue;
        }

        Vector3 prev = ar->pos;
        ar->vel.y -= ARROW_GRAVITY * dt;
        ar->pos = Vector3Add(ar->pos, Vector3Scale(ar->vel, dt));
        ar->life -= dt;

        ar->trailT -= dt;
        if (ar->trailT <= 0) {
            ar->trailT = 0.02f;
            for (int k = TRAIL_POINTS - 1; k > 0; k--) ar->trail[k] = ar->trail[k - 1];
            ar->trail[0] = prev;
            if (ar->trailCount < TRAIL_POINTS) ar->trailCount++;
        }
        if (IsClient()) {
            WatchArrow(ar, prev);
            continue;
        }

        // An unaware animal takes double damage; a near miss still sends it running (or the wolves after you).
        Animal *hit = ArrowHit(prev, ar->pos);
        int person = PersonOnSegment(prev, ar->pos), player = G.pvp ? PlayerOnSegment(prev, ar->pos, ar->owner) : -1;
        if (player >= 0) {
            Vector3 pp = G.peers[player].pos;
            HitPlayer(player, (float)ar->damage, prev, false);
            ShowImpact((Vector3){ pp.x, pp.y + 1.2f, pp.z });
            Spend(ar);
            continue;
        }
        if (!hit && person < 0)
            for (int k = 0; k < G.animalCount; k++) {
                Animal *a = &G.animals[k];
                if (a->active && a->state != ST_DEAD && SegDist(prev, ar->pos, Center(a)) < CLOSE_FLYBY) React(a);
            }
        if (person >= 0) {
            Vector3 pos;
            float hp, maxHp;
            const char *name;
            bool hostile, dead;
            FighterInfo(person, &pos, &hp, &maxHp, &name, &hostile, &dead);
            DamagePerson(person, (float)ar->damage);
            ShowImpact((Vector3){ pos.x, pos.y + 1.2f, pos.z });
            Spend(ar);
        } else if (hit) {
            int damage = ar->damage;
            if (hit->state == ST_CALM) damage *= 2;
            hit->arrowsIn++;
            ShowImpact(Center(hit));
            HitAnimal(hit, damage);
            Spend(ar);
        } else if (ar->pos.y <= TerrainHeight(ar->pos.x, ar->pos.z) || (ar->pos.y <= WATER_LEVEL && InWater(ar->pos.x, ar->pos.z))) {
            Vector3 at = { ar->pos.x, TerrainHeight(ar->pos.x, ar->pos.z) + 0.05f, ar->pos.z };
            if (InWater(at.x, at.z)) {
                at.y = WATER_LEVEL + 0.05f;
                SpawnBurst(at, (Color){ 210, 230, 240, 255 }, 14, 2.2f);
                SpawnRing(at, (Color){ 220, 240, 250, 255 });
                ShowImpact(at);
                Spend(ar);
                ReactAround(at, CLOSE_LANDING);
            } else {
                ar->pos = at;
                SpawnBurst(at, (Color){ 150, 128, 92, 255 }, 10, 1.6f);
                SpawnRing(at, (Color){ 255, 214, 140, 255 });
                ShowImpact(at);
                StickArrow(ar);
            }
        } else if (PointInSolid(ar->pos)) {
            ar->pos = prev;
            SpawnBurst(prev, (Color){ 170, 160, 150, 255 }, 8, 1.4f);
            ShowImpact(prev);
            StickArrow(ar);
        } else if (ar->life <= 0) {
            ar->active = false;
        }
    }
    G.actor = actor;
}

Vector3 AimPoint(void)
{
    Vector3 o = G.cam.position;
    Vector3 dir = Vector3Normalize(Vector3Subtract(G.cam.target, o));
    Ray ray = { o, dir };
    float best = 150.0f, tmin = G.camDist + 0.5f;

    float t = RayTerrain(o, dir, best);
    if (t > tmin && t < best) best = t;
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (!b->solid || b->water) continue;
        BoundingBox bb = { Vector3Subtract(b->pos, Vector3Scale(b->size, 0.5f)),
                           Vector3Add(b->pos, Vector3Scale(b->size, 0.5f)) };
        RayCollision hit = GetRayCollisionBox(ray, bb);
        if (hit.hit && hit.distance > tmin && hit.distance < best) best = hit.distance;
    }
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (!a->active || a->state == ST_DEAD) continue;
        RayCollision hit = GetRayCollisionSphere(ray, Center(a), Radius(a));
        if (hit.hit && hit.distance > tmin && hit.distance < best) best = hit.distance;
    }
    // People and the other players too: aimed past them, the arrow (which leaves from the shoulder,
    // not from the eye) would cross the line of sight somewhere behind them and pass them by.
    for (int i = 0; i < FighterCount() + MAX_PLAYERS; i++) {
        Vector3 p;
        if (i < FighterCount()) {
            float hp, maxHp;
            const char *name;
            bool hostile, dead;
            if (!FighterInfo(i, &p, &hp, &maxHp, &name, &hostile, &dead) || dead) continue;
        } else {
            int s = i - FighterCount();
            if (s == G.self || !PlayerAwake(s)) continue;
            p = G.peers[s].pos;
        }
        for (int k = 0; k < 2; k++) {
            RayCollision hit = GetRayCollisionSphere(ray, (Vector3){ p.x, p.y + (k ? 1.45f : 0.7f), p.z }, k ? 0.38f : 0.42f);
            if (hit.hit && hit.distance > tmin && hit.distance < best) best = hit.distance;
        }
    }
    return Vector3Add(o, Vector3Scale(dir, best));
}

static void Part(float x, float y, float z, float w, float h, float l, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, l }, c);
}

static void Blob(float x, float y, float z, float w, float h, float l, Color c)
{
    DrawShadedBlob((Vector3){ x, y, z }, (Vector3){ w * 0.56f, h * 0.6f, l * 0.56f }, c);
}

static void Legs(const AnimalDef *d, float w, Color c)
{
    float x = d->body.x / 2 - w / 2 - 0.02f, z = d->body.z / 2 - 0.14f;
    for (int i = 0; i < 4; i++)
        DrawShadedCylinder((Vector3){ (i & 1) ? x : -x, 0, (i & 2) ? z : -z }, w * 0.55f, w * 0.55f, d->legH + 0.08f, 0.65f, 5, c);
}

static void DrawAnimal(const Animal *a)
{
    const AnimalDef *d = &DEFS[a->type];
    float bw = d->body.x, bh = d->body.y, bl = d->body.z, lh = d->legH, top = lh + bh;
    bool moving = a->speed > 0.1f && a->state != ST_DEAD;
    float t = (float)GetTime();
    float bob = moving ? fabsf(sinf(t * a->speed * 2.5f)) * 0.05f : 0.0f;

    rlPushMatrix();
    rlTranslatef(a->pos.x, a->pos.y + bob, a->pos.z);
    rlRotatef(a->heading * RAD2DEG, 0, 1, 0);
    if (a->state == ST_DEAD && a->type != AN_SNAKE) {
        rlTranslatef(0, bw / 2, 0);
        rlRotatef(90, 0, 0, 1);
    }

    switch (a->type) {
    case AN_GAZELLE:
        Legs(d, 0.09f, d->dark);
        Blob(0, lh + bh / 2, 0, bw, bh, bl, d->coat);
        Blob(0, lh + bh * 0.32f, 0, bw * 0.92f, bh * 0.45f, bl * 0.82f, (Color){ 236, 222, 196, 255 });
        Part(0, top + 0.12f, bl / 2 - 0.05f, 0.2f, 0.5f, 0.22f, d->coat);
        Blob(0, top + 0.4f, bl / 2 + 0.11f, 0.2f, 0.22f, 0.4f, d->coat);
        Part(-0.07f, top + 0.67f, bl / 2 + 0.03f, 0.04f, 0.36f, 0.04f, d->dark);
        Part(0.07f, top + 0.67f, bl / 2 + 0.03f, 0.04f, 0.36f, 0.04f, d->dark);
        Part(0, top - 0.1f, -bl / 2 - 0.02f, 0.08f, 0.2f, 0.06f, d->dark);
        break;
    case AN_ONAGER:
        Legs(d, 0.12f, d->coat);
        Blob(0, lh + bh / 2, 0, bw, bh, bl, d->coat);
        Part(0, top + 0.01f, 0, 0.08f, 0.03f, bl, d->dark);
        Part(0, top + 0.1f, bl / 2, 0.26f, 0.5f, 0.35f, d->coat);
        Blob(0, top + 0.3f, bl / 2 + 0.35f, 0.25f, 0.28f, 0.58f, d->coat);
        Part(0, top + 0.27f, bl / 2 + 0.6f, 0.2f, 0.18f, 0.08f, d->dark);
        Part(0, top + 0.38f, bl / 2 - 0.02f, 0.06f, 0.18f, 0.32f, d->dark);
        Part(-0.07f, top + 0.55f, bl / 2 + 0.2f, 0.05f, 0.22f, 0.05f, d->coat);
        Part(0.07f, top + 0.55f, bl / 2 + 0.2f, 0.05f, 0.22f, 0.05f, d->coat);
        Part(0, top - 0.28f, -bl / 2 - 0.04f, 0.06f, 0.45f, 0.06f, d->dark);
        break;
    case AN_BOAR:
        Legs(d, 0.12f, d->dark);
        Blob(0, lh + bh / 2, 0, bw, bh, bl, d->coat);
        Part(0, top + 0.04f, -0.05f, 0.1f, 0.1f, bl * 0.8f, d->dark);
        Blob(0, lh + bh * 0.5f, bl / 2 + 0.2f, 0.42f, 0.42f, 0.46f, d->coat);
        Part(0, lh + bh * 0.35f, bl / 2 + 0.47f, 0.22f, 0.18f, 0.14f, (Color){ 120, 90, 80, 255 });
        Part(-0.09f, lh + bh * 0.32f, bl / 2 + 0.43f, 0.03f, 0.1f, 0.03f, COL_LINEN);
        Part(0.09f, lh + bh * 0.32f, bl / 2 + 0.43f, 0.03f, 0.1f, 0.03f, COL_LINEN);
        Part(-0.13f, lh + bh * 0.5f + 0.24f, bl / 2 + 0.1f, 0.08f, 0.12f, 0.05f, d->dark);
        Part(0.13f, lh + bh * 0.5f + 0.24f, bl / 2 + 0.1f, 0.08f, 0.12f, 0.05f, d->dark);
        break;
    case AN_WOLF: {
        float head = (a->state == ST_HUNT) ? -0.12f : 0.0f;
        Legs(d, 0.09f, d->dark);
        Blob(0, lh + bh / 2, 0, bw, bh, bl, d->coat);
        Blob(0, lh + bh * 0.3f, 0.05f, bw * 0.92f, bh * 0.5f, bl * 0.72f, (Color){ 176, 168, 154, 255 });
        Blob(0, top + head, bl / 2 + 0.12f, 0.3f, 0.3f, 0.34f, d->coat);
        Part(0, top - 0.06f + head, bl / 2 + 0.38f, 0.16f, 0.14f, 0.26f, d->dark);
        Part(-0.08f, top + 0.2f + head, bl / 2 + 0.08f, 0.06f, 0.14f, 0.05f, d->dark);
        Part(0.08f, top + 0.2f + head, bl / 2 + 0.08f, 0.06f, 0.14f, 0.05f, d->dark);
        Part(0, top - 0.12f, -bl / 2 - 0.22f, 0.1f, 0.1f, 0.45f, d->dark);
        break;
    }
    case AN_SNAKE: {
        // Coiled at rest, an S while it moves, the neck raised while it hisses and strikes.
        bool raised = a->state == ST_ALERT || a->state == ST_HUNT;
        float lunge = a->state == ST_HUNT ? Clamp((a->bite - (SNAKE_COOLDOWN - 0.3f)) / 0.3f, 0.0f, 1.0f) : 0.0f;
        for (int k = 8; k >= 0; k--) {
            float u = (float)k / 8.0f, x, y = 0.06f, z;
            if (moving || a->state == ST_DEAD) {
                z = 0.5f - u * 1.1f;
                x = sinf((a->state == ST_DEAD ? 0.0f : t * 9.0f) + u * 6.0f) * 0.12f;
            } else {
                float ang = u * 4.6f, r = 0.27f - u * 0.13f;
                x = sinf(ang) * r;
                z = cosf(ang) * r;
            }
            if (raised && k < 3) {
                x = 0;
                y = 0.06f + 0.18f * (float)(3 - k);
                z = 0.1f + 0.07f * (float)(2 - k) + 0.4f * lunge;
            }
            float s = (k == 0 ? 0.13f : 0.11f - 0.006f * (float)k);
            Blob(x, y, z, s * 1.6f, s * 1.3f, s * 2.0f, k == 0 ? d->dark : (k & 1) ? d->coat : Mix(d->coat, d->dark, 0.45f));
            if (k == 0 && raised && sinf(t * 22.0f) > 0.2f) Part(x, y - 0.02f, z + 0.16f, 0.02f, 0.01f, 0.14f, (Color){ 200, 40, 40, 255 });
        }
        break;
    }
    case AN_GOOSE: {
        const Color beak = { 230, 130, 40, 255 };
        Part(-0.07f, lh / 2, 0, 0.04f, lh, 0.04f, beak);
        Part(0.07f, lh / 2, 0, 0.04f, lh, 0.04f, beak);
        Blob(0, lh + bh / 2, 0, bw, bh, bl, d->coat);
        Part(0, top - 0.05f, -bl / 2 - 0.05f, 0.18f, 0.12f, 0.12f, d->dark);
        Part(0, top + 0.2f, bl / 2 - 0.04f, 0.08f, 0.42f, 0.08f, d->dark);
        Blob(0, top + 0.42f, bl / 2 + 0.01f, 0.11f, 0.11f, 0.15f, d->dark);
        Part(0, top + 0.4f, bl / 2 + 0.12f, 0.05f, 0.04f, 0.1f, beak);
        if (a->pos.y > 0.3f && a->state != ST_DEAD) {
            float flap = sinf(t * 14.0f) * 35.0f;
            for (int s = -1; s <= 1; s += 2) {
                rlPushMatrix();
                rlTranslatef(s * bw / 2, top - 0.05f, 0);
                rlRotatef(s * flap, 0, 0, 1);
                Part(s * 0.35f, 0, 0, 0.7f, 0.03f, 0.32f, d->coat);
                rlPopMatrix();
            }
        } else {
            Part(-bw / 2, lh + bh * 0.6f, -0.05f, 0.04f, 0.18f, 0.4f, d->dark);
            Part(bw / 2, lh + bh * 0.6f, -0.05f, 0.04f, 0.18f, 0.4f, d->dark);
        }
        break;
    }
    default:
        break;
    }
    rlPopMatrix();
}

// The tuft of long grass a snake lies up in; it moves with the snake when it finds a new hiding place.
static void DrawSnakeGrass(const Animal *a, int i)
{
    for (int k = 0; k < 10; k++) {
        float ang = (float)k * 0.63f + (float)i, r = 0.22f + 0.45f * Hash2(i, k);
        Vector3 base = { a->lair.x + cosf(ang) * r, a->lair.y - 0.05f, a->lair.z + sinf(ang) * r };
        DrawShadedCylinder(base, 0.2f, 0.2f, 0.55f + 0.4f * Hash2(k, i + 3), 0.0f, 5, Mix(COL_GRASS, COL_WHEAT_GREEN, Hash2(k, i)));
    }
}

void DrawWildlife(void)
{
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        float d = XZDist(a->pos, G.cam.position);
        if (!a->active || d > 260.0f || (a->type == AN_SNAKE && d > 110.0f)) continue;
        LightFor(a->pos);
        if (a->type == AN_SNAKE) DrawSnakeGrass(a, i);
        DrawAnimal(a);
    }
    G.drawTorch = G.drawFog = 0;
}

static void KnockBack(Animal *a, Vector3 dir, float dist)
{
    if (a->state == ST_DEAD || dist <= 0.3f) return;
    Vector3 push = Vector3Scale(dir, 0.9f / dist);
    if (!SolidAt(a->pos.x + push.x, a->pos.z + push.z, 0.3f)) {
        a->pos.x += push.x;
        a->pos.z += push.z;
    }
}

// The war mace sweeps a wide arc: every animal in reach and every foe in front is struck. A bystander
// is only hit when the blow was clearly aimed at them (nearest of everything in front).
static void MaceStrike(Vector3 chest, Vector3 fwd)
{
    int people[16], n = PeopleInArc(chest, fwd, MACE_REACH, MACE_ARC, people, 16), players[MAX_PLAYERS];
    int np = G.pvp ? PlayersInArc(chest, fwd, MACE_REACH, MACE_ARC, G.actor, players, MAX_PLAYERS) : 0;
    for (int k = 0; k < np; k++) HitPlayer(players[k], (float)MACE_DAMAGE, chest, true);
    float nearestAnimal = 1e9f;

    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (!a->active || a->state == ST_DEAD) continue;
        Vector3 d = Vector3Subtract(Center(a), chest);
        d.y = 0;
        float dist = Vector3Length(d);
        if (dist - Radius(a) * 0.6f > MACE_REACH || fabsf(Center(a).y - chest.y) > 1.8f) continue;
        if (dist > 0.3f && Vector3DotProduct(Vector3Scale(d, 1.0f / dist), fwd) < MACE_ARC) continue;
        nearestAnimal = fminf(nearestAnimal, dist);
        HitAnimal(a, a->state == ST_CALM ? MACE_DAMAGE * 3 / 2 : MACE_DAMAGE);
        KnockBack(a, d, dist);
    }
    for (int k = 0; k < n; k++) {
        Vector3 pos;
        float hp, maxHp;
        const char *name;
        bool hostile, dead;
        FighterInfo(people[k], &pos, &hp, &maxHp, &name, &hostile, &dead);
        bool aimed = k == 0 && XZDist(pos, chest) < nearestAnimal;
        if (aimed || FighterIsFoe(people[k])) DamagePerson(people[k], (float)MACE_DAMAGE);
    }
    for (int k = -3; k <= 3; k++) {
        float a = atan2f(fwd.x, fwd.z) + 0.32f * (float)k;
        Vector3 at = { chest.x + sinf(a) * 2.0f, chest.y - 0.6f, chest.z + cosf(a) * 2.0f };
        SpawnBurst(at, (Color){ 196, 176, 132, 255 }, 3, 1.4f);
    }
}

// A blow by the acting player (G.actor): the closest animal or person in front of the chest, toward
// where they look; unaware animals take half again as much. The war mace sweeps instead.
void StrikeFor(Vector3 chest, Vector3 fwd, int equip, int weapon)
{
    Animal *target = NULL;
    float best = 1e9f;
    Vector3 dir = { 0 };

    if (equip == EQ_MACE) {
        MaceStrike(chest, fwd);
        return;
    }
    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (!a->active || a->state == ST_DEAD) continue;
        Vector3 d = Vector3Subtract(Center(a), chest);
        d.y = 0;
        float dist = Vector3Length(d);
        if (dist - Radius(a) * 0.6f > 2.3f || fabsf(Center(a).y - chest.y) > 1.8f) continue;
        if (dist > 0.3f && Vector3DotProduct(Vector3Scale(d, 1.0f / dist), fwd) < 0.45f) continue;
        if (dist < best) {
            best = dist;
            target = a;
            dir = d;
        }
    }
    int damage = MELEE_DAMAGE[weapon < 0 ? 0 : weapon > 2 ? 2 : weapon];
    float personDist;
    int person = PersonInFront(chest, fwd, 2.3f, &personDist), player;
    if (G.pvp && PlayersInArc(chest, fwd, 2.3f, 0.45f, G.actor, &player, 1) == 1) {
        float d = XZDist(G.peers[player].pos, chest);
        if ((person < 0 || d < personDist) && (!target || d < best)) {
            HitPlayer(player, (float)damage, chest, true);
            return;
        }
    }
    if (person >= 0 && (!target || personDist < best)) {
        DamagePerson(person, (float)damage);
        return;
    }
    if (!target) return;

    if (target->state == ST_CALM) damage = damage * 3 / 2;
    HitAnimal(target, damage);
    KnockBack(target, dir, best);
}

// This machine's player swings; in someone else's game the host plays the blow out.
void MeleeStrike(void)
{
    Player *p = &G.player;
    Vector3 fwd = { -sinf(p->yaw), 0, -cosf(p->yaw) };
    Vector3 chest = { p->pos.x, p->pos.y + 1.0f, p->pos.z };

    p->facing = p->yaw + PI;
    if (IsClient()) NetStrike(chest, fwd, G.equip, UseWeapon());
    else StrikeFor(chest, fwd, G.equip, UseWeapon());
}

void DrawArrow(const Arrow *ar)
{
    if (!ar->active || ar->spent) return;
    LightFor(ar->pos);
    Vector3 v = ar->vel;
    float yaw = atan2f(v.x, v.z), pitch = atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z));
    rlPushMatrix();
    rlTranslatef(ar->pos.x, ar->pos.y, ar->pos.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    rlRotatef(-pitch * RAD2DEG, 1, 0, 0);
    DrawShadedBox((Vector3){ 0, 0, -0.36f }, (Vector3){ 0.03f, 0.03f, 0.72f }, COL_REED);
    DrawShadedBox((Vector3){ 0, 0, 0.02f }, (Vector3){ 0.05f, 0.05f, 0.08f }, COL_STONE);
    DrawShadedBox((Vector3){ 0, 0, -0.66f }, (Vector3){ 0.12f, 0.02f, 0.12f }, COL_LINEN);
    rlPopMatrix();
}

void DrawArrows(void)
{
    for (int i = 0; i < MAX_ARROWS; i++) DrawArrow(&G.arrows[i]);
}
