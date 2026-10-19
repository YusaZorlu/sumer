// Multiplayer: what the host and its guests tell each other. The host runs the whole world and sends
// a picture of it ten times a second. Each guest runs only its own player: it reports where it is,
// and asks the host before it takes anything from the world (a carcass, a resource, the eggs...).
#include "game.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NET_VERSION   5
#define SNAP_EVERY    0.1f
#define STATE_EVERY   0.066f
#define RECORD_EVERY  15.0f
#define SAVE_EVERY    60.0f
#define SAVE_SOON     2.0f
#define SLOW_EVERY    5
#define MAX_PENDING   24
#define MAX_EFFECTS   48
#define MSG_MAX       60000
#define MAX_ARGS      REQ_ARGS

enum { M_HELLO = 1, M_WELCOME, M_ROSTER, M_SNAP, M_STATE, M_REQ, M_REPLY, M_EVENT, M_RECORD, M_KICK };
enum { EV_HURT = 1, EV_SAY, EV_HIT, EV_IMPACT, EV_SKIP, EV_PARRIED, EV_STUNNED };

typedef struct { int id, kind, args[MAX_ARGS], nargs; float age; } Pending;
typedef struct { Vector3 pos; Color color; int count; float speed; bool ring; } Effect;

static enum { PHASE_OFF, PHASE_CONNECTING, PHASE_WAITING, PHASE_PLAYING } phase;
static bool hosting;
static char myName[NAME_LEN], myRoom[32], failure[200];
static float snapT, stateT, recordT, saveT, lostT;
static int snapCount, nextId = 1, worldSign;
static Pending pending[MAX_PENDING];
static Effect effects[MAX_EFFECTS];
static int effectCount;
static unsigned char out[MSG_MAX];
static Arrow netArrows[MAX_ARROWS];

static Writer Begin(int type)
{
    Writer w = { out, 0, MSG_MAX, false };
    W8(&w, (unsigned)type);
    return w;
}

// Positions in sixteenths of a meter (heights in 64ths), headings in 256ths of a turn.
static void WQ(Writer *w, float v, float scale)
{
    int q = (int)roundf(Clamp(v * scale, -32767.0f, 32767.0f));
    W16(w, (unsigned)q & 0xffffu);
}

static float RQ(Reader *r, float scale)
{
    return (float)(short)R16(r) / scale;
}

static void WPos(Writer *w, Vector3 p)
{
    WQ(w, p.x, 16.0f);
    WQ(w, p.y, 64.0f);
    WQ(w, p.z, 16.0f);
}

static Vector3 RPos(Reader *r)
{
    Vector3 p;
    p.x = RQ(r, 16.0f);
    p.y = RQ(r, 64.0f);
    p.z = RQ(r, 16.0f);
    return p;
}

static void WAngle(Writer *w, float a)
{
    float t = a / (2.0f * PI);
    t -= floorf(t);
    W8(w, (unsigned)(t * 256.0f) & 255u);
}

static float RAngle(Reader *r)
{
    return (float)R8(r) * 2.0f * PI / 256.0f;
}

static unsigned Byte(float v)
{
    return (unsigned)Clamp(v, 0.0f, 255.0f);
}

static int SlotOfNet(int net)
{
    for (int s = 1; s < MAX_PLAYERS; s++)
        if (G.peers[s].active && G.peers[s].net == net) return s;
    return -1;
}

static void SendGuest(int slot, Writer *w)
{
    if (!w->bad && PlayerIn(slot) && slot != G.self) NetSend(G.peers[slot].net, w->d, w->n);
}

static void SendAll(Writer *w)
{
    if (!w->bad) NetSend(255, w->d, w->n);
}

static void SendHost(Writer *w)
{
    if (!w->bad) NetSend(0, w->d, w->n);
}

// A fingerprint of the world built from the seed: a page with another version of the game builds a
// different one, and is turned away rather than shown a world that does not match.
int WorldSignature(void)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        h = (h ^ (unsigned)(int)(b->pos.x * 8.0f)) * 16777619u;
        h = (h ^ (unsigned)(int)(b->pos.z * 8.0f)) * 16777619u;
        h = (h ^ (unsigned)(int)(b->size.y * 8.0f)) * 16777619u;
    }
    for (int i = 0; i < G.resCount; i++) h = (h ^ (unsigned)(int)(G.res[i].pos.x * 8.0f + G.res[i].pos.z * 64.0f)) * 16777619u;
    h = (h ^ (unsigned)(G.animalCount * 1031 + G.villagerCount * 7919 + G.workerCount * 17)) * 16777619u;
    return (int)(h & 0x7fffffffu);
}

// Seconds since the host's last picture of the world (a guest only).
float NetSilence(void)
{
    return G.role == NET_CLIENT ? lostT : 0.0f;
}

bool Online(void)
{
    return G.role != NET_SOLO;
}

bool IsClient(void)
{
    return G.role == NET_CLIENT;
}

// ---- Vitals: hit points, fighting and where someone fighting stands ----

static void WVitals(Writer *w, const Vitals *v)
{
    unsigned flags = (v->used ? 1u : 0u) | (v->dead ? 2u : 0u) | (v->override ? 4u : 0u) | (v->looted ? 8u : 0u) |
                     (v->hostile > 0 ? 16u : 0u) | ((unsigned)(v->mark & 3) << 5) | (v->drawing ? 128u : 0u);
    W8(w, flags);
    if (!v->used) return;
    W16(w, (unsigned)Clamp(v->hp * 4.0f, 0.0f, 65535.0f));
    if (v->override) {
        WPos(w, v->at);
        WAngle(w, v->face);
    }
    W8(w, Byte(v->swing * 200.0f));
    W8(w, Byte(v->windup * 200.0f));
    W8(w, Byte(v->markT * 200.0f));
    W8(w, v->hostile > 0 && v->target >= 0 && v->target < MAX_PLAYERS ? (unsigned)v->target : 255u);
}

static void RVitals(Reader *r, Vitals *v)
{
    unsigned flags = R8(r);
    if (!(flags & 1u)) {
        *v = (Vitals){ .used = false };
        return;
    }
    v->used = true;
    v->hp = (float)R16(r) / 4.0f;
    bool over = (flags & 4u) != 0;
    if (over) {
        Vector3 at = RPos(r);
        float face = RAngle(r);
        if (!v->override || Vector3Distance(v->at, at) > 6.0f) v->at = at;
        v->netAt = at;
        v->face = face;
    }
    v->override = over;
    v->dead = (flags & 2u) != 0;
    v->looted = (flags & 8u) != 0;
    v->hostile = (flags & 16u) ? 1.0f : 0.0f;
    v->mark = (unsigned char)((flags >> 5) & 3u);
    v->drawing = (flags & 128u) != 0;
    v->swing = (float)R8(r) / 200.0f;
    v->windup = (float)R8(r) / 200.0f;
    v->markT = (float)R8(r) / 200.0f;
    unsigned t = R8(r);
    v->target = t == 255u ? -1 : (int)t;
}

