// Sumer Party: up to twelve players play four minigames of four minutes, with half a minute between them.
// Everyone in says they are ready (two or more), and ten seconds later the party begins. While they wait,
// each can vote to drop one of the five minigames: the most voted goes (a tie, or no votes: one of them by
// chance), and the other four come in a random order. Each game gives points as it is played, and its top
// three +10, +6 and +3 more; the totals rank the party, whose top three gain experience. Then the scores
// start again from nothing. The world is the city's, with no guards and no caravans; food stays full, the
// houses are of no use, and nothing is kept. Someone who joins while a game is on watches until the next.
// The ready check and sending away whoever idles without being ready serve the battle royale too.
#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PARTY_START  10.0f    // seconds from everyone being ready to the first game
#define GAME_TIME    240.0f
#define BREAK_TIME   30.0f
#define END_TIME     25.0f    // the final scores stay up this long
#define IDLE_KICK    180.0f   // not ready and not moving this long in a waiting room: sent away
#define DOWN_TIME    5.0f     // brought down in a game: back in this much later
#define LAVA_DEPTH   0.5f     // floor is lava: anything lower than this above the ground burns
#define DATES_MOST   30       // date stashing: the most points dates can bring
#define LAVA_HURT    8.0f     // health a second, through armor
#define HIDE_TICK    30.0f    // hide and seek: a point for every this long a hider stays hidden
#define HIDE_MOST    8
#define HIDE_LAST    6        // and this many more for one never found
#define FIND_POINTS  6        // the seeker's, for each hider found
#define SEEKER_SLIPS 5        // townsfolk the seeker may bring down: at this many the seeker loses
#define STRAY_HURT   5.0f     // a hider out of the city loses this much health a second

static bool mode, modeKnown;
// The party as the host runs it and sends it to everyone. 'order' holds the four games in the order
// they are played, 'joined' who plays the game under way (a bit for each slot).
static struct {
    int phase;
    float t;
    int game, order[PARTY_GAMES], vetoed;
    unsigned joined;
    int round[MAX_PLAYERS], total[MAX_PLAYERS];
    int seeker, npcKills;
    unsigned found;   // hide and seek: the hiders found
} pty;
static int lastSeeker = -1;   // the host's: no one seeks twice in a row
// This machine's player ('caught': found in hide and seek, before the host's word of it arrives).
static bool ready, waiting, caught;
static int vote = -1;
static float downT;
static Vector3 lastPos[MAX_PLAYERS];
static float idle[MAX_PLAYERS];

bool Party(void)
{
    if (!modeKnown) {
        modeKnown = true;
        mode = strcmp(PageParam("mode"), "party") == 0;
    }
    return mode;
}

void SetParty(bool on)
{
    mode = on;
    modeKnown = true;
}

// A game of rounds: a battle royale or a party.
bool Match(void)
{
    return Royale() || Party();
}

// The raiders, barbarians and bandits stay away: in a battle royale, and in a party except while the
// barbarians attack the city.
bool RaidersAway(void)
{
    return Royale() || (Party() && PartyGame() != PG_DEFENCE);
}

int PartyPhase(void)
{
    return Party() ? pty.phase : PP_OFF;
}

// The minigame under way, or -1.
int PartyGame(void)
{
    return Party() && pty.phase == PP_GAME ? pty.order[pty.game] : -1;
}

// The games' names and rules in English: news sent to everyone carries these, and each screen translates.
static const char *const NAMES[PG_COUNT] = { "Floor is lava", "Date stashing", "One-shot archers", "City defence", "Hide and seek" };
static const char *const RULES[PG_COUNT] = {
        "Stay off the ground: anything lower than half a metre burns. +6 a kill, -2 a death.",
        "Gather dates and keep them: 1 point for every 5 you carry when time runs out (30 at most).",
        "Bows only, and one arrow kills. +6 a kill, -2 a death.",
        "The barbarians attack the city. +2 for every one you bring down.",
        "One seeker hunts the others hidden among the townsfolk. Hiders: +1 every 30 s and +6 for lasting.",
};

const char *PartyGameName(int g)
{
    return g >= 0 && g < PG_COUNT ? L(NAMES[g]) : "";
}

const char *PartyGameRule(int g)
{
    return g >= 0 && g < PG_COUNT ? L(RULES[g]) : "";
}

int PartyRound(int slot)
{
    return slot >= 0 && slot < MAX_PLAYERS ? pty.round[slot] : 0;
}

int PartyTotal(int slot)
{
    return slot >= 0 && slot < MAX_PLAYERS ? pty.total[slot] : 0;
}

