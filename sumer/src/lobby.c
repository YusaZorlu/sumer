// The browser's title screen. First the account: log in, or create one on a tab of its own (account.c).
// Then the ways to play: Sumer Survival (alone, or on one of the game's servers, together or with PvP)
// and Sumer Royale (the battle royale servers), each server picked from a list that shows who is on it.
// For tests and links, ?auto=solo plays alone and ?auto=join&room=ID joins that server, once logged in.
#include "game.h"
#include <stdio.h>
#include <string.h>

static enum { LOBBY_MENU, LOBBY_CONNECT } stage;
// The title's pages: the ways to play, then Sumer Survival's (alone, or a server) or Sumer Royale's.
// PAGE_ACCOUNT: changing the password, deleting the account.
static enum { PAGE_MODES, PAGE_SURVIVAL, PAGE_ROYALE, PAGE_PARTY, PAGE_ACCOUNT } page;
static bool open, loaded, autoDone, createTab, deleteArmed;
static char note[200];
static float connectT;

const char *PlayerName(void)
{
    return LoggedIn() ? AccountName() : "Player";
}

void OpenLobby(const char *why)
{
    if (!loaded) {
        loaded = true;
        if (Royale()) page = PAGE_ROYALE;   // ?mode=br: straight to the battle royale
        if (Party()) page = PAGE_PARTY;     // ?mode=party: straight to the party
        AccountStart();
    }
    open = true;
    stage = LOBBY_MENU;
    snprintf(note, sizeof(note), "%s", why && why[0] ? L(why) : "");
    LockMouse(false);
    WantPointer(false);
    AccountFlush();
}

bool InLobby(void)
{
    return open;
}

// Out of a game and back to the title screen, saying why.
void LeaveToLobby(const char *why)
{
    NetShut();
    G.role = NET_SOLO;
    OpenLobby(why);
}

// Into the game: the first time on this computer, choosing a look comes first (not for test links).
static void Enter(void)
{
    open = false;
    G.quit = false;
    AccountForm(FORM_NONE, (Rectangle){ 0 }, (Rectangle){ 0 }, (Rectangle){ 0 });
    SetPanel(StyleChosen() || PageParam("auto")[0] ? PANEL_NONE : PANEL_LOOK);
    G.wasCaptured = false;
}

#ifdef SUMER_TEST
// For tests: back at the title, start again the way the page's address says (?auto=solo plays alone).
void LobbyAgain(void)
{
    autoDone = false;
}
#endif

static void PlayAlone(void)
{
    SetRoyale(false);
    SetParty(false);
    SetSaveSlot(LastSlot());
    GameInit();
    if (LoadGame()) {
        Say("Welcome back. Day %d. Slot %d was loaded.", G.day, SaveSlot() + 1);
        G.msgT = 6.0f;
    }
    Enter();
}

// Onto server 'id', a room of kind 'mode'.
static void Join(const char *id, int mode)
{
    note[0] = '\0';
    connectT = 0;
    SetRoyale(mode == ROOM_ROYALE);
    SetParty(mode == ROOM_PARTY);
    if (NetBegin(AccountName(), id)) stage = LOBBY_CONNECT;
    else snprintf(note, sizeof(note), "%s", NetFailure());
}

static void Label(const char *s, float x, float y, int size, Color c)
{
    DrawTextEx(GameFont(), L(s), (Vector2){ (float)(int)x, (float)(int)y }, (float)size, (float)(size / 10), c);
}

static void Centered(const char *s, float cx, float y, int size, Color c)
{
    s = L(s);
    Vector2 m = MeasureTextEx(GameFont(), s, (float)size, (float)(size / 10));
    DrawTextEx(GameFont(), s, (Vector2){ (float)(int)(cx - m.x / 2), (float)(int)y }, (float)size, (float)(size / 10), c);
}

static bool Choice(Rectangle r, const char *label, bool enabled)
{
    int size = r.height < 30 ? 15 : 20;
    bool hover = enabled && CheckCollisionPointRec(UiMouse(), r);
    DrawRectangleRec(r, enabled ? (hover ? (Color){ 150, 110, 60, 255 } : (Color){ 112, 82, 48, 255 }) : (Color){ 60, 54, 48, 255 });
    DrawRectangleLinesEx(r, 1, (Color){ 150, 120, 80, 255 });
    Centered(label, r.x + r.width / 2, r.y + (r.height - (float)size) / 2, size, enabled ? (Color){ 240, 230, 210, 255 } : (Color){ 150, 140, 125, 255 });
    return hover && MousePressed(MOUSE_BUTTON_LEFT);
}

