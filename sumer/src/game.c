#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define FOOD_IDLE      0.8f
#define FOOD_WALK      1.2f
#define FOOD_RUN       3.0f
#define FOOD_SLEEP     0.5f
#define REGEN_FOOD     20.0f
#define AUTOSAVE_EVERY 120.0f

Game G;

static float bedCool;

void GameInit(void)
{
    memset(&G, 0, sizeof(G));
    SetRandomSeed(2100);

    G.player = (Player){ .pos = { 11, 0, 22 }, .facing = PI, .pitch = 0.35f, .dist = 9, .grounded = true };
    G.cam = (Camera3D){ .up = { 0, 1, 0 }, .fovy = 55, .projection = CAMERA_PERSPECTIVE };
    G.camDist = G.player.dist;
    G.bagSlots = BAG_START;
    G.quiverMax = QUIVER_START;
    G.quiver = QUIVER_START;
    G.food = 80.0f;
    G.health = HEALTH_MAX;
    G.followArrow = -1;
    G.arrowCam = true;
    G.torchOn = true;
    LoadGamePrefs();
    G.day = 1;
    G.dawnDay = 1;
    G.hour = 7.0f;
    G.equip = EQ_BOW;
    G.farmWater = WATER_DAYS;
    G.coopWater = WATER_DAYS;
    G.peers[0].active = true;
    snprintf(G.peers[0].name, NAME_LEN, "%s", PlayerName());
    SyncSelf();

    BuildWorld();
    BuildCaravans();
    // The world comes from a fixed seed; animals and weather differ every game.
    SetRandomSeed((unsigned int)time(NULL));
    InitWildlife();
    NewDay();

    BagAdd(IT_DATES, 4);
    SlotsAdd(G.store, STORE_SLOTS, IT_BUCKET, 1);
    SlotsAdd(G.store, STORE_SLOTS, IT_ARROWS, 20);
    SlotsAdd(G.store, STORE_SLOTS, IT_SEEDS, 6);
    SlotsAdd(G.store, STORE_SLOTS, IT_DATES, 6);
    SlotsAdd(G.store, STORE_SLOTS, IT_WOOD, 4);

    UpdateSky();
    FollowCamera(1.0f);
    Say("Ur, c. 2100 BC. Home: blue lintel. Market: inside the ziggurat. Press H for controls.");
    G.msgT = 8.0f;
}

// Alone, the night passes at once. In company it passes when every player lies in bed.
void StartSleep(void)
{
    if (Online()) {
        G.waitBed = true;
        G.player.crouching = G.player.aiming = false;
        G.player.draw = 0;
        Say("You lie down. The night passes once every player is in bed. E gets you up.");
        return;
    }
    G.sleeping = true;
    G.slept = false;
    G.sleepT = 0;
}

// Personal timers that run with the world's hours: a blessing wears off.
void PassHours(float h)
{
    if (G.blessT > 0) G.blessT = fmaxf(0.0f, G.blessT - h);
}

static void UpdateSleep(float dt)
{
    if (G.waitBed && G.panel == PANEL_NONE &&
        (KeyPressed(KEY_E) || KeyPressed(KEY_SPACE) || IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D))) {
        G.waitBed = false;
        Say("You get up.");
    }
    if (!G.sleeping) return;

    G.sleepT += dt;
    if (!G.slept && G.sleepT >= 1.1f) {
        bool night = G.hour >= 19.0f || G.hour < 5.0f;
        float target = night ? 6.0f : 19.0f;
        float hours = target - G.hour;
        bool rained;

        if (hours <= 0) {
            hours += 24.0f;
            rained = RainBefore(24.0f);
            G.day++;
        } else {
            rained = RainBefore(target);
        }
        G.hour = target;
        AdvanceHours(hours);
        PassHours(hours);
        G.food = fmaxf(0.0f, G.food - hours * FOOD_SLEEP);
        G.health = HEALTH_MAX;
        bool saved = SaveGame();
        Say("%s%s%s", L(night ? "You slept until dawn." : "You rested until dusk."),
            rained ? L(" It rained meanwhile; the farm and the coop are full of water.") : "", saved ? L(" Game saved.") : "");
        G.slept = true;
    }
    if (G.sleepT >= 2.2f) G.sleeping = false;
}

