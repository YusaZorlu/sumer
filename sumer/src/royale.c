// Battle royale: up to twenty players on a map with no guards, raiders or barbarians, where houses
// are of no use. A round starts by itself half a minute after two or more players are in: everyone is
// set down somewhere across the map, far from the others, with the best gear. A sandstorm closes in on the city in stages and hurts
// whoever is caught in it. Fifteen statues of the gods stand about the land; praying at one gives a
// blessing for the rest of the round (+5% attack speed, +5% running speed or +10% healing), the first
// to reach it takes it. Whoever is brought down watches the others; the last one standing wins, and
// gains ten experience for every player the round began with.
#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { BOON_ATTACK, BOON_RUN, BOON_HEAL, BOONS };

typedef struct { Vector3 pos; int kind; const char *god; } Statue;

#define ROUND_WAIT 30.0f   // seconds from two players being in to the round's start

static bool mode, modeKnown;
static Statue statues[STATUES];
// 'countdown': seconds to the next round, or below 0 while fewer than two players are in.
static struct { int phase; float t; unsigned claimed; int winner, started; float countdown; } rnd;
static int boons[BOONS];
static bool out;            // this machine's player was brought down this round and only watches
static int watched = -1;    // the player a watcher follows
static float stormTick;

bool Royale(void)
{
    if (!modeKnown) {
        modeKnown = true;
        mode = strcmp(PageParam("mode"), "br") == 0;
    }
    return mode;
}

void SetRoyale(bool on)
{
    mode = on;
    modeKnown = true;
}

// Players in one game: twenty in a battle royale, six otherwise.
int PlayerLimit(void)
{
    return Royale() ? MAX_SEATS : 6;
}

float RoyaleCountdown(void)
{
    return rnd.countdown;
}

// How many players the round under way (or the last one) began with.
int RoyaleStarted(void)
{
    return rnd.started;
}

int RoyalePhase(void)
{
    return Royale() ? rnd.phase : RP_OFF;
}

bool RoyaleOut(void)
{
    return Royale() && out;
}

// Test tools only: jump the round's clock, or the wait for the next round.
void RoyaleSkipTo(float t)
{
    rnd.t = t;
}

void RoyaleCountdownTo(float t)
{
    if (rnd.countdown >= 0) rnd.countdown = t;
}

int RoyaleWinner(void)
{
    return rnd.winner;
}

// ---- The sandstorm ----

static const Vector2 STORM_CENTER = { 0.0f, CITY_Z };

// The storm's edge t seconds into a round: it waits a minute, then closes in, rests, closes in again.
// Each row: the time a stage ends and the radius it ends at.
static const float STAGES[][2] = {
    { 60, 800 }, { 240, 380 }, { 300, 380 }, { 450, 190 }, { 510, 190 }, { 630, 90 }, { 690, 90 }, { 810, 25 },
};
#define STAGE_COUNT ((int)(sizeof(STAGES) / sizeof(STAGES[0])))

float StormRadius(float t)
{
    float r0 = STAGES[0][1], t0 = 0;
    for (int k = 0; k < STAGE_COUNT; k++) {
        if (t <= STAGES[k][0]) return Lerp(r0, STAGES[k][1], (t - t0) / fmaxf(STAGES[k][0] - t0, 1.0f));
        t0 = STAGES[k][0];
        r0 = STAGES[k][1];
    }
    return r0;
}

// Health lost each second in the storm, worse as it closes.
static float StormHurt(float t)
{
    return t < 300 ? 2.0f : t < 510 ? 4.0f : t < 690 ? 7.0f : 12.0f;
}

// Seconds until the storm next starts to move, or 0 while it is moving or done.
static float StormWait(float t)
{
    for (int k = 0; k < STAGE_COUNT; k++)
        if (t < STAGES[k][0]) return (k == 0 || STAGES[k][1] == STAGES[k - 1][1]) ? STAGES[k][0] - t : 0.0f;
    return 0.0f;
}

bool InStorm(Vector3 p)
{
    if (RoyalePhase() != RP_RUN) return false;
    return Vector2Distance((Vector2){ p.x, p.z }, STORM_CENTER) > StormRadius(rnd.t);
}

// ---- The world as it is for a battle royale ----

