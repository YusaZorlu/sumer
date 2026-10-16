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
#define SAFE_RADIUS    22.0f

typedef struct { ItemId item; int amount; float regrow; int tool; const char *verb, *wait; float food; } GatherDef;

// Picking dates is free; everything else costs food. Mining costs twice what woodcutting does.
static const GatherDef GATHER[RES_KINDS] = {
    [RES_PALM]  = { IT_DATES,  3, 24.0f, 0,         "Pick dates",      "Dates ripen again",      0.0f },
    [RES_REEDS] = { IT_REEDS,  4, 48.0f, 0,         "Cut reeds",       "Reeds regrowing",        1.0f },
    [RES_TREE]  = { IT_WOOD,   2, 16.0f, TOOL_AXE,  "Chop wood",       "Felled tree regrowing",  8.0f },
    [RES_CLAY]  = { IT_CLAY,   4, 48.0f, 0,         "Dig clay",        "Clay pit refilling",     2.0f },
    [RES_ORE]   = { IT_COPPER, 2, 10.0f, TOOL_PICK, "Mine copper ore", "Vein worked out",       16.0f },
    [RES_FISH]  = { IT_FISH,   2, 24.0f, 0,         "Cast your net",   "Fish scattered",         1.5f },
    [RES_HERB]  = { IT_HERB,   2, 36.0f, 0,         "Gather herbs",    "Herbs growing back",     1.0f },
};

Game G;

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
    G.day = 1;
    G.dawnDay = 1;
    G.hour = 7.0f;
    G.equip = EQ_BOW;
    G.farmWater = WATER_DAYS;
    G.coopWater = WATER_DAYS;

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

void StartSleep(void)
{
    G.sleeping = true;
    G.slept = false;
    G.sleepT = 0;
}

static void UpdateSleep(float dt)
{
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
        G.food = fmaxf(0.0f, G.food - hours * FOOD_SLEEP);
        G.health = HEALTH_MAX;
        bool saved = SaveGame();
        Say("%s%s%s", L(night ? "You slept until dawn." : "You rested until dusk."),
            rained ? L(" It rained meanwhile; the farm and the coop are full of water.") : "", saved ? L(" Game saved.") : "");
        G.slept = true;
    }
    if (G.sleepT >= 2.2f) G.sleeping = false;
}

static bool RaidersHostile(void)
{
    for (int i = 0; i < MAX_RAIDERS; i++)
        if (G.raiders[i].vit.hostile > 0 && !G.raiders[i].vit.dead) return true;
    return false;
}

static void Collapse(void)
{
    Player *p = &G.player;
    float hours = (G.hour < 7.0f) ? 7.0f - G.hour : 31.0f - G.hour;
    const char *how = G.wanted > 0 ? "The guards beat you down and dumped you at your door. Your carcasses were taken."
                    : RaidersHostile() ? "Raiders left you for dead. A shepherd carried you home. Your carcasses are gone."
                    : "You were brought down. A shepherd carried you home. Your carcasses are gone.";

    for (int i = 0; i < G.bagSlots; i++)
        if (G.bag[i].item >= IT_GAZELLE && G.bag[i].item <= IT_GOOSE) G.bag[i] = (Slot){ IT_NONE, 0 };
    CalmWolves();
    for (int i = 0; i < MAX_RAIDERS; i++) G.raiders[i].vit.hostile = 0;
    G.wolfAggro = 0;
    G.climb = 0;
    if (G.hour >= 7.0f) {
        RainBefore(24.0f);
        G.day++;
    }
    G.hour = 7.0f;
    AdvanceHours(hours);
    RespawnPeople();
    if (G.dawnDay != G.day) {
        G.dawnDay = G.day;
        NewDay();
    }

    G.health = 0.3f * HEALTH_MAX;
    p->pos = Vector3Add(G.stations[STN_BED], (Vector3){ -1.6f, 0, 0.5f });
    p->vy = 0;
    p->crouching = p->aiming = false;
    p->draw = 0;
    if (G.panel != PANEL_NONE) SetPanel(PANEL_NONE);
    G.sleeping = true;
    G.slept = true;
    G.sleepT = 1.1f;
    Say("%s", L(how));
}

