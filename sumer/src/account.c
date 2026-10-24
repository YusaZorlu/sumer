// The player's account on the game's server (server.py): logging in and creating one on the title screen,
// the level and K/D it keeps, the copy of the singleplayer saves it keeps, and the list of its servers.
// The server keeps only the account's name, a hash of its password and the game: no e-mail, no real name
// (web/privacy.html says what is kept). The password can be changed and the account deleted. A new account costs a moment of work (sha256.c), which keeps anyone from making thousands.
// The answers come as lines of tab-separated words. Natively (the server program, the tests) nothing is
// sent: the numbers are only kept here.
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLUSH_EVERY 10.0f
#define ROOMS_EVERY 4.0
#define SAVE_BYTES  8192
#define MAX_ROOMS   16

typedef struct { char id[24], title[40]; int mode, players, max, state; } Room;

static int state = ACC_UNKNOWN;
static char name[NAME_LEN], note[200];
static long xp;
static int kills, deaths;
static int meReq = -1, sendReq = -1, challengeReq = -1, progressReq = -1, savesReq = -1, roomsReq = -1, uploadReq = -1, logoutReq = -1;
static int changeReq = -1, deleteReq = -1;
// The work a new account costs: the server's challenge, how many zero bits, the next number to try.
static unsigned char challenge[16];
static char challengeHex[33];
static int workBits = -1;
static unsigned workNonce;
static bool savesReady;
// Gains made alone, not yet sent; and what the last report sent (the server's answer counts them in).
static int pendXp, pendKills, pendDeaths, sentXp, sentKills, sentDeaths;
static float flushT;
static Room rooms[MAX_ROOMS];
static int roomCount;
static double roomsAt = -100.0;
// The latest of each singleplayer save, waiting to go to the server (length 0: the slot was emptied).
static unsigned char upload[SAVE_SLOTS][SAVE_BYTES];
static int uploadLen[SAVE_SLOTS], uploadSlot = -1;
static bool uploadDirty[SAVE_SLOTS];
static unsigned char reply[SAVE_SLOTS * (SAVE_BYTES + 4) + 16];

bool LoggedIn(void)
{
    return state == ACC_IN;
}

int AccountState(void)
{
    return state;
}

const char *AccountName(void)
{
    return name;
}

long AccountXp(void)
{
    return xp;
}

int AccountKills(void)
{
    return kills;
}

int AccountDeaths(void)
{
    return deaths;
}

const char *AccountNote(void)
{
    return note;
}

bool AccountSavesReady(void)
{
    return savesReady;
}

void AccountNoteClear(void)
{
    note[0] = '\0';
}

static void Note(const char *s)
{
    int n = 0;
    for (; s[n] && n < (int)sizeof(note) - 1; n++) note[n] = s[n];
    note[n] = '\0';
}

// "name<TAB>xp<TAB>kills<TAB>deaths", as server.py tells it.
static bool ReadProfile(const char *text)
{
    char n[64];
    long x;
    int k, d;
    if (sscanf(text, "%63[^\t]\t%ld\t%d\t%d", n, &x, &k, &d) != 4 || !n[0] || strlen(n) >= NAME_LEN) return false;
    snprintf(name, sizeof(name), "%s", n);
    xp = x;
    kills = k;
    deaths = d;
    return true;
}

// The singleplayer saves the server keeps for the account (each slot as a length, 4 bytes, and the save
// itself; empty for a slot with nothing in it) replace this browser's. An account with none takes up this
// browser's own instead, made before accounts (or by it): they go to the server. The browser remembers whose
// its saves are (sumer.owner), so another account never takes them.
#define OWNER_FILE "sumer.owner"

