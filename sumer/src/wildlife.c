#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stddef.h>

#define ARROW_GRAVITY  5.0f
#define ALERT_LEVEL    0.45f
#define CALM_LEVEL     0.25f
#define WOLF_BITE      6.0f
#define BITE_COOLDOWN  2.0f

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
                     1.0f, 6.0f, 18.0f, 0.8f, 1.0f, 45, IT_BOAR, false, false },
    [AN_GOOSE]   = { "Goose", { 0.3f, 0.3f, 0.55f }, 0.24f, { 204, 200, 190, 255 }, { 90, 90, 92, 255 },
                     0.8f, 5.5f, 15.0f, 1.6f, 0.45f, 12, IT_GOOSE, true, true },
    [AN_WOLF]    = { "Wolf", { 0.36f, 0.46f, 1.1f }, 0.55f, { 128, 122, 112, 255 }, { 72, 68, 62, 255 },
                     1.8f, 7.0f, 22.0f, 1.0f, 1.0f, 35, IT_WOLF, false, false },
};

// East of the river, a den by the far-north ford, and the empty west bank.
static const Herd HERDS[] = {
    { AN_GAZELLE, 6, { 40, 150, 260, 180 } },
    { AN_ONAGER,  4, { 60, -330, 240, 170 } },
    { AN_BOAR,    4, { -126, 20, 24, 300 } },
    { AN_GOOSE,   6, { -147, -320, 16, 640 } },
    { AN_WOLF,    3, { -125, -350, 475, 700 } },
    { AN_WOLF,    3, { -125, -350, 475, 700 } },
    { AN_WOLF,    4, { -125, -354, 110, 70 } },
    { AN_GAZELLE, 5, { -350, -120, 190, 320 } },
    { AN_ONAGER,  4, { -350, -340, 190, 200 } },
    { AN_BOAR,    3, { -350, 160, 190, 180 } },
    { AN_WOLF,    3, { -354, -354, 200, 708 } },
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
    return BOW_SPEED[G.bow];
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

static bool GoodSpot(const AnimalDef *d, float x, float z)
{
    if (!d->swims && InWater(x, z)) return false;
    if (InsideWalls(x, z, d == &DEFS[AN_WOLF] ? -20.0f : -6.0f)) return false;
    return !SolidAt(x, z, 0.8f);
}

static void Respawn(Animal *a)
{
    const AnimalDef *d = &DEFS[a->type];
    Rectangle zone = HERDS[a->pack].zone;
    Vector2 player = { G.player.pos.x, G.player.pos.z };
    const Animal *mate = NULL;

    for (int i = 0; i < G.animalCount; i++) {
        const Animal *m = &G.animals[i];
        if (m != a && m->active && m->type == a->type && m->pack == a->pack && m->state != ST_DEAD &&
            Vector2Distance((Vector2){ m->pos.x, m->pos.z }, player) > 45.0f) {
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
        if (!GoodSpot(d, x, z) || (tries < 80 && Vector2Distance((Vector2){ x, z }, player) < 45.0f)) continue;
        a->pos = (Vector3){ x, 0, z };
        found = true;
    }
    if (!found) {
        a->active = false;
        a->respawn = 10.0f;
        return;
    }
    a->pos.y = d->flies ? 0.0f : TerrainHeight(a->pos.x, a->pos.z);
    a->state = ST_CALM;
    a->heading = RandF(-PI, PI);
    a->speed = 0;
    a->timer = RandF(1, 4);
    a->awareness = 0;
    a->hp = d->hp;
    a->arrowsIn = 0;
    a->bite = 0;
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
        bool carcass = a->active && a->state == ST_DEAD && XZDist(a->pos, G.player.pos) > 80.0f;
        if ((!a->active && a->respawn < 0) || carcass) Respawn(a);
    }
}

unsigned long long AnimalsAwaitingSpring(void)
{
    unsigned long long mask = 0;
    for (int i = 0; i < G.animalCount && i < 64; i++)
        if ((!G.animals[i].active && G.animals[i].respawn < 0) || (G.animals[i].active && G.animals[i].state == ST_DEAD)) mask |= 1ull << i;
    return mask;
}

void HoldAnimalsForSpring(unsigned long long mask)
{
    for (int i = 0; i < G.animalCount && i < 64; i++) {
        if (!(mask >> i & 1)) continue;
        G.animals[i].active = false;
        G.animals[i].respawn = -1.0f;
    }
}

static float Stance(void)
{
    const Player *p = &G.player;
    float f;

    if (!p->grounded) f = 1.3f;
    else if (p->crouching) f = p->moving ? 0.45f : 0.3f;
    else if (p->running) f = 1.6f;
    else f = p->moving ? 1.0f : 0.7f;
    return f;
}

static bool Intimidated(void)
{
    return G.armor == ARMOR_WOLF && G.wolfAggro <= 0;
}

static bool Wounded(const Animal *a)
{
    return (float)a->hp < 0.45f * (float)DEFS[a->type].hp;
}

static void Startle(Animal *a, bool herd)
{
    if (a->state == ST_DEAD || a->type == AN_WOLF) return;
    a->state = ST_FLEE;
    a->timer = 5.0f;
    a->awareness = 1.2f;

    if (!herd) return;
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
    bool airborne = d->flies && a->pos.y > 0.8f;
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

static void HuntPack(const Animal *leader)
{
    for (int i = 0; i < G.animalCount; i++) {
        Animal *m = &G.animals[i];
        if (m->active && m->type == AN_WOLF && m->pack == leader->pack && m->state != ST_DEAD && !Wounded(m) &&
            Vector3Distance(m->pos, leader->pos) < 45.0f) {
            m->state = ST_HUNT;
            m->timer = 0;
        }
    }
    if (G.wolfWarn <= 0) {
        Say("Wolves! They hunt people who are alone. Run to the city or to other people, or fight.");
        G.wolfWarn = 30.0f;
    }
}

// Hunts only a player who is alone outside the walls; bites, circles, bites again.
static void UpdateWolf(Animal *a, const AnimalDef *d, float dt)
{
    const Player *p = &G.player;
    Vector3 to = Vector3Subtract(p->pos, a->pos);
    to.y = 0;
    float dist = Vector3Length(to);
    bool night = G.daylight < 0.45f;
    float range = (night ? 38.0f : 22.0f) * (p->crouching ? 0.6f : p->running ? 1.3f : 1.0f);

    a->bite -= dt;
    switch (a->state) {
    case ST_CALM:
    case ST_ALERT:
        Wander(a, d, dt);
        for (int i = 0; i < G.animalCount; i++) {
            const Animal *lead = &G.animals[i];
            if (lead->type == AN_WOLF && lead->pack == a->pack && lead->active && lead->state != ST_DEAD) {
                if (lead != a && Vector3Distance(lead->pos, a->pos) > 12.0f) {
                    a->heading = atan2f(lead->pos.x - a->pos.x, lead->pos.z - a->pos.z);
                    a->speed = d->walk * 1.5f;
                }
                break;
            }
        }
        if (G.wolfAggro > 0 ? dist < range * 1.5f : (G.alone && dist < range && !Intimidated())) HuntPack(a);
        break;
    case ST_HUNT:
        if ((!G.alone && G.wolfAggro <= 0) || dist > 80.0f || Intimidated() || Wounded(a)) {
            a->state = ST_FLEE;
            a->timer = Wounded(a) ? 12.0f : 6.0f;
            if (Intimidated() && dist < 30.0f && G.wolfWarn <= 0) {
                Say("The wolves shy away from your wolf-pelt cloak.");
                G.wolfWarn = 30.0f;
            }
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
                HurtPlayer(WOLF_BITE);
                a->bite = BITE_COOLDOWN;
                a->timer = 1.2f;
            }
        }
        break;
    case ST_FLEE:
        a->timer -= dt;
        a->heading = atan2f(-to.x, -to.z);
        a->speed = d->run;
        if (a->timer <= 0) {
            a->state = ST_CALM;
            a->speed = 0;
        }
        break;
    case ST_DEAD:
        break;
    }
    Step(a, d, dt);
}

bool AnyWolfHunting(void)
{
    for (int i = 0; i < G.animalCount; i++)
        if (G.animals[i].active && G.animals[i].type == AN_WOLF && G.animals[i].state == ST_HUNT) return true;
    return false;
}

void CalmWolves(void)
{
    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (a->type == AN_WOLF && a->active && a->state == ST_HUNT) {
            a->state = ST_FLEE;
            a->timer = 20.0f;
        }
    }
}

