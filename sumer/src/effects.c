#include "game.h"
#include "rlgl.h"
#include <math.h>

static Particle *FreeParticle(void)
{
    Particle *oldest = &G.particles[0];
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (G.particles[i].life <= 0) return &G.particles[i];
        if (G.particles[i].life < oldest->life) oldest = &G.particles[i];
    }
    return oldest;
}

// The host passes what it spawns on to the guests who can see it.
void SpawnBurst(Vector3 pos, Color c, int count, float speed)
{
    if (G.role == NET_HOST) NetEffect(pos, c, count, speed, false);
    for (int i = 0; i < count; i++) {
        Particle *p = FreeParticle();
        float a = RandF(0, 2 * PI), up = RandF(0.4f, 1.0f);
        Vector3 v = { cosf(a) * speed * RandF(0.3f, 1.0f), up * speed, sinf(a) * speed * RandF(0.3f, 1.0f) };
        float life = RandF(0.35f, 0.7f);
        *p = (Particle){ pos, v, life, life, RandF(0.05f, 0.11f), c, false };
    }
}

void SpawnRing(Vector3 pos, Color c)
{
    if (G.role == NET_HOST) NetEffect(pos, c, 1, 0, true);
    Particle *p = FreeParticle();
    *p = (Particle){ pos, { 0, 0, 0 }, 2.0f, 2.0f, 0.3f, c, true };
}

void UpdateEffects(float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &G.particles[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        if (p->ring) continue;
        p->vel.y -= 9.0f * dt;
        p->pos = Vector3Add(p->pos, Vector3Scale(p->vel, dt));
        float ground = TerrainHeight(p->pos.x, p->pos.z) + 0.03f;
        if (p->pos.y < ground) {
            p->pos.y = ground;
            p->vel = Vector3Scale(p->vel, 0.3f);
        }
    }
    if (G.hitMarkT > 0) G.hitMarkT -= dt;
    if (G.impactT > 0) G.impactT -= dt;
}

static void Ribbon(Vector3 a, Vector3 b, float wa, float wb, Color ca, Color cb)
{
    Vector3 eye = G.cam.position;
    Vector3 seg = Vector3Subtract(b, a);
    Vector3 side = Vector3CrossProduct(seg, Vector3Subtract(eye, a));
    float len = Vector3Length(side);
    if (len < 1e-5f) return;
    side = Vector3Scale(side, 1.0f / len);

    Vector3 a0 = Vector3Add(a, Vector3Scale(side, wa)), a1 = Vector3Subtract(a, Vector3Scale(side, wa));
    Vector3 b0 = Vector3Add(b, Vector3Scale(side, wb)), b1 = Vector3Subtract(b, Vector3Scale(side, wb));
    rlColor4ub(ca.r, ca.g, ca.b, ca.a);
    rlVertex3f(a0.x, a0.y, a0.z);
    rlVertex3f(a1.x, a1.y, a1.z);
    rlColor4ub(cb.r, cb.g, cb.b, cb.a);
    rlVertex3f(b1.x, b1.y, b1.z);
    rlVertex3f(b0.x, b0.y, b0.z);
}

// Bright streak behind each arrow, fading toward the tail and after it lands.
void DrawArrowTrails(const Arrow *arrows)
{
    const Color base = { 255, 196, 100, 255 };

    rlDisableBackfaceCulling();
    for (int i = 0; i < MAX_ARROWS; i++) {
        const Arrow *ar = &arrows[i];
        if (!ar->active || ar->trailCount == 0 || (!ar->flying && ar->fade <= 0)) continue;
        float fade = ar->flying ? 1.0f : ar->fade / 0.6f;
        Vector3 head = ar->spent || !ar->flying ? ar->trail[0] : ar->pos;
        int n = ar->trailCount;

        rlCheckRenderBatchLimit(4 * (n + 1));
        rlBegin(RL_QUADS);
        Vector3 prev = head;
        for (int k = 0; k < n; k++) {
            float ta = (float)k / (float)n, tb = (float)(k + 1) / (float)n;
            Color ca = base, cb = base;
            ca.a = (unsigned char)(255.0f * (1.0f - ta) * fade);
            cb.a = (unsigned char)(255.0f * (1.0f - tb) * fade);
            Ribbon(prev, ar->trail[k], 0.1f * (1.0f - ta) + 0.02f, 0.1f * (1.0f - tb) + 0.02f, ca, cb);
            prev = ar->trail[k];
        }
        rlEnd();
        if (ar->flying) DrawGlowBox(ar->pos, (Vector3){ 0.14f, 0.14f, 0.14f }, (Color){ 255, 236, 170, 255 });
    }
    rlEnableBackfaceCulling();
}

void DrawEffects(void)
{
    G.drawTorch = G.drawFog = 0;
    DrawArrowTrails(G.arrows);
    if (IsClient()) DrawArrowTrails(NetArrows());
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &G.particles[i];
        if (p->life <= 0) continue;
        float k = p->life / p->maxLife;
        Color c = p->color;
        c.a = (unsigned char)(255.0f * Clamp(k * 1.5f, 0.0f, 1.0f));
        if (p->ring) {
            float s = p->size + (1.0f - k) * 2.2f, t = 0.09f;
            Vector3 at = { p->pos.x, p->pos.y + 0.02f, p->pos.z };
            DrawGlowBox((Vector3){ at.x, at.y, at.z - s / 2 }, (Vector3){ s, 0.01f, t }, c);
            DrawGlowBox((Vector3){ at.x, at.y, at.z + s / 2 }, (Vector3){ s, 0.01f, t }, c);
            DrawGlowBox((Vector3){ at.x - s / 2, at.y, at.z }, (Vector3){ t, 0.01f, s }, c);
            DrawGlowBox((Vector3){ at.x + s / 2, at.y, at.z }, (Vector3){ t, 0.01f, s }, c);
        } else {
            DrawGlowBox(p->pos, (Vector3){ p->size, p->size, p->size }, c);
        }
    }
}
