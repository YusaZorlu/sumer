// Board games at the alehouse's tables: the Royal Game of Ur and fifty-eight holes ("hounds and jackals").
// Two sit at a table, one on each side: two players, or a player and a townsman the alehouse finds for
// them. The host keeps every board (who sits where, the pieces, whose turn, the throw) and throws the dice;
// the players ask it to sit, throw and move (RQ_BOARD), and everyone sees the boards in the snapshots. The
// winner gains experience. Not in a battle royale or a party: no one has time to sit down there.
//
// The Royal Game of Ur (found at Ur, about 2600-2400 BC), as Irving Finkel read its rules from a later
// tablet: seven pieces each, four pyramid dice (each tip marked or not: a throw is 0 to 4). A piece goes
// 4 squares on its own side, 8 down the middle row (where it can take an opponent's piece by landing on it,
// sending it back), then 2 on its own side, and off with an exact throw. A rosette (the 4th, 8th and 14th
// square of the way) gives another throw, and the one in the middle row keeps a piece safe there.
//
// Fifty-eight holes (Egypt and Mesopotamia, from about 2000 BC): its rules are lost; this is a modern
// reading. Five pins each, 29 holes on each side's way and a shared hole at the top that ends it. Two
// knucklebones are thrown, each landing on a side worth 1, 3, 4 or 6 (the broad sides most often); the
// pins go up by their sum. Lines on the board join hole 6 with 20 and 8 with 10: a pin that lands on one
// end goes to the other, up or down. No two pins of a side share a hole; the first to bring all five home
// wins.
#include "game.h"
#include <math.h>
#include <string.h>

#define UR_HOME     15
#define HOLES_HOME  30
#define NPC_THINK   1.1f    // the townsman's pause before he throws or moves
#define PASS_SHOWN  1.6f    // a throw that cannot be used stays on the board this long
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
        if (to > home) return -1;   // off only with an exact throw
        if (to == home) return to;
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

// The throw: four pyramid dice for Ur (marked tips up), two knucklebones for the holes.
void BoardThrow(Board *b)
{
    int sum = 0;
    if (b->kind == BG_UR) {
        for (int k = 0; k < 4; k++) {
            b->dice[k] = (unsigned char)GetRandomValue(0, 1);
            sum += b->dice[k];
        }
    } else {
        static const int faces[10] = { 1, 3, 3, 3, 3, 4, 4, 4, 4, 6 };
        for (int k = 0; k < 2; k++) {
            b->dice[k] = (unsigned char)faces[GetRandomValue(0, 9)];
            sum += b->dice[k];
        }
        b->dice[2] = b->dice[3] = 0;
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
                    if (to != 8 && o < to && o >= to - 4 && (o >= 5 || to - o <= 4)) s -= 4;   // within reach behind
                }
            if (Shared(from) && from != 8)
                for (int k = 0; k < UR_PIECES; k++) {
                    int o = b->pos[1 - side][k];
                    if (o < from && o >= from - 4) s += 3;   // out of danger
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
    Say(BoardAt(b)->kind == BG_UR ? "The Royal Game of Ur: seven pieces each down the board and off. Rosettes throw again; the middle one is safe."
                                  : "Fifty-eight holes: five pins each up to the shared hole. The lines between holes carry a pin up, or down.");
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

// The board on the table seen from above: local x across (toward side 1), z along; rot 3 as the alehouse.
static Vector3 OnTable(int b, float x, float z, float up)
{
    Vector3 p = BoardSpot(b);
    return (Vector3){ p.x + z, p.y + up, p.z - x };
}

static const Color PIECE[2] = { { 34, 34, 46, 255 }, { 232, 226, 210, 255 } };

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
        int waiting = 0, home = 0;
        for (int k = 0; k < UR_PIECES; k++) {
            int p = x->pos[s][k], col, row;
            Vector3 at;
            // Waiting beside the board on the owner's side, those home further out.
            if (p == 0) at = OnTable(b, (s ? 1 : -1) * 0.22f, -0.27f + 0.085f * (float)waiting++, 0.0f);
            else if (p >= 15) at = OnTable(b, (s ? 1 : -1) * 0.31f, -0.27f + 0.085f * (float)home++, 0.0f);
            else {
                UrSquare(0, s, p, &col, &row);
                at = OnTable(b, (float)(row - 1) * -sq, ((float)col - 3.5f) * sq, 0.03f);
            }
            DrawShadedCylinder(at, 0.038f, 0.038f, 0.018f, 1.0f, 10, PIECE[s]);
        }
    }
    // The four pyramid dice, marked tips white.
    for (int k = 0; k < 4; k++) {
        Vector3 d = OnTable(b, -0.13f + 0.085f * (float)k, 0.51f, 0.0f);
        DrawShadedCylinder(d, 0.03f, 0.03f, 0.045f, 0.0f, 3, (Color){ 120, 90, 60, 255 });
        if (x->roll >= 0 && x->dice[k]) DrawShadedBox((Vector3){ d.x, d.y + 0.046f, d.z }, (Vector3){ 0.012f, 0.012f, 0.012f }, RAYWHITE);
    }
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
            int p = x->pos[s][k];
            Vector3 at;
            if (p == 0) at = OnTable(b, (s ? 1 : -1) * 0.3f, -0.2f + 0.05f * (float)k, 0.0f);
            else {
                Vector2 uv = HolesHole(0, s, p);
                at = OnTable(b, (uv.x - 0.5f) * w + (p >= 30 ? 0.02f * (float)(k - 2) : 0), (uv.y - 0.5f) * h, 0.03f);
            }
            DrawShadedCylinder(at, 0.008f, 0.008f, 0.07f, 1.0f, 6, PIECE[s]);
            DrawShadedBlob((Vector3){ at.x, at.y + 0.08f, at.z }, (Vector3){ 0.016f, 0.016f, 0.016f }, s ? (Color){ 200, 160, 90, 255 } : (Color){ 90, 70, 50, 255 });
        }
    for (int k = 0; k < 2; k++) {
        Vector3 d = OnTable(b, -0.05f + 0.1f * (float)k, 0.45f, 0.0f);
        DrawShadedBlob((Vector3){ d.x, d.y + 0.02f, d.z }, (Vector3){ 0.035f, 0.02f, 0.025f }, (Color){ 230, 220, 196, 255 });
    }
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