void UpdateWildlife(float dt)
{
    float stance = Stance();

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

        Vector3 away = Vector3Subtract(a->pos, G.player.pos);
        away.y = 0;
        float dist = Vector3Length(away);
        float range = d->sense * stance * Lerp(1.0f, d->nightSense, G.night);

        if (dist < range) a->awareness += d->wariness * (1.0f - dist / range) * 1.8f * dt;
        else a->awareness -= 0.15f * dt;
        if (dist < 3.0f) a->awareness += 2.0f * dt;
        a->awareness = Clamp(a->awareness, 0.0f, 1.2f);

        switch (a->state) {
        case ST_CALM:
            Wander(a, d, dt);
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
            float target = (a->state == ST_FLEE) ? 3.0f : 0.0f;
            a->pos.y += (target - a->pos.y) * fminf(1.0f, dt * 1.5f);
        }
    }
}

static void Kill(Animal *a)
{
    a->state = ST_DEAD;
    a->speed = 0;
    a->timer = 240.0f;
    a->awareness = 0;
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

    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (!a->active || a->state == ST_DEAD || a->type == AN_WOLF || Vector3Distance(a->pos, ar->pos) > 7.0f) continue;
        a->awareness += 0.6f;
        if (a->awareness >= 1.0f) Startle(a, true);
        else if (a->state == ST_CALM) a->state = ST_ALERT;
    }
}