// The host lets the night pass once every player lies in bed, and tells everyone.
static void UpdateBeds(float dt)
{
    if (bedCool > 0) bedCool -= dt;
    if (G.role != NET_HOST || G.sleeping || bedCool > 0) return;
    int beds = PlayersInBed();
    if (beds == 0 || beds < PlayerCount()) return;

    bool dawn = G.hour >= 19.0f || G.hour < 5.0f;
    float target = dawn ? 6.0f : 19.0f, hours = target - G.hour;
    bool rained;
    if (hours <= 0) {
        hours += 24.0f;
        rained = RainBefore(24.0f);
        G.day++;
    } else {
        rained = RainBefore(target);
    }
    G.hour = target;
    AdvanceHours(hours);
    bedCool = 5.0f;
    NetSkip(hours, rained, dawn);
    WakeAfterNight(hours, rained, dawn);
    // The guests hand in their things on waking; the save after takes them in too.
    NetSaveSoon();
}

// Every player wakes when the host says the night has passed.
void WakeAfterNight(float hours, bool rained, bool dawn)
{
    G.waitBed = false;
    G.food = fmaxf(0.0f, G.food - hours * FOOD_SLEEP);
    G.health = HEALTH_MAX;
    PassHours(hours);
    G.sleeping = true;
    G.slept = true;
    G.sleepT = 0.6f;
    bool saved = G.role == NET_HOST && SaveNetGame();
    if (G.role == NET_CLIENT) NetSendRecord();
    Say("%s%s%s", L(dawn ? "You slept until dawn." : "You rested until dusk."),
        rained ? L(" It rained meanwhile; the farm and the coop are full of water.") : "", saved ? L(" Game saved.") : "");
}

// The baskets as the day began are the player's own; whatever they gained since (gathered, bought,
// found) is at risk until it is put away in their storage at home. Taking something out of storage
// makes it theirs again (see the storage panel). Eating or spending uses up today's gains first.
void TrackGoods(void)
{
    for (int i = 1; i < IT_COUNT; i++) {
        int have = BagCount(i);
        G.safe[i] = G.safeDay != G.day ? have : G.safe[i] < have ? G.safe[i] : have;
    }
    G.safeDay = G.day;
}

// What brought the player down, in a PvP game: nobody carries them home, they come to somewhere in the city.
static const char *BroughtDownInCity(int by)
{
    const char *what;
    switch (by) {
    case BY_GUARD: what = L("The guards beat you down."); break;
    case BY_CARAVAN: what = L("The caravan guards beat you down."); break;
    case BY_TOWNSFOLK: what = L("The townsfolk beat you down."); break;
    case BY_VILLAGERS: what = L("The villagers beat you down."); break;
    case BY_RAIDER: what = L("Raiders left you for dead."); break;
    case BY_BARBARIAN: what = L("Barbarians left you for dead."); break;
    case BY_BANDIT: what = L("Bandits left you for dead."); break;
    case BY_WOLF: what = L("Wolves brought you down."); break;
    case BY_BOAR: what = L("A boar brought you down."); break;
    case BY_SNAKE: what = L("A snake's venom brought you down."); break;
    default:
        if (by >= BY_PLAYER && by < BY_PLAYER + MAX_PLAYERS && G.peers[by - BY_PLAYER].name[0])
            what = TextFormat(L("%s beat you down."), G.peers[by - BY_PLAYER].name);
        else what = L("You were brought down.");
    }
    return TextFormat("%s %s", what, L("You came to somewhere else in the city."));
}

// What brought the player down, in a sentence.
static const char *BroughtDown(int by)
{
    if (G.pvp) return BroughtDownInCity(by);
    switch (by) {
    case BY_GUARD: return L("The guards beat you down and dumped you at your door.");
    case BY_CARAVAN: return L("The caravan guards beat you down. A shepherd carried you home.");
    case BY_TOWNSFOLK: return L("The townsfolk beat you down and left you at your door.");
    case BY_VILLAGERS: return L("The villagers beat you down. A shepherd carried you home.");
    case BY_RAIDER: return L("Raiders left you for dead. A shepherd carried you home.");
    case BY_BARBARIAN: return L("Barbarians left you for dead. A shepherd carried you home.");
    case BY_BANDIT: return L("Bandits left you for dead. A shepherd carried you home.");
    case BY_WOLF: return L("Wolves brought you down. A shepherd carried you home.");
    case BY_BOAR: return L("A boar brought you down. A shepherd carried you home.");
    case BY_SNAKE: return L("A snake's venom brought you down. A shepherd carried you home.");
    default:
        if (by >= BY_PLAYER && by < BY_PLAYER + MAX_PLAYERS && G.peers[by - BY_PLAYER].name[0])
            return TextFormat(L("%s beat you down. You came to at home."), G.peers[by - BY_PLAYER].name);
        return L("You were brought down. A shepherd carried you home.");
    }
}