// ---- The host's picture of the world ----

void NetEffect(Vector3 pos, Color c, int count, float speed, bool ring)
{
    if (G.role != NET_HOST || effectCount >= MAX_EFFECTS) return;
    bool seen = false;
    for (int s = 0; s < MAX_PLAYERS && !seen; s++)
        if (s != G.self && PlayerIn(s) && XZDist(G.peers[s].pos, pos) < 160.0f) seen = true;
    if (seen) effects[effectCount++] = (Effect){ pos, c, count, speed, ring };
}

static void WriteSnapshot(Writer *w, bool slow)
{
    W8(w, slow ? 1u : 0u);
    W16(w, (unsigned)G.day);
    WF(w, G.hour);
    W8(w, (G.raining ? 1u : 0u) | (G.rainToday ? 2u : 0u) | (G.rainDone ? 4u : 0u));
    WF(w, G.rainStart);
    WF(w, G.rainEnd);
    W16(w, (unsigned)G.seasonSeen);

    for (int s = 0; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        W8(w, p->active ? 1u : 0u);
        if (!p->active) continue;
        WPos(w, p->pos);
        WAngle(w, p->facing);
        WF(w, p->stride);
        W8(w, Byte(p->draw * 255.0f));
        W8(w, Byte(p->health));
        W8(w, Byte(p->swingT * 200.0f));
        W16(w, (unsigned)p->flags);
        W8(w, (unsigned)p->equip); W8(w, (unsigned)p->weapon); W8(w, (unsigned)p->bow); W8(w, (unsigned)p->armor);
        W16(w, (unsigned)Clamp(p->wanted, 0.0f, 65000.0f));
        W8(w, p->alone ? 1u : 0u);
        W8(w, (unsigned)(p->kills < 255 ? p->kills : 255));
        W8(w, (unsigned)(p->deaths < 255 ? p->deaths : 255));
        W8(w, (unsigned)(p->mark & 3));
    }

    W8(w, (unsigned)G.animalCount);
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        W8(w, (a->active ? 1u : 0u) | ((unsigned)a->state << 1));
        if (!a->active) continue;
        WPos(w, a->pos);
        WAngle(w, a->heading);
        W8(w, Byte(a->speed * 10.0f));
        W8(w, Byte((float)a->hp));
        W8(w, a->state == ST_HUNT && a->target >= 0 && a->target < MAX_PLAYERS ? (unsigned)a->target : 255u);
        if (a->type == AN_SNAKE) {
            WQ(w, a->lair.x, 16.0f);
            WQ(w, a->lair.z, 16.0f);
        }
    }

    W8(w, (unsigned)G.villagerCount);
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        W16(w, (unsigned)Clamp((v->t + 0.5f) / 2.0f * 65535.0f, 0.0f, 65535.0f));
        W8(w, (unsigned)(int)roundf(Clamp(v->side * 40.0f, -127.0f, 127.0f)) & 255u);
        W8(w, (v->dir > 0 ? 1u : 0u) | (v->moving ? 2u : 0u));
        W8(w, v->from >= 0 ? (unsigned)v->from : 255u);
        W8(w, v->to >= 0 ? (unsigned)v->to : 255u);
        WAngle(w, v->face);
        WVitals(w, &v->vit);
    }
    W8(w, (unsigned)G.workerCount);
    for (int i = 0; i < G.workerCount; i++) WVitals(w, &G.workers[i].vit);
    for (int i = 0; i < TR_COUNT; i++)
        if (!G.traders[i].roaming) WVitals(w, &G.traders[i].vit);
    for (int c = 0; c < MAX_CARAVANS; c++) {
        const Caravan *cv = &G.caravans[c];
        W8(w, (unsigned)cv->state);
        WF(w, cv->s);
        for (int k = 0; k <= CARAVAN_GUARDS; k++) WVitals(w, &cv->vit[k]);
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        const Raider *r = &G.raiders[i];
        WPos(w, r->pos);
        WAngle(w, r->face);
        W8(w, r->moving ? 1u : 0u);
        WVitals(w, &r->vit);
    }

    int arrows = 0;
    for (int i = 0; i < MAX_ARROWS; i++) arrows += G.arrows[i].active;
    W8(w, (unsigned)arrows);
    for (int i = 0; i < MAX_ARROWS; i++) {
        const Arrow *a = &G.arrows[i];
        if (!a->active) continue;
        W8(w, (unsigned)i);
        W8(w, (unsigned)(a->owner & 15) | (a->flying ? 16u : 0u) | (a->spent ? 32u : 0u));
        WPos(w, a->pos);
        WQ(w, a->vel.x, 16.0f); WQ(w, a->vel.y, 16.0f); WQ(w, a->vel.z, 16.0f);
        W8(w, Byte(a->fade * 200.0f));
    }

    W8(w, (unsigned)effectCount);
    for (int i = 0; i < effectCount; i++) {
        const Effect *e = &effects[i];
        WPos(w, e->pos);
        W8(w, e->color.r); W8(w, e->color.g); W8(w, e->color.b);
        W8(w, (unsigned)e->count);
        W8(w, Byte(e->speed * 20.0f));
        W8(w, e->ring ? 1u : 0u);
    }
    effectCount = 0;

    if (!slow) return;
    for (int i = 0; i < MAX_PLOTS; i++) {
        W8(w, (unsigned)G.plots[i].state);
        W8(w, Byte(G.plots[i].growth * 250.0f));
        W8(w, Byte(G.plots[i].dry * 40.0f));
    }
    WF(w, G.farmWater); WF(w, G.coopWater); WF(w, G.coopDry);
    W8(w, (unsigned)G.henCount); W8(w, (unsigned)G.coopEggs);
    WF(w, G.coopFeed); WF(w, G.layClock);
    for (int i = 0; i < G.henCount; i++) {
        WF(w, G.henBorn[i]);
        WF(w, G.henMeal[i]);
    }
    for (int c = 0; c < CAMPS; c++)
        for (int k = 0; k < 4; k++) {
            W8(w, (unsigned)G.stash[c][k].item);
            W8(w, (unsigned)G.stash[c][k].count);
        }
    int regrowing = 0;
    for (int i = 0; i < G.resCount; i++) regrowing += G.res[i].regrow > 0;
    W16(w, (unsigned)regrowing);
    for (int i = 0; i < G.resCount; i++) {
        if (G.res[i].regrow <= 0) continue;
        W16(w, (unsigned)i);
        W16(w, (unsigned)Clamp(G.res[i].regrow * 100.0f, 1.0f, 65535.0f));
    }
}

// A guest's copy of a palm grown on the farm, for the plot the host says has one.
static void SyncPalms(void)
{
    for (int i = 0; i < MAX_PLOTS; i++) {
        if (G.plots[i].state != PLOT_PALM) continue;
        bool found = false;
        for (int k = 0; k < G.resCount && !found; k++)
            found = G.res[k].kind == RES_PALM && G.res[k].farm && XZDist(G.res[k].pos, G.plots[i].pos) < 0.5f;
        if (!found) {
            int k = AddResource(G.plots[i].pos.x, G.plots[i].pos.z, RES_PALM);
            if (k >= 0) G.res[k].farm = true;
        }
    }
}