// Out of the game under way: joined while it was on (watching), or brought down and back in a moment.
bool PartyOut(void)
{
    return Party() && pty.phase == PP_GAME && (waiting || downT > 0);
}

// ---- Ready, votes and idlers (a battle royale's waiting too) ----

bool MatchReady(void)
{
    return ready;
}

void ToggleReady(void)
{
    ready = !ready;
}

// Waiting for a round or a party to begin: the time to say you are ready.
bool LobbyWaiting(void)
{
    if (Royale()) return RoyalePhase() != RP_RUN;
    if (Party()) return pty.phase == PP_WAIT || pty.phase == PP_COUNT || pty.phase == PP_END;
    return false;
}

// How many of the players in are ready ('players': how many are in).
int ReadyCount(int *players)
{
    int n = 0, in = 0;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (G.peers[s].active) {
            in++;
            n += (G.peers[s].flags & PF_READY) != 0;
        }
    if (players) *players = in;
    return n;
}

int PartyVote(void)
{
    return vote;
}

void SetPartyVote(int g)
{
    vote = g >= 0 && g < PG_COUNT && g != vote ? g : -1;
}

// Votes to drop game g, as the players' flags carry them.
int PartyVotes(int g)
{
    int n = 0;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (G.peers[s].active && ((G.peers[s].flags & PF_VOTE_MASK) >> PF_VOTE_SHIFT) == (unsigned)(g + 1)) n++;
    return n;
}

// The host, while a room waits: whoever is not ready and has not moved for three minutes is sent away.
void UpdateIdle(float dt)
{
    for (int s = 0; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active || s == G.self) {
            idle[s] = 0;
            continue;
        }
        if ((p->flags & PF_READY) || XZDist(p->pos, lastPos[s]) > 0.3f) {
            idle[s] = 0;
            lastPos[s] = p->pos;
            continue;
        }
        if ((idle[s] += dt) > IDLE_KICK) {
            idle[s] = 0;
            KickSlot(s, "You were away too long without saying you were ready, and left the game.");
        }
    }
}

// ---- The world for a party ----

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

// Once the world is built, before anyone plays: the guards and the caravans go.
void ShapeParty(void)
{
    if (!Party()) return;
    RemoveGuards();
    PartyCrowd();
    for (int c = 0; c < MAX_CARAVANS; c++) G.caravans[c].state = CV_AWAY;
    memset(&pty, 0, sizeof(pty));
    pty.phase = PP_WAIT;
    pty.seeker = -1;
    lastSeeker = -1;
    ready = waiting = caught = false;
    vote = -1;
    downT = 0;
}

// The rules of the moment: who can hurt whom, and with what.
static void ApplyRules(void)
{
    int g = PartyGame();
    G.pvp = g == PG_LAVA || g == PG_DATES || g == PG_ARCHERS || g == PG_HIDE;
    G.combat = true;
}

// ---- The host runs the party ----

static bool Joined(int s)
{
    return (pty.joined >> s & 1u) != 0;
}

static void EndGame(void);

// ---- Hide and seek ----

static bool Found(int s)
{
    return (pty.found >> s & 1u) != 0 || (s == G.self && caught);
}

int PartySeeker(void)
{
    return PartyGame() == PG_HIDE ? pty.seeker : -1;
}

// Someone still hiding: no armor, no weapon, no running, a townsman's look, no harm to the seeker.
bool PartyHider(int s)
{
    return PartyGame() == PG_HIDE && s >= 0 && s < MAX_PLAYERS && s != pty.seeker && Joined(s) && !Found(s);
}

// Whether player a's blow or arrow may hurt player s: in hide and seek only the seeker's, and only hiders.
bool PartyMayHurt(int a, int s)
{
    return PartyGame() != PG_HIDE || (a == pty.seeker && PartyHider(s));
}

bool PartyDisguise(int s, Look *look)
{
    static const Color robes[6] = { { 200, 190, 160, 255 }, { 150, 110, 80, 255 }, { 180, 170, 140, 255 },
                                    { 160, 120, 90, 255 },  { 120, 140, 150, 255 }, { 186, 150, 120, 255 } };
    if (!PartyHider(s)) return false;
    *look = PersonLook(LOOK_TOWN, 700u + 13u * (unsigned)s, robes[s % 6]);
    return true;
}