void HurtPlayer(float dmg)
{
    if (G.sleeping) return;
    G.health -= dmg * (1.0f - ArmorReduction(G.armor));
    G.hurtT = 0.45f;
    G.safeT = 0;
    if (G.panel == PANEL_BAG || G.panel == PANEL_STORAGE || G.panel == PANEL_TRADE) SetPanel(PANEL_NONE);
    if (G.health <= 0) Collapse();
}

static void UpdateDawn(void)
{
    if (G.hour >= 6.0f && G.hour < 12.0f && G.dawnDay != G.day) {
        G.dawnDay = G.day;
        RespawnPeople();
        NewDay();
    }
}

static const Vitals *TraderVitals(int i)
{
    const Trader *t = &G.traders[i];
    if (t->roaming)
        for (int c = 0; c < MAX_CARAVANS; c++)
            if (G.caravans[c].trader == i) return &G.caravans[c].vit[0];
    return &t->vit;
}

static bool TraderWilling(int i)
{
    const Vitals *v = TraderVitals(i);
    return !v->dead && !v->override && v->hostile <= 0 && G.wanted <= 0;
}

static void UpdateClock(float dt)
{
    if (G.sleeping) return;

    float scale = (G.panel == PANEL_NONE && IsKeyDown(KEY_T)) ? 40.0f : 1.0f;
    float h = dt * scale / HOUR_SECONDS;

    G.hour += h;
    if (G.hour >= 24.0f) {
        G.hour -= 24.0f;
        G.day++;
    }
    AdvanceHours(h);

    const Player *p = &G.player;
    float rate = p->running ? FOOD_RUN : p->moving ? FOOD_WALK : FOOD_IDLE;
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
    if (G.snakeWarn > 0) G.snakeWarn -= dt;
}

static void Consider(Target *best, float *score, TargetKind kind, int index, Vector3 pos, float range)
{
    float s = XZDist(pos, G.player.pos) / range;
    if (s < *score && fabsf(pos.y - G.player.pos.y) < 2.5f) {
        *score = s;
        *best = (Target){ kind, index };
    }
}

static void BuildPrompts(void)
{
    const int n = (int)sizeof(G.promptE);
    int i = G.target.index;

    G.promptE[0] = G.promptF[0] = '\0';
    G.okE = G.okF = false;

    switch (G.target.kind) {
    case TGT_CARCASS:
        G.okE = true;
        if (G.animals[i].type == AN_WOLF) Fmt(G.promptE, n, "[E] Skin the wolf for its pelt");
        else Fmt(G.promptE, n, "[E] Take the %s carcass (needs a free basket)", AnimalName(G.animals[i].type));
        break;
    case TGT_RES: {
        const Resource *r = &G.res[i];
        const GatherDef *g = &GATHER[r->kind];
        bool heavy = g->food >= 2.0f;
        G.okE = r->regrow <= 0 && (!heavy || G.food >= g->food);
        if (r->kind == RES_PALM && r->regrow > 0 && Season() == WINTER) Fmt(G.promptE, n, "No dates in winter");
        else if (r->regrow >= 24.0f) Fmt(G.promptE, n, "%s: about %.1f days", L(g->wait), r->regrow / 24.0f);
        else if (r->regrow > 0) Fmt(G.promptE, n, "%s: about %d hours", L(g->wait), (int)ceilf(r->regrow));
        else if (!G.okE) Fmt(G.promptE, n, "Too hungry for this work. Eat first (needs %d food)", (int)g->food);
        else if (heavy) Fmt(G.promptE, n, "[E] %s (hard work: -%d food)", L(g->verb), (int)g->food);
        else if (g->food > 0) Fmt(G.promptE, n, "[E] %s (-%g food)", L(g->verb), (double)g->food);
        else Fmt(G.promptE, n, "[E] %s", L(g->verb));
        break;
    }
    case TGT_STATION:
        if (StashCamp(i) >= 0) StashPrompt(StashCamp(i), G.promptE, n, &G.okE);
        else if (i == STN_FERRY_E || i == STN_FERRY_W) FerryPrompt(i, G.promptE, G.promptF, n, &G.okE, &G.okF);
        else StationPrompt(i, G.promptE, n, &G.okE);
        if (i == STN_SMELTER) ForgePrompt(G.promptF, n, &G.okF);
        if (i == STN_BENCH) CloakPrompt(G.promptF, n, &G.okF);
        break;
    case TGT_PLOT:
        PlotPrompt(i, G.promptE, G.promptF, n, &G.okE, &G.okF);
        break;
    case TGT_COOP:
        CoopPrompt(G.promptE, G.promptF, n, &G.okE, &G.okF);
        break;
    case TGT_TRADER:
        G.okE = true;
        Fmt(G.promptE, n, "[E] Barter with the %s", L(G.traders[i].name));
        break;
    case TGT_BODY: {
        Vector3 pos;
        float hp, maxHp;
        const char *name = "";
        bool hostile, dead;
        FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead);
        G.okE = true;
        Fmt(G.promptE, n, "[E] Search the %s's body", L(name));
        break;
    }
    case TGT_OFFER:
        OfferPrompt(G.promptE, n, &G.okE);
        break;
    case TGT_LADDER:
        G.okE = true;
        Fmt(G.promptE, n, (i & 1) ? "[E] Climb down the ladder" : "[E] Climb the ladder to the wall walk");
        break;
    case TGT_WATER:
        G.okE = true;
        Fmt(G.promptE, n, "[E] Fill the bucket");
        break;
    default:
        break;
    }
}