// A tab: lit while it is the one open.
static bool Tab(Rectangle r, const char *label, bool on)
{
    bool hover = CheckCollisionPointRec(UiMouse(), r);
    DrawRectangleRec(r, on ? (Color){ 150, 110, 60, 255 } : hover ? (Color){ 90, 68, 42, 255 } : (Color){ 58, 48, 38, 255 });
    DrawRectangleLinesEx(r, 1, on ? (Color){ 240, 200, 120, 255 } : (Color){ 150, 120, 80, 255 });
    Centered(label, r.x + r.width / 2, r.y + (r.height - 18) / 2, 18, (Color){ 240, 230, 210, 255 });
    return hover && !on && MousePressed(MOUSE_BUTTON_LEFT);
}

// Where a server stands, in a few words.
static const char *RoomLine(int mode, int players, int max, int state)
{
    const char *kind = L(mode == ROOM_PVP ? "PvP: players can fight" : mode == ROOM_ROYALE ? "Battle royale" : mode == ROOM_PARTY ? "Party of minigames"
                                                                                                                         : "Together, no PvP");
    if (state == 3) return TextFormat("%s - %s", kind, L("cannot open now: all servers busy"));
    if (state == 0) return TextFormat("%s - %s", kind, L("nobody here; joining opens it"));
    if (players >= max) return TextFormat("%s - %s", kind, L("full"));
    return TextFormat(L(state == 2 ? "%s - %d/%d players, a round under way" : "%s - %d/%d players"), kind, players, max);
}

// The servers of the kinds wanted, one row each from y on; returns where the rows end.
static float RoomRows(float x, float y, int kind, bool menu)
{
    char id[24], title[40];
    int mode, players, max, state, shown = 0;
    for (int i = 0; i < RoomCount(); i++) {
        if (!RoomInfo(i, id, (int)sizeof(id), title, (int)sizeof(title), &mode, &players, &max, &state)) continue;
        if (kind == ROOM_SURVIVAL ? mode != ROOM_SURVIVAL && mode != ROOM_PVP : mode != kind) continue;
        bool ok = menu && state != 3 && players < max;
        if (Choice((Rectangle){ x, y, 250, 36 }, title, ok)) Join(id, mode);
        Label(RoomLine(mode, players, max, state), x + 270, y + 10, 15, (Color){ 150, 140, 125, 255 });
        y += 44;
        shown++;
    }
    if (!shown) {
        Label(RoomCount() ? "No servers of this kind are set up." : "Asking the game server for its servers...", x, y + 8, 15, (Color){ 150, 140, 125, 255 });
        y += 44;
    }
    return y;
}

// ?auto=solo|join&room=ID: once logged in (and, to join, once the list of servers says what kind it is).
static void AutoStart(void)
{
    const char *mode = PageParam("auto");
    if (!mode[0]) {
        autoDone = true;
        return;
    }
    if (!LoggedIn() || !AccountSavesReady()) return;
    if (strcmp(mode, "solo") == 0) {
        autoDone = true;
        PlayAlone();
        return;
    }
    char want[32], id[24], title[40];
    int kind, players, max, state;
    snprintf(want, sizeof(want), "%s", PageParam("room"));
    AccountRooms();
    if (!RoomCount()) return;
    // A room not in the list (a test server's) is of the kind its name begins with.
    int room = strncmp(want, "royale", 6) == 0 ? ROOM_ROYALE : strncmp(want, "party", 5) == 0 ? ROOM_PARTY : strncmp(want, "pvp", 3) == 0 ? ROOM_PVP : ROOM_SURVIVAL;
    for (int i = 0; i < RoomCount(); i++)
        if (RoomInfo(i, id, (int)sizeof(id), title, (int)sizeof(title), &kind, &players, &max, &state) && strcmp(id, want) == 0) room = kind;
    autoDone = true;
    Join(want, room);
}

