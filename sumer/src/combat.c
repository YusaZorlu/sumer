#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    float maxHp, armor, damage, cooldown, range, speed, sight;
    bool fights;
} RoleDef;

static const RoleDef ROLES[ROLE_COUNT] = {
    [ROLE_CIVILIAN] = { 120, 0.00f,  5, 1.35f, 1.5f, 3.8f, 24, true },
    [ROLE_WORKER]   = { 130, 0.00f,  8, 1.35f, 1.6f, 4.0f, 24, true },
    [ROLE_CLERIC]   = { 100, 0.00f,  0, 1.0f,  1.0f, 4.2f, 24, false },
    [ROLE_MERCHANT] = { 110, 0.00f,  0, 1.0f,  1.0f, 4.2f, 24, false },
    [ROLE_GUARD]    = { 320, 0.35f, 13, 1.05f, 2.3f, 5.4f, 32, true },
    [ROLE_RAIDER]   = { 190, 0.15f, 15, 1.2f,  1.8f, 5.2f, 22, true },
};

#define WINDUP       0.55f
#define MARK_TIME    0.8f
#define PARRY_STUN   0.9f
#define RIPOSTE      1.25f
#define FOE_DAMAGE   1.8f
#define GUARD_LEASH  60.0f

enum { LOOT_CIVILIAN, LOOT_GUARD, LOOT_CLERIC, LOOT_TRADER, LOOT_WORKER, LOOT_MERCHANT, LOOT_RAIDER };

typedef struct {
    Vitals *v;
    Role role;
    Vector3 home;
    float homeFace;
    int owner, loot, lootArg;
    const char *name;
} Fighter;

static Fighter fighters[MAX_FIGHTERS];
static int fighterCount;
static float darkness;

static const char *const JOB_NAMES[] = { "Woodcutter", "Miner", "Potter", "Kiln worker", "Smith", "Fisherman", "Fisherman", "Farmer" };

static void Add(Vitals *v, Role role, Vector3 home, float face, int owner, int loot, int arg, const char *name)
{
    if (fighterCount >= MAX_FIGHTERS) return;
    if (!v->used) {
        *v = (Vitals){ .hp = ROLES[role].maxHp, .used = true };
    }
    fighters[fighterCount++] = (Fighter){ v, role, home, face, owner, loot, arg, name };
}

// Rebuilt every frame after the scripted routines have moved everyone.
static void CollectFighters(void)
{
    fighterCount = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        Villager *v = &G.villagers[i];
        Role r = v->kind == NPC_GUARD ? ROLE_GUARD : v->kind == NPC_CLERIC ? ROLE_CLERIC : ROLE_CIVILIAN;
        int loot = r == ROLE_GUARD ? LOOT_GUARD : r == ROLE_CLERIC ? LOOT_CLERIC : LOOT_CIVILIAN;
        const char *name = r == ROLE_GUARD ? "Guard" : r == ROLE_CLERIC ? "Cleric" : "Townsperson";
        Add(&v->vit, r, VillagerLanePos(v), v->face, OWNER_VILLAGER + i, loot, 0, name);
    }
    for (int i = 0; i < G.workerCount; i++) {
        Worker *w = &G.workers[i];
        if (w->job == JOB_BOAT || (!w->visible && !w->vit.override)) continue;
        Add(&w->vit, ROLE_WORKER, WorkerScriptPos(w), w->facing, OWNER_WORKER + i, LOOT_WORKER, (int)w->job, JOB_NAMES[w->job]);
    }
    for (int i = 0; i < TR_COUNT; i++) {
        Trader *t = &G.traders[i];
        if (!t->roaming) Add(&t->vit, ROLE_MERCHANT, t->pos, t->facing, OWNER_TRADER + i, LOOT_TRADER, i, t->name);
    }
    for (int c = 0; c < MAX_CARAVANS; c++) {
        Caravan *cv = &G.caravans[c];
        Vector3 p;
        float f;
        if (!CaravanMemberScript(c, 0, &p, &f)) continue;
        Add(&cv->vit[0], ROLE_MERCHANT, p, f, OWNER_CARAVAN + c, LOOT_MERCHANT, 0, G.traders[cv->trader].name);
        CaravanMemberScript(c, 3, &p, &f);
        Add(&cv->vit[1], ROLE_GUARD, p, f, OWNER_CARAVAN + c, LOOT_GUARD, 0, "Caravan guard");
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        Raider *r = &G.raiders[i];
        Add(&r->vit, ROLE_RAIDER, r->pos, r->face, OWNER_RAIDER + i, LOOT_RAIDER, i, "Raider");
    }
}

