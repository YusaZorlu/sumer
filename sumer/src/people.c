#include "game.h"
#include <math.h>

#define MAX_BODIES 128

typedef struct { Vector3 pos; float r; int owner; } Body;

static Body bodies[MAX_BODIES];
static int bodyCount;

static void Add(Vector3 p, float r, int owner)
{
    if (bodyCount < MAX_BODIES) bodies[bodyCount++] = (Body){ p, r, owner };
}

// Rebuilt once per frame; everyone collides against last frame's positions.
void CollectBodies(void)
{
    bodyCount = 0;
    Add(G.player.pos, PLAYER_RADIUS, OWNER_PLAYER);
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
        for (int k = 0; k < 4; k++) {
            Vector3 p;
            float f;
            if ((k == 0 && G.caravans[c].vit[0].dead) || (k == 3 && G.caravans[c].vit[1].dead)) continue;
            if (CaravanMember(c, k, &p, &f)) Add(p, (k == 1 || k == 2) ? 0.55f : 0.35f, OWNER_CARAVAN + c);
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

Vector2 PushOutBodies(Vector2 p, float r, int self)
{
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
