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

#define WINDUP       NPC_WINDUP
#define MARK_TIME    0.8f
#define PARRY_STUN   0.9f
#define RIPOSTE      1.25f
#define FOE_DAMAGE   1.8f
#define WANTED_TIME  120.0f
#define PVP_SCALE    0.5f
#define BOW_REST     1.6f
#define BOW_RANGE    34.0f
#define PROVOKED     60.0f
#define VILLAGE_RAGE 120.0f

enum { LOOT_CIVILIAN, LOOT_GUARD, LOOT_CLERIC, LOOT_TRADER, LOOT_WORKER, LOOT_MERCHANT, LOOT_RAIDER, LOOT_BARBARIAN };

// Whose side someone is on: the city's guards keep its peace, caravan guards their caravan, village
// folk stand together for their village.
typedef enum { FK_TOWN, FK_CITY_GUARD, FK_CARAVAN, FK_VILLAGER, FK_TRAVELLER, FK_RAIDER } FighterKind;

typedef struct {
    Vitals *v;
    Role role;
    Vector3 home;
    float homeFace;
    int owner, loot, lootArg;
    const char *name;
    FighterKind kind;
    int village, bow;
} Fighter;

// The bows other people shoot: a village guard's hunting bow, a gate guard's composite bow, a
// barbarian's war bow.
static const float NPC_BOW_SPEED[4] = { 0, 40, 52, 62 };
static const float NPC_BOW_DAMAGE[4] = { 0, 16, 22, 30 };

static Fighter fighters[MAX_FIGHTERS];
static int fighterCount;
static float darkness[MAX_PLAYERS];
// struck[a][b]: seconds since player a last hurt player b, counting down from PROVOKED; whoever
// hits back within that time is defending themselves, not starting a fight.
static float struck[MAX_PLAYERS][MAX_PLAYERS];

static const char *const JOB_NAMES[] = { "Woodcutter", "Miner", "Potter", "Kiln worker", "Smith", "Fisherman", "Fisherman", "Farmer" };