// Only some townsfolk dare to fight; the rest run.
static bool Fights(const Fighter *f)
{
    if (f->role == ROLE_CIVILIAN) return Hash2(f->owner, 7) < 0.35f;
    return ROLES[f->role].fights;
}

static Vector3 FPos(const Fighter *f)
{
    return f->v->override ? f->v->at : f->home;
}

static bool BlockedAt(float x, float z, float y)
{
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (!IsBlocking(b, y)) continue;
        if (fabsf(x - b->pos.x) <= b->size.x / 2 + 0.3f && fabsf(z - b->pos.z) <= b->size.z / 2 + 0.3f) return true;
    }
    return false;
}

// Heads for the goal, trying side steps around walls and people; true once within 'stop'.
static bool StepToward(Vitals *v, int owner, Vector3 goal, float speed, float dt, float stop)
{
    static const float tries[5] = { 0.0f, 0.7f, -0.7f, 1.4f, -1.4f };
    float dx = goal.x - v->at.x, dz = goal.z - v->at.z, d = sqrtf(dx * dx + dz * dz);
    if (d <= stop + 0.05f) return true;

    float base = atan2f(dx, dz), step = fminf(speed * dt, d - stop);
    for (int k = 0; k < 5; k++) {
        float a = base + tries[k], nx = v->at.x + sinf(a) * step, nz = v->at.z + cosf(a) * step;
        Vector3 next = { nx, v->at.y, nz };
        if (BlockedAt(nx, nz, v->at.y) || BodyAhead(v->at, next, 0.35f, owner)) continue;
        v->at.x = nx;
        v->at.z = nz;
        v->at.y = GroundHeight(nx, nz, v->at.y + 0.5f);
        v->face = a;
        return false;
    }
    return false;
}

static float SightOf(Role r)
{
    return ROLES[r].sight * (1.0f - 0.55f * darkness);
}

// A raised melee weapon held toward the attacker turns the blow aside.
static bool PlayerParries(Vector3 from)
{
    const Player *p = &G.player;
    if (G.parryT <= 0 || G.equip != EQ_MELEE) return false;
    Vector2 to = Vector2Normalize((Vector2){ from.x - p->pos.x, from.z - p->pos.z });
    return to.x * sinf(p->facing) + to.y * cosf(p->facing) > 0.25f;
}

static void Strike(const Fighter *f, float dist)
{
    Vitals *v = f->v;
    const RoleDef *r = &ROLES[f->role];

    v->cool = r->cooldown;
    v->swing = 0.35f;
    if (dist > r->range * 1.3f || fabsf(v->at.y - G.player.pos.y) > 1.5f || G.sleeping) {
        v->mark = MARK_NONE;
        return;
    }
    v->markT = MARK_TIME;
    if (PlayerParries(v->at)) {
        v->mark = MARK_PARRIED;
        v->cool += PARRY_STUN;
        G.parryT = 0;
        G.parryCool = 0;
        SpawnBurst((Vector3){ (v->at.x + G.player.pos.x) / 2, G.player.pos.y + 1.3f, (v->at.z + G.player.pos.z) / 2 }, (Color){ 255, 230, 150, 255 }, 8, 2.5f);
    } else {
        v->mark = MARK_HIT;
        HurtPlayer(r->damage);
    }
}

static void StrikeFoe(const Fighter *f, const Fighter *t)
{
    Vitals *v = f->v, *tv = t->v;
    Vector3 tp = FPos(t);

    v->cool = ROLES[f->role].cooldown;
    v->swing = 0.35f;
    v->mark = MARK_NONE;
    if (XZDist(v->at, tp) > ROLES[f->role].range * 1.3f || fabsf(v->at.y - tp.y) > 1.5f) return;
    tv->hp -= ROLES[f->role].damage * FOE_DAMAGE * (1.0f - ROLES[t->role].armor);
    SpawnBurst((Vector3){ tp.x, tp.y + 1.2f, tp.z }, (Color){ 170, 30, 30, 255 }, 8, 2.0f);
    if (!tv->override) {
        tv->override = true;
        tv->at = tp;
        tv->face = t->homeFace;
    }
    tv->foe = f->owner;
    tv->foeT = fmaxf(tv->foeT, 8.0f);
    if (tv->hp <= 0) {
        tv->hp = 0;
        tv->dead = true;
        tv->hostile = 0;
        tv->foeT = 0;
        if (XZDist(tp, G.player.pos) > 40.0f) return;
        if (t->role == ROLE_RAIDER) Say("A guard cut down a raider.");
        else Say("Raiders killed a guard.");
    }
}