void LobbyFrame(void)
{
    const Color gold = { 240, 200, 120, 255 }, text = { 240, 230, 210, 255 }, dim = { 150, 140, 125, 255 }, bad = { 230, 110, 90, 255 };
    float sw = UiWidth(), sh = UiHeight(), cx = sw / 2;

    AccountFrame();
    if (!autoDone && stage == LOBBY_MENU) AutoStart();
    if (!open) return;
    if (stage == LOBBY_CONNECT) {
        int state = NetLobby();
        connectT += GetFrameTime();
        if (state > 0) {
            Enter();
            return;
        }
        if (state < 0) {
            stage = LOBBY_MENU;
            snprintf(note, sizeof(note), "%s", NetFailure());
        }
    }

    BeginDrawing();
    ClearBackground((Color){ 30, 24, 20, 255 });
    UiBegin();
    DrawRectangleGradientV(0, 0, (int)sw, (int)(sh * 0.6f), (Color){ 92, 132, 170, 255 }, (Color){ 214, 178, 120, 255 });
    DrawRectangle(0, (int)(sh * 0.6f), (int)sw, (int)(sh * 0.4f), (Color){ 168, 136, 96, 255 });
    // The ziggurat on the horizon: three stages of mud brick.
    float base = sh * 0.6f;
    DrawRectangle((int)(cx - 210), (int)(base - 60), 420, 60, (Color){ 150, 112, 76, 255 });
    DrawRectangle((int)(cx - 140), (int)(base - 105), 280, 45, (Color){ 160, 120, 82, 255 });
    DrawRectangle((int)(cx - 80), (int)(base - 140), 160, 35, (Color){ 170, 128, 88, 255 });
    DrawRectangle((int)(cx - 9), (int)(base - 60), 18, 60, (Color){ 128, 94, 64, 255 });

    Rectangle panel = { cx - 360, 0, 720, 530 };
    panel.y = sh > panel.height + 16 ? (sh - panel.height) / 2 : 8;
    DrawRectangleRec(panel, (Color){ 30, 24, 20, 238 });
    DrawRectangleLinesEx(panel, 2, (Color){ 150, 120, 80, 255 });
    Centered("SUMER", cx, panel.y + 18, 50, gold);
    bool in = LoggedIn();
    Centered(!in ? "Ur, c. 2100 BC" : page == PAGE_ROYALE ? "Sumer Royale" : page == PAGE_PARTY ? "Sumer Party" : page == PAGE_SURVIVAL ? "Sumer Survival" : "Ur, c. 2100 BC",
             cx, panel.y + 74, 20, in && (page == PAGE_ROYALE || page == PAGE_PARTY) ? gold : text);

    float x = panel.x + 40, y = panel.y + 120;
    bool menu = stage == LOBBY_MENU;
    int acc = AccountState();
    Rectangle hidden = { 0 };
    if (!in) {
        // The account: log in, or create one. The fields are the page's own, laid over these frames.
        bool busy = acc != ACC_OUT;
        if (page == PAGE_ACCOUNT) page = PAGE_MODES;
        if (Tab((Rectangle){ cx - 205, y, 200, 32 }, "Log in", !createTab) && !busy) createTab = false;
        if (Tab((Rectangle){ cx + 5, y, 200, 32 }, "Create account", createTab) && !busy) createTab = true;
        y += 52;
        Rectangle fields[3];
        const char *labels[3] = { "Account name", "Password", "Password again" };
        int rows = createTab ? 3 : 2;
        for (int k = 0; k < rows; k++) {
            fields[k] = (Rectangle){ x + 230, y, 380, 38 };
            Label(labels[k], x + 20, y + 9, 20, text);
            DrawRectangleRec(fields[k], (Color){ 58, 48, 38, 255 });
            DrawRectangleLinesEx(fields[k], 1, busy ? dim : gold);
            y += 48;
        }
        if (rows == 2) fields[2] = hidden;
        AccountForm(acc == ACC_UNKNOWN ? FORM_NONE : createTab ? FORM_CREATE : FORM_LOGIN, fields[0], fields[1], fields[2]);
        y += 8;
        bool go = Choice((Rectangle){ cx - 125, y, 250, 44 }, createTab ? "Create account" : "Log in", !busy) || (!busy && AccountEnter());
        if (go) AccountSubmit(createTab);
        y += 60;
        if (acc == ACC_UNKNOWN) Centered("Connecting to the game server...", cx, y, 17, gold);
        else if (AccountNote()[0]) Centered(AccountNote(), cx, y, 17, acc == ACC_BUSY ? gold : bad);
        else if (note[0]) Centered(note, cx, y, 17, bad);
        Centered(createTab ? "No e-mail needed: only your account name and a hash of your password are kept."
                           : "Your level, K/D and things are kept with your account on the game's server.",
                 cx, panel.y + panel.height - 40, 15, dim);
    } else {
        if (page != PAGE_ACCOUNT) AccountForm(FORM_NONE, hidden, hidden, hidden);
        Rectangle row = { x, y, 250, 44 };
        if (page == PAGE_ACCOUNT) {
            // The password, changed after the current one; or the account deleted (asked twice).
            Label("Your account", x, y, 20, gold);
            y += 32;
            Rectangle fields[3];
            const char *labels[3] = { "Current password", "New password", "New password again" };
            for (int k = 0; k < 3; k++) {
                fields[k] = (Rectangle){ x + 230, y, 380, 38 };
                Label(labels[k], x + 20, y + 9, 20, text);
                DrawRectangleRec(fields[k], (Color){ 58, 48, 38, 255 });
                DrawRectangleLinesEx(fields[k], 1, AccountWaiting() ? dim : gold);
                y += 48;
            }
            AccountForm(FORM_PASSWORD, fields[0], fields[1], fields[2]);
            y += 8;
            bool ready = menu && !AccountWaiting();
            if (Choice((Rectangle){ cx - 260, y, 250, 44 }, "Change password", ready) || (ready && AccountEnter())) {
                deleteArmed = false;
                AccountChangePassword();
            }
            if (Choice((Rectangle){ cx + 10, y, 250, 44 }, deleteArmed ? "Delete for good" : "Delete account", ready)) {
                if (deleteArmed) AccountDelete();
                deleteArmed = !deleteArmed;
            }
            y += 56;
            if (deleteArmed) Centered("Click \"Delete for good\" to delete your account, level, saves and things.", cx, y, 17, bad);
            else if (AccountNote()[0]) Centered(AccountNote(), cx, y, 17, AccountWaiting() ? gold : text);
            y += 28;
            Centered("Deleting needs your current password and cannot be undone.", cx, y, 15, dim);
            y += 26;
            if (Choice((Rectangle){ x, y, 120, 26 }, "Back", menu)) {
                page = PAGE_MODES;
                deleteArmed = false;
            }
            y += 44;
        } else if (page == PAGE_MODES) {
            // The ways to play. The third place is kept for one to come.
            if (Choice(row, "Sumer Survival", menu)) {
                SetRoyale(false);
                SetParty(false);
                page = PAGE_SURVIVAL;
            }
            Label("Alone or with others: farm, hunt, trade", x + 270, y + 4, 15, dim);
            Label("and get through the seasons in Ur.", x + 270, y + 24, 15, dim);
            y += 58;
            if (Choice((Rectangle){ x, y, 250, 44 }, "Sumer Royale", menu)) page = PAGE_ROYALE;
            Label("Battle royale for up to twenty players:", x + 270, y + 4, 15, dim);
            Label("the last one standing wins.", x + 270, y + 24, 15, dim);
            y += 58;
            if (Choice((Rectangle){ x, y, 250, 44 }, "Sumer Party", menu)) page = PAGE_PARTY;
            Label("Four minigames in a row for up to twelve:", x + 270, y + 4, 15, dim);
            Label("points in each, the best three win.", x + 270, y + 24, 15, dim);
            y += 70;
        } else if (page == PAGE_SURVIVAL) {
            AccountRooms();
            bool ready = AccountSavesReady();
            if (Choice(row, "Singleplayer", menu && ready)) PlayAlone();
            Label(ready ? "Three save slots, kept with your account." : "Fetching your saves...", x + 270, y + 14, 15, dim);
            y += 56;
            Label("Servers: the world goes on while you are away; your things come with you.", x, y, 15, text);
            y += 24;
            y = RoomRows(x, y, ROOM_SURVIVAL, menu);
            if (Choice((Rectangle){ x, y + 4, 120, 26 }, "Back", menu)) page = PAGE_MODES;
            y += 44;
        } else if (page == PAGE_PARTY) {
            // A party: four of five minigames, once everyone in (two or more) is ready.
            AccountRooms();
            Label("Four minigames of four minutes, chosen from five (vote one out while you wait). Points", x, y, 15, text);
            Label("in each game, +10, +6, +3 for its top three; the party's best three gain experience.", x, y + 20, 15, text);
            y += 50;
            y = RoomRows(x, y, ROOM_PARTY, menu);
            if (Choice((Rectangle){ x, y + 4, 120, 26 }, "Back", menu)) page = PAGE_MODES;
            y += 44;
        } else {
            // A battle royale: a round starts once everyone in (two or more) is ready.
            AccountRooms();
            Label("A round starts ten seconds after everyone in (two or more) is ready. Everyone", x, y, 15, text);
            Label("starts with the best gear; the winner gains 10 XP for every player in the round.", x, y + 20, 15, text);
            y += 50;
            y = RoomRows(x, y, ROOM_ROYALE, menu);
            if (Choice((Rectangle){ x, y + 4, 120, 26 }, "Back", menu)) page = PAGE_MODES;
            y += 44;
        }
        if (stage == LOBBY_CONNECT) {
            Centered(connectT < 8.0f ? "Connecting..." : "Still connecting...", cx, y, 20, gold);
            if (Choice((Rectangle){ cx - 80, y + 30, 160, 34 }, "Cancel", true)) {
                NetShut();
                stage = LOBBY_MENU;
            }
        } else if (note[0]) {
            Centered(note, cx, y, 17, bad);
        }
        // Who is playing: their level and their K/D (kills and deaths at other players' hands).
        long xp = AccountXp();
        int kills = AccountKills(), deaths = AccountDeaths();
        float ratio = deaths > 0 ? (float)kills / (float)deaths : (float)kills;
        const char *who = TextFormat("%s   %s   K/D %d/%d (%.1f)", AccountName(), LevelText(xp), kills, deaths, ratio);
        float by = panel.y + panel.height - 40;
        DrawTextEx(GameFont(), who, (Vector2){ x, by + 5 }, 18, 1, gold);
        float frac = LevelOf(xp) >= MAX_LEVEL ? 1.0f : (float)(xp - LevelStart(LevelOf(xp))) / (float)(LevelStart(LevelOf(xp) + 1) - LevelStart(LevelOf(xp)));
        DrawRectangle((int)x, (int)by + 26, 400, 4, (Color){ 58, 48, 38, 255 });
        DrawRectangle((int)x, (int)by + 26, (int)(400 * frac), 4, gold);
        if (Choice((Rectangle){ panel.x + panel.width - 160, by, 120, 26 }, "Log out", menu)) {
            AccountLogout();
            page = PAGE_MODES;
        }
    }

    if (Choice((Rectangle){ panel.x + 14, panel.y + 12, 214, 26 }, TextFormat(L("Music: %s"), L(MusicLevelName())), menu))
        MusicSetLevel((MusicLevel() + 1) % 4);
    if (Choice((Rectangle){ panel.x + 14, panel.y + 44, 214, 26 }, TextFormat(L("Graphics: %s"), GfxLabel()), menu))
        GfxSetChoice(GfxChoice() + 1);
    if (Choice((Rectangle){ panel.x + 14, panel.y + 76, 214, 26 }, TextFormat(L("Sounds: %s"), L(SfxLevelName())), menu))
        SfxSetLevel((SfxLevel() + 1) % 4);
    const char *lang = Language() == LANG_TR ? "Dil / Language: Türkçe" : "Language / Dil: English";
    if (Choice((Rectangle){ panel.x + panel.width - 214, panel.y + 12, 200, 26 }, lang, menu)) {
        SetLanguage(Language() == LANG_TR ? LANG_EN : LANG_TR);
        SaveLanguage();
    }
    if (Choice((Rectangle){ panel.x + panel.width - 214, panel.y + 44, 200, 26 }, "Privacy", menu)) OpenPrivacy();
    if (in && page != PAGE_ACCOUNT && Choice((Rectangle){ panel.x + panel.width - 214, panel.y + 76, 200, 26 }, "Your account", menu)) {
        page = PAGE_ACCOUNT;
        deleteArmed = false;
        AccountNoteClear();
    }
    UiEnd();
    EndDrawing();
}