// Alone, the player wakes at home the next morning. In company the world's clock is everyone's, so
// they come to at home at once, and whatever was after them gives up. Carcasses and everything
// gained today that was not put away in storage are lost.
static void Collapse(void)
{
    Player *p = &G.player;
    float hours = (G.hour < 7.0f) ? 7.0f - G.hour : 31.0f - G.hour;
    char how[200], lost[200] = "";

    snprintf(how, sizeof(how), "%s", BroughtDown(G.hurtBy));
    TrackGoods();
    for (int i = 1; i < IT_COUNT; i++) {
        bool carcass = i >= IT_GAZELLE && i <= IT_GOOSE;
        int gone = BagCount(i) - (carcass ? 0 : G.safe[i]);
        if (gone <= 0) continue;
        BagRemove(i, gone);
        if (strlen(lost) < sizeof(lost) - 40)
            snprintf(lost + strlen(lost), sizeof(lost) - strlen(lost), "%s%d %s", lost[0] ? ", " : "", gone, ItemName(i));
    }
    G.climb = 0;
    G.waitBed = false;
    int by = G.hurtBy;
    G.hurtBy = BY_NONE;
    Act(RQ_COLLAPSE, &by, 1);
    if (!Online()) {
        CalmBeasts(-1);
        for (int i = 0; i < MAX_RAIDERS; i++) G.raiders[i].vit.hostile = 0;
        if (G.hour >= 7.0f) {
            RainBefore(24.0f);
            G.day++;
        }
        G.hour = 7.0f;
        AdvanceHours(hours);
        PassHours(hours);
        RespawnPeople();
        if (G.dawnDay != G.day) {
            G.dawnDay = G.day;
            NewDay();
        }
    }

    G.health = (G.combat ? 1.0f : 0.3f) * HEALTH_MAX;
    if (G.pvp) {
        p->pos = CitySpawn(G.self);
    } else {
        p->pos = Vector3Add(G.stations[STN_BED], (Vector3){ -1.6f, 0, 0.5f });
    }
    p->pos.y = GroundHeight(p->pos.x, p->pos.z, p->pos.y + 0.5f);
    p->vy = 0;
    p->crouching = p->aiming = false;
    p->draw = 0;
    if (G.panel != PANEL_NONE) SetPanel(PANEL_NONE);
    G.sleeping = true;
    G.slept = true;
    G.sleepT = 1.1f;
    if (lost[0]) Say("%s %s %s.", how, L("You lost what you had not stored:"), lost);
    else Say("%s", how);
    G.msgT = 6.0f;
}

// Where a blow came from: red lights on the edge of the screen (the top is ahead of you, the bottom behind,
// then left and right as you look) and a flinch of the body, away from whoever struck.
static void NoteHit(float dmg, const Vector3 *src)
{
    const Player *p = &G.player;
    Vector2 away = { -sinf(p->facing), -cosf(p->facing) };   // without a known source, as if struck from the front

    G.flinchT = FLINCH_TIME;
    if (src && XZDist(*src, p->pos) > 0.2f) {
        Vector2 to = Vector2Normalize((Vector2){ src->x - p->pos.x, src->z - p->pos.z });
        Vector2 fwd = { -sinf(p->yaw), -cosf(p->yaw) }, right = { cosf(p->yaw), -sinf(p->yaw) };
        float f = to.x * fwd.x + to.y * fwd.y, r = to.x * right.x + to.y * right.y;
        float power = Clamp(0.6f + dmg / 40.0f, 0.6f, 1.0f);
        G.hitLight[0] = fmaxf(G.hitLight[0], fmaxf(f, 0.0f) * power);
        G.hitLight[1] = fmaxf(G.hitLight[1], fmaxf(-f, 0.0f) * power);
        G.hitLight[2] = fmaxf(G.hitLight[2], fmaxf(-r, 0.0f) * power);
        G.hitLight[3] = fmaxf(G.hitLight[3], fmaxf(r, 0.0f) * power);
        away = (Vector2){ -to.x, -to.y };
    }
    G.flinchX = away.x;
    G.flinchZ = away.y;
}