static void FindTarget(bool play)
{
    Target best = { TGT_NONE, -1 };
    float score = 1.0f;

    if (play) {
        for (int i = 0; i < G.animalCount; i++)
            if (G.animals[i].active && G.animals[i].state == ST_DEAD && CarcassItem(G.animals[i].type) != IT_NONE)
                Consider(&best, &score, TGT_CARCASS, i, G.animals[i].pos, 2.4f);
        for (int i = 0; i < G.resCount; i++)
            Consider(&best, &score, TGT_RES, i, G.res[i].pos, 2.4f);
        for (int i = 0; i < STN_COUNT; i++)
            Consider(&best, &score, TGT_STATION, i, G.stations[i], 1.9f);
        for (int i = 0; i < MAX_PLOTS; i++)
            if (G.plots[i].state != PLOT_PALM) Consider(&best, &score, TGT_PLOT, i, G.plots[i].pos, 2.1f);
        Consider(&best, &score, TGT_COOP, 0, G.coop, 4.2f);
        for (int i = 0; i < TR_COUNT; i++)
            if (!G.traders[i].away && TraderWilling(i)) Consider(&best, &score, TGT_TRADER, i, G.traders[i].pos, 3.2f);
        for (int i = 0; i < FighterCount(); i++) {
            Vector3 pos;
            float hp, maxHp;
            const char *name;
            bool hostile, dead;
            if (BodyLootable(i) && FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead))
                Consider(&best, &score, TGT_BODY, i, pos, 2.2f);
        }
        if (!G.templeAccess && G.wanted <= 0) Consider(&best, &score, TGT_OFFER, 0, GatePos(), 2.4f);
        for (int i = 0; i < G.ladderCount; i++) {
            const Ladder *l = &G.ladders[i];
            Consider(&best, &score, TGT_LADDER, i * 2, l->base, 1.5f);
            Consider(&best, &score, TGT_LADDER, i * 2 + 1, (Vector3){ l->wall.x, l->top, l->wall.z }, 1.5f);
        }
        if (best.kind == TGT_NONE && BagCount(IT_BUCKET) > 0 && NearWater(G.player.pos)) best = (Target){ TGT_WATER, 0 };
    }
    G.target = best;
    BuildPrompts();
}