static void Engage(const Fighter *f, const Fighter *foe, float dt)
{
    Vitals *v = f->v;
    const RoleDef *r = &ROLES[f->role];
    Vector3 target = foe ? FPos(foe) : G.player.pos;
    float dist = XZDist(v->at, target);

    if (!Fights(f)) {
        Vector3 away = Vector3Add(v->at, Vector3Scale(Vector3Normalize((Vector3){ v->at.x - target.x, 0, v->at.z - target.z }), 5.0f));
        StepToward(v, f->owner, away, r->speed, dt, 0.0f);
        return;
    }
    v->face = atan2f(target.x - v->at.x, target.z - v->at.z);
    if (v->windup > 0) {
        v->windup -= dt;
        if (v->windup <= 0) {
            if (foe) StrikeFoe(f, foe);
            else Strike(f, dist);
        }
        return;
    }
    if (dist > r->range * 0.8f) StepToward(v, f->owner, target, r->speed, dt, r->range * 0.8f);
    if (dist <= r->range && v->cool <= 0 && fabsf(v->at.y - target.y) < 1.5f && (foe || !G.sleeping)) {
        v->windup = WINDUP;
        v->mark = foe ? MARK_NONE : MARK_WARN;
        v->markT = foe ? 0.0f : WINDUP;
    }
}

static const Fighter *FindFoe(int owner)
{
    for (int k = 0; k < fighterCount; k++) {
        const Fighter *t = &fighters[k];
        if (t->owner == owner && (t->role == ROLE_GUARD || t->role == ROLE_RAIDER) && !t->v->dead) return t;
    }
    return NULL;
}

// Guards go after any raider that has left its camp.
static const Fighter *SpotRaider(const Fighter *g, Vector3 pos)
{
    const Fighter *best = NULL;
    float bestD = SightOf(ROLE_GUARD);

    for (int k = 0; k < fighterCount; k++) {
        const Fighter *t = &fighters[k];
        if (t->role != ROLE_RAIDER || t->v->dead || !t->v->override) continue;
        Vector3 p = FPos(t);
        float d = XZDist(p, pos);
        if (d < bestD && fabsf(p.y - pos.y) < 6.0f && XZDist(p, g->home) < GUARD_LEASH) {
            best = t;
            bestD = d;
        }
    }
    if (best) g->v->foe = best->owner;
    return best;
}

static const Fighter *CurrentFoe(const Fighter *f, Vector3 pos, float dt)
{
    Vitals *v = f->v;
    const Fighter *foe = NULL;

    if (v->foeT > 0) {
        v->foeT -= dt;
        foe = FindFoe(v->foe);
        if (foe && (XZDist(FPos(foe), pos) > 25.0f || (f->role == ROLE_GUARD && XZDist(FPos(foe), f->home) > GUARD_LEASH))) foe = NULL;
    }
    if (!foe && f->role == ROLE_GUARD) foe = SpotRaider(f, pos);
    v->foeT = foe ? fmaxf(v->foeT, 2.0f) : 0.0f;
    return foe;
}

void UpdateCombat(float dt)
{
    CollectFighters();
    if (G.wanted > 0) G.wanted -= dt;
    darkness = G.night * (1.0f - Clamp(TorchLightAt(G.player.pos) * 1.6f, 0.0f, 1.0f));

    for (int i = 0; i < fighterCount; i++) {
        const Fighter *f = &fighters[i];
        Vitals *v = f->v;
        if (v->cool > 0) v->cool -= dt;
        if (v->swing > 0) v->swing -= dt;
        if (v->markT > 0) v->markT -= dt;
        else if (v->windup <= 0) v->mark = MARK_NONE;
        if (v->dead) {
            v->windup = 0;
            v->mark = MARK_NONE;
            continue;
        }

        Vector3 pos = FPos(f);
        float dist = XZDist(pos, G.player.pos);
        bool sees = dist < SightOf(f->role) && fabsf(pos.y - G.player.pos.y) < 6.0f && !G.sleeping;
        if (sees && (f->role == ROLE_RAIDER || (f->role == ROLE_GUARD && G.wanted > 0))) v->hostile = fmaxf(v->hostile, 8.0f);
        const Fighter *foe = CurrentFoe(f, pos, dt);
        bool angry = v->hostile > 0;

        if (angry || foe) {
            if (!v->override) {
                v->override = true;
                v->at = pos;
                v->face = f->homeFace;
            }
            if (angry) {
                v->hostile -= dt;
                if (sees && dist < 10.0f && Fights(f)) v->hostile = fmaxf(v->hostile, 3.0f);
                if (foe && dist < XZDist(v->at, FPos(foe))) foe = NULL;
            }
            Engage(f, foe, dt);
        } else if (v->override) {
            v->windup = 0;
            bool unseen = XZDist(v->at, G.cam.position) > 70.0f && XZDist(f->home, G.cam.position) > 70.0f;
            if (unseen || StepToward(v, f->owner, f->home, ROLES[f->role].speed * 0.6f, dt, 0.4f)) v->override = false;
        }
    }
}

