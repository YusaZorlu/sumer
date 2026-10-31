// Board games at the alehouse's tables: the Royal Game of Ur and fifty-eight holes ("hounds and jackals").
// Two sit at a table, one on each side: two players, or a player and a townsman the alehouse finds for
// them. The host keeps every board (who sits where, the pieces, whose turn, the throw) and throws the dice;
// the players ask it to sit, throw and move (RQ_BOARD), and everyone sees the boards in the snapshots. The
// winner gains experience. Not in a battle royale or a party: no one has time to sit down there.
//
// Both games are thrown with the same dice, cubes numbered 1 to 6, so no one has to learn old dice first.
//
// The Royal Game of Ur (found at Ur, about 2600-2400 BC), after Irving Finkel's reading of its rules from a
// later tablet: seven pieces each and one die. A piece goes 4 squares on its own side, 8 down the middle row
// (where it can take an opponent's piece by landing on it, sending it back), then 2 on its own side, and
// off with a throw that reaches past the end. A rosette (the 4th, 8th and 14th square of the way) gives
// another throw, and the one in the middle row keeps a piece safe there.
//
// Fifty-eight holes (Egypt and Mesopotamia, from about 2000 BC): its rules are lost; this is a modern
// reading. Five pins each, 29 holes on each side's way and a shared hole at the top that ends it. Two dice
// are thrown and added; the pins go up by the sum. Lines on the board join hole 6 with 20 and 8 with 10: a
// pin that lands on one end goes to the other, up or down. No two pins of a side share a hole; a throw past
// the last hole brings a pin home; the first to bring all five home wins.
#include "game.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <string.h>

#define NPC_THINK   1.6f    // the townsman's pause before he throws or moves (time to watch a piece go)
#define PASS_SHOWN  2.2f    // a throw that cannot be used stays on the board this long
#define IDLE_TURN   45.0f   // a player who does nothing this long has the turn played for them
#define WON_SHOWN   8.0f    // the finished board stays this long, then the pieces go back
#define XP_TOWNSMAN 10
#define XP_RIVAL    20
#define FORFEIT_MIN 20      // moves played before a walk-out counts as a win for the other

static Board boards[BOARDS];

static const int KINDS[BOARDS] = { BG_UR, BG_HOLES, BG_UR };

bool BoardsOn(void)
{
    return !Match();
}

Board *BoardAt(int b)
{
    return b >= 0 && b < BOARDS ? &boards[b] : NULL;
}

int BoardPieces(int kind)
{
    return kind == BG_UR ? UR_PIECES : HOLES_PIECES;
}

int BoardHome(int kind)
{
    return kind == BG_UR ? UR_HOME : HOLES_HOME;
}

static void Reset(Board *b, int first)
{
    memset(b->pos, 0, sizeof(b->pos));
    b->turn = (unsigned char)first;
    b->roll = -1;
    memset(b->dice, 0, sizeof(b->dice));
    b->winner = -1;
    b->last = -1;
    b->lastSide = 0;
    b->moves = 0;
    b->wait = 0;
    b->idle = 0;
}

void BoardsInit(void)
{
    for (int i = 0; i < BOARDS; i++) {
        boards[i].kind = (unsigned char)KINDS[i];
        boards[i].seat[0] = boards[i].seat[1] = SEAT_NONE;
        Reset(&boards[i], 0);
    }
}

// ---- The rules ----

bool UrRosette(int p)
{
    return p == 4 || p == 8 || p == 14;
}

static bool Shared(int p)
{
    return p >= 5 && p <= 12;
}

// Where a pin that lands on hole p ends up: the lines join 6 with 20 and 8 with 10.
int HolesLink(int p)
{
    return p == 6 ? 20 : p == 20 ? 6 : p == 8 ? 10 : p == 10 ? 8 : p;
}

// Where piece i of 'side' would go with the throw on the board, or -1 if it cannot move.
int BoardTarget(const Board *b, int side, int i)
{
    int n = BoardPieces(b->kind), home = BoardHome(b->kind), from = b->pos[side][i];
    if (b->roll <= 0 || i < 0 || i >= n || from >= home) return -1;
    int to = from + b->roll;
    if (b->kind == BG_UR) {
        if (to >= home) return home;   // off with any throw that reaches past the end
        for (int k = 0; k < n; k++)
            if (k != i && b->pos[side][k] == to) return -1;
        if (to == 8)
            for (int k = 0; k < n; k++)
                if (b->pos[1 - side][k] == 8) return -1;   // safe on the middle rosette
        return to;
    }
    if (to >= home) return home;
    to = HolesLink(to);
    for (int k = 0; k < n; k++)
        if (k != i && b->pos[side][k] == to) return -1;
    return to;
}