// The seeker brought down one of the townsfolk: five and the seeker loses the game.
void PartyNpcKill(int a)
{
    if (PartyGame() != PG_HIDE || a != pty.seeker || G.role == NET_CLIENT) return;
    if (++pty.npcKills >= SEEKER_SLIPS) {
        Announce("The seeker brought down %d townsfolk and loses the game!", SEEKER_SLIPS);
        EndGame();
    } else {
        SayTo(a, "That was one of the townsfolk (%d of %d): at %d you lose.", pty.npcKills, SEEKER_SLIPS, SEEKER_SLIPS);
    }
}

// Found hiders hold out in the village farthest from the city.
static Vector3 FarVillage(void)
{
    int far = 0;
    for (int v = 1; v < VILLAGES; v++)
        if (XZDist(VillageCenter(v), (Vector3){ 0, 0, CITY_Z }) > XZDist(VillageCenter(far), (Vector3){ 0, 0, CITY_Z })) far = v;
    Vector3 c = VillageCenter(far);
    for (int k = 0; k < 30; k++) {
        float x = c.x + RandF(-12.0f, 12.0f), z = c.z + RandF(-12.0f, 12.0f);
        if (!InWater(x, z) && !SolidAt(x, z, 0.6f)) return (Vector3){ x, TerrainHeight(x, z), z };
    }
    return c;
}

static int HidersLeft(void)
{
    int n = 0;
    for (int s = 0; s < MAX_PLAYERS; s++) n += PartyHider(s) && PlayerIn(s);
    return n;
}

static void PickSeeker(void)
{
    int c[MAX_PLAYERS], n = 0;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (Joined(s) && s != lastSeeker) c[n++] = s;
    for (int s = 0; s < MAX_PLAYERS && n == 0; s++)
        if (Joined(s)) c[n++] = s;
    pty.seeker = lastSeeker = n > 0 ? c[GetRandomValue(0, n - 1)] : -1;
    pty.found = 0;
}

// The host: points for hiding as time goes by, and a hider out of the city wastes away.
static void HideUpdate(float dt)
{
    static float tick;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (PartyHider(s)) pty.round[s] = (int)(pty.t / HIDE_TICK) < HIDE_MOST ? (int)(pty.t / HIDE_TICK) : HIDE_MOST;
    if ((tick += dt) >= 0.5f) {
        tick = 0;
        for (int s = 0; s < MAX_PLAYERS; s++) {
            const Peer *p = &G.peers[s];
            if (PartyHider(s) && PlayerIn(s) && !(p->flags & PF_OUT) && !InsideWalls(p->pos.x, p->pos.z, 0.0f)) HurtSlotFrom(s, STRAY_HURT * 0.5f, BY_STRAY, NULL);
        }
    }
    if (HidersLeft() == 0) {
        Announce("Every hider is found!");
        EndGame();
    }
}

// Where player s starts a game (or waits out a break): somewhere in the city of its own.
static Vector3 StartSpot(int s, int g)
{
    if (g == PG_HIDE && s != pty.seeker && Found(s)) return FarVillage();
    if (g != PG_LAVA || RoofCount() == 0) return CitySpawn(s);
    // Floor is lava: up on a roof, as far from the others as a few tries find.
    Vector3 best = RoofSpot(0);
    float bestAway = -1.0f;
    for (int k = 0; k < 40; k++) {
        Vector3 r = RoofSpot(GetRandomValue(0, RoofCount() - 1));
        float away = 1e9f;
        for (int o = 0; o < MAX_PLAYERS; o++)
            if (o != s && PlayerIn(o) && !(G.peers[o].flags & PF_OUT)) away = fminf(away, XZDist(G.peers[o].pos, r));
        if (away > bestAway) {
            bestAway = away;
            best = r;
        }
    }
    return best;
}

bool InLava(Vector3 pos)
{
    return PartyGame() == PG_LAVA && pos.y < TerrainHeight(pos.x, pos.z) + LAVA_DEPTH;
}

// The lava burns whoever stands in it, twice a second. Everyone wears bronze in a party: the burn is
// made to come through it whole.
static void BurnLava(float dt)
{
    static float tick;
    // A few seconds' grace as the game begins, for where everyone was put to reach the host.
    if (PartyGame() != PG_LAVA || pty.t < 3.0f || (tick += dt) < 0.5f) return;
    tick = 0;
    float hurt = LAVA_HURT * 0.5f / (1.0f - ArmorReduction(ARMOR_BRONZE));
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (Joined(s) && PlayerIn(s) && !(G.peers[s].flags & PF_OUT) && InLava(G.peers[s].pos)) HurtSlotFrom(s, hurt, BY_LAVA, NULL);
}

static void Place(int s, Vector3 at, int kind)
{
    if (s == G.self) PartyBegin(at, kind);
    else NetParty(s, at, kind);
}