static void RaiseAlarm(const Fighter *victim)
{
    int witnesses = 0;

    for (int i = 0; i < fighterCount; i++) {
        const Fighter *f = &fighters[i];
        if (f == victim || f->v->dead || f->role == ROLE_RAIDER) continue;
        Vector3 p = FPos(f);
        if (XZDist(p, G.player.pos) > SightOf(f->role) || fabsf(p.y - G.player.pos.y) > 6.0f) continue;
        f->v->hostile = fmaxf(f->v->hostile, Fights(f) ? 45.0f : 20.0f);
        witnesses++;
    }
    if (witnesses) {
        G.wanted = 120.0f;
        Say(witnesses > 1 ? "%d people saw you attack. Guards will hunt you!" : "%d person saw you attack. Guards will hunt you!", witnesses);
    } else {
        Say("No one else saw that.");
    }
}

static void RallyRaiders(const Fighter *victim)
{
    Vector3 at = FPos(victim);
    for (int i = 0; i < fighterCount; i++) {
        const Fighter *f = &fighters[i];
        if (f->role == ROLE_RAIDER && !f->v->dead && XZDist(FPos(f), at) < 35.0f) f->v->hostile = fmaxf(f->v->hostile, 25.0f);
    }
}

void DamagePerson(int i, float dmg)
{
    if (i < 0 || i >= fighterCount || fighters[i].v->dead) return;
    const Fighter *f = &fighters[i];
    Vitals *v = f->v;

    bool unaware = v->hostile <= 0 && v->foeT <= 0, opened = v->mark == MARK_PARRIED && v->markT > 0;
    Vector3 pos = FPos(f);
    v->hp -= dmg * (unaware ? 1.5f : opened ? RIPOSTE : 1.0f) * (1.0f - ROLES[f->role].armor);
    G.hitMarkT = 0.3f;
    SpawnBurst((Vector3){ pos.x, pos.y + 1.2f, pos.z }, (Color){ 170, 30, 30, 255 }, 10, 2.2f);
    if (!v->override) {
        v->override = true;
        v->at = pos;
        v->face = f->homeFace;
    }
    v->hostile = fmaxf(v->hostile, 40.0f);
    if (f->role == ROLE_RAIDER) RallyRaiders(f);
    else if (unaware) RaiseAlarm(f);
    if (v->hp <= 0) {
        v->hp = 0;
        v->dead = true;
        v->hostile = 0;
        v->foeT = 0;
        Say("%s killed. Search the body with E.", L(f->name));
    }
}

static int SegmentHit(Vector3 a, Vector3 b, Vector3 c, float r)
{
    Vector3 ab = Vector3Subtract(b, a);
    float len2 = Vector3DotProduct(ab, ab);
    float t = len2 > 0 ? Clamp(Vector3DotProduct(Vector3Subtract(c, a), ab) / len2, 0.0f, 1.0f) : 0.0f;
    return Vector3Distance(Vector3Add(a, Vector3Scale(ab, t)), c) < r;
}

int PersonOnSegment(Vector3 a, Vector3 b)
{
    for (int i = 0; i < fighterCount; i++) {
        if (fighters[i].v->dead) continue;
        Vector3 p = FPos(&fighters[i]);
        if (SegmentHit(a, b, (Vector3){ p.x, p.y + 0.7f, p.z }, 0.42f) || SegmentHit(a, b, (Vector3){ p.x, p.y + 1.45f, p.z }, 0.38f))
            return i;
    }
    return -1;
}

int PersonInFront(Vector3 chest, Vector3 fwd, float range, float *dist)
{
    int best = -1;
    *dist = 1e9f;
    for (int i = 0; i < fighterCount; i++) {
        if (fighters[i].v->dead) continue;
        Vector3 p = FPos(&fighters[i]);
        Vector3 d = { p.x - chest.x, 0, p.z - chest.z };
        float len = Vector3Length(d);
        if (len - 0.35f > range || fabsf(p.y + 1.0f - chest.y) > 1.6f) continue;
        if (len > 0.3f && Vector3DotProduct(Vector3Scale(d, 1.0f / len), fwd) < 0.45f) continue;
        if (len < *dist) {
            *dist = len;
            best = i;
        }
    }
    return best;
}