// No guards anywhere, city or village: everyone else is a townsman, a villager or a beast.
static void RemoveGuards(void)
{
    int kept = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (v->kind == NPC_GUARD || v->kind == NPC_WARDEN) continue;
        G.villagers[kept++] = G.villagers[i];
    }
    G.villagerCount = kept;
}

// Somewhere dry and clear near (x, z), looked for in a widening spiral; the same answer on every machine.
static Vector3 ClearNear(float x, float z, bool outside)
{
    for (int k = 0; k < 200; k++) {
        float a = (float)k * 2.4f, r = 1.5f * sqrtf((float)k);
        float px = x + cosf(a) * r, pz = z + sinf(a) * r;
        if (fabsf(px) > WORLD_HALF - 15 || fabsf(pz) > WORLD_HALF - 15) continue;
        if (!FreeSpot(px, pz, 1.6f) || (outside && InsideWalls(px, pz, 6.0f))) continue;
        float ground = TerrainHeight(px, pz);
        if (GroundHeight(px, pz, ground + 0.5f) > ground + 0.3f) continue;
        return (Vector3){ px, ground, pz };
    }
    return (Vector3){ x, TerrainHeight(x, z), z };
}

// A worshipper statue of gypsum on a brick plinth, hands clasped, as they stood in the temples.
static void BuildStatue(Vector3 p, float face)
{
    const Color stone = { 222, 214, 196, 255 }, brick = { 150, 112, 78, 255 }, dark = { 40, 32, 30, 255 };
    float y = p.y, sx = sinf(face), cz = cosf(face);
    AddBoxOn(p.x, p.z, 1.6f, 1.0f, 1.6f, brick, true);
    AddShape(p.x, y + 1.0f, p.z, 0.9f, 1.2f, 0.75f, stone, true, SHAPE_CYL);
    AddShape(p.x, y + 2.15f, p.z, 0.8f, 0.85f, 0.55f, stone, false, SHAPE_BLOB);
    AddShape(p.x + sx * 0.18f, y + 2.05f, p.z + cz * 0.18f, 0.3f, 0.22f, 0.3f, stone, false, SHAPE_BLOB);
    AddShape(p.x, y + 2.75f, p.z, 0.42f, 0.5f, 0.42f, stone, false, SHAPE_BLOB);
    AddShape(p.x, y + 2.55f, p.z, 0.36f, 0.35f, 0.36f, dark, false, SHAPE_BLOB);
    AddBox(p.x + sx * 0.2f, y + 2.8f, p.z + cz * 0.2f, 0.32f, 0.08f, 0.08f, dark, false);
}

void BuildRoyale(void)
{
    if (!Royale()) return;
    static const char *const gods[BOONS] = { "Ninurta", "Enlil", "Gula" };
    // Five in the city, ten across the land on both banks of the river.
    static const float spots[STATUES][2] = {
        { -20, 20 }, { 40, -40 }, { -60, 50 }, { 60, 60 }, { -70, -60 },
        { 230, 120 }, { 300, -200 }, { 120, -330 }, { -40, 300 }, { 200, 330 },
        { -300, 200 }, { -330, -150 }, { -200, -360 }, { 380, 40 }, { -420, 420 },
    };
    for (int i = 0; i < STATUES; i++) {
        bool outside = i >= 5;
        Vector3 p = ClearNear(spots[i][0], spots[i][1], outside);
        statues[i] = (Statue){ p, i % BOONS, gods[i % BOONS] };
        BuildStatue(p, Hash2(i, 77) * 2.0f * PI);
        KeepOut(p.x - 1.5f, p.z - 1.5f, p.x + 1.5f, p.z + 1.5f);
    }
    // More trees out in the land to hide behind.
    int planted = 0;
    for (int k = 0; k < 2000 && planted < 150; k++) {
        float x = (Hash2(k, 5) * 2.0f - 1.0f) * (WORLD_HALF - 20), z = (Hash2(k, 9) * 2.0f - 1.0f) * (WORLD_HALF - 20);
        if (InsideWalls(x, z, -12.0f) || !FreeSpot(x, z, 3.0f)) continue;
        AddTree(x, z, 0.8f + Hash2(k, 3) * 0.6f, Hash2(k, 4) < 0.25f);
        KeepOut(x - 1, z - 1, x + 1, z + 1);
        planted++;
    }
}