static void Add(Vitals *v, Role role, Vector3 home, float face, int owner, int loot, int arg, const char *name, FighterKind kind)
{
    if (fighterCount >= MAX_FIGHTERS) return;
    if (!v->used) {
        *v = (Vitals){ .hp = ROLES[role].maxHp, .target = -1, .used = true };
    }
    fighters[fighterCount++] = (Fighter){ v, role, home, face, owner, loot, arg, name, kind, kind == FK_VILLAGER ? VillageAt(home) : -1, 0 };
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
        bool warden = v->kind == NPC_WARDEN;
        Role r = v->kind == NPC_GUARD || warden ? ROLE_GUARD : v->kind == NPC_CLERIC ? ROLE_CLERIC : ROLE_CIVILIAN;
        int loot = r == ROLE_GUARD ? LOOT_GUARD : r == ROLE_CLERIC ? LOOT_CLERIC : LOOT_CIVILIAN;
        const char *name = warden ? "Village guard" : r == ROLE_GUARD ? "Guard" : r == ROLE_CLERIC ? "Cleric"
                         : v->kind == NPC_TRAVELLER ? "Traveller" : v->kind == NPC_VILLAGER ? "Villager" : "Townsperson";
        FighterKind kind = v->kind == NPC_GUARD ? FK_CITY_GUARD : v->kind == NPC_VILLAGER || warden ? FK_VILLAGER : v->kind == NPC_TRAVELLER ? FK_TRAVELLER : FK_TOWN;
        Add(&v->vit, r, VillagerLanePos(v), v->face, OWNER_VILLAGER + i, loot, 0, name, kind);
        if (fighterCount > 0 && fighters[fighterCount - 1].v == &v->vit) fighters[fighterCount - 1].bow = v->bow;
    }
    for (int i = 0; i < G.workerCount; i++) {
        Worker *w = &G.workers[i];
        if (w->job == JOB_BOAT || (!w->visible && !w->vit.override)) continue;
        Add(&w->vit, ROLE_WORKER, WorkerScriptPos(w), w->facing, OWNER_WORKER + i, LOOT_WORKER, (int)w->job, JOB_NAMES[w->job], FK_TOWN);
    }
    for (int i = 0; i < TR_COUNT; i++) {
        Trader *t = &G.traders[i];
        if (!t->roaming) Add(&t->vit, ROLE_MERCHANT, t->pos, t->facing, OWNER_TRADER + i, LOOT_TRADER, i, t->name, VillageAt(t->pos) >= 0 ? FK_VILLAGER : FK_TOWN);
    }
    for (int c = 0; c < MAX_CARAVANS; c++) {
        Caravan *cv = &G.caravans[c];
        Vector3 p;
        float f;
        if (!CaravanMemberScript(c, 0, &p, &f)) continue;
        Add(&cv->vit[0], ROLE_MERCHANT, p, f, CARAVAN_OWNER(c, 0), LOOT_MERCHANT, 0, G.traders[cv->trader].name, FK_CARAVAN);
        for (int g = 0; g < CARAVAN_GUARDS; g++) {
            CaravanMemberScript(c, 3 + g, &p, &f);
            Add(&cv->vit[1 + g], ROLE_GUARD, p, f, CARAVAN_OWNER(c, 3 + g), LOOT_GUARD, 0, "Caravan guard", FK_CARAVAN);
        }
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        Raider *r = &G.raiders[i];
        bool west = r->camp >= EAST_CAMPS;
        Add(&r->vit, west ? ROLE_BARBARIAN : ROLE_RAIDER, r->pos, r->face, OWNER_RAIDER + i, west ? LOOT_BARBARIAN : LOOT_RAIDER, i,
            RaiderBowman(i) ? "Barbarian bowman" : west ? "Barbarian" : "Raider", FK_RAIDER);
        if (fighterCount > 0 && fighters[fighterCount - 1].v == &r->vit) fighters[fighterCount - 1].bow = RaiderBowman(i) ? 3 : 0;
    }
}

// Only some townsfolk dare to fight; the rest run. Village folk all stand and fight.
static bool Fights(const Fighter *f)
{
    if (f->role == ROLE_CIVILIAN) return f->kind == FK_VILLAGER || Hash2(f->owner, 7) < 0.35f;
    return ROLES[f->role].fights;
}

// How far from home someone goes after a foe: the city's guards stay in the city's lands and near
// their posts, caravan guards by their caravan, village folk around their village.
static bool InReach(const Fighter *f, Vector3 p)
{
    float d = XZDist(p, f->home);
    switch (f->kind) {
    case FK_CITY_GUARD: return d < 40.0f && InCityLands(p.x, p.z);
    case FK_CARAVAN: return d < 14.0f;
    case FK_VILLAGER: return d < 45.0f;
    case FK_RAIDER: return true;
    default: return d < 30.0f;
    }
}