int ShootArrow(Vector3 from, Vector3 to, float power)
{
    Arrow *slot = NULL;

    for (int i = 0; i < MAX_ARROWS && !slot; i++)
        if (!G.arrows[i].active) slot = &G.arrows[i];
    for (int i = 0; i < MAX_ARROWS && !slot; i++)
        if (!G.arrows[i].flying) slot = &G.arrows[i];
    if (!slot) return -1;

    Vector3 dir = Vector3Normalize(Vector3Subtract(to, from));
    float speed = BOW_SPEED[G.bow] * (0.35f + 0.65f * power);
    *slot = (Arrow){ .pos = from, .vel = Vector3Scale(dir, speed), .life = 4.0f, .fade = 1.0f, .flying = true, .active = true };
    return (int)(slot - G.arrows);
}

static void HitAnimal(Animal *a, int damage)
{
    a->hp -= damage;
    G.hitMarkT = 0.3f;
    SpawnBurst(Center(a), (Color){ 170, 30, 30, 255 }, 12, 2.4f);
    if (a->hp <= 0) {
        Kill(a);
        Say("%s down! Walk up to it and press E to take the carcass.", AnimalName(a->type));
    } else if (a->type == AN_WOLF) {
        G.wolfAggro = 90.0f;
        if (Wounded(a)) {
            a->state = ST_FLEE;
            a->timer = 12.0f;
            Say("Wolf badly wounded. It limps away.");
        } else {
            HuntPack(a);
            Say("The wolf turns on you, and its pack with it!");
        }
    } else {
        Startle(a, true);
        Say("%s wounded. It bolts!", AnimalName(a->type));
    }
}