// The pieces that can move with the throw. Of those still waiting only the first counts: they are alike.
int BoardMoves(const Board *b, int side, int *out)
{
    int n = 0;
    bool waiting = false;
    for (int i = 0; i < BoardPieces(b->kind); i++) {
        if (b->pos[side][i] == 0) {
            if (waiting) continue;
            waiting = true;
        }
        if (BoardTarget(b, side, i) >= 0) out[n++] = i;
    }
    return n;
}

static int Done(const Board *b, int side)
{
    int n = 0;
    for (int i = 0; i < BoardPieces(b->kind); i++) n += b->pos[side][i] == BoardHome(b->kind);
    return n;
}

// The throw: one die for Ur, two added for the holes, each numbered 1 to 6.
void BoardThrow(Board *b)
{
    int dice = b->kind == BG_UR ? 1 : 2, sum = 0;
    memset(b->dice, 0, sizeof(b->dice));
    for (int k = 0; k < dice; k++) {
        b->dice[k] = (unsigned char)GetRandomValue(1, 6);
        sum += b->dice[k];
    }
    b->roll = (signed char)sum;
}

// Moves piece i of the side whose turn it is; then the turn passes, or not (a rosette). True if it moved.
bool BoardMove(Board *b, int i)
{
    int side = b->turn, to = BoardTarget(b, side, i);
    if (to < 0) return false;
    bool again = false;
    b->pos[side][i] = (unsigned char)to;
    if (b->kind == BG_UR && to < UR_HOME) {
        if (Shared(to))
            for (int k = 0; k < UR_PIECES; k++)
                if (b->pos[1 - side][k] == to) b->pos[1 - side][k] = 0;   // taken: back to the start
        again = UrRosette(to);
    }
    b->last = (signed char)i;
    b->lastSide = (unsigned char)side;
    b->moves++;
    b->roll = -1;
    if (Done(b, side) == BoardPieces(b->kind)) {
        b->winner = (signed char)side;
        return true;
    }
    if (!again) b->turn = (unsigned char)(1 - side);
    return true;
}

// The townsman's choice (and a sleeping player's): in Ur he takes, then makes for a rosette, then gets a
// piece home or out of reach, and keeps off squares an opponent behind could reach; with the holes he goes
// for the line up (6) and keeps off the line down (20), else moves the pin that goes furthest.
int BoardBest(const Board *b)
{
    int side = b->turn, moves[8], n = BoardMoves(b, side, moves), best = -1;
    float bestScore = -1e9f;
    for (int m = 0; m < n; m++) {
        int i = moves[m], from = b->pos[side][i], to = BoardTarget(b, side, i);
        float s = (float)(to - from);
        if (b->kind == BG_UR) {
            if (to == UR_HOME) s += 6;
            if (UrRosette(to)) s += to == 8 ? 14 : 10;
            if (from == 8) s -= 6;   // leaving the safe square
            if (Shared(to))
                for (int k = 0; k < UR_PIECES; k++) {
                    int o = b->pos[1 - side][k];
                    if (o == to && to != 8) s += 12 + (float)to;   // taking a piece that has come far
                    if (to != 8 && o < to && to - o <= 6) s -= 4;   // within a throw behind
                }
            if (Shared(from) && from != 8)
                for (int k = 0; k < UR_PIECES; k++) {
                    int o = b->pos[1 - side][k];
                    if (o < from && from - o <= 6) s += 3;   // out of danger
                }
        } else {
            int land = from + b->roll;
            if (land == 6 || land == 8) s += 6;
            if (land == 20 || land == 10) s -= 10;
            if (to == HOLES_HOME) s += 4;
        }
        s += (float)GetRandomValue(0, 10) * 0.05f;   // a townsman is not a machine
        if (s > bestScore) {
            bestScore = s;
            best = i;
        }
    }
    return best;
}

// ---- Who sits where ----

static bool PlayerSeat(int seat)
{
    return seat >= 0 && seat < MAX_PLAYERS;
}

int BoardOf(int slot)
{
    for (int b = 0; b < BOARDS; b++)
        for (int s = 0; s < 2; s++)
            if (boards[b].seat[s] == slot) return b;
    return -1;
}

int BoardSide(int b, int slot)
{
    const Board *x = BoardAt(b);
    return !x ? -1 : x->seat[0] == slot ? 0 : x->seat[1] == slot ? 1 : -1;
}