// Who a player brought down by this fighter was beaten by.
static int Cause(const Fighter *f)
{
    switch (f->kind) {
    case FK_CITY_GUARD: return BY_GUARD;
    case FK_CARAVAN: return BY_CARAVAN;
    case FK_VILLAGER: return BY_VILLAGERS;
    case FK_RAIDER: return f->role == ROLE_BARBARIAN ? BY_BARBARIAN : BY_RAIDER;
    default: return BY_TOWNSFOLK;
    }
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

// Whether the stretch a-b crosses the rectangle, seen from above.
static bool Crosses(Vector3 a, Vector3 b, Rectangle r)
{
    float o[2] = { a.x, a.z }, d[2] = { b.x - a.x, b.z - a.z }, lo = 0.0f, hi = 1.0f;
    float mn[2] = { r.x, r.y }, mx[2] = { r.x + r.width, r.y + r.height };
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

// The way past water too wide to jump (the harbor, the river): over a bridge or round a corner of
// the water, whichever is shorter. A way round of more than 150 m is not taken.
static Vector3 AroundWater(Vector3 at, Vector3 goal)
{
    for (int c = 0; c < ChannelCount(); c++) {
        bool wet;
        Rectangle r = ChannelRect(c, &wet, NULL);
        if (!wet || fminf(r.width, r.height) <= 6.0f) continue;
        Rectangle e = { r.x - 0.6f, r.y - 0.6f, r.width + 1.2f, r.height + 1.2f };
        if (!Crosses(at, goal, e)) continue;

        // A way point the goal can be seen from beats one that leads only to more water.
        Vector3 best = goal;
        float cost = 1e9f, length = 0.0f;
        for (int i = 0; i < BridgeCount(); i++) {
            Vector3 a, b;
            BridgeEnds(i, &a, &b);
            Vector3 mid = Vector3Lerp(a, b, 0.5f);
            if (mid.x < e.x || mid.x > e.x + e.width || mid.z < e.y || mid.z > e.y + e.height) continue;
            float span = XZDist(a, b);
            // On the bridge or at one end: over to whichever end is nearer the goal.
            if (XZDist(at, a) + XZDist(at, b) < span + 3.0f) return XZDist(a, goal) < XZDist(b, goal) ? a : b;
            for (int k = 0; k < 2; k++) {
                Vector3 near = k ? b : a, far = k ? a : b;
                if (Crosses(at, near, e)) continue;
                float l2 = XZDist(at, near) + span + XZDist(far, goal);
                if (l2 < cost) { cost = length = l2; best = near; }
            }
        }
        for (int k = 0; k < 4; k++) {
            Vector3 corner = { (k & 1) ? r.x + r.width + 2.0f : r.x - 2.0f, at.y, (k & 2) ? r.y + r.height + 2.0f : r.y - 2.0f };
            if (XZDist(at, corner) < 1.5f || Crosses(at, corner, e) || InWater(corner.x, corner.z) || SolidAt(corner.x, corner.z, 0.5f)) continue;
            float l2 = XZDist(at, corner) + XZDist(corner, goal), c2 = l2 + (Crosses(corner, goal, e) ? 500.0f : 0.0f);
            if (c2 < cost) { cost = c2; length = l2; best = corner; }
        }
        return length - XZDist(at, goal) < 150.0f ? best : goal;
    }
    return goal;
}

// Water just ahead, narrow enough to jump (a canal, as a player jumps it): starts the jump.
static bool JumpWater(Vitals *v, float angle)
{
    float dx = sinf(angle), dz = cosf(angle);
    bool wet = false;
    for (float d = 0.25f; d <= 6.0f; d += 0.25f) {
        float x = v->at.x + dx * d, z = v->at.z + dz * d;
        if (InWater(x, z)) {
            wet = true;
            continue;
        }
        if (!wet) {
            if (d > 1.0f) return false;
            continue;
        }
        float lx = x + dx * 0.8f, lz = z + dz * 0.8f, ly = GroundHeight(lx, lz, v->at.y + 1.0f);
        if (InWater(lx, lz) || BlockedAt(lx, lz, ly) || fabsf(ly - v->at.y) > 1.5f) return false;
        v->jumpFrom = v->at;
        v->jumpTo = (Vector3){ lx, ly, lz };
        v->jumpT = 0;
        v->jumpDur = 0.3f + 0.08f * (d + 0.8f);
        v->face = angle;
        return true;
    }
    return false;
}

// Heads for the goal, trying side steps around walls and people, jumping canals and finding a way
// round wider water; true once within 'stop'. Round something big (the ziggurat, a block of houses) it
// keeps to one side, as if feeling along the wall, and tries the other side when that gets it nowhere.
static bool StepToward(Vitals *v, int owner, Vector3 goal, float speed, float dt, float stop)
{
    static const float tries[9] = { 0.0f, 0.7f, -0.7f, 1.4f, -1.4f, 2.1f, -2.1f, 2.8f, -2.8f };
    if (v->jumpDur > 0) {
        v->jumpT += dt;
        float k = fminf(1.0f, v->jumpT / v->jumpDur);
        v->at = Vector3Lerp(v->jumpFrom, v->jumpTo, k);
        v->at.y += 3.6f * k * (1.0f - k);
        if (k >= 1.0f) v->jumpDur = 0;
        return false;
    }
    float dx = goal.x - v->at.x, dz = goal.z - v->at.z, d = sqrtf(dx * dx + dz * dz), left = d;
    if (d <= stop + 0.05f) return true;
    Vector3 way = AroundWater(v->at, goal);
    if (way.x != goal.x || way.z != goal.z) {
        dx = way.x - v->at.x;
        dz = way.z - v->at.z;
        d = fmaxf(sqrtf(dx * dx + dz * dz), stop + 0.1f);
    }

    if (v->detour == 0) v->detour = (owner & 1) ? 1 : -1;
    if (left < v->bestLeft - 0.5f || v->bestLeft <= 0 || left > v->bestLeft + 4.0f) {   // nearer, or a new goal
        v->bestLeft = left;
        v->detourT = 0;
    } else if ((v->detourT += dt) > 3.0f) {
        v->detour = (signed char)-v->detour;   // no nearer for a while: try round the other way
        v->detourT = 0;
        v->bestLeft = left;
    }
    float base = atan2f(dx, dz), step = fminf(speed * dt, d - stop);
    // Turning away from the goal only pays when it is still some way off; near it, side steps do.
    for (int k = 0; k < (left > 3.0f ? 9 : 5); k++) {
        float a = base + tries[k] * (float)v->detour;   // the side it keeps to first, then the other
        float nx = v->at.x + sinf(a) * step, nz = v->at.z + cosf(a) * step;
        Vector3 next = { nx, v->at.y, nz };
        if (BlockedAt(nx, nz, v->at.y)) {
            if (k == 0 && JumpWater(v, a)) return false;
            continue;
        }
        if (BodyAhead(v->at, next, 0.35f, owner)) continue;
        v->at.x = nx;
        v->at.z = nz;
        v->at.y = GroundHeight(nx, nz, v->at.y + 0.5f);
        v->face = a;
        return false;
    }
    // Every way blocked: within a step or two of the goal is close enough.
    return left < stop + 1.5f;
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
    if (!(p->flags & PF_PARRY) || p->equip != EQ_MELEE) return false;
    Vector2 to = Vector2Normalize((Vector2){ from.x - p->pos.x, from.z - p->pos.z });
    return to.x * sinf(p->facing) + to.y * cosf(p->facing) > 0.25f;
}

static void Strike(const Fighter *f, int s)
{
    Vitals *v = f->v;
    const RoleDef *r = &ROLES[f->role];
    Vector3 pp = PlayerAt(s);

    v->cool = r->cooldown;
    v->swing = NPC_SWING;
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
            BlowTurnedAside(ChestAhead());
        } else {
            NetParried(s);
        }
        SpawnBurst((Vector3){ (v->at.x + pp.x) / 2, pp.y + 1.3f, (v->at.z + pp.z) / 2 }, (Color){ 255, 230, 150, 255 }, 8, 2.5f);
    } else {
        v->mark = MARK_HIT;
        HurtSlotFrom(s, r->damage, Cause(f), &v->at);
    }
}