static void ReadSnapshot(Reader *r)
{
    bool slow = (R8(r) & 1u) != 0;
    int day = (int)R16(r);
    G.hour = RF(r);
    G.day = day;
    unsigned weather = R8(r);
    G.raining = (weather & 1u) != 0;
    G.rainToday = (weather & 2u) != 0;
    G.rainDone = (weather & 4u) != 0;
    G.rainStart = RF(r);
    G.rainEnd = RF(r);
    G.seasonSeen = (int)R16(r);

    for (int s = 0; s < MAX_PLAYERS; s++) {
        Peer *p = &G.peers[s];
        bool active = R8(r) != 0;
        if (!active) {
            if (s != G.self) p->active = false;
            continue;
        }
        Vector3 pos = RPos(r);
        float facing = RAngle(r), stride = RF(r), draw = (float)R8(r) / 255.0f, health = (float)R8(r);
        float swing = (float)R8(r) / 200.0f;
        int flags = (int)R16(r), equip = (int)R8(r), weapon = (int)R8(r), bow = (int)R8(r), armor = (int)R8(r);
        float wanted = (float)R16(r);
        bool alone = R8(r) != 0;
        int kills = (int)R8(r);
        int deaths = (int)R8(r);
        int mark = (int)(R8(r) & 3u);
        p->kills = kills;
        p->deaths = deaths;
        p->mark = mark;
        if (s == G.self) {
            p->wanted = wanted;
            p->alone = alone;
            continue;
        }
        if (!p->active) {
            p->shown = pos;
            p->shownFacing = facing;
        }
        p->active = true;
        p->pos = pos;
        p->facing = facing;
        p->stride = stride;
        p->draw = draw;
        p->health = health;
        p->swingT = swing;
        p->flags = flags;
        p->equip = equip;
        p->weapon = weapon;
        p->bow = bow;
        p->armor = armor;
        p->wanted = wanted;
        p->alone = alone;
        p->heard = 0;
    }

    int animals = (int)R8(r);
    for (int i = 0; i < animals && !r->bad; i++) {
        Animal *a = i < G.animalCount ? &G.animals[i] : &(Animal){ 0 };
        unsigned bits = R8(r);
        bool active = (bits & 1u) != 0;
        if (!active) {
            a->active = false;
            continue;
        }
        Vector3 pos = RPos(r);
        float heading = RAngle(r);
        a->speed = (float)R8(r) / 10.0f;
        a->hp = (int)R8(r);
        unsigned target = R8(r);
        a->target = target == 255u ? -1 : (int)target;
        if (a->type == AN_SNAKE) {
            a->lair.x = RQ(r, 16.0f);
            a->lair.z = RQ(r, 16.0f);
            a->lair.y = TerrainHeight(a->lair.x, a->lair.z);
        }
        if (!a->active || Vector3Distance(a->pos, pos) > 8.0f) {
            a->pos = pos;
            a->heading = heading;
        }
        a->netPos = pos;
        a->netHeading = heading;
        a->state = (AnimalState)((bits >> 1) & 7u);
        a->active = true;
    }

    int villagers = (int)R8(r);
    for (int i = 0; i < villagers && !r->bad; i++) {
        Villager *v = i < G.villagerCount ? &G.villagers[i] : &(Villager){ 0 };
        float t = (float)R16(r) / 65535.0f * 2.0f - 0.5f, side = (float)(signed char)R8(r) / 40.0f;
        unsigned bits = R8(r);
        int from = (int)R8(r);
        int to = (int)R8(r);
        float face = RAngle(r);
        from = from < StreetNodeCount() ? from : -1;
        to = to < StreetNodeCount() ? to : -1;
        if (from != v->from || to != v->to) {
            // On to another stretch of street: no gliding back along the old one.
            if (from >= 0 && to >= 0) SetStreet(v, from, to);
            v->from = from;
            v->to = to;
            v->t = t;
            v->side = side;
        }
        if (fabsf(v->t - t) > 0.15f) v->t = t;
        if (fabsf(v->side - side) > 1.5f) v->side = side;
        v->netT = t;
        v->netSide = side;
        v->dir = (bits & 1u) ? 1.0f : -1.0f;
        v->moving = (bits & 2u) != 0;
        v->face = face;
        RVitals(r, &v->vit);
    }
    int workers = (int)R8(r);
    for (int i = 0; i < workers && !r->bad; i++) RVitals(r, i < G.workerCount ? &G.workers[i].vit : &(Vitals){ 0 });
    for (int i = 0; i < TR_COUNT; i++)
        if (!G.traders[i].roaming) RVitals(r, &G.traders[i].vit);
    for (int c = 0; c < MAX_CARAVANS; c++) {
        Caravan *cv = &G.caravans[c];
        CaravanState state = (CaravanState)R8(r);
        float s = RF(r);
        if (state != cv->state || fabsf(cv->s - s) > 10.0f) cv->s = s;
        cv->state = state;
        cv->netS = s;
        for (int k = 0; k <= CARAVAN_GUARDS; k++) RVitals(r, &cv->vit[k]);
        Trader *t = &G.traders[cv->trader];
        t->away = cv->state != CV_TRADE || cv->vit[0].dead || cv->vit[0].override;
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        Raider *rd = &G.raiders[i];
        Vector3 pos = RPos(r);
        rd->face = RAngle(r);
        rd->moving = R8(r) != 0;
        if (Vector3Distance(rd->pos, pos) > 8.0f) rd->pos = pos;
        rd->netPos = pos;
        RVitals(r, &rd->vit);
    }

    bool seen[MAX_ARROWS] = { false };
    int arrows = (int)R8(r);
    for (int k = 0; k < arrows && !r->bad; k++) {
        int i = (int)R8(r);
        unsigned bits = R8(r);
        Vector3 pos = RPos(r), vel;
        vel.x = RQ(r, 16.0f);
        vel.y = RQ(r, 16.0f);
        vel.z = RQ(r, 16.0f);
        float fade = (float)R8(r) / 200.0f;
        int owner = (int)(bits & 15u);
        if (i >= MAX_ARROWS || owner == G.self) continue;
        Arrow *a = &netArrows[i];
        if (!a->active || Vector3Distance(a->pos, pos) > 4.0f) *a = (Arrow){ .pos = pos, .active = true };
        a->pos = pos;
        a->vel = vel;
        a->flying = (bits & 16u) != 0;
        a->spent = (bits & 32u) != 0;
        a->fade = fade;
        a->owner = owner;
        seen[i] = true;
    }
    for (int i = 0; i < MAX_ARROWS; i++)
        if (!seen[i]) netArrows[i].active = false;

    int fx = (int)R8(r);
    for (int k = 0; k < fx && !r->bad; k++) {
        Vector3 pos = RPos(r);
        Color c = { 0, 0, 0, 255 };
        c.r = (unsigned char)R8(r);
        c.g = (unsigned char)R8(r);
        c.b = (unsigned char)R8(r);
        int count = (int)R8(r);
        float speed = (float)R8(r) / 20.0f;
        bool ring = R8(r) != 0;
        if (ring) SpawnRing(pos, c);
        else SpawnBurst(pos, c, count, speed);
    }

    if (!slow || r->bad) return;
    for (int i = 0; i < MAX_PLOTS; i++) {
        G.plots[i].state = (PlotState)R8(r);
        G.plots[i].growth = (float)R8(r) / 250.0f;
        G.plots[i].dry = (float)R8(r) / 40.0f;
    }
    G.farmWater = RF(r); G.coopWater = RF(r); G.coopDry = RF(r);
    int hens = (int)R8(r);
    G.coopEggs = (int)R8(r);
    G.coopFeed = RF(r); G.layClock = RF(r);
    if (hens > MAX_HENS) hens = MAX_HENS;
    for (int i = G.henCount; i < hens; i++)
        G.hens[i] = (Wanderer){ Vector3Add(G.coop, (Vector3){ RandF(-1.5f, 1.5f), 0, RandF(-1, 1) }), RandF(-PI, PI), 0, 0 };
    G.henCount = hens;
    for (int i = 0; i < hens; i++) {
        G.henBorn[i] = RF(r);
        G.henMeal[i] = RF(r);
    }
    for (int c = 0; c < CAMPS; c++)
        for (int k = 0; k < 4; k++) {
            int item = (int)R8(r), count = (int)R8(r);
            G.stash[c][k] = (Slot){ item < IT_COUNT ? item : IT_NONE, item < IT_COUNT ? count : 0 };
        }
    if (r->bad) return;
    SyncPalms();
    for (int i = 0; i < G.resCount; i++) G.res[i].regrow = 0;
    int regrowing = (int)R16(r);
    for (int k = 0; k < regrowing && !r->bad; k++) {
        int i = (int)R16(r);
        float h = (float)R16(r) / 100.0f;
        if (i < G.resCount) G.res[i].regrow = h;
    }
}