static void StartGame(int k)
{
    pty.phase = PP_GAME;
    pty.game = k;
    pty.t = 0;
    pty.joined = 0;
    pty.seeker = -1;
    pty.npcKills = 0;
    pty.found = 0;
    int g = pty.order[k];
    for (int s = 0; s < MAX_PLAYERS; s++) {
        pty.round[s] = 0;
        if (!G.peers[s].active) continue;
        pty.joined |= 1u << s;
        G.peers[s].flags &= ~PF_OUT;
    }
    ApplyRules();
    if (g == PG_DEFENCE) {
        int players = 0;
        for (int s = 0; s < MAX_PLAYERS; s++) players += Joined(s);
        StormCity(players);
    }
    if (g == PG_HIDE) PickSeeker();
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (Joined(s)) Place(s, StartSpot(s, g), 1);
    Announce("Game %d of 4: %s. %s", k + 1, NAMES[g], RULES[g]);
}

// The drop vote, then the other four in a random order.
static void StartParty(void)
{
    int most = -1, best = 0, ties[PG_COUNT], nt = 0;
    for (int g = 0; g < PG_COUNT; g++) {
        int v = PartyVotes(g);
        if (v > best) {
            best = v;
            nt = 0;
        }
        if (v == best) ties[nt++] = g;
    }
    most = ties[GetRandomValue(0, nt - 1)];
    pty.vetoed = most;
    int left[PG_COUNT], n = 0;
    for (int g = 0; g < PG_COUNT; g++)
        if (g != most) left[n++] = g;
    for (int k = n - 1; k > 0; k--) {
        int j = GetRandomValue(0, k), x = left[k];
        left[k] = left[j];
        left[j] = x;
    }
    for (int k = 0; k < PARTY_GAMES; k++) pty.order[k] = left[k];
    memset(pty.total, 0, sizeof(pty.total));
    Announce("The party begins! %s is left out by vote.", NAMES[most]);
    StartGame(0);
}

// The slots of those who played, best first by 'score'.
static int Ranked(const int *score, unsigned who, int *out)
{
    int n = 0;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if ((who >> s & 1u) && G.peers[s].active) out[n++] = s;
    for (int a = 1; a < n; a++)
        for (int b = a; b > 0 && score[out[b]] > score[out[b - 1]]; b--) {
            int x = out[b];
            out[b] = out[b - 1];
            out[b - 1] = x;
        }
    return n;
}

static void EndGame(void)
{
    static const int bonus[3] = { 10, 6, 3 };
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (PartyHider(s)) pty.round[s] += HIDE_LAST;
    int order[MAX_PLAYERS], n = Ranked(pty.round, pty.joined, order);
    for (int k = 0; k < n && k < 3; k++) pty.round[order[k]] += bonus[k];
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (Joined(s)) pty.total[s] += pty.round[s];
    if (n > 0) {
        if (n >= 3) Announce("%s is over: 1st %s, 2nd %s, 3rd %s.", NAMES[pty.order[pty.game]], G.peers[order[0]].name,
                             G.peers[order[1]].name, G.peers[order[2]].name);
        else Announce("%s is over: 1st %s.", NAMES[pty.order[pty.game]], G.peers[order[0]].name);
    }
    if (pty.game + 1 >= PARTY_GAMES) {
        // The party's end: its top three gain experience, ten (five, two) for every player in it.
        unsigned all = 0;
        for (int s = 0; s < MAX_PLAYERS; s++)
            if (G.peers[s].active) all |= 1u << s;
        int top[MAX_PLAYERS], m = Ranked(pty.total, all, top);
        static const int xp[3] = { 10, 5, 2 };
        static const char *const place[3] = { "1st", "2nd", "3rd" };
        for (int k = 0; k < m && k < 3; k++) {
            SayTo(top[k], "Congratulations, you finished %s in the party with %d points! +%d XP", place[k], pty.total[top[k]], xp[k] * m);
            AwardXp(top[k], xp[k] * m, 0, 0);
        }
        if (m >= 3) Announce("The party is over! 1st %s (%d), 2nd %s (%d), 3rd %s (%d).", G.peers[top[0]].name, pty.total[top[0]],
                             G.peers[top[1]].name, pty.total[top[1]], G.peers[top[2]].name, pty.total[top[2]]);
        else if (m >= 1) Announce("The party is over! 1st %s (%d).", G.peers[top[0]].name, pty.total[top[0]]);
        pty.phase = PP_END;
        pty.t = END_TIME;
    } else {
        pty.phase = PP_BREAK;
        pty.t = BREAK_TIME;
    }
    pty.joined = 0;
    pty.seeker = -1;
    pty.found = 0;
    ApplyRules();
    // Everyone back in the city for the break.
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (G.peers[s].active) Place(s, CitySpawn(s), 0);
}