static void StrikeFoe(const Fighter *f, const Fighter *t)
{
    Vitals *v = f->v, *tv = t->v;
    Vector3 tp = FPos(t);

    v->cool = ROLES[f->role].cooldown;
    v->swing = NPC_SWING;
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
        if (d < bestD && fabsf(p.y - pos.y) < 6.0f && InReach(g, p)) {
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
        if (foe && (XZDist(FPos(foe), pos) > 25.0f || (f->role == ROLE_GUARD && !InReach(f, FPos(foe))))) foe = NULL;
    }
    if (!foe && f->role == ROLE_GUARD) foe = SpotRaider(f, pos);
    v->foeT = foe ? fmaxf(v->foeT, 2.0f) : 0.0f;
    return foe;
}

// The player this fighter goes for on sight: raiders anyone up and about, the city's guards someone
// wanted within the city's lands.
static int SpotPlayer(const Fighter *f, Vector3 pos)
{
    int best = -1;
    float bestD = 1e9f;
    if (!IsRaider(f->role) && f->kind != FK_CITY_GUARD) return -1;
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!PlayerAwake(s) || (f->kind == FK_CITY_GUARD && (G.peers[s].wanted <= 0 || !InReach(f, G.peers[s].pos)))) continue;
        Vector3 p = G.peers[s].pos;
        float d = XZDist(pos, p);
        if (d < SightOf(f->role, s) && fabsf(pos.y - p.y) < 6.0f && d < bestD) {
            best = s;
            bestD = d;
        }
    }
    return best;
}