int FighterCount(void)
{
    return fighterCount;
}

bool FighterInfo(int i, Vector3 *pos, float *hp, float *maxHp, const char **name, bool *hostile, bool *dead)
{
    if (i < 0 || i >= fighterCount) return false;
    const Fighter *f = &fighters[i];
    *pos = FPos(f);
    *hp = f->v->hp;
    *maxHp = ROLES[f->role].maxHp;
    *name = f->name;
    *hostile = f->v->hostile > 0 && Fights(f);
    *dead = f->v->dead;
    return true;
}

int FighterMark(int i)
{
    if (i < 0 || i >= fighterCount || fighters[i].v->dead) return MARK_NONE;
    return fighters[i].v->mark;
}

bool BodyLootable(int i)
{
    return i >= 0 && i < fighterCount && fighters[i].v->dead && !fighters[i].v->looted;
}

static void Take(char *log, int size, int item, int n)
{
    int got = item == IT_ARROWS ? GiveArrows(n) : BagAdd(item, n);
    if (got > 0) snprintf(log + strlen(log), (size_t)size - strlen(log), "%s%d %s", log[0] ? ", " : "", got, ItemName(item));
}

void LootBody(int i)
{
    static const int traderGoods[][2] = { { IT_ARROWS, 5 }, { IT_SEEDS, 4 }, { IT_REEDS, 4 }, { IT_COWHIDE, 1 }, { IT_BRONZE, 1 } };
    static const int jobGoods[][2] = { { IT_WOOD, 3 }, { IT_COPPER, 2 }, { IT_CLAY, 3 }, { IT_POT, 1 }, { IT_BRONZE, 1 },
                                       { IT_FISH, 2 }, { IT_FISH, 2 }, { IT_WHEAT, 2 } };
    static const int raiderGoods[][2] = { { IT_ARROWS, 4 }, { IT_BREAD, 1 }, { IT_COPPER, 2 }, { IT_BRONZE, 1 }, { IT_TIN, 1 }, { IT_WOOD, 3 } };
    char log[256] = "";

    if (!BodyLootable(i)) return;
    const Fighter *f = &fighters[i];
    f->v->looted = true;
    switch (f->loot) {
    case LOOT_CIVILIAN:
        Take(log, sizeof(log), Hash2(f->owner, G.day) < 0.5f ? IT_DATES : IT_BREAD, Hash2(f->owner, G.day) < 0.5f ? 2 : 1);
        break;
    case LOOT_GUARD:
        if (G.weapon < 2) {
            G.weapon = 2;
            Fmt(log, sizeof(log), "the guard's %s", WeaponName(2));
        } else {
            Take(log, sizeof(log), IT_BRONZE, 2);
        }
        Take(log, sizeof(log), IT_ARROWS, 4);
        break;
    case LOOT_CLERIC:
        Take(log, sizeof(log), IT_BREAD, 2);
        Take(log, sizeof(log), IT_DATES, 3);
        break;
    case LOOT_TRADER:
        if (f->lootArg < 5) Take(log, sizeof(log), traderGoods[f->lootArg][0], traderGoods[f->lootArg][1]);
        break;
    case LOOT_WORKER:
        Take(log, sizeof(log), jobGoods[f->lootArg][0], jobGoods[f->lootArg][1]);
        break;
    case LOOT_MERCHANT:
        Take(log, sizeof(log), IT_TIN, 1);
        Take(log, sizeof(log), IT_DATES, 3);
        break;
    default:
        for (int k = 0; k < 2; k++) {
            int pick = (int)(Hash2(f->owner * 7 + k, G.day) * 5.99f);
            Take(log, sizeof(log), raiderGoods[pick][0], raiderGoods[pick][1]);
        }
        break;
    }
    if (log[0]) Say("Found %s.", log);
    else Say("Nothing you can carry.");
}

void RespawnPeople(void)
{
    for (int i = 0; i < G.villagerCount; i++) G.villagers[i].vit = (Vitals){ .used = false };
    for (int i = 0; i < G.workerCount; i++) G.workers[i].vit = (Vitals){ .used = false };
    for (int i = 0; i < TR_COUNT; i++) G.traders[i].vit = (Vitals){ .used = false };
    for (int c = 0; c < MAX_CARAVANS; c++) {
        Caravan *cv = &G.caravans[c];
        if (cv->vit[0].dead || cv->vit[1].dead) {
            cv->state = CV_AWAY;
            cv->timer = 40.0f;
        }
        cv->vit[0] = cv->vit[1] = (Vitals){ .used = false };
    }
    G.wanted = 0;
    G.templeAccess = false;
}