// Brought down in a game (the host hears of it): -2 for the one who fell in the games of fighting, +6
// for the player who did it.
void PartyDeath(int slot, int killer)
{
    int g = PartyGame();
    if (g < 0 || !Joined(slot)) return;
    if (g == PG_LAVA || g == PG_ARCHERS) {
        pty.round[slot] -= 2;
        if (killer >= 0 && killer != slot && Joined(killer)) pty.round[killer] += 6;
    }
    if (g == PG_HIDE && PartyHider(slot)) {
        pty.found |= 1u << slot;
        if (killer == pty.seeker) {
            pty.round[killer] += FIND_POINTS;
            Announce("%s found %s!", G.peers[killer].name, G.peers[slot].name);
        } else {
            Announce("%s is out of hiding.", G.peers[slot].name);
        }
    }
}

void PartyScore(int slot, int points)
{
    if (PartyGame() >= 0 && slot >= 0 && slot < MAX_PLAYERS && Joined(slot)) pty.round[slot] += points;
}

#ifdef SUMER_TEST
void PartySeekerForTest(int s)
{
    pty.seeker = lastSeeker = s;
}
#endif

// Test tools only: jump the clock of the phase under way.
void PartySkipTo(float t)
{
    if (pty.phase == PP_GAME) pty.t = t;
    else pty.t = fminf(pty.t, t);
}

static void HostUpdate(float dt)
{
    int players, readyN = ReadyCount(&players);
    switch (pty.phase) {
    case PP_WAIT:
    case PP_END:
        UpdateIdle(dt);
        if (pty.phase == PP_END && (pty.t -= dt) > 0) break;
        if (pty.phase == PP_END) {
            pty.phase = PP_WAIT;
            memset(pty.total, 0, sizeof(pty.total));
            memset(pty.round, 0, sizeof(pty.round));
        }
        if (players >= 2 && readyN == players) {
            pty.phase = PP_COUNT;
            pty.t = PARTY_START;
        }
        break;
    case PP_COUNT:
        UpdateIdle(dt);
        if (players < 2 || readyN < players) pty.phase = PP_WAIT;
        else if ((pty.t -= dt) <= 0) StartParty();
        break;
    case PP_GAME:
        pty.t += dt;
        if (players < 2) {
            Announce("Too few players are left: the party stops.");
            pty.phase = PP_WAIT;
            pty.joined = 0;
            ApplyRules();
        } else if (pty.t >= GAME_TIME) {
            EndGame();
        } else {
            BurnLava(dt);
            if (PartyGame() == PG_HIDE) HideUpdate(dt);
            // Date stashing: a point for every five dates carried, thirty at most, as they gather.
            if (PartyGame() == PG_DATES)
                for (int s = 0; s < MAX_PLAYERS; s++)
                    if (Joined(s) && PlayerIn(s)) pty.round[s] = G.peers[s].dates / 5 < DATES_MOST ? G.peers[s].dates / 5 : DATES_MOST;
        }
        break;
    case PP_BREAK:
        if (players < 2) {
            pty.phase = PP_WAIT;
            break;
        }
        if ((pty.t -= dt) <= 0) StartGame(pty.game + 1);
        break;
    default: break;
    }
}

// ---- Every machine ----

// This machine's player is set down at 'at': for a game (kind 1) or the break (0).
void PartyBegin(Vector3 at, int kind)
{
    Player *p = &G.player;
    waiting = false;
    caught = false;
    downT = 0;
    ready = false;
    p->pos = at;
    p->pos.y = GroundHeight(at.x, at.z, at.y + 0.5f);
    p->vy = 0;
    p->crouching = p->aiming = false;
    G.health = HEALTH_MAX;
    G.hurtBy = BY_NONE;
    G.climb = 0;
    G.sleeping = false;
    if (G.panel != PANEL_NONE && G.panel != PANEL_LOOK && G.panel != PANEL_CHAT) SetPanel(PANEL_NONE);
    if (kind == 1) {
        int g = pty.phase == PP_GAME ? pty.order[pty.game] : -1;
        if (g == PG_DATES) BagRemove(IT_DATES, BagCount(IT_DATES));   // everyone starts with none
        Say("%s: %s", PartyGameName(g), PartyGameRule(g));
        G.msgT = 8.0f;
    }
}