static void Tell(const Board *b, const char *fmt, const char *name)
{
    for (int s = 0; s < 2; s++)
        if (PlayerSeat(b->seat[s])) SayTo(b->seat[s], fmt, name);
}

// The end of a game: the winner, if a player, gains experience (more for beating another player).
static void Finish(Board *b, int side)
{
    b->winner = (signed char)side;
    b->wait = WON_SHOWN;
    int loser = 1 - side;
    if (PlayerSeat(b->seat[side])) {
        int xp = PlayerSeat(b->seat[loser]) ? XP_RIVAL : XP_TOWNSMAN;
        SayTo(b->seat[side], "You won the game! +%d XP", xp);
        AwardXp(b->seat[side], xp, 0, 0);
    }
    if (PlayerSeat(b->seat[loser])) {
        if (PlayerSeat(b->seat[side])) SayTo(b->seat[loser], "%s won the game.", G.peers[b->seat[side]].name);
        else SayTo(b->seat[loser], "The townsman won the game.");
    }
}

// Someone gets up: a game under way is lost by them (and won by the other only if it went on a while).
static void Stand(Board *b, int side)
{
    int other = 1 - side;
    bool playing = b->winner < 0 && b->moves > 0 && b->seat[other] != SEAT_NONE;
    if (playing && PlayerSeat(b->seat[other])) {
        if (b->moves >= FORFEIT_MIN) {
            Finish(b, other);
        } else {
            SayTo(b->seat[other], "%s got up from the game.", G.peers[b->seat[side]].name);
        }
    }
    b->seat[side] = SEAT_NONE;
    if (!PlayerSeat(b->seat[other])) b->seat[other] = SEAT_NONE;   // the townsman goes back to his ale
    Reset(b, 0);
}

// ---- The host ----

static void PlayTurn(Board *b)
{
    if (b->roll < 0) {
        BoardThrow(b);
        int moves[8];
        if (BoardMoves(b, b->turn, moves) == 0) b->wait = PASS_SHOWN;   // nothing to do with it
        else b->wait = NPC_THINK;
        b->idle = 0;
        return;
    }
    int i = BoardBest(b);
    if (i >= 0) BoardMove(b, i);
    b->idle = 0;
    b->wait = NPC_THINK;
    if (b->winner >= 0) Finish(b, b->winner);
}

static void UpdateBoard(Board *b, float dt)
{
    // Up and away from the table: the seat is free again.
    for (int s = 0; s < 2; s++) {
        int p = b->seat[s];
        if (PlayerSeat(p) && (!PlayerIn(p) || XZDist(G.peers[p].pos, BoardSeat((int)(b - boards), s, NULL)) > 2.2f ||
                              fabsf(G.peers[p].pos.y - BoardSeat((int)(b - boards), s, NULL).y) > 1.5f))
            Stand(b, s);
    }
    if (b->seat[0] == SEAT_NONE || b->seat[1] == SEAT_NONE) return;
    if (b->winner >= 0) {
        if ((b->wait -= dt) <= 0) Reset(b, 1 - b->winner);   // the loser starts the next
        return;
    }
    int side = b->turn, moves[8];
    // A throw that cannot be used is shown a moment, then the turn passes.
    if (b->roll >= 0 && BoardMoves(b, side, moves) == 0) {
        if ((b->wait -= dt) <= 0) {
            b->roll = -1;
            b->turn = (unsigned char)(1 - side);
            b->wait = NPC_THINK;
        }
        return;
    }
    if (b->seat[side] == SEAT_NPC) {
        if ((b->wait -= dt) <= 0) PlayTurn(b);
    } else if ((b->idle += dt) > IDLE_TURN) {
        PlayTurn(b);
    }
}

void BoardsUpdate(float dt)
{
    if (!BoardsOn() || G.role == NET_CLIENT) return;
    for (int b = 0; b < BOARDS; b++) UpdateBoard(&boards[b], dt);
}