// Once the world is built, before anyone plays: guards go.
void ShapeRoyale(void)
{
    if (!Royale()) return;
    RemoveGuards();
    for (int c = 0; c < MAX_CARAVANS; c++) G.caravans[c].state = CV_AWAY;
    G.pvp = G.combat = true;
    rnd.phase = RP_WAIT;
    rnd.t = 0;
    rnd.claimed = 0;
    rnd.winner = -1;
    rnd.started = 0;
    rnd.countdown = -1;
    memset(boons, 0, sizeof(boons));
    out = false;
    watched = -1;
}

// ---- Statues ----

int StatueCount(void)
{
    return Royale() ? STATUES : 0;
}

Vector3 StatuePos(int i)
{
    return statues[i < 0 || i >= STATUES ? 0 : i].pos;
}

bool StatueClaimed(int i)
{
    return (rnd.claimed >> i & 1u) != 0;
}

static const char *BoonText(int kind)
{
    return L(kind == BOON_ATTACK ? "+5% attack speed" : kind == BOON_RUN ? "+5% running speed" : "+10% healing");
}

void StatuePrompt(int i, char *e, int size, bool *ok)
{
    const Statue *s = &statues[i];
    *ok = RoyalePhase() == RP_RUN && !StatueClaimed(i) && !out;
    if (StatueClaimed(i)) Fmt(e, size, "The statue of %s: its blessing is taken", s->god);
    else if (RoyalePhase() != RP_RUN) Fmt(e, size, "The statue of %s: it blesses once the round begins", s->god);
    else Fmt(e, size, "[E] Pray to %s: %s for the round", s->god, BoonText(s->kind));
}

// The host's half: the first to pray takes the blessing. Returns the blessing's kind, or -1.
int ClaimStatue(int i, Vector3 by, int slot)
{
    if (RoyalePhase() != RP_RUN || i < 0 || i >= STATUES || StatueClaimed(i)) return -1;
    if (XZDist(statues[i].pos, by) > 3.5f || (G.peers[slot].flags & PF_OUT)) return -1;
    rnd.claimed |= 1u << i;
    return statues[i].kind;
}

void GrantBoon(int statue, int kind)
{
    if (kind < 0 || kind >= BOONS) return;
    boons[kind]++;
    const Statue *s = &statues[statue < 0 || statue >= STATUES ? 0 : statue];
    Say("%s blesses you: %s (%d so far).", s->god, BoonText(kind), boons[kind]);
    SpawnRing((Vector3){ s->pos.x, s->pos.y + 3.2f, s->pos.z }, (Color){ 255, 220, 120, 255 });
    SfxPlay(SFX_CLANG, 0.6f);
}

float RoyaleAttack(void)
{
    return 1.0f + 0.05f * (float)boons[BOON_ATTACK];
}

float RoyaleRun(void)
{
    return 1.0f + 0.05f * (float)boons[BOON_RUN];
}

float RoyaleHeal(void)
{
    return 1.0f + 0.10f * (float)boons[BOON_HEAL];
}

// ---- The round ----

static int Alive(int *last)
{
    int n = 0;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (G.peers[s].active && !(G.peers[s].flags & PF_OUT)) {
            n++;
            if (last) *last = s;
        }
    return n;
}

int RoyaleAlive(void)
{
    return Alive(NULL);
}

// This machine's player is set down at (x, z) for a new round.
void RoyaleBegin(float x, float z)
{
    Player *p = &G.player;
    out = false;
    watched = -1;
    memset(boons, 0, sizeof(boons));
    rnd.phase = RP_RUN;
    rnd.t = 0;
    rnd.claimed = 0;
    rnd.winner = -1;
    p->pos = (Vector3){ x, 0, z };
    p->pos.y = GroundHeight(x, z, TerrainHeight(x, z) + 0.5f);
    p->vy = 0;
    p->crouching = p->aiming = false;
    G.health = HEALTH_MAX;
    G.food = fmaxf(G.food, 80.0f);
    G.hurtBy = BY_NONE;
    G.climb = 0;
    if (G.panel != PANEL_NONE && G.panel != PANEL_LOOK) SetPanel(PANEL_NONE);
    Say("The round has begun. A sandstorm is closing in on the city; pray at the gods' statues for blessings. The last one standing wins.");
    G.msgT = 8.0f;
}