static void TakeCarcass(Animal *a)
{
    const char *name = AnimalName(a->type);

    if (BagAdd(CarcassItem(a->type), 1) == 0) {
        Say(a->type == AN_WOLF ? "No room in your baskets for the pelt." : "No free basket. A carcass needs a whole basket to itself.");
        return;
    }
    int back = a->arrowsIn ? GiveArrows(a->arrowsIn) : 0;
    a->active = false;
    a->respawn = -1.0f;
    Exert(1.0f);
    if (a->type == AN_WOLF && back) Say("Skinned the wolf for its pelt. Arrows recovered: %d.", back);
    else if (a->type == AN_WOLF) Say("Skinned the wolf for its pelt.");
    else if (back) Say("Took the %s carcass. Arrows recovered: %d.", name, back);
    else Say("Took the %s carcass.", name);
}

static void Gather(Resource *r)
{
    const GatherDef *g = &GATHER[r->kind];
    int amount = g->amount * ((g->tool && (G.tools & g->tool)) ? 2 : 1);

    if (r->regrow > 0) return;
    if (g->food >= 2.0f && G.food < g->food) {
        Say("Too hungry for this work. Eat something first.");
        return;
    }
    int got = BagAdd(g->item, amount);
    if (!got) {
        Say("No room in your baskets.");
        return;
    }
    r->regrow = g->regrow;
    Exert(g->food);
    if (g->food >= 2.0f) Say("+%d %s. Hard work: -%d food.", got, ItemName(g->item), (int)g->food);
    else if (r->kind == RES_HERB) Say("+%d %s (-%g food). Chew herbs from your baskets (Tab) to heal faster.", got, ItemName(g->item), (double)g->food);
    else if (g->food > 0) Say("+%d %s (-%g food)", got, ItemName(g->item), (double)g->food);
    else Say("+%d %s", got, ItemName(g->item));
}

static void Interact(bool alt)
{
    int i = G.target.index;

    switch (G.target.kind) {
    case TGT_CARCASS: if (!alt) TakeCarcass(&G.animals[i]); break;
    case TGT_RES:     if (!alt) Gather(&G.res[i]); break;
    case TGT_STATION:
        if (!alt && StashCamp(i) >= 0) TakeStash(StashCamp(i));
        else if (!alt && (i == STN_FERRY_E || i == STN_FERRY_W)) TakeFerry(i);
        else if (i == STN_FERRY_E || i == STN_FERRY_W) ChangeFare();
        else if (!alt) UseStation(i);
        else if (i == STN_SMELTER) UseForge();
        else if (i == STN_BENCH) SewCloak();
        break;
    case TGT_BODY:    if (!alt) LootBody(i); break;
    case TGT_OFFER:   if (!alt) MakeOffering(); break;
    case TGT_LADDER:  if (!alt) StartClimb(i / 2, (i & 1) == 0); break;
    case TGT_PLOT:    UsePlot(i, alt); break;
    case TGT_COOP:
        if (!alt) {
            G.slaughterStep = 0;
            SetPanel(PANEL_COOP);
        }
        break;
    case TGT_WATER:   if (!alt) FillBucket(); break;
    case TGT_TRADER:
        if (!alt) {
            G.trader = i;
            SetPanel(PANEL_TRADE);
        }
        break;
    default:
        break;
    }
}

// Screenshot tools and app switches steal focus and the captured mouse; pause so resuming recaptures it.
static void HandleWindow(void)
{
    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if (IsKeyPressed(KEY_F11) || (alt && IsKeyPressed(KEY_ENTER))) ToggleBorderlessWindowed();

    bool focused = IsWindowFocused();
    if (!focused && G.wasFocused && G.panel == PANEL_NONE) SetPanel(PANEL_PAUSE);
    G.wasFocused = focused;
}

