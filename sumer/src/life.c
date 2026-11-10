// Small signs of life, only to look at and kept on this machine: smoke rising from every fire (hearths,
// camp fires, braziers, the kiln, the smelter, your oven), dust kicked up by your running feet and by
// running animals, birds wheeling over the river and the marsh by day, fireflies by the reeds at night.
// (The fish leaping at the fishing spots are drawn with the spots, in world.c.) None of it touches the
// game: it never draws the game's random numbers and nothing of it is sent between machines, so it is
// the same for everyone in hide-and-seek: no one's dust but your own is ever drawn.
#include "game.h"
#include "rlgl.h"
#include <math.h>

static unsigned seed = 2100u;
static float Rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return (float)(seed >> 8) / 16777216.0f;
}

// ---- Dust ----

#define MAX_DUST 64
static struct { Vector3 at, drift; float age, life, size; } dust[MAX_DUST];
static float animalDust[MAX_ANIMALS], playerDust;

static void KickDust(Vector3 at, float size)
{
    int k = 0;   // a free puff, or else the oldest
    for (int i = 0; i < MAX_DUST; i++) {
        if (dust[i].age >= dust[i].life) {
            k = i;
            break;
        }
        if (dust[i].age > dust[k].age) k = i;
    }
    float a = Rnd() * 2.0f * PI;
    dust[k].at = (Vector3){ at.x + cosf(a) * 0.15f, at.y + 0.35f, at.z + sinf(a) * 0.15f };
    dust[k].drift = (Vector3){ cosf(a) * 0.35f, 0.3f + 0.2f * Rnd(), sinf(a) * 0.35f };
    dust[k].age = 0;
    dust[k].life = 0.8f + 0.5f * Rnd();
    dust[k].size = size * (0.8f + 0.4f * Rnd());
}

// How many puffs of dust are in the air (for the tests).
int DustInAir(void)
{
    int n = 0;
    for (int i = 0; i < MAX_DUST; i++) n += dust[i].age < dust[i].life;
    return n;
}

// Running feet and hooves raise dust on dry ground: the city's streets and its mud roofs, and the bare
// plain; not water or the green land by the river.
static bool Dusty(Vector3 p)
{
    if (InWater(p.x, p.z)) return false;
    return InsideWalls(p.x, p.z, 0.0f) || Greenness(p.x, p.z) < 0.5f;
}

static void UpdateWakes(float dt);

void UpdateLife(float dt)
{
    for (int i = 0; i < MAX_DUST; i++)
        if (dust[i].age < dust[i].life) {
            dust[i].age += dt;
            dust[i].at = Vector3Add(dust[i].at, Vector3Scale(dust[i].drift, dt));
            dust[i].drift.y *= 1.0f - 1.5f * dt;
        }
    UpdateWakes(dt);
    const Player *pl = &G.player;
    playerDust -= dt;
    if (pl->running && pl->moving && pl->grounded && playerDust <= 0 && Dusty(pl->pos)) {
        KickDust(pl->pos, 0.65f);
        playerDust = 0.13f;
    }
    for (int i = 0; i < G.animalCount && i < MAX_ANIMALS; i++) {
        const Animal *a = &G.animals[i];
        animalDust[i] -= dt;
        if (!a->active || (a->state != ST_FLEE && a->state != ST_HUNT) || a->speed < 3.0f || animalDust[i] > 0) continue;
        if (a->type == AN_GOOSE || a->type == AN_SNAKE || a->type == AN_PARTRIDGE) continue;
        if (XZDist(a->pos, G.cam.position) > 70.0f || !Dusty(a->pos)) continue;
        KickDust(a->pos, a->type == AN_ONAGER ? 0.8f : 0.6f);
        animalDust[i] = 0.16f;
    }
}

// ---- Rings on the water round whoever wades ----

#define MAX_WAKES 20
static struct { Vector3 at; float age; } wakes[MAX_WAKES];
static float wadeT[MAX_PLAYERS];

// You and the players in sight leave spreading rings as you wade (people of the town never go into the water).
static void UpdateWakes(float dt)
{
    for (int i = 0; i < MAX_WAKES; i++)
        if (wakes[i].age < 1.6f) wakes[i].age += dt;
    for (int s = 0; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        bool self = s == G.self;
        Vector3 at = self ? G.player.pos : p->shown;
        wadeT[s] -= dt;
        if (!(self || p->active) || (p->flags & PF_OUT) || wadeT[s] > 0 || !(self ? G.player.moving : p->stride != 0.0f)) continue;
        if (XZDist(at, G.cam.position) > 60.0f || at.y > WATER_LEVEL + 0.3f || !InWater(at.x, at.z)) continue;
        int k = 0;
        for (int i = 1; i < MAX_WAKES; i++)
            if (wakes[i].age > wakes[k].age) k = i;
        wakes[k].at = (Vector3){ at.x, WATER_LEVEL + 0.04f, at.z };
        wakes[k].age = 0;
        wadeT[s] = 0.45f;
    }
}