// Between pictures from the host everything glides toward where it was last seen.
static void Ease(float dt)
{
    float k = fminf(1.0f, dt * 10.0f);
    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (!a->active) continue;
        a->pos = Vector3Lerp(a->pos, a->netPos, k);
        float d = atan2f(sinf(a->netHeading - a->heading), cosf(a->netHeading - a->heading));
        a->heading += d * k;
    }
    for (int i = 0; i < G.villagerCount; i++) {
        Villager *v = &G.villagers[i];
        v->t += (v->netT - v->t) * k;
        v->side += (v->netSide - v->side) * k;
        if (v->vit.override) v->vit.at = Vector3Lerp(v->vit.at, v->vit.netAt, k);
    }
    for (int i = 0; i < G.workerCount; i++)
        if (G.workers[i].vit.override) G.workers[i].vit.at = Vector3Lerp(G.workers[i].vit.at, G.workers[i].vit.netAt, k);
    for (int i = 0; i < TR_COUNT; i++)
        if (G.traders[i].vit.override) G.traders[i].vit.at = Vector3Lerp(G.traders[i].vit.at, G.traders[i].vit.netAt, k);
    for (int c = 0; c < MAX_CARAVANS; c++) {
        Caravan *cv = &G.caravans[c];
        cv->s += (cv->netS - cv->s) * k;
        for (int g = 0; g <= CARAVAN_GUARDS; g++)
            if (cv->vit[g].override) cv->vit[g].at = Vector3Lerp(cv->vit[g].at, cv->vit[g].netAt, k);
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        Raider *r = &G.raiders[i];
        r->pos = Vector3Lerp(r->pos, r->netPos, k);
        if (r->vit.override) r->vit.at = Vector3Lerp(r->vit.at, r->vit.netAt, k);
    }
    for (int i = 0; i < MAX_ARROWS; i++) {
        Arrow *a = &netArrows[i];
        if (!a->active) continue;
        if (a->spent) a->fade = fmaxf(0.0f, a->fade - dt);
        if (!a->flying) continue;
        a->trailT -= dt;
        if (a->trailT <= 0) {
            a->trailT = 0.02f;
            for (int t = TRAIL_POINTS - 1; t > 0; t--) a->trail[t] = a->trail[t - 1];
            a->trail[0] = a->pos;
            if (a->trailCount < TRAIL_POINTS) a->trailCount++;
        }
        a->vel.y -= 5.0f * dt;
        a->pos = Vector3Add(a->pos, Vector3Scale(a->vel, dt));
    }
}

void DrawNetArrows(void)
{
    if (G.role != NET_CLIENT) return;
    for (int i = 0; i < MAX_ARROWS; i++)
        if (netArrows[i].active) DrawArrow(&netArrows[i]);
}

const Arrow *NetArrows(void)
{
    return netArrows;
}

// ---- Messages for one player, in their own language ----

typedef struct { char type; int i; float f; char s[96]; } SayArg;

// The arguments a printf format takes, read off the list by kind: whole numbers, decimals, words.
static int TakeArgs(const char *fmt, va_list ap, SayArg *args, int max)
{
    int n = 0;
    for (const char *c = fmt; *c && n < max; c++) {
        if (*c != '%') continue;
        c++;
        if (*c == '%' || !*c) {
            if (!*c) break;
            continue;
        }
        while (*c && strchr("-+ #0123456789.", *c)) c++;
        while (*c == 'l' || *c == 'h' || *c == 'z') c++;
        SayArg *a = &args[n];
        if (*c == 'd' || *c == 'i' || *c == 'u' || *c == 'x' || *c == 'c') {
            a->type = 'i';
            a->i = va_arg(ap, int);
        } else if (*c == 'f' || *c == 'g' || *c == 'e') {
            a->type = 'f';
            a->f = (float)va_arg(ap, double);
        } else if (*c == 's') {
            const char *str = va_arg(ap, const char *);
            a->type = 's';
            snprintf(a->s, sizeof(a->s), "%s", str ? str : "");
        } else {
            if (!*c) break;
            continue;
        }
        n++;
        if (!*c) break;
    }
    return n;
}

void NetSayV(int slot, const char *fmt, va_list ap)
{
    SayArg args[MAX_ARGS];
    int n = TakeArgs(fmt, ap, args, MAX_ARGS);
    Writer w = Begin(M_EVENT);
    W8(&w, EV_SAY);
    WStr(&w, fmt);
    W8(&w, (unsigned)n);
    for (int k = 0; k < n; k++) {
        W8(&w, (unsigned char)args[k].type);
        if (args[k].type == 'i') W32(&w, (unsigned)args[k].i);
        else if (args[k].type == 'f') WF(&w, args[k].f);
        else WStr(&w, args[k].s);
    }
    SendGuest(slot, &w);
}