// A player's wish at board b: sit, get up, have a townsman sit across, throw, move a piece.
bool BoardRequest(int slot, int bi, int act, int arg, int *sat)
{
    Board *b = BoardAt(bi);
    if (!b || !BoardsOn() || !PlayerIn(slot)) return false;
    int side = BoardSide(bi, slot);
    *sat = side;
    switch (act) {
    case BA_SIT: {
        if (side >= 0) return true;
        if (Match()) return false;
        if (BoardOf(slot) >= 0) return false;
        int free = b->seat[0] == SEAT_NONE ? 0 : b->seat[1] == SEAT_NONE ? 1 : -1;
        if (free < 0 && b->seat[0] == SEAT_NPC && b->moves == 0) free = 0;   // takes the townsman's place
        if (free < 0 && b->seat[1] == SEAT_NPC && b->moves == 0) free = 1;
        if (free < 0 || XZDist(PlayerAt(slot), BoardSeat(bi, free, NULL)) > 4.0f) return false;
        b->seat[free] = (signed char)slot;
        *sat = free;
        Reset(b, 0);
        if (PlayerSeat(b->seat[1 - free])) Tell(b, "%s sits down to play.", G.peers[slot].name);
        return true;
    }
    case BA_LEAVE:
        if (side < 0) return false;
        Stand(b, side);
        return true;
    case BA_NPC:
        if (side < 0 || b->seat[1 - side] != SEAT_NONE) return false;
        b->seat[1 - side] = SEAT_NPC;
        Reset(b, side);   // the player throws first
        return true;
    case BA_THROW:
        if (side < 0 || b->seat[1 - side] == SEAT_NONE || b->winner >= 0 || b->turn != side || b->roll >= 0) return false;
        BoardThrow(b);
        b->idle = 0;
        {
            int moves[8];
            if (BoardMoves(b, side, moves) == 0) b->wait = PASS_SHOWN;
        }
        return true;
    case BA_MOVE:
        if (side < 0 || b->winner >= 0 || b->turn != side || b->roll < 0 || !BoardMove(b, arg)) return false;
        b->idle = 0;
        b->wait = NPC_THINK;
        if (b->winner >= 0) Finish(b, b->winner);
        return true;
    default:
        return false;
    }
}

// ---- What everyone is told ----

void WriteBoards(Writer *w)
{
    for (int i = 0; i < BOARDS; i++) {
        const Board *b = &boards[i];
        W8(w, (unsigned char)b->seat[0]);
        W8(w, (unsigned char)b->seat[1]);
        for (int s = 0; s < 2; s++)
            for (int k = 0; k < UR_PIECES; k++) W8(w, b->pos[s][k]);
        W8(w, b->turn);
        W8(w, (unsigned char)b->roll);
        for (int k = 0; k < 4; k++) W8(w, b->dice[k]);
        W8(w, (unsigned char)b->winner);
        W8(w, (unsigned char)b->last);
        W8(w, b->lastSide);
        W16(w, b->moves);
        WF(w, b->idle);
    }
}

void ReadBoards(Reader *r)
{
    Board in[BOARDS];
    for (int i = 0; i < BOARDS; i++) {
        Board *b = &in[i];
        b->kind = (unsigned char)KINDS[i];
        b->seat[0] = (signed char)R8(r);
        b->seat[1] = (signed char)R8(r);
        for (int s = 0; s < 2; s++)
            for (int k = 0; k < UR_PIECES; k++) b->pos[s][k] = (unsigned char)R8(r);
        b->turn = (unsigned char)(R8(r) & 1);
        b->roll = (signed char)R8(r);
        for (int k = 0; k < 4; k++) b->dice[k] = (unsigned char)R8(r);
        b->winner = (signed char)R8(r);
        b->last = (signed char)R8(r);
        b->lastSide = (unsigned char)(R8(r) & 1);
        b->moves = (unsigned short)R16(r);
        b->idle = RF(r);
        b->wait = 0;
    }
    if (!r->bad) memcpy(boards, in, sizeof(boards));
}

// ---- On every screen ----

// Where position p of 'side' lies on the Ur board seen with 'view' nearest: column 0-7 along it, row 0
// (the far side), 1 (the middle) or 2 (the near side). A piece enters at column 3, goes to the rosette at
// column 0, down the whole middle row, then back along its own side at columns 7 and 6, and off.
bool UrSquare(int view, int side, int p, int *col, int *row)
{
    if (p < 1 || p > 14) return false;
    int own = side == view ? 2 : 0;
    if (p <= 4) {
        *col = 4 - p;
        *row = own;
    } else if (p <= 12) {
        *col = p - 5;
        *row = 1;
    } else {
        *col = p == 13 ? 7 : 6;
        *row = own;
    }
    return true;
}

bool UrSquareExists(int col, int row)
{
    return row == 1 || col <= 3 || col >= 6;
}

bool UrRosetteSquare(int col, int row)
{
    return (row != 1 && (col == 0 || col == 6)) || (row == 1 && col == 3);
}

