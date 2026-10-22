// The browser's title screen: the player's name, then play alone, host a game or join one. The room
// comes from the page address (?room=...), so friends on one server can run separate games. For tests
// and links, ?auto=solo|host|join&name=... starts at once.
#include "game.h"
#include <stdio.h>
#include <string.h>

#define NAME_FILE "sumer.name"

static enum { LOBBY_MENU, LOBBY_CONNECT } stage;
static bool open, loaded, autoDone, confirmNew, pvp = true, combat, dev;
static char name[NAME_LEN], room[32], note[200];
static float connectT;

static bool NameChar(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c && strchr(" -_.'", c));
}

static void Keep(char *dst, int size, const char *src)
{
    int n = 0;
    for (const char *c = src; *c && n < size - 1; c++)
        if (NameChar(*c)) dst[n++] = *c;
    dst[n] = '\0';
}

static void LoadName(void)
{
    char buf[64] = "";
    FILE *f = fopen(DataPath(NAME_FILE), "r");
    if (f) {
        if (!fgets(buf, sizeof(buf), f)) buf[0] = '\0';
        fclose(f);
    }
    buf[strcspn(buf, "\r\n")] = '\0';
    Keep(name, NAME_LEN, buf);
}

static void SaveName(void)
{
    FILE *f = fopen(DataPath(NAME_FILE), "w");
    if (!f) return;
    fprintf(f, "%s\n", name);
    fclose(f);
    SyncStorage();
}

const char *PlayerName(void)
{
    return name[0] ? name : "Player";
}

void OpenLobby(const char *why)
{
    if (!loaded) {
        loaded = true;
        LoadName();
        Keep(room, sizeof(room), PageParam("room"));
        if (!room[0]) snprintf(room, sizeof(room), "sumer");
        char given[NAME_LEN];
        Keep(given, NAME_LEN, PageParam("name"));
        if (given[0]) snprintf(name, sizeof(name), "%s", given);
        if (strcmp(PageParam("pvp"), "0") == 0) pvp = false;
        if (strcmp(PageParam("combat"), "1") == 0) combat = true;
        if (strcmp(PageParam("dev"), "1") == 0) dev = true;
    }
    open = true;
    stage = LOBBY_MENU;
    snprintf(note, sizeof(note), "%s", why && why[0] ? L(why) : "");
    EnableCursor();
    WantPointer(false);
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
    SetPanel(StyleChosen() || PageParam("auto")[0] ? PANEL_NONE : PANEL_LOOK);
    G.wasCaptured = false;
}

static void PlayAlone(void);

#ifdef SUMER_TEST
// For tests: back at the title, start again the way the page's address says (?auto=solo plays alone).
void LobbyAgain(void)
{
    autoDone = false;
}
#endif

static void PlayAlone(void)
{
    SaveName();
    SetSaveSlot(LastSlot());
    GameInit();
    if (LoadGame()) {
        Say("Welcome back. Day %d. Slot %d was loaded.", G.day, SaveSlot() + 1);
        G.msgT = 6.0f;
    }
    G.devTools = dev;
    Enter();
}

