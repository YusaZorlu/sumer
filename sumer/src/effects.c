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

// Only on this machine: what the one striking sees at once, before the host's answer comes back.
static void LocalBurst(Vector3 pos, Color c, int count, float speed, float size)
{
    for (int i = 0; i < count; i++) {
        Particle *p = FreeParticle();
        float a = RandF(0, 2 * PI), up = RandF(0.3f, 1.0f);
        Vector3 v = { cosf(a) * speed * RandF(0.3f, 1.0f), up * speed, sinf(a) * speed * RandF(0.3f, 1.0f) };
        float life = RandF(0.3f, 0.6f);
        *p = (Particle){ pos, v, life, life, size * RandF(0.6f, 1.2f), c, false };
    }
}

// ---- How a blow feels ----

#define MAX_FLASHES 8
#define FLASH_TIME  0.16f
static struct { Vector3 at; float t; } flashes[MAX_FLASHES];

// Whatever stands at 'at' (a person, an animal, a player) flashes pale for a moment.
void AddFlash(Vector3 at)
{
    int k = 0;
    for (int i = 1; i < MAX_FLASHES; i++)
        if (flashes[i].t < flashes[k].t) k = i;
    flashes[k].at = at;
    flashes[k].t = FLASH_TIME;
}

float FlashAt(Vector3 p)
{
    float most = 0;
    for (int i = 0; i < MAX_FLASHES; i++)
        if (flashes[i].t > 0 && XZDist(flashes[i].at, p) < 1.1f && fabsf(flashes[i].at.y - p.y) < 2.2f)
            most = fmaxf(most, flashes[i].t / FLASH_TIME);
    return most;
}

static void Kick(float amount, float secs)
{
    G.shakeAmp = fmaxf(G.shakeT > 0 ? G.shakeAmp : 0.0f, amount);
    G.shakeT = fmaxf(G.shakeT, secs);
}

// This machine's player landed a blow at 'at' (the war mace's are heavy): the swing holds a moment at
// the point of contact, the view kicks, blood and dust fly, the one struck flashes, and it thuds.
void BlowLanded(Vector3 at, bool heavy)
{
    G.hitStop = HITSTOP_TIME * (heavy ? 1.5f : 1.0f);
    Kick(heavy ? 0.09f : 0.055f, heavy ? 0.22f : 0.16f);
    LocalBurst(at, (Color){ 150, 22, 22, 255 }, heavy ? 18 : 12, heavy ? 3.4f : 2.8f, 0.07f);
    LocalBurst(at, (Color){ 196, 176, 136, 255 }, 6, 1.6f, 0.1f);
    AddFlash((Vector3){ at.x, at.y - 1.1f, at.z });
    SfxPlay(heavy ? SFX_HIT_HEAVY : SFX_HIT, 1.0f);
}

// This machine's player had a blow turned aside, or turned one: bronze rings on bronze and sparks fly.
void BlowTurnedAside(Vector3 at)
{
    G.hitStop = HITSTOP_TIME;
    Kick(0.06f, 0.18f);
    LocalBurst(at, (Color){ 255, 226, 140, 255 }, 12, 3.6f, 0.05f);
    SfxPlay(SFX_CLANG, 0.9f);
}

// This machine's player was struck (the red lights and the flinch come with the damage).
void BlowTaken(void)
{
    SfxPlay(SFX_HURT, 1.0f);
}

// A little in front of this machine's player's chest, where a blow on them is met.
Vector3 ChestAhead(void)
{
    const Player *p = &G.player;
    return (Vector3){ p->pos.x - sinf(p->yaw) * 0.6f, p->pos.y + 1.3f, p->pos.z - cosf(p->yaw) * 0.6f };
}