// Where hole p (1-29, or 30 the shared hole at the top) of 'side' lies on the fifty-eight holes board,
// in fractions of it (u across, v up), with 'view' on the left: each way goes down a column near the
// middle (1-14), then up the outer edge (15-29) to the top.
Vector2 HolesHole(int view, int side, int p)
{
    float s = side == view ? -1.0f : 1.0f;
    if (p >= 30) return (Vector2){ 0.5f, 0.94f };
    if (p <= 14) return (Vector2){ 0.5f + s * 0.11f, 0.80f - (float)(p - 1) * 0.052f };
    return (Vector2){ 0.5f + s * 0.36f, 0.10f + (float)(p - 15) * 0.054f };
}

static int viewing = -1;

int BoardViewing(void)
{
    return viewing;
}

void BoardAct(int b, int act, int arg)
{
    int args[3] = { b, act, arg };
    Act(RQ_BOARD, args, 3);
}

void BoardWatch(int b)
{
    viewing = b;
    SetPanel(PANEL_BOARD);
}

// The answer to this player's wish. Sat down: to the stool, facing the board, and the board open.
void BoardReply(int b, int act, bool ok, int side)
{
    if (act != BA_SIT) return;
    if (!ok || side < 0) {
        Say("Both places at this table are taken: you can watch (E).");
        return;
    }
    float face;
    G.player.pos = BoardSeat(b, side, &face);
    G.player.facing = G.player.yaw = face;
    G.player.vy = 0;
    viewing = b;
    SetPanel(PANEL_BOARD);
    Say(BoardAt(b)->kind == BG_UR ? "The Royal Game of Ur: throw the die and race your seven pieces round the board and off. Rules (R) tells the rest."
                                  : "Fifty-eight holes: throw two dice and race your five pins up to the shared hole. Rules (R) tells the rest.");
}

// Seated at a board: sitting on the stool.
void BoardsLocal(void)
{
    static bool sat;   // the crouch is the stool's: it goes when the player gets up or walks off
    int b = Dedicated() ? -1 : BoardOf(G.self);
    bool on = b >= 0 && XZDist(G.player.pos, BoardSeat(b, BoardSide(b, G.self), NULL)) < 0.6f;
    if (on) G.player.crouching = true;
    else if (sat) G.player.crouching = false;
    sat = on;
}

// ---- What the screen shows as pieces move ----
// The boards change at once (on the host, and in each snapshot); on the screen a piece walks there square by
// square, a taken piece goes back to the start after the one that took it has landed, and the dice tumble a
// moment before they show the throw. What the last move did is kept to be told in words.

#define HOP_TIME    0.16f   // a step from one square (or hole) to the next
#define BACK_TIME   0.5f    // a taken piece's way back, or a pin's ride along a line
#define TUMBLE_TIME 0.7f

typedef struct {
    unsigned char rest;         // where it stands on the screen when not moving
    unsigned char path[40];     // the squares it goes through
    int n;                      // squares in the path (0: not moving)
    float t, delay;             // steps gone; the wait before it sets off
    bool jumpLast;              // the last step is a long one (back to the start, along a line)
} Hop;

static Hop hops[BOARDS][2][UR_PIECES];
static BoardEvent events[BOARDS];
static float tumble[BOARDS];
static signed char seenRoll[BOARDS], lastRoll[BOARDS];
static bool seeded[BOARDS];

// Where a pin that ended on 'to' from 'from' first landed: a line carried it from there.
static int HolesLanding(int from, int to)
{
    if (to == 20 && from < 6) return 6;
    if (to == 6) return 20;
    if (to == 10 && from < 8) return 8;
    if (to == 8) return 10;
    return to;
}

static float HopLength(const Hop *h, int step)
{
    return h->jumpLast && step == h->n - 2 ? BACK_TIME : HOP_TIME;
}

static void Snap(int bi)
{
    const Board *b = &boards[bi];
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < UR_PIECES; i++) hops[bi][s][i] = (Hop){ .rest = b->pos[s][i] };
    memset(&events[bi], 0, sizeof(events[bi]));
    events[bi].side = -1;
    tumble[bi] = 0;
    seenRoll[bi] = b->roll;
    seeded[bi] = true;
}

