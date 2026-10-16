#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    float maxHp, armor, damage, cooldown, range, speed, sight;
    bool fights;
} RoleDef;

// Barbarians across the river are twice as strong as the raiders on the city side.
static const RoleDef ROLES[ROLE_COUNT] = {
    [ROLE_CIVILIAN]  = { 120, 0.00f,  5, 1.35f, 1.5f, 3.8f, 24, true },
    [ROLE_WORKER]    = { 130, 0.00f,  8, 1.35f, 1.6f, 4.0f, 24, true },
    [ROLE_CLERIC]    = { 100, 0.00f,  0, 1.0f,  1.0f, 4.2f, 24, false },
    [ROLE_MERCHANT]  = { 110, 0.00f,  0, 1.0f,  1.0f, 4.2f, 24, false },
    [ROLE_GUARD]     = { 320, 0.35f, 13, 1.05f, 2.3f, 5.4f, 32, true },
    [ROLE_RAIDER]    = { 260, 0.20f, 19, 1.1f,  1.9f, 5.4f, 24, true },
    [ROLE_BARBARIAN] = { 520, 0.20f, 38, 1.1f,  2.0f, 5.4f, 26, true },
};

#define WINDUP       0.55f
#define MARK_TIME    0.8f
#define PARRY_STUN   0.9f
#define RIPOSTE      1.25f
#define FOE_DAMAGE   1.8f
#define GUARD_LEASH  60.0f

enum { LOOT_CIVILIAN, LOOT_GUARD, LOOT_CLERIC, LOOT_TRADER, LOOT_WORKER, LOOT_MERCHANT, LOOT_RAIDER, LOOT_BARBARIAN };

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
static float darkness[MAX_PLAYERS];

static const char *const JOB_NAMES[] = { "Woodcutter", "Miner", "Potter", "Kiln worker", "Smith", "Fisherman", "Fisherman", "Farmer" };

static void Add(Vitals *v, Role role, Vector3 home, float face, int owner, int loot, int arg, const char *name)
{
    if (fighterCount >= MAX_FIGHTERS) return;
    if (!v->used) {
        *v = (Vitals){ .hp = ROLES[role].maxHp, .target = -1, .used = true };
    }
    fighters[fighterCount++] = (Fighter){ v, role, home, face, owner, loot, arg, name };
}

static bool IsRaider(Role r)
{
    return r == ROLE_RAIDER || r == ROLE_BARBARIAN;
}