// Whether an arrow from 'a' would reach 'b' with nothing solid in the way.
static bool ClearShot(Vector3 a, Vector3 b)
{
    float d = Vector3Distance(a, b);
    for (float s = 1.0f; s < d - 0.8f; s += 1.0f)
        if (PointInSolid(Vector3Lerp(a, b, s / d))) return false;
    return true;
}

// How far an archer shoots at player s: its bow's reach, less in the dark.
static float BowRange(const Fighter *f, int s)
{
    return fminf(BOW_RANGE, SightOf(f->role, s) * 1.15f);
}

// The arrow leaves the bow, aimed where the player will be (if they are on the move) and not quite
// true.
static void Loose(const Fighter *f, int s)
{
    Vitals *v = f->v;
    const Peer *p = &G.peers[s];
    Vector3 from = { v->at.x + sinf(v->face) * 0.6f, v->at.y + 1.5f, v->at.z + cosf(v->face) * 0.6f };
    Vector3 to = { p->pos.x, p->pos.y + 1.1f, p->pos.z };
    float speed = NPC_BOW_SPEED[f->bow], flight = Vector3Distance(from, to) / speed, miss = 0.15f + 0.02f * Vector3Distance(from, to);
    if (p->flags & PF_MOVE) {
        float run = (p->flags & PF_RUN) ? 7.5f : (p->flags & PF_CROUCH) ? 2.0f : 4.5f;
        to.x += sinf(p->facing) * run * flight * 0.8f;
        to.z += cosf(p->facing) * run * flight * 0.8f;
    }
    to.x += RandF(-miss, miss);
    to.y += RandF(-0.3f, 0.3f) * miss;
    to.z += RandF(-miss, miss);
    LooseFor(from, to, speed, (int)NPC_BOW_DAMAGE[f->bow], Cause(f));
}

// An archer stands, draws for a moment and looses at player s, then draws again. It does not move
// while its bow is drawn.
static void Archery(const Fighter *f, int s, float dt)
{
    Vitals *v = f->v;
    Vector3 tp = G.peers[s].pos;
    v->face = atan2f(tp.x - v->at.x, tp.z - v->at.z);
    if (v->drawing) {
        v->windup -= dt;
        if (v->windup > 0) return;
        v->drawing = false;
        v->windup = 0;
        v->cool = BOW_REST;
        Loose(f, s);
        return;
    }
    if (v->cool > 0) return;
    Vector3 from = { v->at.x, v->at.y + 1.5f, v->at.z }, to = { tp.x, tp.y + 1.2f, tp.z };
    if (!ClearShot(from, to)) {
        v->cool = 0.8f;
        return;
    }
    v->drawing = true;
    v->windup = BOW_DRAW_TIME;
    v->mark = MARK_NONE;
}

static void UpdateBlows(float dt);

