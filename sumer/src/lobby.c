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
    TitleReset();
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
    DrawGameText(L(s), (float)(int)x, (float)(int)y, (float)size, c);
}

static void Centered(const char *s, float cx, float y, int size, Color c)
{
    s = L(s);
    DrawGameText(s, (float)(int)(cx - GameTextWidth(s, (float)size) / 2), (float)(int)y, (float)size, c);
}

static bool Choice(Rectangle r, const char *label, bool enabled)
{
    return TabletButton(r, label, r.height < 30 ? 15 : 20, enabled, false);
}

// A tab: lit while it is the one open.
static bool Tab(Rectangle r, const char *label, bool on)
{
    return TabletButton(r, label, 18, true, on);
}

// A frame for one of the page's own fields, laid over it: a strip of bitumen set into the clay.
static void Field(Rectangle r, bool lit)
{
    DrawRectangleRounded((Rectangle){ r.x - 1, r.y - 1, r.width + 2, r.height + 3 }, 0.12f, 4, (Color){ 120, 90, 62, 120 });
    DrawRectangleRounded(r, 0.12f, 4, (Color){ 52, 38, 28, 255 });
    DrawRectangleRoundedLinesEx(r, 0.12f, 4, lit ? 2.0f : 1.0f, lit ? (Color){ 34, 70, 156, 255 } : (Color){ 120, 96, 72, 255 });
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
        Label(RoomLine(mode, players, max, state), x + 270, y + 10, 16, (Color){ 96, 66, 44, 255 });
        y += 44;
        shown++;
    }
    if (!shown) {
        Label(RoomCount() ? "No servers of this kind are set up." : "Asking the game server for its servers...", x, y + 8, 16, (Color){ 96, 66, 44, 255 });
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
    // Dark ink on the clay tablet: lapis for what stands out, red for what went wrong.
    const Color gold = { 28, 62, 146, 255 }, text = { 40, 25, 16, 255 }, dim = { 96, 66, 44, 255 }, bad = { 156, 34, 20, 255 };
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
    // Behind the title: the city itself at dusk (TitleBackdrop), or a painted one.
    bool world = TitleBackdrop(GetFrameTime());
    if (!world) ClearBackground((Color){ 30, 24, 20, 255 });
    UiBegin();
    if (!world) {
        DrawRectangleGradientV(0, 0, (int)sw, (int)(sh * 0.6f), (Color){ 70, 96, 140, 255 }, (Color){ 226, 156, 96, 255 });
        DrawRectangle(0, (int)(sh * 0.6f), (int)sw, (int)(sh * 0.4f), (Color){ 150, 116, 80, 255 });
        // The ziggurat on the horizon: three stages of mud brick against the evening.
        float base = sh * 0.6f;
        DrawRectangle((int)(cx - 210), (int)(base - 60), 420, 60, (Color){ 120, 84, 58, 255 });
        DrawRectangle((int)(cx - 140), (int)(base - 105), 280, 45, (Color){ 132, 92, 62, 255 });
        DrawRectangle((int)(cx - 80), (int)(base - 140), 160, 35, (Color){ 142, 100, 68, 255 });
        DrawRectangle((int)(cx - 9), (int)(base - 60), 18, 60, (Color){ 100, 70, 48, 255 });
    }

    Rectangle panel = { cx - 360, 0, 720, 530 };
    panel.y = sh > panel.height + 16 ? (sh - panel.height) / 2 : 8;
    DrawTablet(panel);
    DrawLogo(cx, panel.y + 20, 50);
    bool in = LoggedIn();
    Centered(!in ? "Ur, c. 2100 BC" : page == PAGE_ROYALE ? "Sumer Royale" : page == PAGE_PARTY ? "Sumer Party" : page == PAGE_SURVIVAL ? "Sumer Survival" : "Ur, c. 2100 BC",
             cx, panel.y + 78, 18, in && (page == PAGE_ROYALE || page == PAGE_PARTY) ? gold : dim);

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
            Field(fields[k], !busy);
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
                 cx, panel.y + panel.height - 40, 16, dim);
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
                Field(fields[k], !AccountWaiting());
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
            Centered("Deleting needs your current password and cannot be undone.", cx, y, 16, dim);
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
            Label("Alone or with others: farm, hunt, trade", x + 270, y + 4, 16, dim);
            Label("and get through the seasons in Ur.", x + 270, y + 24, 16, dim);
            y += 58;
            if (Choice((Rectangle){ x, y, 250, 44 }, "Sumer Royale", menu)) page = PAGE_ROYALE;
            Label("Battle royale for up to twenty players:", x + 270, y + 4, 16, dim);
            Label("the last one standing wins.", x + 270, y + 24, 16, dim);
            y += 58;
            if (Choice((Rectangle){ x, y, 250, 44 }, "Sumer Party", menu)) page = PAGE_PARTY;
            Label("Four minigames in a row for up to twelve:", x + 270, y + 4, 16, dim);
            Label("points in each, the best three win.", x + 270, y + 24, 16, dim);
            y += 70;
        } else if (page == PAGE_SURVIVAL) {
            AccountRooms();
            bool ready = AccountSavesReady();
            if (Choice(row, "Singleplayer", menu && ready)) PlayAlone();
            Label(ready ? "Three save slots, kept with your account." : "Fetching your saves...", x + 270, y + 14, 16, dim);
            y += 56;
            Label("Servers: the world goes on while you are away; your things come with you.", x, y, 16, text);
            y += 24;
            y = RoomRows(x, y, ROOM_SURVIVAL, menu);
            if (Choice((Rectangle){ x, y + 4, 120, 26 }, "Back", menu)) page = PAGE_MODES;
            y += 44;
        } else if (page == PAGE_PARTY) {
            // A party: four of five minigames, once everyone in (two or more) is ready.
            AccountRooms();
            Label("Four minigames of four minutes, chosen from five (vote one out while you wait). Points", x, y, 16, text);
            Label("in each game, +10, +6, +3 for its top three; the party's best three gain experience.", x, y + 20, 16, text);
            y += 50;
            y = RoomRows(x, y, ROOM_PARTY, menu);
            if (Choice((Rectangle){ x, y + 4, 120, 26 }, "Back", menu)) page = PAGE_MODES;
            y += 44;
        } else {
            // A battle royale: a round starts once everyone in (two or more) is ready.
            AccountRooms();
            Label("A round starts ten seconds after everyone in (two or more) is ready. Everyone", x, y, 16, text);
            Label("starts with the best gear; the winner gains 10 XP for every player in the round.", x, y + 20, 16, text);
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
        DrawGameText(who, x, by + 5, 18, gold);
        float frac = LevelOf(xp) >= MAX_LEVEL ? 1.0f : (float)(xp - LevelStart(LevelOf(xp))) / (float)(LevelStart(LevelOf(xp) + 1) - LevelStart(LevelOf(xp)));
        DrawRectangleRounded((Rectangle){ x, by + 26, 400, 5 }, 0.5f, 4, (Color){ 120, 92, 64, 140 });
        DrawRectangleRounded((Rectangle){ x, by + 26, 400 * frac, 5 }, 0.5f, 4, gold);
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