// Rebuilt every frame after the scripted routines have moved everyone; a guest rebuilds it from
// what the host last said, the same way, so an index means the same person on every machine.
void CollectFighters(void)
{
    fighterCount = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        Villager *v = &G.villagers[i];
        Role r = v->kind == NPC_GUARD ? ROLE_GUARD : v->kind == NPC_CLERIC ? ROLE_CLERIC : ROLE_CIVILIAN;
        int loot = r == ROLE_GUARD ? LOOT_GUARD : r == ROLE_CLERIC ? LOOT_CLERIC : LOOT_CIVILIAN;
        const char *name = r == ROLE_GUARD ? "Guard" : r == ROLE_CLERIC ? "Cleric"
                         : v->kind == NPC_TRAVELLER ? "Traveller" : v->kind == NPC_VILLAGER ? "Villager" : "Townsperson";
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
        Add(&cv->vit[0], ROLE_MERCHANT, p, f, CARAVAN_OWNER(c, 0), LOOT_MERCHANT, 0, G.traders[cv->trader].name);
        for (int g = 0; g < CARAVAN_GUARDS; g++) {
            CaravanMemberScript(c, 3 + g, &p, &f);
            Add(&cv->vit[1 + g], ROLE_GUARD, p, f, CARAVAN_OWNER(c, 3 + g), LOOT_GUARD, 0, "Caravan guard");
        }
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        Raider *r = &G.raiders[i];
        bool west = r->camp >= EAST_CAMPS;
        Add(&r->vit, west ? ROLE_BARBARIAN : ROLE_RAIDER, r->pos, r->face, OWNER_RAIDER + i, west ? LOOT_BARBARIAN : LOOT_RAIDER, i,
            west ? "Barbarian" : "Raider");
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

// How far a fighter sees player s: less in the dark, unless torches light the player up.
static float SightOf(Role r, int s)
{
    return ROLES[r].sight * (1.0f - 0.55f * darkness[s >= 0 && s < MAX_PLAYERS ? s : 0]);
}

// A raised melee weapon held toward the attacker turns the blow aside.
static bool PlayerParries(int s, Vector3 from)
{
    const Peer *p = &G.peers[s];
    if (!(p->flags & PF_PARRY) || (p->equip != EQ_MELEE && p->equip != EQ_MACE)) return false;
    Vector2 to = Vector2Normalize((Vector2){ from.x - p->pos.x, from.z - p->pos.z });
    return to.x * sinf(p->facing) + to.y * cosf(p->facing) > 0.25f;
}

static void Strike(const Fighter *f, int s)
{
    Vitals *v = f->v;
    const RoleDef *r = &ROLES[f->role];
    Vector3 pp = PlayerAt(s);

    v->cool = r->cooldown;
    v->swing = 0.35f;
    if (!PlayerAwake(s) || XZDist(v->at, pp) > r->range * 1.3f || fabsf(v->at.y - pp.y) > 1.5f) {
        v->mark = MARK_NONE;
        return;
    }
    v->markT = MARK_TIME;
    if (PlayerParries(s, v->at)) {
        v->mark = MARK_PARRIED;
        v->cool += PARRY_STUN;
        if (s == G.self) {
            G.parryT = 0;
            G.parryCool = 0;
        } else {
            NetParried(s);
        }
        SpawnBurst((Vector3){ (v->at.x + pp.x) / 2, pp.y + 1.3f, (v->at.z + pp.z) / 2 }, (Color){ 255, 230, 150, 255 }, 8, 2.5f);
    } else {
        v->mark = MARK_HIT;
        HurtSlot(s, r->damage);
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
        if (IsRaider(t->role)) SayNear(tp, 40.0f, "A guard cut down a raider.");
        else SayNear(tp, 40.0f, "Raiders killed a guard.");
    }
}

// Fights a foe, or else player s.
static void Engage(const Fighter *f, const Fighter *foe, int s, float dt)
{
    Vitals *v = f->v;
    const RoleDef *r = &ROLES[f->role];
    Vector3 target = foe ? FPos(foe) : PlayerAt(s);
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
            else Strike(f, s);
        }
        return;
    }
    if (dist > r->range * 0.8f) StepToward(v, f->owner, target, r->speed, dt, r->range * 0.8f);
    if (dist <= r->range && v->cool <= 0 && fabsf(v->at.y - target.y) < 1.5f && (foe || PlayerAwake(s))) {
        v->windup = WINDUP;
        v->mark = foe ? MARK_NONE : MARK_WARN;
        v->markT = foe ? 0.0f : WINDUP;
    }
}

static const Fighter *FindFoe(int owner)
{
    for (int k = 0; k < fighterCount; k++) {
        const Fighter *t = &fighters[k];
        if (t->owner == owner && (t->role == ROLE_GUARD || IsRaider(t->role)) && !t->v->dead) return t;
    }
    return NULL;
}

// Guards go after any raider that has left its camp.
static const Fighter *SpotRaider(const Fighter *g, Vector3 pos)
{
    const Fighter *best = NULL;
    float bestD = ROLES[ROLE_GUARD].sight * (1.0f - 0.55f * G.night);

    for (int k = 0; k < fighterCount; k++) {
        const Fighter *t = &fighters[k];
        if (!IsRaider(t->role) || t->v->dead || !t->v->override) continue;
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

// The player this fighter goes for on sight: raiders anyone up and about, guards someone wanted.
static int SpotPlayer(const Fighter *f, Vector3 pos)
{
    int best = -1;
    float bestD = 1e9f;
    if (!IsRaider(f->role) && f->role != ROLE_GUARD) return -1;
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!PlayerAwake(s) || (f->role == ROLE_GUARD && G.peers[s].wanted <= 0)) continue;
        Vector3 p = G.peers[s].pos;
        float d = XZDist(pos, p);
        if (d < SightOf(f->role, s) && fabsf(pos.y - p.y) < 6.0f && d < bestD) {
            best = s;
            bestD = d;
        }
    }
    return best;
}