void BoardsAnimate(float dt)
{
    if (!BoardsOn() || Dedicated()) return;
    for (int bi = 0; bi < BOARDS; bi++) {
        const Board *b = &boards[bi];
        int n = BoardPieces(b->kind), home = BoardHome(b->kind);
        if (!seeded[bi]) Snap(bi);   // the first look: everything where it stands
        if (b->roll >= 0 && seenRoll[bi] < 0) tumble[bi] = TUMBLE_TIME;
        if (b->roll >= 0) lastRoll[bi] = b->roll;
        seenRoll[bi] = b->roll;
        if (tumble[bi] > 0) tumble[bi] -= dt;
        events[bi].age += dt;
        if (b->moves == 0) {   // a new game: the pieces are set out, no walking
            for (int s = 0; s < 2; s++)
                for (int i = 0; i < UR_PIECES; i++) hops[bi][s][i] = (Hop){ .rest = b->pos[s][i] };
            events[bi].side = -1;
            continue;
        }

        // More than a move's worth changed at once (a board set out anew, a snapshot after a long gap): no
        // walking, everything where it stands.
        int changed = 0;
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < n; i++) {
                const Hop *h = &hops[bi][s][i];
                changed += b->pos[s][i] != (h->n ? h->path[h->n - 1] : h->rest);
            }
        if (changed > 2) {
            for (int s = 0; s < 2; s++)
                for (int i = 0; i < UR_PIECES; i++) hops[bi][s][i] = (Hop){ .rest = b->pos[s][i] };
            events[bi].side = -1;
            continue;
        }
        // A piece that went forward (or rode a line) sets off along its way; one that went back to 0 was taken.
        float moving = 0;
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < n; i++) {
                Hop *h = &hops[bi][s][i];
                int to = b->pos[s][i], from = h->n ? h->path[h->n - 1] : h->rest;
                if (to == from || to == 0) continue;
                int land = b->kind == BG_HOLES ? HolesLanding(from, to) : to;
                Hop nh = { .rest = (unsigned char)from };
                for (int p = from; nh.n < 39; p++) {
                    int q = p < home ? p : home;
                    nh.path[nh.n++] = (unsigned char)q;
                    if (q >= land || q >= home) break;
                }
                if (land != to && nh.n < 40) {
                    nh.path[nh.n++] = (unsigned char)to;
                    nh.jumpLast = true;
                }
                *h = nh;
                for (int k = 0; k < h->n - 1; k++) moving += HopLength(h, k);
                BoardEvent *e = &events[bi];
                *e = (BoardEvent){ .side = (signed char)s, .piece = (unsigned char)i, .from = (unsigned char)from, .to = (unsigned char)to,
                                   .steps = (unsigned char)(to < home || lastRoll[bi] < home - from ? land - from : lastRoll[bi]),
                                   .again = b->kind == BG_UR && b->turn == s && b->winner < 0 && to < home,
                                   .home = to >= home, .up = land != to && to > land, .down = land != to && to < land };
            }
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < n; i++) {
                Hop *h = &hops[bi][s][i];
                int from = h->n ? h->path[h->n - 1] : h->rest;
                if (b->pos[s][i] != 0 || from == 0) continue;
                *h = (Hop){ .rest = (unsigned char)from, .n = 2, .path = { (unsigned char)from, 0 }, .jumpLast = true, .delay = moving };
                if (events[bi].side == 1 - s) events[bi].took = true;
            }
        // On their way.
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < n; i++) {
                Hop *h = &hops[bi][s][i];
                if (h->n < 2) continue;
                if (h->delay > 0) {
                    h->delay -= dt;
                    continue;
                }
                int step = (int)h->t;
                h->t += dt / HopLength(h, step < h->n - 1 ? step : h->n - 2);
                if (h->t >= (float)(h->n - 1)) {
                    h->rest = h->path[h->n - 1];
                    h->n = 0;
                    h->t = 0;
                }
            }
    }
}

// Piece i of 'side' on the screen: between positions *a and *b, *k of the way there, lifted by *lift (0-1;
// more for a long jump).
void BoardAnim(int bi, int side, int i, int *a, int *b, float *k, float *lift)
{
    const Board *x = BoardAt(bi);
    *k = *lift = 0;
    if (!x || side < 0 || side > 1 || i < 0 || i >= UR_PIECES) {
        *a = *b = 0;
        return;
    }
    const Hop *h = &hops[bi][side][i];
    if (!seeded[bi] || h->n < 2) {
        *a = *b = h->n ? h->path[h->n - 1] : (seeded[bi] ? h->rest : x->pos[side][i]);
        if (!seeded[bi]) *a = *b = x->pos[side][i];
        return;
    }
    if (h->delay > 0) {
        *a = *b = h->path[0];
        return;
    }
    int step = (int)h->t;
    if (step > h->n - 2) step = h->n - 2;
    *a = h->path[step];
    *b = h->path[step + 1];
    *k = h->t - (float)step;
    *lift = sinf(*k * PI) * (h->jumpLast && step == h->n - 2 ? 2.2f : 1.0f);
}