// The host starts a round: every player somewhere of their own across the land, far apart. Never for
// one player alone.
void RoyaleStart(void)
{
    if (!Royale() || G.role == NET_CLIENT) return;
    int n = 0, slots[MAX_PLAYERS];
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (G.peers[s].active) slots[n++] = s;
    if (n < 2) return;
    rnd.countdown = -1;
    float turn = RandF(0, 2.0f * PI);
    rnd.started = n;
    for (int k = 0; k < n; k++) {
        float a = turn + 2.0f * PI * (float)k / (float)n + RandF(-0.2f, 0.2f), r = RandF(240.0f, 420.0f);
        Vector3 at = ClearNear(STORM_CENTER.x + cosf(a) * r, STORM_CENTER.y + sinf(a) * r, true);
        int s = slots[k];
        G.peers[s].flags &= ~PF_OUT;
        G.peers[s].kills = G.peers[s].deaths = 0;
        if (s == G.self) RoyaleBegin(at.x, at.z);
        else NetRound(s, at.x, at.z);
    }
    rnd.phase = RP_RUN;
    rnd.t = 0;
    rnd.claimed = 0;
    rnd.winner = -1;
}

// Brought down during a round: out, to watch the others. Returns false when it is not a round's death.
bool RoyaleFall(void)
{
    if (RoyalePhase() != RP_RUN) return false;
    out = true;
    watched = -1;
    G.health = HEALTH_MAX;
    G.player.aiming = false;
    G.player.draw = 0;
    G.climb = 0;
    if (G.panel != PANEL_NONE) SetPanel(PANEL_NONE);
    return true;
}

// A guest who joins while a round is on watches until the next one.
void RoyaleJoinLate(bool running)
{
    if (!Royale()) return;
    out = running;
    if (running) rnd.phase = RP_RUN;
}

static void NextWatched(int step)
{
    for (int k = 1; k <= MAX_PLAYERS; k++) {
        int s = ((watched < 0 ? 0 : watched) + step * k + MAX_PLAYERS * 4) % MAX_PLAYERS;
        if (s != G.self && G.peers[s].active && !(G.peers[s].flags & PF_OUT)) {
            watched = s;
            return;
        }
    }
    watched = -1;
}

void RoyaleUpdate(float dt)
{
    if (!Royale()) return;
    if (rnd.phase == RP_RUN) rnd.t += dt;
    if (out && (watched < 0 || !G.peers[watched].active || (G.peers[watched].flags & PF_OUT))) NextWatched(1);
    if (out && (MousePressed(MOUSE_BUTTON_LEFT) || KeyPressed(KEY_SPACE)) && G.panel == PANEL_NONE) NextWatched(1);

    bool host = G.role != NET_CLIENT;
    if (!host) {
        if (rnd.countdown > 0) rnd.countdown = fmaxf(0.0f, rnd.countdown - dt);
        return;
    }
    // Between rounds: half a minute once two or more are in, then the round begins.
    if (rnd.phase != RP_RUN) {
        if (PlayerCount() < 2) rnd.countdown = -1;
        else if (rnd.countdown < 0) rnd.countdown = ROUND_WAIT;
        else if ((rnd.countdown -= dt) <= 0) RoyaleStart();
        return;
    }

    // The storm hurts whoever it catches, twice a second.
    stormTick += dt;
    if (stormTick >= 0.5f) {
        stormTick = 0;
        float hurt = StormHurt(rnd.t) * 0.5f;
        for (int s = 0; s < MAX_PLAYERS; s++)
            if (G.peers[s].active && !(G.peers[s].flags & PF_OUT) && InStorm(G.peers[s].pos)) HurtSlotFrom(s, hurt, BY_STORM, NULL);
    }
    // The last one standing wins; a round that everyone has left ends without a winner.
    int last = -1, alive = Alive(&last);
    if ((rnd.started >= 2 && alive <= 1) || alive == 0) {
        rnd.phase = RP_OVER;
        rnd.winner = alive == 1 ? last : -1;
        if (rnd.winner >= 0) {
            Announce("%s is the last one standing and wins the round!", G.peers[rnd.winner].name);
            SayTo(rnd.winner, "Congratulations, you are the last one standing and win the round! +%d XP", XP_WIN * rnd.started);
            AwardXp(rnd.winner, XP_WIN * rnd.started, 0, 0);
        } else {
            Announce("No one is left standing. The storm wins this round.");
        }
    }
}

