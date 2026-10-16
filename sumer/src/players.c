// The players as the world sees them. Creatures and people react to whichever player is near, and
// what happens to a player who is on another machine is passed to that machine.
#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static float warnT[MAX_PLAYERS][WARN_KINDS];

// The local player's own state, copied to where the rest of the game looks for every player.
void SyncSelf(void)
{
    Peer *p = &G.peers[G.self];
    const Player *pl = &G.player;

    p->active = true;
    p->pos = p->shown = pl->pos;
    p->facing = p->shownFacing = pl->facing;
    p->stride = pl->stride;
    p->draw = pl->draw;
    p->health = G.health;
    p->swingT = G.swingT;
    p->flags = (pl->crouching ? PF_CROUCH : 0) | (pl->running ? PF_RUN : 0) | (pl->moving ? PF_MOVE : 0) |
               (pl->grounded ? PF_GROUND : 0) | (pl->aiming ? PF_AIM : 0) | (G.sleeping || G.waitBed ? PF_SLEEP : 0) |
               (G.climb ? PF_CLIMB : 0) | (G.parryT > 0 ? PF_PARRY : 0) | (G.waitBed ? PF_BED : 0);
    p->equip = G.equip;
    p->weapon = G.weapon;
    p->bow = G.bow;
    p->armor = G.armor;
    p->heard = 0;
}

bool PlayerIn(int s)
{
    return s >= 0 && s < MAX_PLAYERS && G.peers[s].active;
}

bool PlayerAwake(int s)
{
    return PlayerIn(s) && !(G.peers[s].flags & PF_SLEEP);
}

Vector3 PlayerAt(int s)
{
    return PlayerIn(s) ? G.peers[s].pos : G.player.pos;
}

// The nearest player who is up and about, or -1 when everyone sleeps.
int NearestPlayer(Vector3 p, float *dist)
{
    int best = -1;
    float bestD = 1e9f;
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!PlayerAwake(s)) continue;
        float d = XZDist(p, G.peers[s].pos);
        if (d < bestD) {
            bestD = d;
            best = s;
        }
    }
    if (dist) *dist = bestD;
    return best;
}

// Distance to the nearest player at all, asleep or not: things change only where no one sees them.
float NearestAnyone(Vector3 p)
{
    float best = 1e9f;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (PlayerIn(s)) best = fminf(best, XZDist(p, G.peers[s].pos));
    return best;
}

// How noticeable a player is: crouching hides them, running and jumping draw the eye.
float PlayerStance(int s)
{
    int f = G.peers[s].flags;
    if (!(f & PF_GROUND)) return 1.3f;
    if (f & PF_CROUCH) return (f & PF_MOVE) ? 0.45f : 0.3f;
    if (f & PF_RUN) return 1.6f;
    return (f & PF_MOVE) ? 1.0f : 0.7f;
}

// The host judges for everyone who is alone outside the walls: other people or another player
// close by is company enough to keep the wolves away.
void UpdatePlayerSafety(float dt)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        Peer *p = &G.peers[s];
        for (int k = 0; k < WARN_KINDS; k++)
            if (warnT[s][k] > 0) warnT[s][k] -= dt;
        if (!p->active) continue;
        bool company = PeopleNear(p->pos, SAFE_RADIUS);
        for (int o = 0; o < MAX_PLAYERS && !company; o++)
            if (o != s && G.peers[o].active && XZDist(G.peers[o].pos, p->pos) < SAFE_RADIUS) company = true;
        p->alone = !InsideWalls(p->pos.x, p->pos.z, -2.0f) && !company;
        if (p->wanted > 0) p->wanted = fmaxf(0.0f, p->wanted - dt);
    }
}

// A warning to a player that is not repeated for a while.
bool Warn(int s, int kind, float quiet)
{
    if (s < 0 || s >= MAX_PLAYERS || warnT[s][kind] > 0) return false;
    warnT[s][kind] = quiet;
    return true;
}

void HurtSlot(int s, float dmg)
{
    if (s == G.self) HurtPlayer(dmg);
    else if (G.role == NET_HOST && PlayerIn(s)) NetHurt(s, dmg);
}

// The acting player's blow or arrow landed.
void HitMark(void)
{
    if (G.actor == G.self) G.hitMarkT = 0.3f;
    else NetHitMark(G.actor);
}

void ShowImpact(Vector3 at)
{
    if (G.actor == G.self) {
        G.impact = at;
        G.impactT = 3.0f;
    } else {
        NetImpact(G.actor, at);
    }
}

// SayTo, Announce and SayNear take English words for their %s: each screen translates them itself.

// A message for one player, wherever they are playing.
void SayTo(int s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (s == G.self || G.role != NET_HOST) LocalSayV(fmt, ap);
    else if (PlayerIn(s)) NetSayV(s, fmt, ap);
    va_end(ap);
}