// The kinds of each % in a format, so a translation that changed them is not trusted.
static int Kinds(const char *fmt, char *kinds, int max)
{
    int n = 0;
    for (const char *c = fmt; *c; c++) {
        if (*c != '%') continue;
        c++;
        if (*c == '%' || !*c) {
            if (!*c) break;
            continue;
        }
        while (*c && strchr("-+ #0123456789.", *c)) c++;
        while (*c == 'l' || *c == 'h' || *c == 'z') c++;
        char k = (*c == 'd' || *c == 'i' || *c == 'u' || *c == 'x' || *c == 'c') ? 'i' : (*c == 'f' || *c == 'g' || *c == 'e') ? 'f' : *c == 's' ? 's' : 0;
        if (k && n < max) kinds[n++] = k;
        if (!*c) break;
    }
    return n;
}

// Formats with the words translated; false when the format does not fit the arguments.
static bool FormatSay(char *dst, int size, const char *fmt, const SayArg *args, int n)
{
    char kinds[16];
    if (Kinds(fmt, kinds, 16) != n) return false;
    for (int k = 0; k < n; k++)
        if (kinds[k] != args[k].type) return false;
    int len = 0, next = 0;
    dst[0] = '\0';
    for (const char *c = fmt; *c && len < size - 1; c++) {
        if (*c != '%') {
            dst[len++] = *c;
            dst[len] = '\0';
            continue;
        }
        const char *start = c++;
        if (*c == '%') {
            dst[len++] = '%';
            dst[len] = '\0';
            continue;
        }
        while (*c && strchr("-+ #0123456789.", *c)) c++;
        while (*c == 'l' || *c == 'h' || *c == 'z') c++;
        if (!*c) break;
        char spec[16];
        int sl = (int)(c - start) + 1;
        if (sl > 15) sl = 15;
        memcpy(spec, start, (size_t)sl);
        spec[sl] = '\0';
        const SayArg *a = next < n ? &args[next++] : NULL;
        if (!a) break;
        if (a->type == 'i') len += snprintf(dst + len, (size_t)(size - len), spec, a->i);
        else if (a->type == 'f') len += snprintf(dst + len, (size_t)(size - len), spec, (double)a->f);
        else len += snprintf(dst + len, (size_t)(size - len), spec, L(a->s));
        if (len >= size) len = size - 1;
    }
    return true;
}

// Shown in this player's language, or in English if the translation does not fit the arguments.
static void Show(const char *fmt, const SayArg *args, int n)
{
    if (!FormatSay(G.msg, (int)sizeof(G.msg), L(fmt), args, n) && !FormatSay(G.msg, (int)sizeof(G.msg), fmt, args, n)) return;
    G.msgT = 3.0f;
}

void LocalSayV(const char *fmt, va_list ap)
{
    SayArg args[MAX_ARGS];
    int n = TakeArgs(fmt, ap, args, MAX_ARGS);
    Show(fmt, args, n);
}

static void ShowSay(Reader *r)
{
    char fmt[256];
    SayArg args[MAX_ARGS];
    RStr(r, fmt, sizeof(fmt));
    int n = (int)R8(r);
    if (n > MAX_ARGS) n = MAX_ARGS;
    for (int k = 0; k < n; k++) {
        args[k].type = (char)R8(r);
        if (args[k].type == 'i') args[k].i = (int)R32(r);
        else if (args[k].type == 'f') args[k].f = RF(r);
        else RStr(r, args[k].s, sizeof(args[k].s));
    }
    if (!r->bad) Show(fmt, args, n);
}

void NetHurt(int slot, float dmg, int by, const Vector3 *src)
{
    Writer w = Begin(M_EVENT);
    W8(&w, EV_HURT);
    WF(&w, dmg);
    W8(&w, (unsigned)by);
    W8(&w, src ? 1u : 0u);
    if (src) {
        WF(&w, src->x);
        WF(&w, src->z);
    }
    SendGuest(slot, &w);
}

// The guest's blow was turned aside: they cannot swing again for a moment.
void NetStunned(int slot, float secs)
{
    Writer w = Begin(M_EVENT);
    W8(&w, EV_STUNNED);
    WF(&w, secs);
    SendGuest(slot, &w);
}

void NetHitMark(int slot)
{
    Writer w = Begin(M_EVENT);
    W8(&w, EV_HIT);
    SendGuest(slot, &w);
}

void NetImpact(int slot, Vector3 at)
{
    Writer w = Begin(M_EVENT);
    W8(&w, EV_IMPACT);
    WF(&w, at.x); WF(&w, at.y); WF(&w, at.z);
    SendGuest(slot, &w);
}

void NetParried(int slot)
{
    Writer w = Begin(M_EVENT);
    W8(&w, EV_PARRIED);
    SendGuest(slot, &w);
}

void NetSkip(float hours, bool rained, bool dawn)
{
    Writer w = Begin(M_EVENT);
    W8(&w, EV_SKIP);
    WF(&w, hours);
    W8(&w, (rained ? 1u : 0u) | (dawn ? 2u : 0u));
    SendAll(&w);
}

// ---- Requests: a guest asks the host before taking anything from the shared world ----

bool Asking(int kind, int first)
{
    for (int i = 0; i < MAX_PENDING; i++)
        if (pending[i].id && pending[i].kind == kind && (first < 0 || (pending[i].nargs > 0 && pending[i].args[0] == first))) return true;
    return false;
}

void Ask(int kind, const int *args, int n)
{
    if (G.role != NET_CLIENT || n > MAX_ARGS) return;
    Pending *slot = NULL;
    for (int i = 0; i < MAX_PENDING && !slot; i++)
        if (!pending[i].id) slot = &pending[i];
    if (!slot) return;
    *slot = (Pending){ nextId, kind, { 0 }, n, 0 };
    for (int k = 0; k < n; k++) slot->args[k] = args[k];
    nextId = nextId % 60000 + 1;
    Writer w = Begin(M_REQ);
    W16(&w, (unsigned)slot->id);
    W8(&w, (unsigned)kind);
    W8(&w, (unsigned)n);
    for (int k = 0; k < n; k++) W32(&w, (unsigned)args[k]);
    SendHost(&w);
}

// Shots and blows need no answer: the host plays them out and tells everyone what they hit.
void NetShoot(Vector3 from, Vector3 vel, int damage)
{
    Writer w = Begin(M_REQ);
    W16(&w, 0);
    W8(&w, RQ_SHOOT);
    WF(&w, from.x); WF(&w, from.y); WF(&w, from.z);
    WF(&w, vel.x); WF(&w, vel.y); WF(&w, vel.z);
    W16(&w, (unsigned)damage);
    SendHost(&w);
}

void NetStrike(Vector3 chest, Vector3 fwd, int equip, int weapon)
{
    Writer w = Begin(M_REQ);
    W16(&w, 0);
    W8(&w, RQ_STRIKE);
    WF(&w, chest.x); WF(&w, chest.y); WF(&w, chest.z);
    WF(&w, fwd.x); WF(&w, fwd.z);
    W8(&w, (unsigned)equip);
    W8(&w, (unsigned)weapon);
    SendHost(&w);
}