// The game settings each player keeps for themselves (sumer.prefs): the arrow camera and the first-person view.
void SaveGamePrefs(void)
{
    FILE *f = fopen(DataPath("sumer.prefs"), "w");
    if (!f) return;
    fprintf(f, "arrowcam %d\nfirstperson %d\n", G.arrowCam ? 1 : 0, G.firstPerson ? 1 : 0);
    fclose(f);
    SyncStorage();
}

void LoadGamePrefs(void)
{
    char key[32];
    int v;
    FILE *f = fopen(DataPath("sumer.prefs"), "r");
    if (!f) return;
    while (fscanf(f, "%31s %d", key, &v) == 2) {
        if (strcmp(key, "arrowcam") == 0) G.arrowCam = v != 0;
        else if (strcmp(key, "firstperson") == 0) G.firstPerson = v != 0;
    }
    fclose(f);
}

void HurtPlayer(float dmg, int by)
{
    HurtPlayerFrom(dmg, by, NULL);
}

void HurtPlayerFrom(float dmg, int by, const Vector3 *src)
{
    if (G.sleeping || G.god) return;
    // This happens to this machine's player, whoever struck: what it says is for them.
    int actor = G.actor;
    G.actor = G.self;
    G.waitBed = false;
    G.health -= dmg * (1.0f - ArmorReduction(UseArmor()));
    G.hurtT = 0.45f;
    NoteHit(dmg, src);
    BlowTaken();
    G.safeT = 0;
    G.hurtBy = by;
    if (G.panel == PANEL_BAG || G.panel == PANEL_STORAGE || G.panel == PANEL_TRADE) SetPanel(PANEL_NONE);
    if (G.health <= 0) Collapse();
    G.actor = actor;
}

static void UpdateDawn(void)
{
    if (G.hour >= 6.0f && G.hour < 12.0f && G.dawnDay != G.day) {
        G.dawnDay = G.day;
        RespawnPeople();
        NewDay();
    }
}

// A guest's dawn: the temple gate closes to them again.
static void GuestDawn(void)
{
    if (G.hour >= 6.0f && G.hour < 12.0f && G.dawnDay != G.day) {
        G.dawnDay = G.day;
        G.templeAccess = false;
    }
}

// The world's clock runs where the world runs; a guest's comes with the host's pictures.
static void UpdateTime(float h)
{
    G.hour += h;
    if (G.hour >= 24.0f) {
        G.hour -= 24.0f;
        G.day++;
    }
    AdvanceHours(h);
}

// Hunger, healing and herbs, on every player's own machine.
static void UpdateBody(float dt, float h)
{
    const Player *p = &G.player;
    float rate = G.waitBed ? FOOD_SLEEP : p->running ? FOOD_RUN : p->moving ? FOOD_WALK : FOOD_IDLE;

    G.food = fmaxf(0.0f, G.food - rate * h);
    G.hungerNag -= dt;
    if (G.food < 15.0f && G.hungerNag <= 0) {
        if (G.food <= 0) Say("You are starving and can't run. Eat something (Tab).");
        else Say("You are getting hungry. Eat from your baskets (Tab).");
        G.hungerNag = 40.0f;
    }

    G.safeT += dt;
    if (G.safeT > 5.0f && G.food >= REGEN_FOOD) G.health = fminf(HEALTH_MAX, G.health + 1.5f * dt);
    // Chewed herbs keep healing for a while, even in a fight and on an empty stomach.
    if (G.herbT > 0) {
        G.herbT = fmaxf(0.0f, G.herbT - dt);
        G.health = fminf(HEALTH_MAX, G.health + HERB_RATE * dt);
    }
    PassHours(h);
}

// Alone, holding T runs time forty times as fast and sleep stops the clock. In company time is shared.
static void UpdateClock(float dt)
{
    if (Online()) {
        float h = dt / HOUR_SECONDS;
        if (!IsClient()) UpdateTime(h);
        if (!G.sleeping) UpdateBody(dt, h);
        return;
    }
    if (G.sleeping) return;
    float scale = (G.panel == PANEL_NONE && IsKeyDown(KEY_T)) ? 40.0f : 1.0f;
    float h = dt * scale / HOUR_SECONDS;
    UpdateTime(h);
    UpdateBody(dt, h);
}

void SaveNow(void)
{
    if (G.role == NET_HOST) {
        if (SaveNetGame()) Say("Game saved in this browser, with every player's things.");
        else Say("Could not save the game in this browser.");
    } else if (G.role == NET_CLIENT) {
        NetSendRecord();
        Say("The host keeps your things; they are sent along every few seconds.");
    } else if (SaveGame()) {
        Say("Game saved to %s.", SaveFileName(SaveSlot()));
    } else {
        Say("Could not write %s in this folder.", SaveFileName(SaveSlot()));
    }
}