// Brought down: in a game, back in a moment somewhere fitting; between games, back in the city. Every
// one keeps what they carry. Returns false outside a party.
bool PartyFall(const char *how)
{
    if (!Party()) return false;
    G.health = HEALTH_MAX;
    G.player.aiming = false;
    G.player.draw = 0;
    G.climb = 0;
    if (G.panel != PANEL_NONE) SetPanel(PANEL_NONE);
    if (PartyHider(G.self)) {
        caught = true;
        downT = DOWN_TIME;
        Say("%s %s", how, L("You were found! Hold out in the farthest village until the game ends."));
    } else if (pty.phase == PP_GAME && !waiting) {
        downT = DOWN_TIME;
        Say("%s %s", how, L("Back in the game in a moment."));
    } else {
        G.player.pos = CitySpawn(G.self);
        G.player.pos.y = GroundHeight(G.player.pos.x, G.player.pos.z, G.player.pos.y + 0.5f);
        Say("%s", how);
    }
    G.msgT = 5.0f;
    return true;
}

// A guest who joins while a game is on watches until the next one.
void PartyJoinLate(bool running)
{
    if (!Party()) return;
    waiting = running;
}

bool PartyWaiting(void)
{
    return Party() && waiting && pty.phase == PP_GAME;
}

void PartyUpdate(float dt)
{
    if (!Party()) return;
    ApplyRules();
    if (!Dedicated()) {
        G.food = FOOD_MAX;
        if (downT > 0 && (downT -= dt) <= 0) {
            downT = 0;
            Vector3 at = StartSpot(G.self, PartyGame());
            G.player.pos = (Vector3){ at.x, GroundHeight(at.x, at.z, at.y + 0.5f), at.z };
            G.player.vy = 0;
            G.health = HEALTH_MAX;
            if (PartyGame() == PG_HIDE && Found(G.self) && G.self != pty.seeker) G.equip = EQ_BOW;   // the wild beasts await
        }
        // One-shot archers: the bow and nothing else.
        if (PartyGame() == PG_ARCHERS && G.equip != EQ_BOW) G.equip = EQ_BOW;
        if (PartyHider(G.self) && G.equip != EQ_NONE) G.equip = EQ_NONE;   // a hider carries nothing
        // Once the party begins, the next one wants everyone ready again.
        if (pty.phase == PP_GAME && ready) ready = false;
    }
    if (G.role == NET_CLIENT) {
        if (pty.phase == PP_COUNT || pty.phase == PP_BREAK || pty.phase == PP_END) pty.t = fmaxf(0.0f, pty.t - dt);
        else if (pty.phase == PP_GAME) pty.t += dt;
        return;
    }
    HostUpdate(dt);
}

// ---- What everyone is told ----

void WriteParty(Writer *w)
{
    W8(w, (unsigned)pty.phase);
    WF(w, pty.t);
    W8(w, (unsigned)pty.game);
    for (int k = 0; k < PARTY_GAMES; k++) W8(w, (unsigned)pty.order[k]);
    W8(w, (unsigned)pty.vetoed);
    W32(w, pty.joined);
    W8(w, pty.seeker >= 0 ? (unsigned)pty.seeker : 255u);
    W8(w, (unsigned)pty.npcKills);
    W32(w, pty.found);
    for (int s = 0; s < MAX_PLAYERS; s++) {
        W16(w, (unsigned)(pty.round[s] & 0xffff));
        W16(w, (unsigned)(pty.total[s] & 0xffff));
    }
}

void ReadParty(Reader *r)
{
    int phase = (int)R8(r);
    float t = RF(r);
    int game = (int)R8(r), order[PARTY_GAMES];
    for (int k = 0; k < PARTY_GAMES; k++) order[k] = (int)R8(r);
    int vetoed = (int)R8(r);
    unsigned joined = R32(r);
    int seeker = (int)R8(r), npc = (int)R8(r);
    unsigned found = R32(r);
    int round[MAX_PLAYERS], total[MAX_PLAYERS];
    for (int s = 0; s < MAX_PLAYERS; s++) {
        round[s] = (short)R16(r);
        total[s] = (short)R16(r);
    }
    if (r->bad || game >= PARTY_GAMES) return;
    if (phase != pty.phase || fabsf(pty.t - t) > 1.0f) pty.t = t;
    pty.phase = phase;
    pty.game = game;
    for (int k = 0; k < PARTY_GAMES; k++) pty.order[k] = order[k] % PG_COUNT;
    pty.vetoed = vetoed;
    pty.joined = joined;
    pty.seeker = seeker < MAX_PLAYERS ? seeker : -1;
    pty.npcKills = npc;
    pty.found = found;
    memcpy(pty.round, round, sizeof(round));
    memcpy(pty.total, total, sizeof(total));
}