static Vector3 RVec(Reader *r)
{
    Vector3 v;
    v.x = RF(r);
    v.y = RF(r);
    v.z = RF(r);
    return v;
}

static void HostRequest(int slot, Reader *r)
{
    int id = (int)R16(r), kind = (int)R8(r);
    int prev = G.actor;
    G.actor = slot;
    if (kind == RQ_SHOOT) {
        Vector3 from = RVec(r), vel = RVec(r);
        int damage = (int)R16(r);
        if (!r->bad) ShootFor(slot, from, vel, damage);
    } else if (kind == RQ_STRIKE) {
        Vector3 chest = RVec(r), fwd = { 0, 0, 0 };
        fwd.x = RF(r);
        fwd.z = RF(r);
        int equip = (int)R8(r), weapon = (int)R8(r);
        fwd = Vector3Normalize(fwd);
        if (!r->bad && Vector3Distance(chest, G.peers[slot].pos) < 5.0f) StrikeFor(chest, fwd, equip, weapon);
    } else {
        int args[MAX_ARGS], res[MAX_ARGS], n = (int)R8(r), nres = 0;
        if (n > MAX_ARGS) n = MAX_ARGS;
        for (int k = 0; k < n; k++) args[k] = (int)R32(r);
        bool ok = !r->bad && ServeRequest(slot, kind, args, n, res, &nres);
        // Plots, the coop and the chests travel in the slower part of the picture: send it next.
        if (ok) snapCount = SLOW_EVERY - 1;
        if (id) {
            Writer w = Begin(M_REPLY);
            W16(&w, (unsigned)id);
            W8(&w, ok ? 1u : 0u);
            W8(&w, (unsigned)nres);
            for (int k = 0; k < nres; k++) W32(&w, (unsigned)res[k]);
            SendGuest(slot, &w);
        }
    }
    G.actor = prev;
}

static void ClientReply(Reader *r)
{
    int id = (int)R16(r);
    bool ok = R8(r) != 0;
    int n = (int)R8(r), res[MAX_ARGS];
    if (n > MAX_ARGS) n = MAX_ARGS;
    for (int k = 0; k < n; k++) res[k] = (int)R32(r);
    for (int i = 0; i < MAX_PENDING; i++) {
        Pending *p = &pending[i];
        if (p->id != id) continue;
        int kind = p->kind, args[MAX_ARGS], nargs = p->nargs;
        memcpy(args, p->args, sizeof(args));
        p->id = 0;
        if (!r->bad) ApplyReply(kind, args, nargs, ok, res, n);
        return;
    }
}

// ---- Roster, welcome and the player's own reports ----

static void SendRoster(void)
{
    Writer w = Begin(M_ROSTER);
    for (int s = 0; s < MAX_PLAYERS; s++) {
        W8(&w, G.peers[s].active ? 1u : 0u);
        WStr(&w, G.peers[s].active ? G.peers[s].name : "");
    }
    SendAll(&w);
}

static void ReadRoster(Reader *r)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        bool active = R8(r) != 0;
        char name[NAME_LEN];
        RStr(r, name, NAME_LEN);
        if (r->bad) return;
        if (s == G.self) continue;
        if (active) snprintf(G.peers[s].name, NAME_LEN, "%s", name);
        else G.peers[s].active = false;
    }
}

static void Kick(int net, const char *why)
{
    Writer w = Begin(M_KICK);
    WStr(&w, why);
    NetSend(net, w.d, w.n);
}

static void Snapshot(int slotOrAll, bool slow)
{
    Writer w = Begin(M_SNAP);
    WriteSnapshot(&w, slow);
    if (slotOrAll < 0) SendAll(&w);
    else SendGuest(slotOrAll, &w);
}

// A guest's name, tidied: letters, digits, spaces and a few marks; never empty.
static void TidyName(char *name, int fallback)
{
    char clean[NAME_LEN];
    int n = 0;
    for (const char *c = name; *c && n < NAME_LEN - 1; c++)
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || strchr(" -_.'", *c)) clean[n++] = *c;
    while (n > 0 && clean[n - 1] == ' ') n--;
    clean[n] = '\0';
    if (!n) snprintf(clean, sizeof(clean), "Player %u", (unsigned)fallback % 10u);
    snprintf(name, NAME_LEN, "%s", clean);
}

static void HostHello(int net, Reader *r)
{
    int version = (int)R8(r);
    char name[NAME_LEN];
    RStr(r, name, NAME_LEN);
    if (r->bad) return;
    if (version != NET_VERSION) {
        Kick(net, "This page has a different version of the game than the host's. Reload it.");
        return;
    }
    TidyName(name, net);
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (G.peers[s].active && SameName(G.peers[s].name, name)) {
            Kick(net, "Someone with that name is already playing. Pick another name.");
            return;
        }
    const PlayerRecord *rec = FindRecord(name);
    int slot = -1;
    if (rec && rec->slot > 0 && !G.peers[rec->slot].active) slot = rec->slot;
    for (int s = 1; s < MAX_PLAYERS && slot < 0; s++)
        if (!G.peers[s].active) slot = s;
    if (slot < 0) {
        Kick(net, "The game is full: six players already.");
        return;
    }
    // Everyone already playing hears of it; the newcomer gets a welcome of their own instead, which
    // this news would otherwise cover up.
    Announce("%s joined the game.", name);

    Peer *p = &G.peers[slot];
    *p = (Peer){ .active = true, .net = net, .health = HEALTH_MAX, .flags = PF_GROUND };
    snprintf(p->name, NAME_LEN, "%s", name);
    // In PvP everyone starts somewhere else in the city; otherwise a returning player is where they left.
    p->pos = p->shown = G.pvp ? CitySpawn(slot) : rec ? rec->pos : G.houses[slot].door;

    Writer w = Begin(M_WELCOME);
    W8(&w, (unsigned)slot);
    W32(&w, (unsigned)worldSign);
    WStr(&w, myRoom);
    W8(&w, (G.pvp ? 1u : 0u) | (G.combat ? 2u : 0u));
    W8(&w, rec ? 1u : 0u);
    if (rec) WriteRecord(&w, rec);
    WF(&w, p->pos.x);
    WF(&w, p->pos.z);
    SendGuest(slot, &w);
    SendRoster();
    Snapshot(slot, true);
}

// The host's save follows within a couple of seconds.
void NetSaveSoon(void)
{
    if (saveT < SAVE_EVERY - SAVE_SOON) saveT = SAVE_EVERY - SAVE_SOON;
}

static void HostLeave(int net)
{
    int slot = SlotOfNet(net);
    if (slot < 0) return;
    Peer *p = &G.peers[slot];
    char name[NAME_LEN];
    snprintf(name, sizeof(name), "%s", p->name);
    p->active = false;
    CalmBeasts(slot);
    SendRoster();
    Announce("%s left the game.", name);
    NetSaveSoon();
}