// Screenshot tools and app switches steal focus and the captured mouse; pause so resuming recaptures it.
// In a browser Esc releases the mouse, so losing it pauses too. True when this frame paused the game.
static bool HandleWindow(void)
{
    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if (KeyPressed(KEY_F11) || (alt && KeyPressed(KEY_ENTER))) ToggleBorderlessWindowed();

    bool focused = IsWindowFocused(), paused = false;
    if (!focused && G.wasFocused && G.panel == PANEL_NONE) {
        SetPanel(PANEL_PAUSE);
        paused = true;
    }
    G.wasFocused = focused;

    if (!OnWeb()) return paused;
    bool captured = PointerCaptured();
    if (G.wasCaptured && !captured && G.panel == PANEL_NONE) {
        SetPanel(PANEL_PAUSE);
        paused = true;
        // The Esc that released the mouse can reach the game a frame later; it must not close the menu again.
        G.escQuiet = 0.3f;
    }
    G.wasCaptured = captured;
    // The click that takes the mouse back is not a shot.
    if (!captured) G.ignoreClick = true;
    return paused;
}

// 'paused': the Esc that released the mouse already opened the pause menu; it does not close it again.
static void HandleMenus(bool paused)
{
    if (G.escQuiet > 0) G.escQuiet -= GetFrameTime();
    if (KeyPressed(KEY_ESCAPE) && !paused && G.escQuiet <= 0) {
        SetPanel(G.panel == PANEL_NONE ? PANEL_PAUSE : PANEL_NONE);
    } else if (KeyPressed(KEY_H) && (G.panel == PANEL_NONE || G.panel == PANEL_PAUSE || G.panel == PANEL_HELP)) {
        SetPanel(G.panel == PANEL_HELP ? PANEL_NONE : PANEL_HELP);
    } else if (G.devTools && (KeyPressed(KEY_GRAVE) || KeyPressed(KEY_F2))) {
        if (G.panel == PANEL_NONE || G.panel == PANEL_BAG) SetPanel(PANEL_CONSOLE);
        else if (G.panel == PANEL_CONSOLE) SetPanel(PANEL_NONE);
    } else if (KeyPressed(KEY_TAB) || (KeyPressed(KEY_I) && G.panel != PANEL_CONSOLE)) {
        if (G.panel == PANEL_NONE) SetPanel(PANEL_BAG);
        else if (G.panel != PANEL_PAUSE && G.panel != PANEL_HELP && G.panel != PANEL_SETTINGS) SetPanel(PANEL_NONE);
    }
    if (G.panel == PANEL_PAUSE && KeyPressed(KEY_Q)) G.quit = true;
    if (KeyPressed(KEY_F5) && !G.sleeping) SaveNow();
    if (G.panel == PANEL_TRADE && G.traders[G.trader].away) {
        SetPanel(PANEL_NONE);
        Say("The %s is packing up to leave.", L(G.traders[G.trader].name));
    } else if (G.panel == PANEL_TRADE && !TraderWilling(G.trader)) {
        SetPanel(PANEL_NONE);
        Say("The %s won't trade with you now.", L(G.traders[G.trader].name));
    }
    if (G.panel == PANEL_NONE && !G.cursorLocked && MousePressed(MOUSE_BUTTON_LEFT)) {
        DisableCursor();
        WantPointer(true);
        G.cursorLocked = true;
        G.ignoreClick = true;
    }
}

// The game in play is saved first, so switching never loses progress.
static void SwitchSlot(int slot)
{
    SaveGame();
    SetSaveSlot(slot);
    RememberSlot();
    GameInit();
    if (LoadGame()) {
        Say("Slot %d loaded. Day %d.", slot + 1, G.day);
    } else {
        SaveGame();
        Say("Slot %d: a new game begins. Ur, c. 2100 BC.", slot + 1);
    }
}

// The world: everything when playing alone or hosting. A guest only eases the host's picture of it
// (in NetFrame), lets the livestock wander and keeps its own player.
static void UpdateWorld(float dt)
{
    UpdateWorkers();
    if (!IsClient()) UpdateCaravans(dt);
    UpdateVillage(dt);
    if (IsClient()) {
        CollectFighters();
    } else {
        UpdateRaiders(dt);
        UpdateCombat(dt);
        UpdatePlayerSafety(dt);
        UpdateWildlife(dt);
    }
    UpdateArrows(dt);
    UpdateEffects(dt);
    UpdateHitFeel(dt);
}