// ---- Drawing ----

// Floor is lava: a glowing red sheet half a metre over the ground around the camera.
void DrawLava(void)
{
    if (PartyGame() != PG_LAVA) return;
    const float cell = 2.0f;
    const int n = 50;
    Vector3 cam = G.cam.position;
    float x0 = floorf(cam.x / cell) * cell - n / 2 * cell, z0 = floorf(cam.z / cell) * cell - n / 2 * cell;
    unsigned char a = (unsigned char)(110 + 30 * sinf((float)GetTime() * 3.0f));
    rlDisableBackfaceCulling();
    for (int i = 0; i < n; i++) {
        rlCheckRenderBatchLimit(n * 4);
        rlBegin(RL_QUADS);
        rlColor4ub(230, 50, 20, a);
        float xa = x0 + i * cell, xb = xa + cell;
        for (int k = 0; k < n; k++) {
            float za = z0 + k * cell, zb = za + cell;
            rlVertex3f(xa, TerrainHeight(xa, za) + LAVA_DEPTH, za);
            rlVertex3f(xa, TerrainHeight(xa, zb) + LAVA_DEPTH, zb);
            rlVertex3f(xb, TerrainHeight(xb, zb) + LAVA_DEPTH, zb);
            rlVertex3f(xb, TerrainHeight(xb, za) + LAVA_DEPTH, za);
        }
        rlEnd();
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// The line at the top: what is happening and how long is left; and the scores at the right, always.
void DrawPartyHUD(float sw, float sh)
{
    if (!Party()) return;
    const Color gold = { 240, 200, 120, 255 }, text = { 240, 230, 210, 255 }, dim = { 170, 160, 140, 255 };
    int players, readyN = ReadyCount(&players);
    const char *line;
    switch (pty.phase) {
    case PP_COUNT:
        line = TextFormat(L("Sumer Party: everyone is ready. The party starts in %d s."), (int)ceilf(pty.t));
        break;
    case PP_GAME: {
        int left = (int)fmaxf(0.0f, GAME_TIME - pty.t);
        line = TextFormat(L("Game %d of 4: %s   %d:%02d left"), pty.game + 1, PartyGameName(pty.order[pty.game]), left / 60, left % 60);
        break;
    }
    case PP_BREAK:
        line = TextFormat(L("Next, game %d of 4: %s, in %d s"), pty.game + 2, PartyGameName(pty.order[(pty.game + 1) % PARTY_GAMES]), (int)ceilf(pty.t));
        break;
    case PP_END:
        line = TextFormat(L("The party is over. %d of %d ready for the next one. %s"), readyN, players,
                          L(ready ? "Waiting for the others (Enter: not ready)." : "Press Enter when you are ready."));
        break;
    default:
        line = players < 2 ? TextFormat(L("Sumer Party: a party needs two or more players (%d here). Esc: vote."), players)
                           : TextFormat(L("Sumer Party: %d of %d ready. %s Esc: vote."), readyN, players,
                                        L(ready ? "Waiting for the others (Enter: not ready)." : "Press Enter when you are ready."));
        break;
    }
    // Both boards are kept small, so the game shows round them.
    Vector2 m = (Vector2){ GameTextWidth(line, 16), 16 };
    DrawRectangle((int)(sw / 2 - m.x / 2 - 8), 8, (int)m.x + 16, 24, Fade(BLACK, 0.55f));
    DrawGameText(line, sw / 2 - m.x / 2, 12, 16, pty.phase == PP_GAME ? text : gold);
    if (pty.phase == PP_GAME) {
        const char *rule = PartyGameRule(pty.order[pty.game]);
        Vector2 rm = (Vector2){ GameTextWidth(rule, 14), 14 };
        DrawRectangle((int)(sw / 2 - rm.x / 2 - 6), 34, (int)rm.x + 12, 20, Fade(BLACK, 0.45f));
        DrawGameText(rule, sw / 2 - rm.x / 2, 37, 14, dim);
        // Hide and seek: this player's part in it.
        const char *part = NULL;
        if (PartyGame() == PG_HIDE && Joined(G.self) && !waiting) {
            if (G.self == pty.seeker) part = TextFormat(L("You seek: one blow finds a hider. Townsfolk brought down: %d of %d."), pty.npcKills, SEEKER_SLIPS);
            else if (Found(G.self)) part = L("You were found: hold out in the village until the game ends.");
            else part = TextFormat(L("You hide among the townsfolk: no running, and stay in the city. %s seeks."),
                                   pty.seeker >= 0 ? G.peers[pty.seeker].name : "?");
        }
        if (part) {
            Vector2 pm = (Vector2){ GameTextWidth(part, 15), 15 };
            DrawRectangle((int)(sw / 2 - pm.x / 2 - 6), 56, (int)pm.x + 12, 21, Fade(BLACK, 0.5f));
            DrawGameText(part, sw / 2 - pm.x / 2, 59, 15, gold);
        }
    }

    // The scores, this game's and the party's: the best three by the party's, and this player if not among them.
    unsigned all = 0;
    for (int s = 0; s < MAX_PLAYERS; s++)
        if (G.peers[s].active) all |= 1u << s;
    int order[MAX_PLAYERS], n = Ranked(pty.total, all, order), rows[4], nr = 0;
    for (int k = 0; k < n && k < 3; k++) rows[nr++] = k;
    for (int k = 3; k < n; k++)
        if (order[k] == G.self) rows[nr++] = k;
    float x = sw - 212, y = 232, w = 200;   // under the map and its frame counter
    DrawRectangle((int)x, (int)y, (int)w, 21 + 14 * nr, Fade(BLACK, 0.5f));
    DrawGameText(L("Party"), x + 6, y + 4, 12, gold);
    DrawGameText(L("game"), x + 120, y + 4, 10, dim);
    DrawGameText(L("total"), x + 160, y + 4, 10, dim);
    for (int r = 0; r < nr; r++) {
        int k = rows[r], s = order[k];
        float ry = y + 19 + 14 * (float)r;
        Color c = s == G.self ? gold : text;
        DrawGameText(TextFormat("%d. %s%s", k + 1, G.peers[s].name, (G.peers[s].flags & PF_READY) && LobbyWaiting() ? " *" : ""), x + 6, ry, 11, c);
        DrawGameText(TextFormat("%d", pty.round[s]), x + 120, ry, 11, c);
        DrawGameText(TextFormat("%d", pty.total[s]), x + 160, ry, 11, c);
    }
    if (PartyWaiting()) {
        const char *w2 = L("You joined during a game: you play from the next one.");
        Vector2 wm = (Vector2){ GameTextWidth(w2, 18), 18 };
        DrawRectangle((int)(sw / 2 - wm.x / 2 - 10), (int)sh - 120, (int)wm.x + 20, 28, Fade(BLACK, 0.55f));
        DrawGameText(w2, sw / 2 - wm.x / 2, sh - 115, 18, gold);
    } else if (downT <= 0 && PartyHider(G.self) && !InsideWalls(G.player.pos.x, G.player.pos.z, 0.0f)) {
        DrawRectangle(0, 0, (int)sw + 1, (int)sh + 1, Fade((Color){ 120, 90, 60, 255 }, 0.3f));
        const char *w2 = L("Back into the city! Out here a hider wastes away.");
        Vector2 wm = (Vector2){ GameTextWidth(w2, 22), 22 };
        DrawRectangle((int)(sw / 2 - wm.x / 2 - 10), (int)(sh / 2 + 56), (int)wm.x + 20, 32, Fade(BLACK, 0.5f));
        DrawGameText(w2, sw / 2 - wm.x / 2, sh / 2 + 60, 22, (Color){ 255, 200, 160, 255 });
    } else if (downT <= 0 && InLava(G.player.pos)) {
        DrawRectangle(0, 0, (int)sw + 1, (int)sh + 1, Fade((Color){ 200, 40, 10, 255 }, 0.3f));
        const char *w2 = L("The floor is lava! Get up onto something.");
        Vector2 wm = (Vector2){ GameTextWidth(w2, 22), 22 };
        DrawRectangle((int)(sw / 2 - wm.x / 2 - 10), (int)(sh / 2 + 56), (int)wm.x + 20, 32, Fade(BLACK, 0.5f));
        DrawGameText(w2, sw / 2 - wm.x / 2, sh / 2 + 60, 22, (Color){ 255, 200, 160, 255 });
    } else if (downT > 0) {
        const char *w2 = TextFormat(L("Back in the game in %d s."), (int)ceilf(downT));
        Vector2 wm = (Vector2){ GameTextWidth(w2, 22), 22 };
        DrawRectangle((int)(sw / 2 - wm.x / 2 - 10), (int)(sh / 2 + 56), (int)wm.x + 20, 32, Fade(BLACK, 0.5f));
        DrawGameText(w2, sw / 2 - wm.x / 2, sh / 2 + 60, 22, gold);
    }
}