void UpdateArrows(float dt)
{
    for (int i = 0; i < MAX_ARROWS; i++) {
        Arrow *ar = &G.arrows[i];
        if (!ar->active) continue;

        if (!ar->flying) {
            ar->fade = fmaxf(0.0f, ar->fade - dt);
            if (ar->spent) {
                if (ar->fade <= 0) ar->active = false;
                continue;
            }
            ar->life -= dt;
            Vector3 d = Vector3Subtract(ar->pos, G.player.pos);
            d.y -= 0.9f;
            if (ar->life <= 0) {
                ar->active = false;
            } else if (Vector3Length(d) < 1.4f && G.quiver < G.quiverMax) {
                ar->active = false;
                G.quiver++;
                Say("Arrow recovered (%d/%d).", G.quiver, G.quiverMax);
            }
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

        // An unaware animal takes double damage.
        Animal *hit = ArrowHit(prev, ar->pos);
        int person = PersonOnSegment(prev, ar->pos);
        if (person >= 0) {
            Vector3 pos;
            float hp, maxHp;
            const char *name;
            bool hostile, dead;
            FighterInfo(person, &pos, &hp, &maxHp, &name, &hostile, &dead);
            DamagePerson(person, (float)(BOW_DAMAGE[G.bow] + ((G.tools & TOOL_ARROWHEADS) ? 10 : 0)));
            G.impact = (Vector3){ pos.x, pos.y + 1.2f, pos.z };
            G.impactT = 3.0f;
            ar->flying = false;
            ar->spent = true;
            ar->fade = 0.6f;
        } else if (hit) {
            int damage = BOW_DAMAGE[G.bow] + ((G.tools & TOOL_ARROWHEADS) ? 10 : 0);
            if (hit->state == ST_CALM) damage *= 2;
            hit->arrowsIn++;
            G.impact = Center(hit);
            G.impactT = 3.0f;
            HitAnimal(hit, damage);
            ar->flying = false;
            ar->spent = true;
            ar->fade = 0.6f;
        } else if (ar->pos.y <= TerrainHeight(ar->pos.x, ar->pos.z)) {
            Vector3 at = { ar->pos.x, TerrainHeight(ar->pos.x, ar->pos.z) + 0.05f, ar->pos.z };
            if (InWater(at.x, at.z)) {
                at.y = 0.1f;
                SpawnBurst(at, (Color){ 210, 230, 240, 255 }, 14, 2.2f);
                SpawnRing(at, (Color){ 220, 240, 250, 255 });
                G.impact = at;
                G.impactT = 3.0f;
                ar->flying = false;
                ar->spent = true;
                ar->fade = 0.6f;
            } else {
                ar->pos = at;
                SpawnBurst(at, (Color){ 150, 128, 92, 255 }, 10, 1.6f);
                SpawnRing(at, (Color){ 255, 214, 140, 255 });
                G.impact = at;
                G.impactT = 3.0f;
                StickArrow(ar);
            }
        } else if (PointInSolid(ar->pos)) {
            ar->pos = prev;
            SpawnBurst(prev, (Color){ 170, 160, 150, 255 }, 8, 1.4f);
            G.impact = prev;
            G.impactT = 3.0f;
            StickArrow(ar);
        } else if (ar->life <= 0) {
            ar->active = false;
        }
    }
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
    if (a->state == ST_DEAD) {
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

void DrawWildlife(void)
{
    for (int i = 0; i < G.animalCount; i++) {
        if (!G.animals[i].active || XZDist(G.animals[i].pos, G.cam.position) > 260.0f) continue;
        LightFor(G.animals[i].pos);
        DrawAnimal(&G.animals[i]);
    }
    G.drawTorch = G.drawFog = 0;
}

// Strikes the closest animal in front, toward where the camera looks; unaware animals take half again as much.
void MeleeStrike(void)
{
    Player *p = &G.player;
    Vector3 fwd = { -sinf(p->yaw), 0, -cosf(p->yaw) };
    Vector3 chest = { p->pos.x, p->pos.y + 1.0f, p->pos.z };
    Animal *target = NULL;
    float best = 1e9f;
    Vector3 dir = { 0 };

    p->facing = p->yaw + PI;
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
    int damage = MELEE_DAMAGE[G.weapon < 0 ? 0 : G.weapon > 2 ? 2 : G.weapon];
    float personDist;
    int person = PersonInFront(chest, fwd, 2.3f, &personDist);
    if (person >= 0 && (!target || personDist < best)) {
        DamagePerson(person, (float)damage);
        return;
    }
    if (!target) return;

    if (target->state == ST_CALM) damage = damage * 3 / 2;
    HitAnimal(target, damage);
    if (target->state != ST_DEAD && best > 0.3f) {
        Vector3 push = Vector3Scale(dir, 0.9f / best);
        if (!SolidAt(target->pos.x + push.x, target->pos.z + push.z, 0.3f)) {
            target->pos.x += push.x;
            target->pos.z += push.z;
        }
    }
}

void DrawArrows(void)
{
    for (int i = 0; i < MAX_ARROWS; i++) {
        const Arrow *ar = &G.arrows[i];
        if (!ar->active || ar->spent) continue;

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
}