static void TakeSaves(const unsigned char *d, int n)
{
    char owner[NAME_LEN + 2] = "";
    FILE *f = fopen(DataPath(OWNER_FILE), "r");
    if (f) {
        if (!fgets(owner, sizeof(owner), f)) owner[0] = '\0';
        fclose(f);
        owner[strcspn(owner, "\r\n")] = '\0';
    }
    int at = 0, slots = 0, kept = 0, from[SAVE_SLOTS], size[SAVE_SLOTS];
    while (slots < SAVE_SLOTS && at + 4 <= n) {
        int len = (int)(d[at] | d[at + 1] << 8 | d[at + 2] << 16 | (unsigned)d[at + 3] << 24);
        at += 4;
        if (len < 0 || len > n - at || len > SAVE_BYTES) break;
        from[slots] = at;
        size[slots++] = len;
        kept += len > 0;
        at += len;
    }
    if (!kept && (!owner[0] || SameName(owner, name))) {
        static unsigned char own[SAVE_BYTES];
        for (int s = 0; s < SAVE_SLOTS; s++) {
            int len = ReadFile(SaveFileName(s), own, SAVE_BYTES);
            if (len > 0) AccountSaved(s, own, len);
        }
    } else {
        for (int s = 0; s < slots; s++) {
            if (size[s] > 0) {
                Writer w = { (unsigned char *)d + from[s], size[s], size[s], false };
                WriteFile(SaveFileName(s), &w);
            } else {
                remove(DataPath(SaveFileName(s)));
            }
        }
    }
    f = fopen(DataPath(OWNER_FILE), "w");
    if (f) {
        fprintf(f, "%s\n", name);
        fclose(f);
    }
    SyncStorage();
}

static void LoggedInNow(void)
{
    state = ACC_IN;
    note[0] = '\0';
    pendXp = pendKills = pendDeaths = 0;
    savesReady = false;
    savesReq = ApiStart("GET", "/api/saves", NULL, 0);
    if (savesReq < 0) savesReady = true;
}

// At the start: is this browser still logged in?
void AccountStart(void)
{
    state = ACC_UNKNOWN;
    meReq = ApiStart("GET", "/api/me", NULL, 0);
    if (meReq < 0) state = ACC_OUT;
}

// Log in, or create the account, with what is typed in the page's fields (the password never comes
// into the game: the page sends it).
void AccountSubmit(bool create)
{
    if (state != ACC_OUT) return;
    char typed[64];
    AccountFieldName(typed, (int)sizeof(typed));
    int problem = AccountFieldCheck(create);
    int len = (int)strlen(typed);
    bool okName = len >= 3 && len < NAME_LEN;
    for (int i = 0; i < len; i++) {
        char c = typed[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) okName = false;
    }
    if (!okName) {
        Note("An account name is 3 to 15 letters or digits (and _ - .).");
        return;
    }
    if (problem == 1) {
        Note("A password needs at least 6 characters.");
        return;
    }
    if (problem == 2) {
        Note("The two passwords are not the same.");
        return;
    }
    state = ACC_BUSY;
    if (create) {
        Note("Creating your account...");
        challengeReq = ApiStart("POST", "/api/challenge", NULL, 0);
    } else {
        Note("Logging in...");
        sendReq = AccountSend(false, "", 0);
    }
    if (challengeReq < 0 && sendReq < 0) {
        state = ACC_OUT;
        Note("Could not reach the game server.");
    }
}

static void SendProgress(void)
{
    char body[64];
    snprintf(body, sizeof(body), "%d %d %d", pendXp, pendKills, pendDeaths);
    progressReq = ApiStart("POST", "/api/progress", body, (int)strlen(body));
    sentXp = pendXp;
    sentKills = pendKills;
    sentDeaths = pendDeaths;
    pendXp = pendKills = pendDeaths = 0;
    flushT = 0;
}

// Logged out here (the server ended the session, or the account is gone).
static void Forget(void)
{
    state = ACC_OUT;
    name[0] = '\0';
    xp = kills = deaths = 0;
    pendXp = pendKills = pendDeaths = 0;
    savesReady = false;
    roomCount = 0;
    workBits = -1;
    note[0] = '\0';
}

void AccountLogout(void)
{
    if (state == ACC_IN && progressReq < 0 && (pendXp || pendKills || pendDeaths)) SendProgress();
    logoutReq = ApiStart("POST", "/api/logout", NULL, 0);
    Forget();
}

// A password request is under way.
bool AccountWaiting(void)
{
    return changeReq >= 0 || deleteReq >= 0;
}