// The round's state, as the host sends it to everyone.
void WriteRoyale(Writer *w)
{
    W8(w, (unsigned)rnd.phase);
    WF(w, rnd.t);
    W16(w, rnd.claimed);
    W8(w, rnd.winner >= 0 ? (unsigned)rnd.winner : 255u);
    W8(w, (unsigned)rnd.started);
    W8(w, rnd.countdown >= 0 ? (unsigned)Clamp(ceilf(rnd.countdown), 0.0f, 254.0f) : 255u);
}

void ReadRoyale(Reader *r)
{
    int phase = (int)R8(r);
    float t = RF(r);
    unsigned claimed = R16(r);
    int winner = (int)R8(r), started = (int)R8(r), countdown = (int)R8(r);
    if (r->bad) return;
    rnd.started = started;
    if (countdown == 255) rnd.countdown = -1;
    else if (rnd.countdown < 0 || fabsf(rnd.countdown - (float)countdown) > 1.5f) rnd.countdown = (float)countdown;
    rnd.phase = phase;
    if (fabsf(rnd.t - t) > 1.0f || phase != RP_RUN) rnd.t = t;
    rnd.claimed = claimed;
    rnd.winner = winner == 255 ? -1 : winner;
}

// ---- Watching ----

// A watcher's camera rides behind the player they watch; true when it did.
bool RoyaleCamera(void)
{
    if (!RoyaleOut() || watched < 0) return false;
    const Peer *p = &G.peers[watched];
    const Player *me = &G.player;
    Vector3 at = { p->shown.x, p->shown.y + 1.6f, p->shown.z };
    float cp = cosf(me->pitch);
    Vector3 dir = { cp * sinf(me->yaw), sinf(me->pitch), cp * cosf(me->yaw) };
    G.cam.position = Vector3Add(at, Vector3Scale(dir, 6.0f));
    G.cam.target = at;
    G.cam.fovy = 55.0f;
    return true;
}

// ---- Drawing ----

// A blessing still to be taken burns over its statue: red for Ninurta's strength, sky blue for Enlil's
// wind, green for Gula's healing.
void DrawRoyale(void)
{
    if (!Royale()) return;
    static const Color glow[BOONS] = { { 255, 90, 60, 255 }, { 120, 200, 255, 255 }, { 120, 240, 120, 255 } };
    float t = (float)GetTime();
    for (int i = 0; i < STATUES; i++) {
        if (StatueClaimed(i) || XZDist(statues[i].pos, G.cam.position) > 260.0f) continue;
        Vector3 p = statues[i].pos;
        float bob = 0.12f * sinf(t * 2.0f + (float)i);
        DrawGlowBox((Vector3){ p.x, p.y + 3.5f + bob, p.z }, (Vector3){ 0.32f, 0.32f, 0.32f }, glow[statues[i].kind]);
        DrawGlowBox((Vector3){ p.x, p.y + 3.5f + bob, p.z }, (Vector3){ 0.18f, 0.5f, 0.18f }, (Color){ 255, 250, 220, 255 });
    }
}