static void Connect(bool host)
{
    SaveName();
    note[0] = '\0';
    connectT = 0;
    NetRules(pvp, combat, dev);
    if (NetBegin(host, PlayerName(), room)) stage = LOBBY_CONNECT;
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

static void TypeName(void)
{
    int c, n = (int)strlen(name);
    while ((c = GetCharPressed()) > 0)
        if (NameChar(c) && n < NAME_LEN - 1 && !(c == ' ' && n == 0)) {
            name[n++] = (char)c;
            name[n] = '\0';
        }
    if ((KeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && n > 0) name[n - 1] = '\0';
}

void LobbyFrame(void)
{
    const Color gold = { 240, 200, 120, 255 }, text = { 240, 230, 210, 255 }, dim = { 150, 140, 125, 255 }, bad = { 230, 110, 90, 255 };
    float sw = UiWidth(), sh = UiHeight(), cx = sw / 2;

    if (!autoDone) {
        autoDone = true;
        const char *mode = PageParam("auto");
        if (strcmp(mode, "solo") == 0) PlayAlone();
        else if (strcmp(mode, "host") == 0) Connect(true);
        else if (strcmp(mode, "join") == 0) Connect(false);
        if (!open) return;
    }
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
    } else {
        TypeName();
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
    Centered("Ur, c. 2100 BC", cx, panel.y + 74, 20, text);

    float x = panel.x + 40, y = panel.y + 120;
    Label("Your name", x, y + 8, 20, text);
    Rectangle field = { x + 150, y, 330, 38 };
    DrawRectangleRec(field, (Color){ 58, 48, 38, 255 });
    DrawRectangleLinesEx(field, 1, stage == LOBBY_MENU ? gold : dim);
    DrawTextEx(GameFont(), name, (Vector2){ field.x + 10, field.y + 9 }, 20, 2, text);
    if (stage == LOBBY_MENU && ((int)(GetTime() * 2.0) & 1)) {
        Vector2 m = MeasureTextEx(GameFont(), name, 20, 2);
        DrawRectangle((int)(field.x + 12 + m.x), (int)field.y + 8, 2, 22, text);
    }
    y += 50;
    Label(TextFormat(L("Room: %s   (add ?room=a-name to the address for a game of your own)"), room), x, y, 15, dim);
    y += 40;

    bool menu = stage == LOBBY_MENU, named = name[0] != '\0';
    if (Choice((Rectangle){ x, y, 250, 46 }, "Play alone", menu)) PlayAlone();
    Label("Kept in this browser, in three save slots.", x + 270, y + 14, 15, dim);
    y += 62;
    if (Choice((Rectangle){ x, y, 250, 46 }, "Host a game", menu && named)) Connect(true);
    int day;
    float hour;
    Label("Up to five friends join from this same address.", x + 270, y + 4, 15, dim);
    if (NetSaveSummary(&day, &hour)) {
        // The hosted world carries on until its host starts a new one (two clicks).
        Label(TextFormat(L("Your hosted world: day %d."), day), x + 270, y + 26, 15, dim);
        if (Choice((Rectangle){ panel.x + panel.width - 160, y + 22, 120, 24 }, confirmNew ? "Erase it?" : "New world", menu)) {
            if (confirmNew) {
                DeleteNetGame();
                snprintf(note, sizeof(note), "%s", L("The hosted world is erased. Host a game to begin a new one."));
            }
            confirmNew = !confirmNew;
        }
    }
    y += 62;
    // The rules of the game this player hosts; whoever joins plays by them.
    Label("Rules when you host:", x, y + 6, 15, dim);
    if (Choice((Rectangle){ x + 200, y, 190, 28 }, pvp ? "PvP: on" : "PvP: off", menu)) pvp = !pvp;
    if (Choice((Rectangle){ x + 400, y, 240, 28 }, pvp && combat ? "Combat mode: on" : "Combat mode: off", menu && pvp)) combat = !combat;
    y += 34;
    // Dev tools go for playing alone too: the dev console (Tab, then its button, or `).
    if (Choice((Rectangle){ x + 200, y, 190, 28 }, dev ? "Dev tools: on" : "Dev tools: off", menu)) dev = !dev;
    Label("The dev console, alone or hosting.", x + 400, y + 6, 13, dim);
    y += 40;
    if (Choice((Rectangle){ x, y, 250, 46 }, "Join a game", menu && named)) Connect(false);
    Label("Joins the game hosted in this room.", x + 270, y + 14, 15, dim);
    y += 70;

    if (stage == LOBBY_CONNECT) {
        Centered(connectT < 8.0f ? "Connecting..." : "Still connecting. Is server.py running?", cx, y, 20, gold);
        if (Choice((Rectangle){ cx - 80, y + 34, 160, 38 }, "Cancel", true)) {
            NetShut();
            stage = LOBBY_MENU;
        }
    } else if (note[0]) {
        Centered(note, cx, y, 17, bad);
    } else if (!named) {
        Centered("Type a name to host or join a game.", cx, y, 17, dim);
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
    UiEnd();
    EndDrawing();
}