static void HandleMenus(void)
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        SetPanel(G.panel == PANEL_NONE ? PANEL_PAUSE : PANEL_NONE);
    } else if (IsKeyPressed(KEY_H) && (G.panel == PANEL_NONE || G.panel == PANEL_PAUSE || G.panel == PANEL_HELP)) {
        SetPanel(G.panel == PANEL_HELP ? PANEL_NONE : PANEL_HELP);
    } else if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_I)) {
        if (G.panel == PANEL_NONE) SetPanel(PANEL_BAG);
        else if (G.panel != PANEL_PAUSE && G.panel != PANEL_HELP) SetPanel(PANEL_NONE);
    }
    if (G.panel == PANEL_PAUSE && IsKeyPressed(KEY_Q)) G.quit = true;
    if (IsKeyPressed(KEY_F5) && !G.sleeping) {
        if (SaveGame()) Say("Game saved to %s.", SaveFileName(SaveSlot()));
        else Say("Could not write %s in this folder.", SaveFileName(SaveSlot()));
    }
    if (G.panel == PANEL_TRADE && G.traders[G.trader].away) {
        SetPanel(PANEL_NONE);
        Say("The %s is packing up to leave.", L(G.traders[G.trader].name));
    } else if (G.panel == PANEL_TRADE && !TraderWilling(G.trader)) {
        SetPanel(PANEL_NONE);
        Say("The %s won't trade with you now.", L(G.traders[G.trader].name));
    }
    if (G.panel == PANEL_NONE && !G.cursorLocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        DisableCursor();
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

void GameUpdate(void)
{
    float dt = fminf(GetFrameTime(), 0.05f);

    if (G.requestNew) {
        DeleteSave();
        GameInit();
        SetPanel(PANEL_NONE);
        Say("A new game begins in slot %d. Ur, c. 2100 BC.", SaveSlot() + 1);
        return;
    }
    if (G.requestSlot) {
        SwitchSlot(G.requestSlot - 1);
        SetPanel(PANEL_NONE);
        return;
    }
    HandleWindow();
    HandleMenus();
    if (G.panel == PANEL_PAUSE || G.panel == PANEL_HELP) return;

    bool play = G.panel == PANEL_NONE && !G.sleeping;
    CollectBodies();
    UpdatePlayer(dt, play);
    FollowCamera(dt);
    UpdateWorkers();
    UpdateCaravans(dt);
    UpdateVillage(dt);
    UpdateRaiders(dt);
    UpdateCombat(dt);
    G.alone = !InsideWalls(G.player.pos.x, G.player.pos.z, -2.0f) && !PeopleNear(G.player.pos, SAFE_RADIUS);
    UpdateWildlife(dt);
    UpdateArrows(dt);
    UpdateEffects(dt);
    UpdateClock(dt);
    UpdateSleep(dt);
    UpdateDawn();
    UpdateWeather();
    UpdateFields();
    UpdateSky();
    UpdateLights();
    FindTarget(play);

    if (play) {
        if (IsKeyPressed(KEY_E)) Interact(false);
        if (IsKeyPressed(KEY_F)) Interact(true);
        if (IsKeyPressed(KEY_R)) RefillQuiver();
        if (IsKeyPressed(KEY_ONE)) SetEquip(EQ_BOW);
        if (IsKeyPressed(KEY_TWO)) SetEquip(EQ_MELEE);
        if (IsKeyPressed(KEY_THREE)) SetEquip(EQ_NONE);
        if (IsKeyPressed(KEY_FOUR)) SetEquip(EQ_MACE);
        if (IsKeyPressed(KEY_M)) G.mapZoom = (G.mapZoom + 1) % 3;
        if (IsKeyPressed(KEY_Z)) {
            G.arrowCam = !G.arrowCam;
            Say(G.arrowCam ? "Arrow camera on: the view follows each shot (Z to turn off)." : "Arrow camera off.");
        }
    }
    if (G.msgT > 0) G.msgT -= dt;
    if (G.hurtT > 0) G.hurtT -= dt;
    if (G.wolfWarn > 0) G.wolfWarn -= dt;
    if (G.wolfAggro > 0) G.wolfAggro -= dt;
}

void GameDraw(void)
{
    ClearBackground(G.sky);
    BeginMode3D(G.cam);
    DrawSky();
    DrawTerrain();
    DrawResources();
    DrawVillage();
    DrawWorkers();
    DrawFerries();
    DrawCaravans();
    DrawRaiders();
    DrawWildlife();
    DrawArrows();
    DrawPlayer();
    DrawEffects();
    DrawRain();
    EndMode3D();
    DrawUI();
}