static void HostState(int slot, Reader *r)
{
    Peer *p = &G.peers[slot];
    Vector3 pos = RVec(r);
    float facing = RAngle(r), stride = RF(r), draw = (float)R8(r) / 255.0f, health = (float)R8(r), swing = (float)R8(r) / 200.0f;
    int flags = (int)R16(r), equip = (int)R8(r), weapon = (int)R8(r), bow = (int)R8(r), armor = (int)R8(r);
    if (r->bad || !isfinite(pos.x) || fabsf(pos.x) > WORLD_HALF + 5 || fabsf(pos.z) > WORLD_HALF + 5) return;
    p->pos = pos;
    p->facing = facing;
    p->stride = stride;
    p->draw = draw;
    p->health = health;
    p->swingT = swing;
    p->flags = flags;
    p->equip = equip;
    p->weapon = weapon;
    p->bow = bow;
    p->armor = armor;
    p->heard = 0;
}

static void HostRecord(int slot, Reader *r)
{
    PlayerRecord rec;
    if (!ReadRecord(r, &rec)) return;
    snprintf(rec.name, NAME_LEN, "%s", G.peers[slot].name);
    rec.slot = slot;
    KeepRecord(&rec);
    NetSaveSoon();
}

static void SendState(void)
{
    const Peer *p = &G.peers[G.self];
    Writer w = Begin(M_STATE);
    WF(&w, p->pos.x); WF(&w, p->pos.y); WF(&w, p->pos.z);
    WAngle(&w, p->facing);
    WF(&w, p->stride);
    W8(&w, Byte(p->draw * 255.0f));
    W8(&w, Byte(p->health));
    W8(&w, Byte(p->swingT * 200.0f));
    W16(&w, (unsigned)p->flags);
    W8(&w, (unsigned)p->equip); W8(&w, (unsigned)p->weapon); W8(&w, (unsigned)p->bow); W8(&w, (unsigned)p->armor);
    SendHost(&w);
}

void NetSendRecord(void)
{
    if (G.role != NET_CLIENT) return;
    PlayerRecord rec;
    CaptureRecord(&rec);
    Writer w = Begin(M_RECORD);
    WriteRecord(&w, &rec);
    SendHost(&w);
    recordT = 0;
}

// ---- Starting, playing and leaving ----

static void ResetSession(void)
{
    memset(pending, 0, sizeof(pending));
    memset(netArrows, 0, sizeof(netArrows));
    effectCount = 0;
    snapT = stateT = recordT = saveT = lostT = 0;
    snapCount = 0;
}

bool NetBegin(bool host, const char *name, const char *room)
{
    ResetSession();
    hosting = host;
    snprintf(myName, sizeof(myName), "%s", name);
    snprintf(myRoom, sizeof(myRoom), "%s", room);
    failure[0] = '\0';
    phase = PHASE_CONNECTING;
    if (!NetOpen(host, room)) {
        phase = PHASE_OFF;
        snprintf(failure, sizeof(failure), "%s", L("Multiplayer works in the browser version, served by server.py."));
        return false;
    }
    return true;
}

static void Fail(const char *why)
{
    snprintf(failure, sizeof(failure), "%s", L(why));
    phase = PHASE_OFF;
    NetShut();
}

// Out of a game that is over for this machine, back to the title screen.
static void Drop(const char *why)
{
    phase = PHASE_OFF;
    LeaveToLobby(why);
}

// The rules the host picked on the title screen for the game it opens.
static bool rulePvp = true, ruleCombat;

void NetRules(bool pvp, bool combat)
{
    rulePvp = pvp;
    ruleCombat = pvp && combat;
}

// A player's kills and deaths at other players' hands, and the ratio: "K/D 3/1 (3.0)".
const char *KillDeath(int s)
{
    const Peer *p = &G.peers[s];
    float ratio = p->deaths > 0 ? (float)p->kills / (float)p->deaths : (float)p->kills;
    return TextFormat("K/D %d/%d (%.1f)", p->kills, p->deaths, ratio);
}

// The game's rules in a line.
// What a PvP game's welcome adds: the way to the priest's blessing is open.
static const char *ZigguratHint(void)
{
    return G.pvp ? L(" Tip: the priest on top of the ziggurat gives you a boost: +20% speed for a day. Its stairs are unguarded here.") : "";
}

const char *RulesText(void)
{
    return L(!G.pvp ? "PvP off: players cannot hurt each other." : G.combat ? "Combat mode: best gear, unlimited arrows, guards stay out of player fights."
                    : "PvP on: guards punish whoever starts a fight in the city.");
}

static void StartHosting(void)
{
    GameInit();
    worldSign = WorldSignature();
    G.role = NET_HOST;
    G.self = G.actor = 0;
    G.peers[0] = (Peer){ .active = true, .net = 0 };
    snprintf(G.peers[0].name, NAME_LEN, "%s", myName);
    SetHome(0);
    bool resumed = LoadNetGame();
    const PlayerRecord *mine = FindRecord(myName);
    if (mine) ApplyRecord(mine);
    G.pvp = rulePvp;
    G.combat = ruleCombat;
    if (G.pvp) {
        RemoveZigguratGuards();
        G.player.pos = CitySpawn(0);
        G.player.pos.y = GroundHeight(G.player.pos.x, G.player.pos.z, G.player.pos.y + 0.5f);
    }
    UpdateSky();
    FollowCamera(1.0f);
    phase = PHASE_PLAYING;
    if (resumed) Say("%s %s%s", TextFormat(L("Your game is open again: day %d. Friends join from this same address."), G.day), RulesText(), ZigguratHint());
    else Say("%s %s%s", L("You are hosting a new game. Friends open this same address and press Join."), RulesText(), ZigguratHint());
    G.msgT = 8.0f;
}

static void StartGuest(Reader *r)
{
    int slot = (int)R8(r), sign = (int)R32(r);
    char room[32];
    RStr(r, room, sizeof(room));
    unsigned rules = R8(r);
    bool hasRecord = R8(r) != 0;
    PlayerRecord rec;
    if (hasRecord && !ReadRecord(r, &rec)) hasRecord = false;
    float startX = RF(r), startZ = RF(r);
    if (r->bad || slot <= 0 || slot >= MAX_PLAYERS) {
        Fail("The host sent something this game could not read.");
        return;
    }
    GameInit();
    if (WorldSignature() != sign) {
        Fail("This page has a different version of the game than the host's. Reload it.");
        return;
    }
    G.role = NET_CLIENT;
    G.pvp = (rules & 1u) != 0;
    G.combat = (rules & 2u) != 0;
    G.self = G.actor = slot;
    memset(G.peers, 0, sizeof(G.peers));
    G.peers[slot] = (Peer){ .active = true };
    snprintf(G.peers[slot].name, NAME_LEN, "%s", myName);
    SetHome(slot);
    if (hasRecord) {
        ApplyRecord(&rec);
    } else {
        G.player.pos = G.houses[slot].door;
        G.player.pos.y = GroundHeight(G.player.pos.x, G.player.pos.z, 1.0f);
    }
    if (G.pvp) {
        RemoveZigguratGuards();
        G.player.pos = (Vector3){ startX, 0, startZ };
        G.player.pos.y = GroundHeight(startX, startZ, TerrainHeight(startX, startZ) + 0.5f);
    }
    UpdateSky();
    FollowCamera(1.0f);
    phase = PHASE_PLAYING;
    Say("%s %s%s", TextFormat(L(hasRecord ? "Welcome back. Your home has the %s lintel." : "You joined the game. Your home has the %s lintel."), PlayerColorName(slot)),
        RulesText(), ZigguratHint());
    G.msgT = 9.0f;
}