void UpdateHitFeel(float dt)
{
    for (int i = 0; i < MAX_FLASHES; i++)
        if (flashes[i].t > 0) flashes[i].t -= dt;
    if (G.hitStop > 0) G.hitStop -= dt;
    if (G.shakeT > 0) G.shakeT -= dt;
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

// A point on a swung weapon, 'along' its length (0 at the hand, 1 at the tip or head), t seconds into
// the swing: the same turns DrawWeapon and DrawMace (player.c) and PeerWeapon (players.c) make.
static Vector3 WeaponPoint(Vector3 pos, float facing, float shoulder, bool mace, float t, float along)
{
    Vector3 p;
    if (mace) {
        float yaw = MaceYaw(t) * DEG2RAD, tilt = -6.0f * DEG2RAD, len = 0.98f * along;
        float y = -len * sinf(tilt), z = len * cosf(tilt);
        p = (Vector3){ z * sinf(yaw), -0.15f + y, 0.15f + z * cosf(yaw) };
    } else {
        p = Vector3Transform((Vector3){ 0, 0, 0.85f * along }, SwordFrame(t));
    }
    float c = cosf(facing), sn = sinf(facing);
    return (Vector3){ pos.x + p.x * c + p.z * sn, pos.y + shoulder + p.y, pos.z - p.x * sn + p.z * c };
}

// The pale sweep a weapon leaves behind it in the fast part of a swing.
static void SwingTrail(Vector3 pos, float facing, float shoulder, bool mace, float t)
{
    const int steps = 8;
    float from = mace ? 0.16f : 0.2f, to = mace ? 0.48f : 0.42f;
    if (t < from || t > to) return;
    float t0 = fmaxf(from, t - (mace ? 0.13f : 0.09f));
    rlCheckRenderBatchLimit(4 * steps);
    rlBegin(RL_QUADS);
    for (int k = 0; k < steps; k++) {
        float ta = Lerp(t0, t, (float)k / steps), tb = Lerp(t0, t, (float)(k + 1) / steps);
        unsigned char aa = (unsigned char)(150.0f * (float)k / steps), ab = (unsigned char)(150.0f * (float)(k + 1) / steps);
        Vector3 ia = WeaponPoint(pos, facing, shoulder, mace, ta, 0.45f), oa = WeaponPoint(pos, facing, shoulder, mace, ta, 1.05f);
        Vector3 ib = WeaponPoint(pos, facing, shoulder, mace, tb, 0.45f), ob = WeaponPoint(pos, facing, shoulder, mace, tb, 1.05f);
        rlColor4ub(255, 242, 220, aa);
        rlVertex3f(ia.x, ia.y, ia.z);
        rlVertex3f(oa.x, oa.y, oa.z);
        rlColor4ub(255, 242, 220, ab);
        rlVertex3f(ob.x, ob.y, ob.z);
        rlVertex3f(ib.x, ib.y, ib.z);
    }
    rlEnd();
}

void DrawSwingTrails(void)
{
    rlDisableBackfaceCulling();
    if ((G.equip == EQ_MELEE || G.equip == EQ_MACE) && G.swingT > 0 && G.camDist >= 1.3f) {
        bool mace = G.equip == EQ_MACE;
        SwingTrail(G.player.pos, G.player.facing, G.player.crouching ? 1.04f : 1.42f, mace, SwingElapsed(G.swingT, mace));
    }
    for (int s = 0; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active || s == G.self || p->swingT <= 0 || (p->equip != EQ_MELEE && p->equip != EQ_MACE)) continue;
        bool mace = p->equip == EQ_MACE;
        SwingTrail(p->shown, p->shownFacing, (p->flags & PF_CROUCH) ? 1.04f : 1.42f, mace, SwingElapsed(p->swingT, mace));
    }
    rlDrawRenderBatchActive();   // seen from either side: drawn before culling is back on
    rlEnableBackfaceCulling();
}

void DrawEffects(void)
{
    G.drawTorch = G.drawFog = 0;
    DrawSwingTrails();
    DrawStorm();
    DrawLava();
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