void UpdateCombat(float dt)
{
    CollectFighters();
    for (int s = 0; s < MAX_PLAYERS; s++)
        darkness[s] = PlayerIn(s) ? G.night * (1.0f - Clamp(TorchLightAt(G.peers[s].pos) * 1.6f, 0.0f, 1.0f)) : 0.0f;

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

        // Anger is aimed at one player: whoever hurt them or raised the alarm, or the nearest one spotted.
        Vector3 pos = FPos(f);
        int spotted = SpotPlayer(f, pos);
        if (spotted >= 0) {
            v->hostile = fmaxf(v->hostile, 8.0f);
            if (!PlayerAwake(v->target) || XZDist(pos, G.peers[spotted].pos) + 3.0f < XZDist(pos, PlayerAt(v->target))) v->target = spotted;
        }
        int t = PlayerAwake(v->target) ? v->target : -1;
        float dist = t >= 0 ? XZDist(pos, G.peers[t].pos) : 1e9f;
        bool sees = t >= 0 && dist < SightOf(f->role, t) && fabsf(pos.y - G.peers[t].pos.y) < 6.0f;
        const Fighter *foe = CurrentFoe(f, pos, dt);
        if (v->hostile > 0 && t < 0) v->hostile -= dt;
        bool angry = v->hostile > 0 && t >= 0;

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
            Engage(f, foe, t, dt);
        } else if (v->override) {
            v->windup = 0;
            bool unseen = NearestAnyone(v->at) > 70.0f && NearestAnyone(f->home) > 70.0f;
            if (unseen || StepToward(v, f->owner, f->home, ROLES[f->role].speed * 0.6f, dt, 0.4f)) v->override = false;
        }
    }
}

// Everyone who saw the acting player strike turns on them, and the guards want them.
static void RaiseAlarm(const Fighter *victim)
{
    int s = G.actor, witnesses = 0;
    Vector3 pp = PlayerAt(s);

    for (int i = 0; i < fighterCount; i++) {
        const Fighter *f = &fighters[i];
        if (f == victim || f->v->dead || IsRaider(f->role)) continue;
        Vector3 p = FPos(f);
        if (XZDist(p, pp) > SightOf(f->role, s) || fabsf(p.y - pp.y) > 6.0f) continue;
        f->v->hostile = fmaxf(f->v->hostile, Fights(f) ? 45.0f : 20.0f);
        f->v->target = s;
        witnesses++;
    }
    if (witnesses) {
        if (PlayerIn(s)) G.peers[s].wanted = 120.0f;
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
        if (!IsRaider(f->role) || f->v->dead || XZDist(FPos(f), at) >= 35.0f) continue;
        if (f->v->hostile <= 0 || !PlayerAwake(f->v->target)) f->v->target = G.actor;
        f->v->hostile = fmaxf(f->v->hostile, 25.0f);
    }
}