void UpdateCombat(float dt)
{
    CollectFighters();
    UpdateBlows(dt);
    for (int a = 0; a < MAX_PLAYERS; a++)
        for (int b = 0; b < MAX_PLAYERS; b++)
            if (struck[a][b] > 0) struck[a][b] -= dt;
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
        // Someone who got too far from home is let go, though the anger lasts its time.
        bool reach = t >= 0 && InReach(f, G.peers[t].pos);
        if (v->hostile > 0 && !reach) v->hostile -= dt;
        bool angry = v->hostile > 0 && reach;

        // An archer shoots at whoever it is after: a city guard once they have run beyond its reach,
        // a village guard or a bowman from any distance beyond a few steps.
        bool aim = f->bow > 0 && v->hostile > 0 && t >= 0 && !foe && fabsf(pos.y - G.peers[t].pos.y) < 8.0f &&
                   (v->drawing || (dist <= BowRange(f, t) && dist > 4.0f && (!reach || f->kind != FK_CITY_GUARD)));
        if (aim) {
            if (!v->override) {
                v->override = true;
                v->at = pos;
                v->face = f->homeFace;
            }
            if (reach) v->hostile -= dt;
            Archery(f, t, dt);
            continue;
        }
        if (v->drawing) {
            v->drawing = false;
            v->windup = 0;
        }

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

// The acting player struck a village's folk: the whole village turns on them for two minutes.
static void RouseVillage(int village)
{
    for (int i = 0; i < fighterCount; i++) {
        const Fighter *f = &fighters[i];
        if (f->kind != FK_VILLAGER || f->village != village || f->v->dead) continue;
        f->v->hostile = fmaxf(f->v->hostile, VILLAGE_RAGE);
        f->v->target = G.actor;
    }
    Say("The whole village turns on you! Its people will fight you for two minutes.");
}

// Everyone who saw the acting player strike turns on them. In the city's lands the guards want them
// too; out in the country no one tells the city, and a village stands together.
static void RaiseAlarm(const Fighter *victim)
{
    int s = G.actor, witnesses = 0;
    Vector3 pp = PlayerAt(s), at = FPos(victim);
    bool city = InCityLands(at.x, at.z);

    if (victim->kind == FK_VILLAGER && victim->village >= 0) {
        RouseVillage(victim->village);
        return;
    }
    for (int i = 0; i < fighterCount; i++) {
        const Fighter *f = &fighters[i];
        if (f == victim || f->v->dead || IsRaider(f->role) || (f->kind == FK_CITY_GUARD && !city)) continue;
        Vector3 p = FPos(f);
        if (XZDist(p, pp) > SightOf(f->role, s) || fabsf(p.y - pp.y) > 6.0f) continue;
        f->v->hostile = fmaxf(f->v->hostile, Fights(f) ? 45.0f : 20.0f);
        f->v->target = s;
        witnesses++;
    }
    if (witnesses && city) {
        if (PlayerIn(s)) G.peers[s].wanted = WANTED_TIME;
        Say(witnesses > 1 ? "%d people saw you attack. Guards will hunt you!" : "%d person saw you attack. Guards will hunt you!", witnesses);
    } else if (witnesses) {
        Say(witnesses > 1 ? "%d people saw you attack. Out here no one will tell the city's guards." : "%d person saw you attack. Out here no one will tell the city's guards.", witnesses);
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
    // A blow (not an arrow) drives them back a step.
    Vector3 from = PlayerAt(G.actor);
    Vector3 away = { v->at.x - from.x, 0, v->at.z - from.z };
    float gap = Vector3Length(away);
    if (G.meleeing && gap > 0.1f && gap < 4.0f) {
        Vector3 to = Vector3Add(v->at, Vector3Scale(away, 0.35f / gap));
        if (!SolidAt(to.x, to.z, 0.3f) && !InWater(to.x, to.z)) v->at = to;
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

// Players who can be hurt by another player's blow or arrow: up and about, not lying in bed.
static bool Hittable(int s, int skip)
{
    return s != skip && PlayerAwake(s) && !(G.peers[s].flags & PF_BED);
}

int PlayerOnSegment(Vector3 a, Vector3 b, int skip)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!Hittable(s, skip)) continue;
        Vector3 p = G.peers[s].pos;
        if (SegmentHit(a, b, (Vector3){ p.x, p.y + 0.7f, p.z }, 0.42f) || SegmentHit(a, b, (Vector3){ p.x, p.y + 1.45f, p.z }, 0.38f)) return s;
    }
    return -1;
}

// Players within reach in front, nearest first.
int PlayersInArc(Vector3 chest, Vector3 fwd, float range, float minDot, int skip, int *out, int max)
{
    float dist[MAX_PLAYERS];
    int n = 0;
    for (int s = 0; s < MAX_PLAYERS && n < max; s++) {
        if (!Hittable(s, skip)) continue;
        Vector3 p = G.peers[s].pos, d = { p.x - chest.x, 0, p.z - chest.z };
        float len = Vector3Length(d);
        if (len - PLAYER_RADIUS > range || fabsf(p.y + 1.0f - chest.y) > 1.6f) continue;
        if (len > 0.3f && Vector3DotProduct(Vector3Scale(d, 1.0f / len), fwd) < minDot) continue;
        int k = n++;
        while (k > 0 && dist[k - 1] > len) {
            out[k] = out[k - 1];
            dist[k] = dist[k - 1];
            k--;
        }
        out[k] = s;
        dist[k] = len;
    }
    return n;
}

// Blows between players. A swing at another player takes PVP_WINDUP to land, like a person's does, and
// for that moment the mark over the swinger is yellow: the one about to be hit can raise their weapon in
// time (a person's warning is longer, theirs is short on purpose, but it is there). Then the mark says
// how it came out: green, turned aside (the swinger is stunned and left open to a quick answer), or red,
// it landed. The host settles it all.
#define MAX_BLOWS 16
typedef struct { bool on, opened; int a, s; float t, dmg; } Blow;
static Blow blows[MAX_BLOWS];

static void SetMark(int a, int mark, float secs)
{
    if (a < 0 || a >= MAX_PLAYERS) return;
    G.peers[a].mark = mark;
    G.peers[a].markT = secs;
}

static void StartBlow(int a, int s, float dmg)
{
    Blow *slot = &blows[0];
    for (int k = 0; k < MAX_BLOWS; k++) {
        if (blows[k].on && blows[k].a == a && blows[k].s == s) return;
        if (!blows[k].on) slot = &blows[k];
    }
    // Someone parried in the last moment leaves the one whose blow was turned aside open to a quick answer.
    bool opened = G.peers[s].mark == MARK_PARRIED && G.peers[s].markT > 0;
    *slot = (Blow){ true, opened, a, s, PVP_WINDUP, dmg };
    SetMark(a, MARK_WARN, PVP_WINDUP + 0.1f);
}

// The blow's damage reaches player s, from player a. Whoever starts a fight in the city's lands is wanted
// by the guards, unless the game is in combat mode; hitting back, or hitting someone wanted, is no crime.
static void Hurt(int a, int s, float dmg, bool opened, bool melee)
{
    Vector3 sp = G.peers[s].pos;
    bool provoked = G.peers[s].wanted > 0 || struck[s][a] > 0, fresh = struck[a][s] <= 0;
    struck[a][s] = PROVOKED;
    // The warning first: a blow that brings them down leaves its own message.
    if (fresh && !provoked) SayTo(s, "%s attacks you!", G.peers[a].name);
    HurtSlotFrom(s, dmg * PVP_SCALE * (opened ? RIPOSTE : 1.0f), BY_PLAYER + a, &G.peers[a].pos);
    G.meleeing = melee;
    HitMark();
    G.meleeing = false;
    // The one who swung feels the blow land as it lands.
    if (melee) {
        Vector3 at = { sp.x, sp.y + 1.2f, sp.z };
        bool heavy = G.peers[a].equip == EQ_MACE;
        if (a == G.self) BlowLanded(at, heavy);
        else NetBlowLanded(a, at, heavy);
    }
    SpawnBurst((Vector3){ sp.x, sp.y + 1.2f, sp.z }, (Color){ 170, 30, 30, 255 }, 10, 2.2f);
    if (!G.combat && !provoked && InCityLands(G.peers[a].pos.x, G.peers[a].pos.z)) {
        if (G.peers[a].wanted <= 0) Say("You attacked %s in the city. Guards will hunt you!", G.peers[s].name);
        G.peers[a].wanted = WANTED_TIME;
    }
}

// A blow's moment has come: it lands, or is turned aside, or finds no one.
static void LandBlow(const Blow *b)
{
    int a = b->a, s = b->s, saved = G.actor;
    Vector3 from = G.peers[a].pos, sp = G.peers[s].pos;

    G.actor = a;   // what this says and the hit mark it gives are for the one who swung
    if (!G.pvp || !Hittable(s, a) || !PlayerIn(a) || XZDist(from, sp) > 3.4f || fabsf(sp.y - from.y) > 1.8f) {
        SetMark(a, MARK_NONE, 0);
    } else if (PlayerParries(s, from)) {
        if (s == G.self) {
            G.parryT = 0;
            G.parryCool = 0;
            BlowTurnedAside(ChestAhead());
        } else {
            NetParried(s);
        }
        if (a == G.self) {
            G.swingCool += PARRY_STUN;
            BlowTurnedAside(ChestAhead());
        } else {
            NetStunned(a, PARRY_STUN);
        }
        SetMark(a, MARK_PARRIED, MARK_TIME);
        SpawnBurst((Vector3){ (from.x + sp.x) / 2, sp.y + 1.3f, (from.z + sp.z) / 2 }, (Color){ 255, 230, 150, 255 }, 8, 2.5f);
        Say("%s turned your blow aside!", G.peers[s].name);
    } else {
        Hurt(a, s, b->dmg, b->opened, true);
        SetMark(a, MARK_HIT, MARK_TIME);
    }
    G.actor = saved;
}

static void UpdateBlows(float dt)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        Peer *p = &G.peers[s];
        bool swinging = false;
        for (int k = 0; k < MAX_BLOWS; k++) swinging |= blows[k].on && blows[k].a == s;
        if (p->markT > 0) p->markT -= dt;
        else if (!swinging) p->mark = MARK_NONE;
    }
    for (int k = 0; k < MAX_BLOWS; k++) {
        Blow *b = &blows[k];
        if (!b->on) continue;
        b->t -= dt;
        if (b->t > 0) continue;
        b->on = false;
        LandBlow(b);
    }
}

// The acting player's blow or arrow reaches player s, if the host's game allows players to fight.
// An arrow lands at once; a swing of a weapon takes its moment to land (see above), and a raised weapon
// turns it aside.
void HitPlayer(int s, float dmg, Vector3 from, bool melee)
{
    int a = G.actor;
    (void)from;
    if (!G.pvp || !Hittable(s, a) || !PlayerIn(a)) return;
    if (melee) StartBlow(a, s, dmg);
    else Hurt(a, s, dmg, false, false);
}

// A player brought down (or gone): old fights with them are over.
void ForgetFights(int slot)
{
    if (slot < 0 || slot >= MAX_PLAYERS) return;
    for (int k = 0; k < MAX_PLAYERS; k++) struck[slot][k] = struck[k][slot] = 0;
    for (int k = 0; k < MAX_BLOWS; k++)
        if (blows[k].on && (blows[k].a == slot || blows[k].s == slot)) blows[k].on = false;
    SetMark(slot, MARK_NONE, 0);
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
                                          { IT_ARROWS, 6 }, { IT_SHEEPSKIN, 1 }, { IT_FISH, 4 } };
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