bool BoardMoving(int bi, int side, int i)
{
    return bi >= 0 && bi < BOARDS && side >= 0 && side < 2 && i >= 0 && i < UR_PIECES && hops[bi][side][i].n >= 2;
}

float BoardTumble(int bi)
{
    return bi >= 0 && bi < BOARDS && tumble[bi] > 0 ? tumble[bi] / TUMBLE_TIME : 0;
}

const BoardEvent *BoardLastEvent(int bi)
{
    return bi >= 0 && bi < BOARDS && seeded[bi] && events[bi].side >= 0 ? &events[bi] : NULL;
}

// The board on the table seen from above: local x across (toward side 1), z along; rot 3 as the alehouse.
static Vector3 OnTable(int b, float x, float z, float up)
{
    Vector3 p = BoardSpot(b);
    return (Vector3){ p.x + z, p.y + up, p.z - x };
}

static const Color PIECE[2] = { { 34, 34, 46, 255 }, { 232, 226, 210, 255 } };

static const Color IVORY = { 236, 228, 206, 255 }, INK = { 40, 30, 24, 255 };

// The numbers on the dice, 3 x 5 cells each.
static const char *const DIGITS[6][5] = {
    { ".#.", "##.", ".#.", ".#.", "###" }, { "###", "..#", "###", "#..", "###" }, { "###", "..#", "###", "..#", "###" },
    { "#.#", "#.#", "###", "..#", "..#" }, { "###", "#..", "###", "..#", "###" }, { "###", "#..", "###", "#.#", "###" },
};

// A die on the table, its number on top; 'roll' (1 down to 0) while it is still tumbling.
static void DrawDie3D(Vector3 c, float size, int value, float roll, int seed)
{
    rlPushMatrix();
    rlTranslatef(c.x, c.y + size / 2 + sinf(roll * PI) * 0.07f, c.z);
    if (roll > 0) rlRotatef(roll * 900.0f + (float)seed * 47.0f, 0.6f, 1.0f, 0.3f);
    DrawShadedBox((Vector3){ 0, 0, 0 }, (Vector3){ size, size, size }, IVORY);
    int v = roll > 0 ? 1 + ((int)(roll * 23.0f) + seed) % 6 : value;
    if (v >= 1 && v <= 6) {
        float g = size / 6.5f;
        for (int r = 0; r < 5; r++)
            for (int k = 0; k < 3; k++)
                if (DIGITS[v - 1][r][k] == '#')
                    DrawShadedBox((Vector3){ ((float)k - 1) * g, size / 2 + 0.0015f, ((float)r - 2) * g }, (Vector3){ g * 0.92f, 0.003f, g * 0.92f }, INK);
    }
    rlPopMatrix();
}

static Vector3 UrTablePos(int b, int s, int p, int slot)
{
    const float sq = 0.105f;
    int col, row;
    if (p <= 0) return OnTable(b, (s ? 1 : -1) * 0.22f, -0.27f + 0.085f * (float)slot, 0.0f);
    if (p >= UR_HOME) return OnTable(b, (s ? 1 : -1) * 0.31f, -0.27f + 0.085f * (float)slot, 0.0f);
    UrSquare(0, s, p, &col, &row);
    return OnTable(b, (float)(row - 1) * -sq, ((float)col - 3.5f) * sq, 0.03f);
}