// The password page: a new password, typed twice, after the current one.
void AccountChangePassword(void)
{
    if (state != ACC_IN || AccountWaiting()) return;
    int problem = AccountPasswordCheck(true);
    if (problem) {
        Note(problem == 1 ? "Type your current password first." : problem == 3 ? "A password needs at least 6 characters."
                                                                               : "The two new passwords are not the same.");
        return;
    }
    Note("Changing your password...");
    changeReq = AccountSendPassword("/api/password");
    if (changeReq < 0) Note("Could not reach the game server.");
}

// The account and everything the server keeps with it, for good (asked twice on the title screen).
void AccountDelete(void)
{
    if (state != ACC_IN || AccountWaiting()) return;
    if (AccountPasswordCheck(false)) {
        Note("Type your current password first.");
        return;
    }
    Note("Deleting your account...");
    deleteReq = AccountSendPassword("/api/delete");
    if (deleteReq < 0) Note("Could not reach the game server.");
}

// Experience, kills and deaths gained: shown at once. Alone ('report'), they are sent on to the server
// every few seconds; in a server's game the server has them already.
void AccountGain(int gainXp, int gainKills, int gainDeaths, bool report)
{
    long before = xp;
    xp += gainXp;
    kills += gainKills;
    deaths += gainDeaths;
    if (report && LoggedIn()) {
        pendXp += gainXp;
        pendKills += gainKills;
        pendDeaths += gainDeaths;
    }
    ShowGain(before, xp);
}

// What is left to send goes at once (leaving a game).
void AccountFlush(void)
{
    flushT = FLUSH_EVERY;
}

// A singleplayer save was written: the server keeps a copy for the account (an empty one: the slot was emptied).
void AccountSaved(int slot, const unsigned char *data, int len)
{
    if (slot < 0 || slot >= SAVE_SLOTS || len < 0 || len > SAVE_BYTES || !LoggedIn()) return;
    if (len > 0) memcpy(upload[slot], data, (size_t)len);
    uploadLen[slot] = len;
    uploadDirty[slot] = true;
}

// The list of servers, asked for again every few seconds while it is shown.
void AccountRooms(void)
{
    if (roomsReq >= 0 || GetTime() - roomsAt < ROOMS_EVERY) return;
    roomsAt = GetTime();
    roomsReq = ApiStart("GET", "/api/rooms", NULL, 0);
}

int RoomCount(void)
{
    return roomCount;
}

// Room i: its id (for joining), title, kind (ROOM_*), players and room for them, and its state: 0 asleep
// (joining wakes it), 1 running, 2 a battle royale round under way, 3 asleep and no more can run now.
bool RoomInfo(int i, char *id, int idSize, char *title, int titleSize, int *mode, int *players, int *max, int *st)
{
    if (i < 0 || i >= roomCount) return false;
    snprintf(id, (size_t)idSize, "%s", rooms[i].id);
    snprintf(title, (size_t)titleSize, "%s", rooms[i].title);
    *mode = rooms[i].mode;
    *players = rooms[i].players;
    *max = rooms[i].max;
    *st = rooms[i].state;
    return true;
}

// Lines of "id<TAB>mode<TAB>players<TAB>max<TAB>state<TAB>title".
static void ReadRooms(char *text)
{
    roomCount = 0;
    for (char *line = strtok(text, "\n"); line && roomCount < MAX_ROOMS; line = strtok(NULL, "\n")) {
        Room r;
        if (sscanf(line, "%23[^\t]\t%d\t%d\t%d\t%d\t%39[^\n]", r.id, &r.mode, &r.players, &r.max, &r.state, r.title) == 6) rooms[roomCount++] = r;
    }
}