void GameUpdate(void)
{
    float dt = fminf(GetFrameTime(), 0.05f);
    bool online = Online();

    MusicUpdate(dt, true);
    if (!online && G.requestNew) {
        DeleteSave();
        GameInit();
        SetPanel(PANEL_NONE);
        Say("A new game begins in slot %d. Ur, c. 2100 BC.", SaveSlot() + 1);
        return;
    }
    if (!online && G.requestSlot) {
        SwitchSlot(G.requestSlot - 1);
        SetPanel(PANEL_NONE);
        return;
    }
    G.requestNew = false;
    G.requestSlot = 0;
    HandleMenus(HandleWindow());
    // Alone, the world waits while a menu is open; in company it goes on.
    bool menu = G.panel == PANEL_PAUSE || G.panel == PANEL_HELP || G.panel == PANEL_LOOK || G.panel == PANEL_SETTINGS;
    if (menu && !online) {
        if (G.panel == PANEL_LOOK) FollowCamera(dt);
        return;
    }

    bool play = G.panel == PANEL_NONE && !G.sleeping && !G.waitBed;
    CollectBodies();
    UpdatePlayer(dt, play && !menu);
    TrackGoods();
    SyncSelf();
    FollowCamera(dt);
    UpdateWorld(dt);
    UpdateClock(dt);
    UpdateSleep(dt);
    if (IsClient()) {
        GuestDawn();
    } else {
        UpdateDawn();
        UpdateWeather();
        UpdateBeds(dt);
    }
    UpdateFields();
    UpdateSky();
    UpdateLights();
    if (online) {
        NetFrame(dt);
        if (InLobby()) return;
        UpdatePeers(dt);
    }
    FindTarget(play);

    if (play) {
        if (KeyPressed(KEY_E)) Interact(false);
        if (KeyPressed(KEY_F)) Interact(true);
        if (KeyPressed(KEY_R)) RefillQuiver();
        if (KeyPressed(KEY_ONE)) SetEquip(EQ_BOW);
        if (KeyPressed(KEY_TWO)) SetEquip(EQ_MELEE);
        if (KeyPressed(KEY_THREE)) SetEquip(EQ_MACE);
        if (KeyPressed(KEY_FOUR)) SetEquip(EQ_NONE);
        if (KeyPressed(KEY_M)) G.mapZoom = (G.mapZoom + 1) % 3;
        if (KeyPressed(KEY_N)) {
            static const char *const says[4] = { "Music off.", "Music low.", "Music medium.", "Music high." };
            MusicSetLevel((MusicLevel() + 1) % 4);
            Say("%s", L(says[MusicLevel()]));
        }
        if (KeyPressed(KEY_G)) {
            GfxSetChoice(GfxChoice() + 1);
            Say("Graphics: %s.", GfxLabel());
        }
        if (KeyPressed(KEY_Z)) {
            G.arrowCam = !G.arrowCam;
            SaveGamePrefs();
            Say(G.arrowCam ? "Arrow camera on: the view follows each shot (Z to turn off)." : "Arrow camera off.");
        }
    }
    if (G.msgT > 0) G.msgT -= dt;
    if (G.hurtT > 0) G.hurtT -= dt;
    if (G.flinchT > 0) G.flinchT -= dt;
    for (int k = 0; k < 4; k++) G.hitLight[k] = fmaxf(0.0f, G.hitLight[k] - dt * 1.1f);

    // A browser tab can be closed at any moment: a game played alone there is saved every two minutes.
    if (OnWeb() && !online) {
        G.autosaveT += dt;
        if (G.autosaveT >= AUTOSAVE_EVERY && !G.sleeping) {
            G.autosaveT = 0;
            SaveGame();
        }
    }
}

void GameDraw(void)
{
    if (GfxDrawWorld()) {
        DrawUI();
        return;
    }
    ClearBackground(G.sky);
    BeginMode3D(G.cam);
    DrawSky();
    DrawTerrain();
    DrawResources();
    DrawVillage();
    DrawWorkers();
    DrawCaravans();
    DrawRaiders();
    DrawWildlife();
    DrawArrows();
    DrawNetArrows();
    DrawPeers();
    DrawPlayer();
    if (ExtraFigures) ExtraFigures();
    DrawEffects();
    DrawRain();
    EndMode3D();
    DrawUI();
}
