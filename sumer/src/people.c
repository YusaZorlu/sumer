#include "game.h"
#include <math.h>

#define MAX_BODIES 160

typedef struct { Vector3 pos; float r; int owner; } Body;

static Body bodies[MAX_BODIES];
static int bodyCount;

static void Add(Vector3 p, float r, int owner)
{
    if (bodyCount < MAX_BODIES) bodies[bodyCount++] = (Body){ p, r, owner };
}

// Rebuilt once per frame; everyone collides against last frame's positions. Players lying in bed
// are out of everyone's way. The game's server program has no player of its own.
void CollectBodies(void)
{
    bodyCount = 0;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if ((s == G.self && !OutOfRound() && !Dedicated()) || (s != G.self && PlayerIn(s) && !(G.peers[s].flags & (PF_BED | PF_OUT))))
            Add(s == G.self ? G.player.pos : G.peers[s].pos, PLAYER_RADIUS, OWNER_PLAYER + s);
    for (int i = 0; i < G.villagerCount; i++)
        if (!G.villagers[i].vit.dead) Add(VillagerPos(&G.villagers[i]), 0.35f, OWNER_VILLAGER + i);
    for (int i = 0; i < G.workerCount; i++) {
        const Worker *w = &G.workers[i];
        if (w->visible && w->job != JOB_BOAT && !w->vit.dead) Add(w->pos, 0.35f, OWNER_WORKER + i);
    }
    for (int i = 0; i < TR_COUNT; i++) {
        const Trader *t = &G.traders[i];
        if (!t->roaming && !t->vit.dead) Add(TraderPos(t), 0.4f, OWNER_TRADER + i);
    }
    for (int c = 0; c < MAX_CARAVANS; c++) {
        for (int k = 0; k < 3 + CARAVAN_GUARDS; k++) {
            Vector3 p;
            float f;
            if ((k == 0 && G.caravans[c].vit[0].dead) || (k >= 3 && G.caravans[c].vit[k - 2].dead)) continue;
            if (CaravanMember(c, k, &p, &f)) Add(p, (k == 1 || k == 2) ? 0.55f : 0.35f, CARAVAN_OWNER(c, k));
        }
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        const Raider *r = &G.raiders[i];
        if (r->vit.used && !r->vit.dead) Add(r->vit.override ? r->vit.at : r->pos, 0.38f, OWNER_RAIDER + i);
    }
}

static bool Near(const Body *b, Vector3 p, float r)
{
    float dx = p.x - b->pos.x, dz = p.z - b->pos.z;
    return fabsf(p.y - b->pos.y) < 1.5f && dx * dx + dz * dz < (r + b->r) * (r + b->r);
}

// A circle player-sized things are kept out of.
static Vector2 PushOutCircle(Vector2 p, float r, float cx, float cz, float cr)
{
    float dx = p.x - cx, dz = p.y - cz;
    float d = sqrtf(dx * dx + dz * dz), min = r + cr;
    if (d >= min) return p;
    if (d < 1e-4f) {
        dx = 1;
        dz = 0;
        d = 1;
    }
    return (Vector2){ cx + dx / d * min, cz + dz / d * min };
}

// Cows and sheep come to no harm, but no one walks through them: circles along each body, nose to tail
// ('along' are the circles' places along it). Each machine's livestock wander on their own, so this is
// for the player here only.
static Vector2 PushOutHerd(Vector2 p, float r, const Wanderer *herd, int n, const float *along, int parts, float cr)
{
    for (int i = 0; i < n; i++) {
        const Wanderer *w = &herd[i];
        if (fabsf(w->pos.x - p.x) > 3.0f || fabsf(w->pos.z - p.y) > 3.0f) continue;
        if (fabsf(G.player.pos.y - TerrainHeight(w->pos.x, w->pos.z)) > 1.5f) continue;
        for (int k = 0; k < parts; k++)
            p = PushOutCircle(p, r, w->pos.x + sinf(w->heading) * along[k], w->pos.z + cosf(w->heading) * along[k], cr);
    }
    return p;
}

Vector2 PushOutBodies(Vector2 p, float r, int self)
{
    static const float cow[3] = { -0.6f, 0.2f, 0.95f }, sheep[2] = { -0.25f, 0.3f };
    if (self == OWNER_PLAYER + G.self) {
        p = PushOutHerd(p, r, G.cows, MAX_COWS, cow, 3, 0.45f);
        p = PushOutHerd(p, r, G.sheep, MAX_SHEEP, sheep, 2, 0.35f);
        p = PushOutHerd(p, r, G.flock, MAX_FLOCK, sheep, 2, 0.35f);
        p = PushOutPets(p, r);
    }
    float y = G.player.pos.y;

    for (int i = 0; i < bodyCount; i++) {
        const Body *b = &bodies[i];
        if (b->owner == self || fabsf(y - b->pos.y) > 1.5f) continue;
        float dx = p.x - b->pos.x, dz = p.y - b->pos.z;
        float d = sqrtf(dx * dx + dz * dz), min = r + b->r;
        if (d >= min) continue;
        if (d < 1e-4f) {
            dx = 1;
            dz = 0;
            d = 1;
        }
        p.x = b->pos.x + dx / d * min;
        p.y = b->pos.z + dz / d * min;
    }
    return p;
}

bool BodyAt(Vector3 p, float r, int self)
{
    for (int i = 0; i < bodyCount; i++)
        if (bodies[i].owner != self && Near(&bodies[i], p, r)) return true;
    return false;
}

// True when the step from 'from' to 'to' runs into someone; stepping away is always allowed.
bool BodyAheadIgnoring(Vector3 from, Vector3 to, float r, int self, int ignoreLo, int ignoreHi)
{
    for (int i = 0; i < bodyCount; i++) {
        const Body *b = &bodies[i];
        if (b->owner == self || (b->owner >= ignoreLo && b->owner < ignoreHi) || !Near(b, to, r)) continue;
        if (XZDist(to, b->pos) < XZDist(from, b->pos)) return true;
    }
    return false;
}

bool BodyAhead(Vector3 from, Vector3 to, float r, int self)
{
    return BodyAheadIgnoring(from, to, r, self, 0, 0);
}