static int Hex(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

// The answer to request *req, if it has come: its status (-1: no server), the answer in 'reply'.
static int Answer(int *req, int *len)
{
    if (*req < 0) return 0;
    int status = ApiDone(*req, reply, (int)sizeof(reply), len);
    if (status != 0) *req = -1;
    return status;
}

// What a refused request says, or why there was no answer.
static void Refused(int status)
{
    Note(status < 0 ? "Could not reach the game server." : reply[0] ? (const char *)reply : "The server said no.");
}

// Every frame: the answers that have come, the work for a new account, and what is to be sent.
void AccountFrame(void)
{
    int len, status;
    if ((status = Answer(&meReq, &len)) != 0) {
        if (status == 200 && ReadProfile((const char *)reply)) LoggedInNow();
        else {
            state = ACC_OUT;
            if (status < 0) Note("Could not reach the game server.");
        }
    }
    if ((status = Answer(&challengeReq, &len)) != 0) {
        char hex[40];
        int bits;
        if (status == 200 && sscanf((const char *)reply, "%32[0-9a-f]\t%d", hex, &bits) == 2 && strlen(hex) == 32) {
            snprintf(challengeHex, sizeof(challengeHex), "%s", hex);
            for (int i = 0; i < 16; i++) challenge[i] = (unsigned char)(Hex(hex[2 * i]) << 4 | Hex(hex[2 * i + 1]));
            workBits = bits;
            workNonce = (unsigned)GetRandomValue(0, 1 << 30);
        } else {
            state = ACC_OUT;
            Refused(status);
        }
    }
    // The work, a slice of it each frame so the page stays alive: a number that makes the hash start
    // with enough zero bits.
    if (workBits >= 0) {
        char typed[64];
        AccountFieldName(typed, (int)sizeof(typed));
        double until = GetTime() + 0.008;
        for (int k = 0; workBits >= 0 && (k & 1023 || GetTime() < until); k++, workNonce++)
            if (WorkDone(challenge, typed, workNonce, workBits)) {
                sendReq = AccountSend(true, challengeHex, workNonce);
                workBits = -1;
                if (sendReq < 0) {
                    state = ACC_OUT;
                    Note("Could not reach the game server.");
                }
            }
    }
    if ((status = Answer(&sendReq, &len)) != 0) {
        if (status == 200 && ReadProfile((const char *)reply)) {
            AccountFieldsClear();
            LoggedInNow();
        } else {
            state = ACC_OUT;
            Refused(status);
        }
    }
    if ((status = Answer(&savesReq, &len)) != 0) {
        if (status == 200) TakeSaves(reply, len);
        if (status == 401) AccountLogout();
        savesReady = state == ACC_IN;
    }
    if ((status = Answer(&roomsReq, &len)) != 0 && status == 200) ReadRooms((char *)reply);
    if ((status = Answer(&uploadReq, &len)) != 0) {
        // Not taken: it goes again with the next save.
        if (status != 200 && uploadSlot >= 0 && !uploadDirty[uploadSlot]) uploadDirty[uploadSlot] = status != 413;
        uploadSlot = -1;
    }
    if ((status = Answer(&progressReq, &len)) != 0) {
        // The server's totals, with what was gained since the report was sent; refused, it goes again.
        if (status == 200 && ReadProfile((const char *)reply)) {
            xp += pendXp;
            kills += pendKills;
            deaths += pendDeaths;
        } else if (status != 200) {
            pendXp += sentXp;
            pendKills += sentKills;
            pendDeaths += sentDeaths;
        }
        sentXp = sentKills = sentDeaths = 0;
        if (status == 401) AccountLogout();
    }
    if ((status = Answer(&changeReq, &len)) != 0) {
        if (status == 200) {
            AccountFieldsClear();
            Note("Your password is changed.");
        } else {
            Refused(status);
        }
    }
    if ((status = Answer(&deleteReq, &len)) != 0) {
        if (status == 200) {
            AccountFieldsClear();
            Forget();
            Note("Your account is deleted, with everything the server kept for it.");
        } else {
            Refused(status);
        }
    }
    (void)Answer(&logoutReq, &len);
    if (!LoggedIn()) return;

    flushT += GetFrameTime();
    if (flushT >= FLUSH_EVERY && progressReq < 0 && (pendXp || pendKills || pendDeaths)) SendProgress();
    for (int s = 0; s < SAVE_SLOTS && uploadReq < 0; s++) {
        if (!uploadDirty[s]) continue;
        uploadDirty[s] = false;
        uploadSlot = s;
        uploadReq = ApiStart("POST", TextFormat("/api/save?slot=%d", s), upload[s], uploadLen[s]);
    }
}