// The acting player (G.actor) wounds person i.
void DamagePerson(int i, float dmg)
{
    if (i < 0 || i >= fighterCount || fighters[i].v->dead) return;
    const Fighter *f = &fighters[i];
    Vitals *v = f->v;

    bool unaware = v->hostile <= 0 && v->foeT <= 0, opened = v->mark == MARK_PARRIED && v->markT > 0;
    Vector3 pos = FPos(f);
    v->hp -= dmg * (unaware ? 1.5f : opened ? RIPOSTE : 1.0f) * (1.0f - ROLES[f->role].armor);
    HitMark();
    SpawnBurst((Vector3){ pos.x, pos.y + 1.2f, pos.z }, (Color){ 170, 30, 30, 255 }, 10, 2.2f);
    if (!v->override) {
        v->override = true;
        v->at = pos;
        v->face = f->homeFace;
    }
    v->hostile = fmaxf(v->hostile, 40.0f);
    v->target = G.actor;
    if (IsRaider(f->role)) RallyRaiders(f);
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

// Everyone alive within reach in front, nearest first.
int PeopleInArc(Vector3 chest, Vector3 fwd, float range, float minDot, int *out, int max)
{
    float dist[MAX_FIGHTERS];
    int n = 0;

    for (int i = 0; i < fighterCount && n < max; i++) {
        if (fighters[i].v->dead) continue;
        Vector3 p = FPos(&fighters[i]);
        Vector3 d = { p.x - chest.x, 0, p.z - chest.z };
        float len = Vector3Length(d);
        if (len - 0.35f > range || fabsf(p.y + 1.0f - chest.y) > 1.6f) continue;
        if (len > 0.3f && Vector3DotProduct(Vector3Scale(d, 1.0f / len), fwd) < minDot) continue;
        int k = n++;
        while (k > 0 && dist[k - 1] > len) {
            out[k] = out[k - 1];
            dist[k] = dist[k - 1];
            k--;
        }
        out[k] = i;
        dist[k] = len;
    }
    return n;
}

// Raiders, barbarians and anyone already fighting you: fair game for a sweeping blow.
bool FighterIsFoe(int i)
{
    if (i < 0 || i >= fighterCount || fighters[i].v->dead) return false;
    return IsRaider(fighters[i].role) || fighters[i].v->hostile > 0;
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

// The warning, parry and hit marks are meant for the player being attacked.
int FighterMark(int i)
{
    if (i < 0 || i >= fighterCount || fighters[i].v->dead || fighters[i].v->target != G.self) return MARK_NONE;
    return fighters[i].v->mark;
}

int FighterByOwner(int owner)
{
    for (int i = 0; i < fighterCount; i++)
        if (fighters[i].owner == owner) return i;
    return -1;
}

int FighterOwner(int i)
{
    return i >= 0 && i < fighterCount ? fighters[i].owner : -1;
}

// Raiders and guards who were after this player let it go (they were brought down).
void CalmPeople(int slot)
{
    for (int i = 0; i < fighterCount; i++) {
        Vitals *v = fighters[i].v;
        if (v->target == slot && (IsRaider(fighters[i].role) || fighters[i].role == ROLE_GUARD)) v->hostile = 0;
    }
    for (int i = 0; i < MAX_RAIDERS; i++)
        if (G.raiders[i].vit.target == slot) G.raiders[i].vit.hostile = 0;
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

// The world's half of searching a body: the host marks it searched.
bool ClaimBody(int i)
{
    if (!BodyLootable(i)) return false;
    fighters[i].v->looted = true;
    return true;
}

// The looter's half: what the body carried goes into their baskets.
void GiveLoot(int i)
{
    static const int traderGoods[][2] = { { IT_ARROWS, 5 }, { IT_SEEDS, 4 }, { IT_REEDS, 4 }, { IT_COWHIDE, 1 }, { IT_BRONZE, 1 },
                                          { IT_ARROWS, 6 }, { IT_SHEEPSKIN, 1 } };
    static const int jobGoods[][2] = { { IT_WOOD, 3 }, { IT_COPPER, 2 }, { IT_CLAY, 3 }, { IT_POT, 1 }, { IT_BRONZE, 1 },
                                       { IT_FISH, 2 }, { IT_FISH, 2 }, { IT_WHEAT, 2 } };
    static const int raiderGoods[][2] = { { IT_ARROWS, 4 }, { IT_BREAD, 1 }, { IT_COPPER, 2 }, { IT_BRONZE, 1 }, { IT_TIN, 1 }, { IT_WOOD, 3 } };
    static const int barbarianGoods[][2] = { { IT_BRONZE, 2 }, { IT_TIN, 2 }, { IT_ARROWS, 8 }, { IT_WOLF, 2 }, { IT_COPPER, 4 },
                                             { IT_HERB, 3 }, { IT_ROAST, 2 } };
    char log[256] = "";

    if (i < 0 || i >= fighterCount) return;
    const Fighter *f = &fighters[i];
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
        if (f->lootArg < (int)(sizeof(traderGoods) / sizeof(traderGoods[0])))
            Take(log, sizeof(log), traderGoods[f->lootArg][0], traderGoods[f->lootArg][1]);
        break;
    case LOOT_WORKER:
        Take(log, sizeof(log), jobGoods[f->lootArg][0], jobGoods[f->lootArg][1]);
        break;
    case LOOT_MERCHANT:
        Take(log, sizeof(log), IT_TIN, 1);
        Take(log, sizeof(log), IT_DATES, 3);
        break;
    case LOOT_BARBARIAN:
        for (int k = 0; k < 3; k++) {
            int pick = (int)(Hash2(f->owner * 11 + k, G.day) * 6.99f);
            Take(log, sizeof(log), barbarianGoods[pick][0], barbarianGoods[pick][1]);
        }
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
        bool lost = false;
        for (int k = 0; k <= CARAVAN_GUARDS; k++) lost = lost || cv->vit[k].dead;
        if (lost) {
            cv->state = CV_AWAY;
            cv->timer = 40.0f;
        }
        for (int k = 0; k <= CARAVAN_GUARDS; k++) cv->vit[k] = (Vitals){ .used = false };
    }
    for (int s = 0; s < MAX_PLAYERS; s++) G.peers[s].wanted = 0;
    G.templeAccess = false;
}