// The storm's wall of dust, where its edge crosses the land near the camera.
void DrawStorm(void)
{
    if (RoyalePhase() != RP_RUN) return;
    float r = StormRadius(rnd.t);
    const int segs = 160;
    Vector3 cam = G.cam.position;
    rlDisableBackfaceCulling();
    rlCheckRenderBatchLimit(segs * 8);
    rlBegin(RL_QUADS);
    for (int k = 0; k < segs; k++) {
        float a0 = 2.0f * PI * (float)k / (float)segs, a1 = 2.0f * PI * (float)(k + 1) / (float)segs;
        float x0 = STORM_CENTER.x + cosf(a0) * r, z0 = STORM_CENTER.y + sinf(a0) * r;
        float x1 = STORM_CENTER.x + cosf(a1) * r, z1 = STORM_CENTER.y + sinf(a1) * r;
        if (Vector2Distance((Vector2){ x0, z0 }, (Vector2){ cam.x, cam.z }) > 400.0f) continue;
        rlColor4ub(176, 128, 76, 200);
        rlVertex3f(x0, -6.0f, z0);
        rlVertex3f(x1, -6.0f, z1);
        rlColor4ub(200, 160, 110, 150);
        rlVertex3f(x1, 25.0f, z1);
        rlVertex3f(x0, 25.0f, z0);
        rlVertex3f(x0, 25.0f, z0);
        rlVertex3f(x1, 25.0f, z1);
        rlColor4ub(214, 180, 130, 0);
        rlVertex3f(x1, 70.0f, z1);
        rlVertex3f(x0, 70.0f, z0);
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// The line at the top of the screen, what a watcher sees, and a dust-red screen inside the storm.
void DrawRoyaleHUD(float sw, float sh)
{
    if (!Royale()) return;
    const Color gold = { 240, 200, 120, 255 }, text = { 240, 230, 210, 255 };
    int players = PlayerCount(), alive = Alive(NULL);
    const char *line;
    if (rnd.phase == RP_WAIT || rnd.phase == RP_OVER) {
        const char *done = rnd.phase == RP_OVER ? (rnd.winner >= 0 ? TextFormat(L("%s won the round. "), G.peers[rnd.winner].name)
                                                                           : L("The storm won the round. ")) : "";
        line = rnd.countdown >= 0 ? TextFormat(L("%sBattle royale: %d players here. The round starts in %d s."), done, players, (int)ceilf(rnd.countdown))
                                  : TextFormat(L("%sBattle royale: %d players here. A round starts once two or more players are here."), done, players);
    } else {
        float wait = StormWait(rnd.t);
        line = wait > 0 ? TextFormat(L("%d of %d standing. The storm moves in %d:%02d."), alive, rnd.started, (int)wait / 60, (int)wait % 60)
                        : TextFormat(L("%d of %d standing. The storm is closing in."), alive, rnd.started);
    }
    Vector2 m = MeasureTextEx(GameFont(), line, 18, 1);
    DrawRectangle((int)(sw / 2 - m.x / 2 - 10), 8, (int)m.x + 20, 28, Fade(BLACK, 0.55f));
    DrawTextEx(GameFont(), line, (Vector2){ sw / 2 - m.x / 2, 13 }, 18, 1, rnd.phase == RP_RUN ? text : gold);

    if (out) {
        const char *who = watched >= 0 ? TextFormat(L("You are out. Watching %s (click or Space: the next player)."), G.peers[watched].name)
                                       : L("You are out. No one is left to watch.");
        Vector2 w = MeasureTextEx(GameFont(), who, 18, 1);
        DrawRectangle((int)(sw / 2 - w.x / 2 - 10), (int)sh - 120, (int)w.x + 20, 28, Fade(BLACK, 0.55f));
        DrawTextEx(GameFont(), who, (Vector2){ sw / 2 - w.x / 2, sh - 115 }, 18, 1, gold);
    } else if (InStorm(G.player.pos)) {
        DrawRectangle(0, 0, (int)sw + 1, (int)sh + 1, Fade((Color){ 170, 110, 50, 255 }, 0.38f));
        const char *warn = L("You are in the sandstorm! Get inside the circle on the map.");
        Vector2 w = MeasureTextEx(GameFont(), warn, 22, 2);
        DrawRectangle((int)(sw / 2 - w.x / 2 - 10), (int)(sh / 2 + 56), (int)w.x + 20, 32, Fade(BLACK, 0.5f));
        DrawTextEx(GameFont(), warn, (Vector2){ sw / 2 - w.x / 2, sh / 2 + 60 }, 22, 2, (Color){ 255, 220, 160, 255 });
    }
}

// On the map (inside its clipping): the storm's edge and the blessings still to be taken.
void DrawRoyaleMap(float mx, float mz, float s)
{
    if (!Royale()) return;
    if (rnd.phase == RP_RUN) {
        Vector2 c = { mx + STORM_CENTER.x * s, mz + STORM_CENTER.y * s };
        DrawCircleLinesV(c, StormRadius(rnd.t) * s, (Color){ 230, 140, 60, 255 });
        DrawCircleLinesV(c, StormRadius(rnd.t) * s + 1, (Color){ 230, 140, 60, 255 });
    }
    for (int i = 0; i < STATUES; i++) {
        if (StatueClaimed(i)) continue;
        DrawPoly((Vector2){ mx + statues[i].pos.x * s, mz + statues[i].pos.z * s }, 4, 3.5f, 45, (Color){ 255, 236, 150, 255 });
    }
}