static void DrawWakes(void)
{
    for (int i = 0; i < MAX_WAKES; i++) {
        if (wakes[i].age >= 1.6f || wakes[i].at.y == 0.0f) continue;
        float f = wakes[i].age / 1.6f, r = 0.35f + 1.1f * f;
        Color c = Fade((Color){ 206, 224, 228, 255 }, 0.6f * (1.0f - f));
        for (int k = 0; k < 12; k++) {
            float a = (float)k * 2.0f * PI / 12.0f;
            rlPushMatrix();
            rlTranslatef(wakes[i].at.x + cosf(a) * r, wakes[i].at.y, wakes[i].at.z + sinf(a) * r);
            rlRotatef(-a * RAD2DEG, 0, 1, 0);
            DrawShadedBox((Vector3){ 0, 0, 0 }, (Vector3){ 0.04f, 0.01f, r * 0.4f }, c);
            rlPopMatrix();
        }
    }
}

// ---- Smoke and dust: soft round pictures facing the camera, drawn after the world ----

static Texture2D puff;

static Texture2D Puff(void)
{
    if (!puff.id && IsWindowReady()) {
        // White all through, only its alpha falling away: fading to black would darken every puff's edge.
        Image im = GenImageGradientRadial(64, 64, 0.3f, WHITE, (Color){ 255, 255, 255, 0 });
        puff = LoadTextureFromImage(im);
        UnloadImage(im);
        SetTextureFilter(puff, TEXTURE_FILTER_BILINEAR);
    }
    return puff;
}

// One fire's smoke: puffs rising, swelling and thinning, leaning with the wind. A kiln's rises from its
// chimney, not from the mouth where it is fed.
static void Smoke(Texture2D t, Vector3 fire, float now, Color dark, Color warm)
{
    Vector3 kiln = G.stations[STN_KILN];
    float strength = 1.0f, rise = 6.5f;
    Vector3 s = { fire.x, fire.y + 0.35f, fire.z };
    if (XZDist(fire, kiln) < 2.5f) {
        s = (Vector3){ kiln.x, kiln.y + 2.5f, kiln.z };
        strength = 1.35f;
        rise = 9.0f;
    }
    float d = XZDist(s, G.cam.position);
    if (d > 170.0f || !InView((Vector3){ s.x + 1.5f, s.y + rise * 0.5f, s.z }, rise)) return;
    strength *= 1.0f - Clamp((d - 110.0f) / 60.0f, 0.0f, 1.0f);   // thinning into the distance
    float phase = fire.x * 0.37f + fire.z * 0.71f;
    for (int i = 0; i < 10; i++) {
        float f = fmodf(now * 0.085f + (float)i / 10.0f + phase, 1.0f);
        if (f < 0.0f) f += 1.0f;
        float size = 0.6f + f * 3.2f, a = strength * 0.42f * (1.0f - f) * Clamp(f * 7.0f, 0.0f, 1.0f);
        // Each puff wanders a little of its own way as the wind takes the column.
        float wx = 0.45f * f * sinf(now * 0.6f + (float)i * 2.1f), wz = 0.35f * f * cosf(now * 0.5f + (float)i * 1.7f);
        Vector3 p = { s.x + f * f * 4.0f + wx, s.y + f * rise, s.z + f * f * 1.6f + wz };
        Color c = Mix(warm, dark, Clamp(f * 2.5f, 0.0f, 1.0f));   // lit by its fire low down
        DrawBillboard(G.cam, t, p, size, Fade(c, a));
    }
}

void DrawPuffs(void)
{
    Texture2D t = Puff();
    if (!t.id) return;
    float now = (float)GetTime(), day = Clamp(G.daylight, 0.0f, 1.0f), night = Clamp(G.night, 0.0f, 1.0f);
    Color dark = Mix((Color){ 40, 40, 46, 255 }, (Color){ 138, 134, 130, 255 }, day);
    Color warm = Mix(dark, (Color){ 190, 118, 64, 255 }, 0.55f * night);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ALPHA);
    const Vector3 *fires;
    int n = FireSpots(&fires);
    for (int k = 0; k < n; k++) Smoke(t, fires[k], now, dark, warm);
    // Dust: an ochre haze, darker than the pale streets and paler than the plain's earth.
    Color earth = Mix((Color){ 60, 52, 44, 255 }, (Color){ 178, 150, 110, 255 }, day);
    for (int i = 0; i < MAX_DUST; i++) {
        if (dust[i].age >= dust[i].life) continue;
        float f = dust[i].age / dust[i].life;
        DrawBillboard(G.cam, t, dust[i].at, dust[i].size * (0.8f + 1.8f * f), Fade(earth, 0.85f * (1.0f - f) * Clamp(f * 8.0f, 0.0f, 1.0f)));
    }
    rlDrawRenderBatchActive();
    EndBlendMode();
    rlEnableDepthMask();
}