// News for everyone in the game.
void Announce(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (G.role == NET_HOST)
        for (int s = 0; s < MAX_PLAYERS; s++)
            if (s != G.self && PlayerIn(s)) {
                va_list each;
                va_copy(each, ap);
                NetSayV(s, fmt, each);
                va_end(each);
            }
    if (G.role != NET_CLIENT) LocalSayV(fmt, ap);
    va_end(ap);
}

// For the players who are close enough to see it happen.
void SayNear(Vector3 at, float r, const char *fmt, ...)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        if (!PlayerIn(s) || XZDist(at, G.peers[s].pos) > r) continue;
        va_list ap;
        va_start(ap, fmt);
        if (s == G.self) LocalSayV(fmt, ap);
        else if (G.role == NET_HOST) NetSayV(s, fmt, ap);
        va_end(ap);
    }
}

// Other players, drawn where their machines last said they were and eased toward it.
void UpdatePeers(float dt)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        Peer *p = &G.peers[s];
        if (!p->active || s == G.self) continue;
        p->heard += dt;
        float k = fminf(1.0f, dt * 12.0f);
        if (Vector3Distance(p->shown, p->pos) > 6.0f) p->shown = p->pos;
        else p->shown = Vector3Lerp(p->shown, p->pos, k);
        float diff = atan2f(sinf(p->facing - p->shownFacing), cosf(p->facing - p->shownFacing));
        p->shownFacing += diff * k;
    }
}

static void PeerWeapon(const Peer *p, Vector3 at)
{
    float shoulder = (p->flags & PF_CROUCH) ? 1.04f : 1.42f;
    rlPushMatrix();
    rlTranslatef(at.x, at.y + shoulder, at.z);
    rlRotatef(p->shownFacing * RAD2DEG, 0, 1, 0);
    rlTranslatef(0.32f, 0, 0.1f);
    rlRotatef(p->swingT > 0 ? -60.0f : 25.0f, 1, 0, 0);
    if (p->equip == EQ_MACE) {
        DrawShadedBox((Vector3){ 0, 0, 0.45f }, (Vector3){ 0.06f, 0.06f, 0.95f }, COL_WOOD);
        DrawShadedBlob((Vector3){ 0, 0, 0.98f }, (Vector3){ 0.17f, 0.17f, 0.2f }, (Color){ 120, 112, 100, 255 });
    } else if (p->weapon >= 2) {
        DrawShadedBox((Vector3){ 0, 0, 0.12f }, (Vector3){ 0.05f, 0.05f, 0.25f }, COL_WOOD);
        DrawShadedBox((Vector3){ 0, 0, 0.5f }, (Vector3){ 0.03f, 0.08f, 0.6f }, COL_BRONZE);
    } else {
        DrawShadedBox((Vector3){ 0, 0, 0.35f }, (Vector3){ 0.06f, 0.06f, 0.7f }, COL_WOOD);
        DrawShadedBox((Vector3){ 0, 0, 0.72f }, (Vector3){ 0.13f, 0.13f, 0.22f }, p->weapon ? (Color){ 184, 115, 70, 255 } : COL_PALM_TRUNK);
    }
    rlPopMatrix();
}

// A player waiting for the night lies on their own bed, head on the pillow, if they are beside it.
void DrawInBed(int s, Vector3 at, float facing, Look look)
{
    const House *h = &G.houses[s >= 0 && s < MAX_PLAYERS ? s : 0];
    Vector3 bed = h->stations[0];
    if (XZDist(bed, at) < 3.0f) {
        float f = h->bedFacing;
        DrawPersonBody((Vector3){ bed.x + sinf(f) * 0.95f, bed.y + 0.36f, bed.z + cosf(f) * 0.95f }, f, look);
    } else {
        DrawPersonBody((Vector3){ at.x, at.y + 0.3f, at.z }, facing, look);
    }
}

// Each player wears their own color; waiting for the night they lie in bed.
void DrawPeers(void)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active || s == G.self || XZDist(p->shown, G.cam.position) > 240.0f) continue;
        Color own = PlayerColor(s);
        Look look = { COL_SKIN, own, Mix(own, COL_LINEN, 0.2f), COL_HAIR };
        LightFor(p->shown);
        if (p->flags & PF_BED) {
            DrawInBed(s, p->shown, p->shownFacing, look);
            continue;
        }
        bool aim = (p->flags & PF_AIM) != 0;
        DrawHuman(p->shown, p->shownFacing, p->stride, look, (p->flags & PF_CROUCH) != 0, aim, p->draw);
        if ((p->equip == EQ_MELEE || p->equip == EQ_MACE) && !(p->flags & PF_CLIMB)) PeerWeapon(p, p->shown);
        if (G.night >= 0.3f) DrawHandTorch(p->shown, p->shownFacing);
    }
    G.drawTorch = G.drawFog = 0;
}

int PlayerCount(void)
{
    int n = 0;
    for (int s = 0; s < MAX_PLAYERS; s++) n += G.peers[s].active;
    return n;
}

int PlayersInBed(void)
{
    int n = 0;
    for (int s = 0; s < MAX_PLAYERS; s++) n += G.peers[s].active && (G.peers[s].flags & PF_BED);
    return n;
}
