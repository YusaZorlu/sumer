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
               (G.climb ? PF_CLIMB : 0) | (G.parryT > 0 ? PF_PARRY : 0) | (G.waitBed ? PF_BED : 0) | (OutOfRound() ? PF_OUT : 0) |
               (TorchBurning() ? PF_TORCH : 0) | (MatchReady() ? PF_READY : 0) |
               (Party() && PartyVote() >= 0 ? (unsigned)(PartyVote() + 1) << PF_VOTE_SHIFT : 0);
    p->equip = G.equip;
    p->weapon = UseWeapon();
    p->bow = UseBow();
    p->armor = UseArmor();
    p->style = ChosenStyle();
    p->dates = BagCount(IT_DATES);
    p->heard = 0;
}

bool PlayerIn(int s)
{
    return s >= 0 && s < MAX_PLAYERS && G.peers[s].active;
}

bool PlayerAwake(int s)
{
    return PlayerIn(s) && !(G.peers[s].flags & (PF_SLEEP | PF_OUT));
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

void HurtSlot(int s, float dmg, int by)
{
    HurtSlotFrom(s, dmg, by, NULL);
}

// Like HurtSlot, with the place the blow came from, so the one hit can see which way to turn.
void HurtSlotFrom(int s, float dmg, int by, const Vector3 *src)
{
    if (s == G.self) HurtPlayerFrom(dmg, by, src);
    else if (G.role == NET_HOST && PlayerIn(s)) NetHurt(s, dmg, by, src);
}

// The acting player's blow or arrow landed.
void HitMark(void)
{
    bool arrow = !G.meleeing;
    if (G.actor != G.self) {
        NetHitMark(G.actor, arrow);
        return;
    }
    G.hitMarkT = 0.3f;
    if (arrow) SfxPlay(SFX_ARROW, 0.8f);
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
        // A drop in their health is a blow on them: they flinch (back, as if struck from the front).
        if (p->lastHealth > 0 && p->health < p->lastHealth - 0.9f) {
            p->hitT = FLINCH_TIME;
            p->hitX = -sinf(p->shownFacing);
            p->hitZ = -cosf(p->shownFacing);
        }
        p->lastHealth = p->health;
        // A new swing nearby: you hear it whoosh.
        if (p->swingT > p->lastSwing + 0.05f && (p->equip == EQ_MELEE || p->equip == EQ_MACE))
            SfxPlayAt(p->equip == EQ_MACE ? SFX_SWING_HEAVY : SFX_SWING, p->shown, 0.7f, 25.0f);
        p->lastSwing = p->swingT;
        if (p->hitT > 0) p->hitT -= dt;
        float k = fminf(1.0f, dt * 12.0f);
        Vector3 ahead = Vector3Add(p->pos, Vector3Scale(p->vel, fminf(p->heard, 0.25f)));
        if (Vector3Distance(p->shown, ahead) > 6.0f) p->shown = ahead;
        else p->shown = Vector3Lerp(p->shown, ahead, k);
        float diff = atan2f(sinf(p->facing - p->shownFacing), cosf(p->facing - p->shownFacing));
        p->shownFacing += diff * k;
    }
}

// A new place for another player: how fast they came from the last one carries them on until the next
// (between updates they keep walking instead of stopping and jumping).
void PeerMoved(Peer *p, Vector3 pos)
{
    Vector3 v = Vector3Scale(Vector3Subtract(pos, p->pos), 1.0f / fmaxf(p->heard, 0.05f));
    p->vel = p->active && Vector3Length(v) < 15.0f ? v : (Vector3){ 0, 0, 0 };
    p->pos = pos;
    p->heard = 0;
}

// A body leaning 'degrees' toward the horizontal direction 'dir', about its feet: draw the person between
// BeginLean and EndLean.
void BeginLean(Vector3 at, Vector2 dir, float degrees)
{
    rlPushMatrix();
    rlTranslatef(at.x, at.y, at.z);
    rlRotatef(degrees, dir.y, 0, -dir.x);
    rlTranslatef(-at.x, -at.y, -at.z);
}

void EndLean(void)
{
    rlPopMatrix();
}

// A look flushed red, as a body struck by a blow is, by 'k' (0 to 1).
Look Flushed(Look look, float k)
{
    const Color red = { 220, 40, 34, 255 };
    k = Clamp(k, 0.0f, 1.0f) * 0.55f;
    look.skin = Mix(look.skin, red, k);
    look.cloth = Mix(look.cloth, red, k);
    look.trim = Mix(look.trim, red, k);
    return look;
}

// A player waiting for the night lies on their own bed, head on the pillow, if they are beside it.
void DrawInBed(int s, Vector3 at, float facing, Look look)
{
    const House *h = &G.houses[HouseOf(s)];
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
        if (!p->active || s == G.self || (p->flags & PF_OUT) || XZDist(p->shown, G.cam.position) > 240.0f || !InView(p->shown, 2.0f)) continue;
        Look look = PlayerLook(s, p->style, p->armor);
        LightFor(p->shown);
        if (p->flags & PF_BED) {
            DrawInBed(s, p->shown, p->shownFacing, look);
            continue;
        }
        bool aim = (p->flags & PF_AIM) != 0;
        // Hit, they flinch back and flush red; swinging, they lean into the stroke.
        float hit = Clamp(p->hitT / FLINCH_TIME, 0.0f, 1.0f), swing = 0;
        if (p->equip == EQ_MELEE || p->equip == EQ_MACE) swing = SwingLean(SwingElapsed(p->swingT, p->equip == EQ_MACE), p->equip == EQ_MACE);
        Vector2 ahead = { sinf(p->shownFacing), cosf(p->shownFacing) };
        Vector2 lean = { p->hitX * 24.0f * hit + ahead.x * 12.0f * swing, p->hitZ * 24.0f * hit + ahead.y * 12.0f * swing };
        float degrees = Vector2Length(lean);
        if (degrees > 0.5f) BeginLean(p->shown, Vector2Scale(lean, 1.0f / degrees), degrees);
        // The weapon first: the hands reach for it.
        bool mace = p->equip == EQ_MACE;
        if ((p->equip == EQ_MELEE || mace) && !(p->flags & PF_CLIMB)) {
            HoldWeapon(p->shown, p->shownFacing, (p->flags & PF_CROUCH) != 0, mace, SwingElapsed(p->swingT, mace), (p->flags & PF_PARRY) && !mace);
            DrawWeaponPart(mace ? WEAPON_MACE : p->weapon);
        }
        bool torch = (p->flags & PF_TORCH) && !aim;
        if (torch) GripAt(HAND_OTHER, -0.36f, 1.36f, 0.28f);
        if (p->equip == EQ_BOW && !aim && !(p->flags & PF_CLIMB)) HoldBowFor(p->shown, p->shownFacing, (p->flags & PF_CROUCH) != 0, torch);
        DrawHuman(p->shown, p->shownFacing, p->stride, hit > 0 ? Flushed(look, hit) : look, (p->flags & PF_CROUCH) != 0, aim, p->draw);
        if (degrees > 0.5f) EndLean();
        if (torch) DrawHandTorch(p->shown, p->shownFacing);
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