// ---- Birds and fireflies: drawn with the world's moving things ----

// Birds wheeling high over the river and the marsh by day: a few wing beats, then a glide.
static void DrawBirds(float now)
{
    static const float flocks[3][3] = { { -139, 10, 34 }, { -150, -120, 40 }, { -60, -140, 28 } };   // centre x, z and radius
    const Color dark = { 52, 48, 46, 255 };
    for (int k = 0; k < 9; k++) {
        const float *f = flocks[k % 3];
        float r = f[2] * (0.75f + 0.1f * (float)(k / 3)), w = (0.15f + 0.025f * (float)(k % 4)) * ((k & 1) ? 1.0f : -1.0f);
        float ang = now * w + (float)k * 1.9f, h = 14.0f + 5.0f * (float)(k % 3) + 1.5f * sinf(now * 0.6f + (float)k);
        Vector3 p = { f[0] + cosf(ang) * r, h, f[1] + sinf(ang) * r };
        if (Vector3Distance(p, G.cam.position) > 240.0f || !InView(p, 2.0f)) continue;
        float glide = sinf(now * 0.45f + (float)k * 0.8f) > 0.3f ? 0.12f : 1.0f;
        float flap = sinf(now * 8.0f + (float)k * 2.3f) * 38.0f * glide + 6.0f;
        float heading = (w > 0 ? 0.0f : PI) - ang;   // along the circle, whichever way it turns
        rlPushMatrix();
        rlTranslatef(p.x, p.y, p.z);
        rlRotatef(heading * RAD2DEG, 0, 1, 0);
        rlRotatef(w > 0 ? 14.0f : -14.0f, 0, 0, 1);   // banked into the turn
        rlScalef(1.6f, 1.6f, 1.6f);                    // herons and kites, with wings wide
        DrawShadedBox((Vector3){ 0, 0, 0 }, (Vector3){ 0.12f, 0.1f, 0.44f }, dark);
        DrawShadedBox((Vector3){ 0, 0, -0.3f }, (Vector3){ 0.16f, 0.02f, 0.14f }, dark);
        for (int s = -1; s <= 1; s += 2) {
            rlPushMatrix();
            rlTranslatef(0.05f * (float)s, 0.03f, 0.03f);
            rlRotatef(flap * (float)s, 0, 0, 1);
            DrawShadedBox((Vector3){ 0.3f * (float)s, 0, 0 }, (Vector3){ 0.58f, 0.02f, 0.2f }, dark);
            rlPushMatrix();   // the outer wing, swept back
            rlTranslatef(0.58f * (float)s, 0, -0.04f);
            rlRotatef(flap * 0.5f * (float)s, 0, 0, 1);
            DrawShadedBox((Vector3){ 0.2f * (float)s, 0, -0.04f }, (Vector3){ 0.4f, 0.02f, 0.14f }, dark);
            rlPopMatrix();
            rlPopMatrix();
        }
        rlPopMatrix();
    }
}

// Fireflies drifting round the reeds after dark, blinking.
static void DrawFireflies(float now)
{
    if (G.night < 0.4f) return;
    for (int i = 0, n = 0; i < G.resCount && n < 14; i++) {
        const Resource *r = &G.res[i];
        if (r->kind != RES_REEDS || XZDist(r->pos, G.cam.position) > 55.0f) continue;
        n++;
        for (int k = 0; k < 3; k++) {
            float ph = now * (0.35f + 0.1f * (float)k) + Hash2(i, k) * 6.28f;
            Vector3 p = { r->pos.x + cosf(ph) * (0.9f + 0.4f * (float)k), r->pos.y + 0.8f + 0.5f * sinf(ph * 1.7f),
                          r->pos.z + sinf(ph * 1.3f) * (0.9f + 0.3f * (float)k) };
            float glow = 0.5f + 0.5f * sinf(now * 2.6f + (float)(i * 3 + k) * 1.7f);
            if (glow < 0.3f) continue;
            DrawGlowBox(p, (Vector3){ 0.07f, 0.07f, 0.07f }, Mix((Color){ 60, 70, 30, 255 }, (Color){ 226, 244, 120, 255 }, glow));
        }
    }
}

void DrawLife(void)
{
    float now = (float)GetTime();
    int mat = CurrentMaterial();
    SetMaterial(MAT_THING);
    if (G.daylight > 0.25f) DrawBirds(now);
    DrawFireflies(now);
    DrawWakes();
    SetMaterial(mat);
}