static void DrawUrBoard(int b, const Board *x)
{
    const float sq = 0.105f;
    DrawShadedBox(OnTable(b, 0, 0, 0.01f), (Vector3){ 8 * sq + 0.05f, 0.02f, 3 * sq + 0.05f }, (Color){ 46, 64, 120, 255 });
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 8; col++) {
            if (!UrSquareExists(col, row)) continue;
            Vector3 c = OnTable(b, (float)(row - 1) * -sq, ((float)col - 3.5f) * sq, 0.025f);
            DrawShadedBox(c, (Vector3){ sq * 0.86f, 0.01f, sq * 0.86f }, UrRosetteSquare(col, row) ? (Color){ 196, 78, 60, 255 } : (Color){ 226, 214, 186, 255 });
        }
    for (int s = 0; s < 2; s++) {
        // Waiting beside the board on the owner's side, those home further out; one walking goes between.
        int waitAt[UR_PIECES], homeAt[UR_PIECES], waiting = 0, home = 0;
        for (int k = 0; k < UR_PIECES; k++) {
            waitAt[k] = x->pos[s][k] == 0 ? waiting++ : -1;
            homeAt[k] = x->pos[s][k] >= UR_HOME ? home++ : -1;
        }
        for (int k = 0; k < UR_PIECES; k++) {
            int pa, pb;
            float t, lift;
            BoardAnim(b, s, k, &pa, &pb, &t, &lift);
            int slot = waitAt[k] >= 0 ? waitAt[k] : homeAt[k] >= 0 ? homeAt[k] : (pa == 0 ? waiting : home);
            Vector3 at = Vector3Lerp(UrTablePos(b, s, pa, slot), UrTablePos(b, s, pb, slot), t);
            at.y += lift * 0.035f;
            DrawShadedCylinder(at, 0.038f, 0.038f, 0.018f, 1.0f, 10, PIECE[s]);
        }
    }
    // The die at the end of the table.
    if (x->roll > 0 || BoardTumble(b) > 0) DrawDie3D(OnTable(b, 0.0f, 0.52f, 0.0f), 0.05f, x->dice[0], BoardTumble(b), b);
}

static Vector3 HolesTablePos(int b, int s, int p, int k)
{
    const float w = 0.42f, h = 0.5f;
    if (p <= 0) return OnTable(b, (s ? 1 : -1) * 0.3f, -0.2f + 0.05f * (float)k, 0.0f);
    Vector2 uv = HolesHole(0, s, p);
    return OnTable(b, (uv.x - 0.5f) * w + (p >= HOLES_HOME ? 0.02f * (float)(k - 2) : 0), (uv.y - 0.5f) * h, 0.03f);
}

static void DrawHolesBoard(int b, const Board *x)
{
    const float w = 0.42f, h = 0.5f;   // across (local x), along (local z)
    DrawShadedBox(OnTable(b, 0, 0, 0.015f), (Vector3){ h, 0.03f, w }, (Color){ 214, 196, 160, 255 });
    for (int s = 0; s < 2; s++)
        for (int p = 1; p <= 30; p++) {
            Vector2 uv = HolesHole(0, s, p);
            DrawShadedBox(OnTable(b, (uv.x - 0.5f) * w, (uv.y - 0.5f) * h, 0.031f), (Vector3){ 0.012f, 0.004f, 0.012f }, (Color){ 70, 52, 40, 255 });
        }
    for (int s = 0; s < 2; s++)
        for (int k = 0; k < HOLES_PIECES; k++) {
            int pa, pb;
            float t, lift;
            BoardAnim(b, s, k, &pa, &pb, &t, &lift);
            Vector3 at = Vector3Lerp(HolesTablePos(b, s, pa, k), HolesTablePos(b, s, pb, k), t);
            at.y += lift * 0.04f;
            DrawShadedCylinder(at, 0.008f, 0.008f, 0.07f, 1.0f, 6, PIECE[s]);
            DrawShadedBlob((Vector3){ at.x, at.y + 0.08f, at.z }, (Vector3){ 0.016f, 0.016f, 0.016f }, s ? (Color){ 200, 160, 90, 255 } : (Color){ 90, 70, 50, 255 });
        }
    for (int k = 0; k < 2; k++)
        if (x->roll > 0 || BoardTumble(b) > 0)
            DrawDie3D(OnTable(b, -0.05f + 0.1f * (float)k, 0.46f, 0.0f), 0.045f, x->dice[k], BoardTumble(b), b * 3 + k);
}

// The boards on their tables, their pieces where they stand, and the townsman across from a player.
void DrawBoards(void)
{
    if (!BoardsOn()) return;
    for (int b = 0; b < BOARDS; b++) {
        Vector3 at = BoardSpot(b);
        if (Vector3Distance(at, G.cam.position) > 30.0f) continue;
        LightFor(at);
        const Board *x = &boards[b];
        if (x->kind == BG_UR) DrawUrBoard(b, x);
        else DrawHolesBoard(b, x);
        for (int s = 0; s < 2; s++) {
            if (x->seat[s] != SEAT_NPC) continue;
            float face;
            Vector3 p = BoardSeat(b, s, &face);
            DrawHuman(p, face, 0, PersonLook(LOOK_TOWN, 900u + (unsigned)b, (Color){ 168, 140, 100, 255 }), true, false, 0);
        }
    }
}