// Called every frame while the lobby waits: 1 once the game is on, -1 if it failed (see NetFailure).
int NetLobby(void)
{
    NetMessage m;
    if (phase == PHASE_OFF) return -1;
    if (phase == PHASE_PLAYING) return 1;
    while (NetPoll(&m)) {
        if (m.kind == S_HELLO) {
            if (hosting) {
                StartHosting();
                return 1;
            }
            Writer w = Begin(M_HELLO);
            W8(&w, NET_VERSION);
            WStr(&w, myName);
            SendHost(&w);
            phase = PHASE_WAITING;
        } else if (m.kind == S_TAKEN) {
            Fail("Someone is already hosting a game here. Press Join instead.");
        } else if (m.kind == S_NOHOST) {
            Fail("No one is hosting a game here yet. One player presses Host first.");
        } else if (m.kind == S_FULL) {
            Fail("The game is full: six players already.");
        } else if (m.kind == S_DATA && m.len > 0) {
            Reader r = { m.data, m.len, 1, false };
            if (m.data[0] == M_WELCOME) {
                StartGuest(&r);
                if (phase == PHASE_PLAYING) return 1;
            } else if (m.data[0] == M_KICK) {
                char why[160];
                RStr(&r, why, sizeof(why));
                Fail(why);
            }
        }
        if (phase == PHASE_OFF) return -1;
    }
    if (NetLink() == LINK_CLOSED) {
        Fail("Could not reach the game server. Is server.py running?");
        return -1;
    }
    return 0;
}

const char *NetFailure(void)
{
    return failure;
}

const char *NetRoom(void)
{
    return myRoom;
}

// Saves (the host) or hands in this player's belongings (a guest), then closes the connection.
void NetLeave(void)
{
    if (G.role == NET_HOST) SaveNetGame();
    if (G.role == NET_CLIENT) NetSendRecord();
    NetShut();
    G.role = NET_SOLO;
    phase = PHASE_OFF;
}

static void HostFrame(float dt)
{
    NetMessage m;
    while (NetPoll(&m)) {
        if (m.kind == S_JOIN) continue;
        if (m.kind == S_LEAVE) {
            HostLeave(m.from);
            continue;
        }
        if (m.kind != S_DATA || m.len < 1) continue;
        Reader r = { m.data, m.len, 1, false };
        if (m.data[0] == M_HELLO) {
            HostHello(m.from, &r);
            continue;
        }
        int slot = SlotOfNet(m.from);
        if (slot < 0) continue;
        switch (m.data[0]) {
        case M_STATE:  HostState(slot, &r); break;
        case M_REQ:    HostRequest(slot, &r); break;
        case M_RECORD: HostRecord(slot, &r); break;
        default: break;
        }
    }
    if (NetLink() == LINK_CLOSED) {
        SaveNetGame();
        Drop("Lost the connection to the game server. Your game was saved.");
        return;
    }
    snapT += dt;
    if (snapT >= SNAP_EVERY) {
        snapT = 0;
        bool anyone = PlayerCount() > 1;
        if (anyone) Snapshot(-1, ++snapCount % SLOW_EVERY == 0);
        else effectCount = 0;
    }
    saveT += dt;
    if (saveT >= SAVE_EVERY) {
        saveT = 0;
        SaveNetGame();
    }
}

static void ClientEvent(Reader *r)
{
    int kind = (int)R8(r);
    switch (kind) {
    case EV_HURT: {
        float dmg = RF(r);
        int by = (int)R8(r);
        bool has = R8(r) != 0;
        Vector3 src = { 0 };
        if (has) {
            src.x = RF(r);
            src.z = RF(r);
        }
        if (!r->bad) HurtPlayerFrom(dmg, by, has ? &src : NULL);
        break;
    }
    case EV_STUNNED: {
        float secs = RF(r);
        if (!r->bad && secs >= 0 && secs < 5.0f) G.swingCool += secs;
        break;
    }
    case EV_SAY: ShowSay(r); break;
    case EV_HIT: G.hitMarkT = 0.3f; break;
    case EV_IMPACT: {
        Vector3 at = RVec(r);
        if (!r->bad) {
            G.impact = at;
            G.impactT = 3.0f;
        }
        break;
    }
    case EV_PARRIED:
        G.parryT = 0;
        G.parryCool = 0;
        break;
    case EV_SKIP: {
        float hours = RF(r);
        unsigned bits = R8(r);
        if (!r->bad) WakeAfterNight(hours, (bits & 1u) != 0, (bits & 2u) != 0);
        break;
    }
    default: break;
    }
}

static void ClientFrame(float dt)
{
    NetMessage m;
    while (NetPoll(&m)) {
        if (m.kind == S_HOSTLEFT) {
            Drop("The host left the game.");
            return;
        }
        if (m.kind != S_DATA || m.len < 1) continue;
        Reader r = { m.data, m.len, 1, false };
        switch (m.data[0]) {
        case M_SNAP:   ReadSnapshot(&r); lostT = 0; break;
        case M_ROSTER: ReadRoster(&r); break;
        case M_REPLY:  ClientReply(&r); break;
        case M_EVENT:  ClientEvent(&r); break;
        case M_KICK: {
            char why[160];
            RStr(&r, why, sizeof(why));
            Drop(why);
            return;
        }
        default: break;
        }
    }
    if (NetLink() == LINK_CLOSED) {
        Drop("Lost the connection to the game.");
        return;
    }
    lostT += GetFrameTime();
    Ease(dt);
    for (int i = 0; i < MAX_PENDING; i++)
        if (pending[i].id && (pending[i].age += dt) > 10.0f) pending[i].id = 0;
    stateT += dt;
    if (stateT >= STATE_EVERY) {
        stateT = 0;
        SendState();
    }
    recordT += dt;
    if (recordT >= RECORD_EVERY) NetSendRecord();
}

void NetFrame(float dt)
{
    if (G.role == NET_HOST) HostFrame(dt);
    else if (G.role == NET_CLIENT) ClientFrame(dt);
}

#if defined(SUMER_TEST)
// Test builds only: a picture of the world, read back on this machine, uses up every byte written.
bool SnapshotRoundTrip(int *size)
{
    NetRole role = G.role;
    Writer w = Begin(M_SNAP);
    WriteSnapshot(&w, true);
    Reader r = { w.d, w.n, 1, false };
    G.role = NET_CLIENT;
    ReadSnapshot(&r);
    G.role = role;
    *size = w.n;
    return !w.bad && !r.bad && r.at == r.n;
}
#endif
