#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>

#define WHEAT_DAYS    10.0f
#define SAPLING_DAYS  2.0f
#define EGGS_MAX      24
#define POUR_DAYS     2.0f
#define MEAL_DAYS     3.0f
#define YEAR_DAYS     (4.0f * SEASON_DAYS)

static const float CITY_FARMS[][2] = { { 62, -45 }, { -62, -50 }, { 30, 62 }, { -25, 62 } };
#define CITY_FARM_COUNT ((int)(sizeof(CITY_FARMS) / sizeof(CITY_FARMS[0])))

float NowDays(void)
{
    return (float)(G.day - 1) + G.hour / 24.0f;
}

static float YearDay(void)
{
    return fmodf(NowDays(), YEAR_DAYS);
}

int Season(void)
{
    return (int)(YearDay() / SEASON_DAYS) % 4;
}

int SeasonDay(void)
{
    return (int)fmodf(YearDay(), SEASON_DAYS) + 1;
}

static const char *const SEASONS[4] = { "Spring", "Summer", "Fall", "Winter" };

const char *SeasonName(int s)
{
    return L(SEASONS[s & 3]);
}

float DaysToWinter(void)
{
    float y = YearDay();
    return y < 3.0f * SEASON_DAYS ? 3.0f * SEASON_DAYS - y : 0.0f;
}

void Exert(float food)
{
    G.food = fmaxf(0.0f, G.food - food);
}

int CityFarmCount(void)
{
    return CITY_FARM_COUNT;
}

Vector3 CityFarmSpot(int i)
{
    return (Vector3){ CITY_FARMS[i][0], 0, CITY_FARMS[i][1] };
}

static void Fence(float x0, float z0, float x1, float z1)
{
    const Color wattle = { 150, 130, 82, 255 };
    float w = fabsf(x1 - x0), d = fabsf(z1 - z0), len = fmaxf(w, d);

    AddBox((x0 + x1) / 2, 0, (z0 + z1) / 2, fmaxf(w, 0.12f), 0.9f, fmaxf(d, 0.12f), wattle, true);
    for (float s = 0; s <= len + 0.01f; s += 2.0f) {
        float t = (len > 0) ? s / len : 0;
        AddShape(Lerp(x0, x1, t), 0, Lerp(z0, z1, t), 0.17f, 1.15f, 0.17f, COL_WOOD, false, SHAPE_CYL);
    }
}

// A plot of tilled earth with four ridges, one under each row of the crop.
static void Plot3(float x, float z)
{
    AddBox(x, 0, z, 3.0f, 0.12f, 3.0f, COL_SOIL, false);
    for (int r = 0; r < 4; r++) AddBox(x - 1.1f + 0.73f * (float)r, 0.1f, z, 0.32f, 0.07f, 2.8f, (Color){ 108, 82, 56, 252 }, false);
}

static void BuildCityFarm(float x, float z)
{
    const float hx = x - 7.5f;
    AddBox(hx, 0, z, 5.0f, 3.2f, 5.6f, COL_HOUSE_DARK, true);
    AddBox(hx, 3.14f, z, 5.16f, 0.2f, 5.76f, Mix(COL_HOUSE_DARK, COL_MUD_DARK, 0.35f), false);
    AddBox(hx + 2.5f, 0, z, 0.12f, 2.0f, 1.0f, COL_DOORWAY, false);
    for (int j = 0; j < 6; j++)
        Plot3(x - 2 + 4.0f * (float)(j % 3), z - 2 + 4.0f * (float)(j / 3));
    AddShape(x + 9.0f, 0, z - 4.0f, 0.8f, 1.1f, 0.8f, COL_CLAY, true, SHAPE_BLOB);
    KeepOut(x - 11, z - 5, x + 10, z + 5);
}

void BuildFarm(void)
{
    for (int i = 0; i < MAX_PLOTS; i++) {
        Vector3 p = { 29.0f + 4.0f * (float)(i % 5), 0, 18.0f + 4.0f * (float)(i / 5) };
        G.plots[i] = (Plot){ p, PLOT_EMPTY, 0.0f, 0.0f };
        Plot3(p.x, p.z);
    }
    AddBox(37.25f, -0.05f, 15.6f, 20.5f, 0.08f, 0.8f, (Color){ 84, 62, 42, 252 }, false);
    G.stations[STN_DITCH] = (Vector3){ 26.0f, 0, 15.6f };
    AddShape(26.0f, 0, 15.6f, 1.0f, 0.55f, 1.0f, COL_CLAY, true, SHAPE_CYL);
    KeepOut(25.0f, 14.5f, 47.5f, 28.5f);

    G.stations[STN_WELL] = (Vector3){ 11.0f, 0, 18.0f };
    AddShape(11.0f, 0, 18.0f, 1.6f, 0.8f, 1.6f, COL_STONE, true, SHAPE_CYL);
    AddShape(11.0f, 0.79f, 18.0f, 1.1f, 0.02f, 1.1f, COL_WATER, false, SHAPE_CYL);
    AddBox(10.1f, 0, 18.0f, 0.12f, 2.0f, 0.12f, COL_WOOD, false);
    AddBox(11.9f, 0, 18.0f, 0.12f, 2.0f, 0.12f, COL_WOOD, false);
    AddBox(11.0f, 1.95f, 18.0f, 2.0f, 0.1f, 0.1f, COL_WOOD, false);
    KeepOut(9, 16, 13, 20);

    G.coop = (Vector3){ 18.0f, 0, 31.5f };
    Fence(15, 29, 21, 29);
    Fence(15, 34, 21, 34);
    Fence(15, 29, 15, 34);
    Fence(21, 29, 21, 34);
    AddBox(16.1f, 0, 30.0f, 1.8f, 1.5f, 1.6f, COL_MUD, true);
    AddBox(17.05f, 0, 30.0f, 0.12f, 0.8f, 0.6f, COL_DOORWAY, false);
    AddBox(19.6f, 0, 33.2f, 1.2f, 0.3f, 0.4f, COL_WOOD, true);
    AddBox(19.6f, 0.3f, 33.2f, 1.0f, 0.02f, 0.25f, (Color){ 120, 100, 70, 255 }, false);
    KeepOut(14, 28, 22, 35);

    for (int i = 0; i < CITY_FARM_COUNT; i++) BuildCityFarm(CITY_FARMS[i][0], CITY_FARMS[i][1]);
}

static bool PalmPlanted(void)
{
    for (int i = 0; i < MAX_PLOTS; i++)
        if (G.plots[i].state == PLOT_SAPLING || G.plots[i].state == PLOT_PALM) return true;
    return false;
}

static bool CropsPlanted(void)
{
    for (int i = 0; i < MAX_PLOTS; i++)
        if ((G.plots[i].state == PLOT_WHEAT && G.plots[i].growth < 1.0f) || G.plots[i].state == PLOT_SAPLING) return true;
    return false;
}

static bool TooLateToSow(void)
{
    return Season() == WINTER || DaysToWinter() < WHEAT_DAYS;
}

void PlotPrompt(int p, char *e, char *f, int size, bool *okE, bool *okF)
{
    const Plot *pl = &G.plots[p];

    e[0] = f[0] = '\0';
    *okE = *okF = false;
    switch (pl->state) {
    case PLOT_EMPTY:
        *okE = BagCount(IT_SEEDS) > 0 && !TooLateToSow();
        if (Season() == WINTER) Fmt(e, size, "Winter: nothing is sown until spring");
        else if (TooLateToSow()) Fmt(e, size, "Too late to sow: %d days to winter, wheat needs 10", (int)DaysToWinter());
        else Fmt(e, size, *okE ? "[E] Sow wheat seeds" : "Empty plot: bring wheat seeds");
        *okF = BagCount(IT_DATES) > 0 && !PalmPlanted();
        if (PalmPlanted()) Fmt(f, size, "Farm water: %.1f days. The farm has its one date palm", G.farmWater);
        else Fmt(f, size, *okF ? "[F] Plant a date pit (the farm takes one palm)" : "or dates to grow the farm's one palm");
        break;
    case PLOT_WHEAT:
        *okE = pl->growth >= 1.0f;
        if (*okE) Fmt(e, size, "[E] Harvest wheat (4 wheat, 2 seeds)");
        else if (Season() == WINTER) Fmt(e, size, "Wheat %d%%, resting until spring", (int)(pl->growth * 100));
        else if (G.farmWater <= 0) Fmt(e, size, "Wheat %d%%: dry for %.1f days, withers at 6", (int)(pl->growth * 100), pl->dry);
        else Fmt(e, size, "Wheat %d%%, ripe in about %d days", (int)(pl->growth * 100), (int)ceilf((1.0f - pl->growth) * WHEAT_DAYS));
        Fmt(f, size, "Farm water: %.1f days", G.farmWater);
        break;
    case PLOT_SAPLING:
        Fmt(e, size, "Date palm sapling: %d%%", (int)(pl->growth * 100));
        Fmt(f, size, "Farm water: %.1f days", G.farmWater);
        break;
    case PLOT_DEAD:
        *okE = true;
        Fmt(e, size, "[E] Clear the withered wheat (plant waste for the herder)");
        break;
    default:
        break;
    }
}

// What E or F would do on plot p with what this player carries, or -1 (saying why when it helps).
static int PlotAction(int p, bool alt)
{
    const Plot *pl = &G.plots[p];

    if (pl->state == PLOT_EMPTY && !alt) {
        if (TooLateToSow()) {
            Say(Season() == WINTER ? "Nothing grows in winter. Sow in spring." : "Too late: the wheat would not ripen before winter.");
            return -1;
        }
        return BagCount(IT_SEEDS) > 0 ? PLOT_SOW : -1;
    }
    if (pl->state == PLOT_EMPTY && alt) return !PalmPlanted() && BagCount(IT_DATES) > 0 ? PLOT_PIT : -1;
    if (pl->state == PLOT_WHEAT && !alt && pl->growth >= 1.0f) {
        if (SlotsSpace(G.bag, G.bagSlots, IT_WHEAT) > 0) return PLOT_REAP;
        Say("No room in your baskets for the harvest.");
        return -1;
    }
    if (pl->state == PLOT_DEAD && !alt) {
        if (SlotsSpace(G.bag, G.bagSlots, IT_WASTE) > 0) return PLOT_CLEAR;
        Say("No room in your baskets.");
    }
    return -1;
}

void UsePlot(int p, bool alt)
{
    int args[2] = { p, PlotAction(p, alt) };
    if (args[1] >= 0) Act(RQ_PLOT, args, 2);
}

// The field's half, done where the world runs: false when someone else changed the plot first.
bool PlotServe(int p, int action)
{
    if (p < 0 || p >= MAX_PLOTS) return false;
    Plot *pl = &G.plots[p];
    PlotState now = pl->state;
    if (action == PLOT_SOW && now == PLOT_EMPTY && !TooLateToSow()) now = PLOT_WHEAT;
    else if (action == PLOT_PIT && now == PLOT_EMPTY && !PalmPlanted()) now = PLOT_SAPLING;
    else if (action == PLOT_REAP && now == PLOT_WHEAT && pl->growth >= 1.0f) now = PLOT_EMPTY;
    else if (action == PLOT_CLEAR && now == PLOT_DEAD) now = PLOT_EMPTY;
    else return false;
    *pl = (Plot){ pl->pos, now, 0.0f, 0.0f };
    return true;
}

// The farmer's half: seed out of the baskets, the harvest into them.
void PlotApply(int action, bool ok)
{
    if (!ok) {
        Say("Someone else got to that plot first.");
        return;
    }
    switch (action) {
    case PLOT_SOW:
        BagRemove(IT_SEEDS, 1);
        Exert(0.5f);
        Say("Sown. With water the wheat ripens in %d days.", (int)WHEAT_DAYS);
        break;
    case PLOT_PIT:
        BagRemove(IT_DATES, 1);
        Exert(0.5f);
        Say("Planted a date pit. With water it fruits in %d days.", (int)SAPLING_DAYS);
        break;
    case PLOT_REAP: {
        int wheat = BagAdd(IT_WHEAT, 4), seeds = BagAdd(IT_SEEDS, 2);
        Exert(1.5f);
        Say("Harvested %d wheat and %d seeds.", wheat, seeds);
        break;
    }
    case PLOT_CLEAR:
        BagAdd(IT_WASTE, 1);
        Exert(1.0f);
        Say("Cleared the withered wheat. The cattle herder takes plant waste.");
        break;
    default:
        break;
    }
}

// The farm and the coop are everyone's: their news goes to every player.
static void WaterWarning(float before, float after, bool coop, bool matters)
{
    if (!matters) return;
    if (before > 2.0f && after <= 2.0f && after > 0) Announce(coop ? "The coop has 2 days of water left." : "The farm has 2 days of water left.");
    else if (before > 0 && after <= 0) Announce(coop ? "The coop is out of water." : "The farm is out of water.");
}

static void RemoveHen(int i)
{
    for (int k = i; k < G.henCount - 1; k++) {
        G.hens[k] = G.hens[k + 1];
        G.henBorn[k] = G.henBorn[k + 1];
        G.henMeal[k] = G.henMeal[k + 1];
    }
    G.henCount--;
}

bool AddHen(void)
{
    if (G.henCount >= MAX_HENS) return false;
    int i = G.henCount++;
    G.hens[i] = (Wanderer){ Vector3Add(G.coop, (Vector3){ RandF(-1.5f, 1.5f), 0, RandF(-1, 1) }), RandF(-PI, PI), 0, 0 };
    G.henBorn[i] = NowDays();
    G.henMeal[i] = NowDays() + MEAL_DAYS;
    return true;
}

static void CoopHours(float h)
{
    float now = NowDays(), before = G.coopWater;

    G.coopWater = fmaxf(0.0f, G.coopWater - h / 24.0f);
    WaterWarning(before, G.coopWater, true, G.henCount > 0);
    if (G.coopWater <= 0) G.coopDry += h / 24.0f;
    else G.coopDry = 0;

    for (int i = 0; i < G.henCount; i++)
        while (G.henMeal[i] <= now && G.coopFeed >= 1.0f) {
            G.coopFeed -= 1.0f;
            G.henMeal[i] += MEAL_DAYS;
        }

    G.layClock += h;
    while (G.layClock >= 24.0f) {
        G.layClock -= 24.0f;
        for (int i = 0; i < G.henCount; i++)
            if (G.henMeal[i] > now && G.coopWater > 0 && G.coopEggs < EGGS_MAX) G.coopEggs++;
    }

    for (int i = G.henCount - 1; i >= 0; i--) {
        const char *cause = now - G.henBorn[i] >= HEN_LIFE_DAYS ? "old age"
                          : now - G.henMeal[i] >= STARVE_DAYS ? "hunger"
                          : G.coopDry >= STARVE_DAYS ? "thirst" : NULL;
        if (!cause) continue;
        RemoveHen(i);
        G.slaughterStep = 0;
        Announce("A hen died of %s.", cause);
    }
}

// Growth needs water and stops in winter; six dry days kill young wheat but never the palm.
void FarmHours(float h)
{
    bool winter = Season() == WINTER, wet = G.farmWater > 0;
    float before = G.farmWater;
    int withered = 0;

    G.farmWater = fmaxf(0.0f, G.farmWater - h / 24.0f);
    WaterWarning(before, G.farmWater, false, CropsPlanted());

    for (int i = 0; i < MAX_PLOTS; i++) {
        Plot *p = &G.plots[i];
        bool growing = (p->state == PLOT_WHEAT && p->growth < 1.0f) || p->state == PLOT_SAPLING;
        if (!growing || winter) continue;
        if (!wet) {
            p->dry += h / 24.0f;
            if (p->state == PLOT_WHEAT && p->dry >= WATER_DAYS) {
                p->state = PLOT_DEAD;
                withered++;
            }
            continue;
        }
        p->dry = 0;
        if (p->state == PLOT_WHEAT) {
            p->growth = fminf(1.0f, p->growth + h / (24.0f * WHEAT_DAYS));
        } else {
            p->growth += h / (24.0f * SAPLING_DAYS);
            if (p->growth >= 1.0f) {
                int r = AddResource(p->pos.x, p->pos.z, RES_PALM);
                if (r >= 0) G.res[r].farm = true;
                p->state = PLOT_PALM;
            }
        }
    }
    if (withered) Announce(withered > 1 ? "Six days without water: %d plots of wheat withered." : "Six days without water: %d plot of wheat withered.", withered);
    CoopHours(h);
}

void DitchPrompt(char *buf, int size, bool *ok)
{
    *ok = BagCount(IT_WATER) > 0 && G.farmWater < WATER_DAYS - 0.05f;
    if (*ok) Fmt(buf, size, "[E] Pour a bucket into the ditch (farm water %.1f -> %.1f days)", G.farmWater, fminf(WATER_DAYS, G.farmWater + POUR_DAYS));
    else if (BagCount(IT_WATER) > 0) Fmt(buf, size, "The farm is full of water (%.1f days)", G.farmWater);
    else Fmt(buf, size, "Farm water: %.1f of 6 days. Fill a bucket at the well or the river", G.farmWater);
}

static void EmptyBucket(void)
{
    BagRemove(IT_WATER, 1);
    BagAdd(IT_BUCKET, 1);
    Exert(0.5f);
}

void PourDitch(void)
{
    if (BagCount(IT_WATER) > 0 && G.farmWater < WATER_DAYS - 0.05f) Act(RQ_DITCH, NULL, 0);
}

void PourCoop(void)
{
    if (BagCount(IT_WATER) > 0 && G.coopWater < WATER_DAYS - 0.05f) Act(RQ_COOPWATER, NULL, 0);
}

// The water's half: the ditch or the trough fills up. Returns the days of water after, in hundredths.
int PourServe(bool coop)
{
    float *water = coop ? &G.coopWater : &G.farmWater;
    if (*water >= WATER_DAYS - 0.05f) return -1;
    *water = fminf(WATER_DAYS, *water + POUR_DAYS);
    return (int)roundf(*water * 100.0f);
}

void PourApply(bool coop, bool ok, int after)
{
    if (!ok) {
        Say(coop ? "The hens' trough is already full." : "The farm is already full of water.");
        return;
    }
    EmptyBucket();
    if (coop) Say("Filled the hens' trough: %.1f days of water.", (float)after / 100.0f);
    else Say("Watered the farm: %.1f days of water.", (float)after / 100.0f);
}

bool NearWater(Vector3 p)
{
    if (XZDist(p, G.stations[STN_WELL]) < 2.3f) return true;
    for (int k = 0; k < 8; k++) {
        float a = (float)k * PI / 4;
        if (InWater(p.x + cosf(a) * 1.4f, p.z + sinf(a) * 1.4f)) return true;
    }
    return false;
}

void FillBucket(void)
{
    if (BagRemove(IT_BUCKET, 1) == 0) {
        Say("You need an empty bucket. There is one in your storeroom.");
        return;
    }
    BagAdd(IT_WATER, 1);
    Exert(0.5f);
    Say("Filled the bucket. Pour it into the farm ditch or the hens' trough.");
}

void CoopPrompt(char *e, char *f, int size, bool *okE, bool *okF)
{
    e[0] = f[0] = '\0';
    *okF = false;
    *okE = true;
    if (G.henCount == 0) Fmt(e, size, "[E] Coop: no hens yet (the farmer sells them). Water %.1f days", G.coopWater);
    else Fmt(e, size, "[E] Tend the hens: %d hens, %d eggs, feed %d, water %.1f days", G.henCount, G.coopEggs, (int)G.coopFeed, G.coopWater);
}

// Wheat is worth 2 feed, seeds 1; at most 3 wheat or 5 seeds at a time.
static void Feed(bool wheat)
{
    int item = wheat ? IT_WHEAT : IT_SEEDS, most = wheat ? 3 : 5;
    int args[2] = { wheat, BagCount(item) };
    if (args[1] > most) args[1] = most;
    if (args[1] > 0 && G.coopFeed <= FEED_MAX - (wheat ? 2.0f : 1.0f)) Act(RQ_FEED, args, 2);
}

void FeedWheat(void)
{
    Feed(true);
}

void FeedSeeds(void)
{
    Feed(false);
}

// The coop's half: how many of the n offered the trough takes. 'feed' is the feed after.
int FeedServe(bool wheat, int n, int *feed)
{
    float per = wheat ? 2.0f : 1.0f;
    int room = (int)((FEED_MAX - G.coopFeed) / per);
    if (n > room) n = room;
    if (n > (wheat ? 3 : 5)) n = wheat ? 3 : 5;
    if (n < 0) n = 0;
    G.coopFeed += per * (float)n;
    *feed = (int)G.coopFeed;
    return n;
}

void FeedApply(bool wheat, int n, int feed)
{
    n = BagRemove(wheat ? IT_WHEAT : IT_SEEDS, n);
    if (!n) return;
    Exert(0.3f);
    if (wheat) Say("Fed %d wheat. Feed %d/%d.", n, feed, (int)FEED_MAX);
    else Say("Scattered %d seeds. Feed %d/%d.", n, feed, (int)FEED_MAX);
}

void CollectEggs(void)
{
    int room = SlotsSpace(G.bag, G.bagSlots, IT_EGG);
    if (room <= 0) Say("No room in your baskets for eggs.");
    else if (G.coopEggs > 0) Act(RQ_EGGS, &room, 1);
}

int EggsServe(int most)
{
    int n = most < G.coopEggs ? most : G.coopEggs;
    if (n < 0) n = 0;
    G.coopEggs -= n;
    return n;
}

void EggsApply(int n)
{
    n = BagAdd(IT_EGG, n);
    if (n) Say(n > 1 ? "Collected %d eggs." : "Collected %d egg.", n);
    else Say("Someone else took the eggs.");
}

void UseCoop(bool alt)
{
    if (alt) FeedWheat();
    else if (G.coopEggs > 0) CollectEggs();
    else FeedSeeds();
}

// A hen is known by its hatching time, so the request names the same bird on every machine.
static int HenKey(int i)
{
    return (int)roundf(G.henBorn[i] * 1000.0f);
}

bool SlaughterHen(int i)
{
    G.slaughterStep = 0;
    if (i < 0 || i >= G.henCount || NowDays() - G.henBorn[i] < SEASON_DAYS) return false;
    if (SlotsSpace(G.bag, G.bagSlots, IT_HEN) <= 0) {
        Say("No room in your baskets.");
        return false;
    }
    int args[2] = { i, HenKey(i) };
    Act(RQ_SLAUGHTER, args, 2);
    return true;
}

bool SlaughterServe(int i, int key)
{
    if (i < 0 || i >= G.henCount || HenKey(i) != key || NowDays() - G.henBorn[i] < SEASON_DAYS) return false;
    RemoveHen(i);
    return true;
}

void SlaughterApply(bool ok)
{
    if (!ok) {
        Say("That hen is gone.");
        return;
    }
    BagAdd(IT_HEN, 1);
    Exert(0.5f);
    Say("You killed the hen. Roast it in the oven.");
}

static bool RainWindow(void)
{
    return G.rainToday && G.hour >= G.rainStart && G.hour < G.rainEnd;
}

static void Downpour(bool tell)
{
    G.rainDone = true;
    G.farmWater = WATER_DAYS;
    G.coopWater = WATER_DAYS;
    G.coopDry = 0;
    if (!tell) return;
    if (Season() == WINTER) Announce("Winter rain fills the farm and the coop. Nothing grows until spring.");
    else Announce("Rain! The farm and the coop are full of water.");
}

// A time skip up to 'hour' (24 = midnight) that passed today's rain still waters the farm.
bool RainBefore(float hour)
{
    if (!G.rainToday || G.rainDone || G.rainStart >= hour) return false;
    Downpour(false);
    return true;
}

void NewDay(void)
{
    static const float odds[4] = { 0.20f, 0.01f, 0.20f, 0.35f };
    int s = Season(), index = (G.day - 1) / SEASON_DAYS;

    G.rainToday = RandF(0, 1) < odds[s];
    G.rainStart = RandF(7.0f, 15.0f);
    G.rainEnd = G.rainStart + RandF(3.0f, 7.0f);
    G.rainDone = false;
    if (index != G.seasonSeen) {
        G.seasonSeen = index;
        if (s == SPRING) {
            SpringRespawn();
            Announce("Spring. The herds are back and the fields can be sown.");
        } else if (s == WINTER) {
            Announce("Winter. Crops rest until spring; sowing waits too.");
        } else {
            if (s == FALL) Announce("Fall begins. Too late to sow wheat now.");
            else Announce("%s begins.", SEASONS[s]);
        }
    }
}

void UpdateWeather(void)
{
    if (RainWindow() && !G.rainDone) Downpour(true);
    else if (G.rainToday && !G.rainDone && G.hour >= G.rainEnd) Downpour(false);
    G.raining = RainWindow();
}

// Streaks sit on a 50 m grid that wraps around the camera, so walking does not drag the rain along.
void DrawRain(void)
{
    if (!G.raining) return;
    const float span = 50.0f, half = span / 2;
    float t = (float)GetTime();
    Vector3 c = G.cam.position;
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, c));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, (Vector3){ 0, 1, 0 }));
    Color drop = { 196, 206, 222, 120 };
    // Roofs and ceilings near the camera (anything overhead, not standing on the ground) keep the rain off.
    static int roofs[256];
    int nroof = 0;
    for (int i = 0; i < G.boxCount && nroof < 256; i++) {
        const Box *b = &G.boxes[i];
        float bottom = b->pos.y - b->size.y / 2;
        if (b->water || fabsf(b->pos.x - c.x) > half + b->size.x / 2 || fabsf(b->pos.z - c.z) > half + b->size.z / 2) continue;
        if (bottom > TerrainHeight(b->pos.x, b->pos.z) + 1.8f) roofs[nroof++] = i;
    }

    rlBegin(RL_QUADS);
    for (int i = 0; i < 1400; i++) {
        float x = Hash2(i, 1) * span - c.x, z = Hash2(i, 2) * span - c.z;
        x = c.x + x - span * floorf((x + half) / span);
        z = c.z + z - span * floorf((z + half) / span);
        if ((x - c.x) * (x - c.x) + (z - c.z) * (z - c.z) < 6.0f) continue;
        float y = c.y + 14.0f - fmodf(t * 19.0f + Hash2(i, 3) * 28.0f, 28.0f);
        bool dry = false;
        for (int k = 0; k < nroof && !dry; k++) {
            const Box *b = &G.boxes[roofs[k]];
            dry = y < b->pos.y && fabsf(x - b->pos.x) < b->size.x / 2 && fabsf(z - b->pos.z) < b->size.z / 2;
        }
        if (dry) continue;
        float len = 0.55f + 0.35f * Hash2(i, 4), w = 0.018f + 0.012f * Hash2(i, 5);
        Vector3 top = { x + 0.12f, y + len, z + 0.05f }, bot = { x, y, z };
        if (i % 64 == 0) rlCheckRenderBatchLimit(256);
        rlColor4ub(drop.r, drop.g, drop.b, drop.a);
        rlVertex3f(top.x - right.x * w, top.y, top.z - right.z * w);
        rlVertex3f(bot.x - right.x * w, bot.y, bot.z - right.z * w);
        rlVertex3f(bot.x + right.x * w, bot.y, bot.z + right.z * w);
        rlVertex3f(top.x + right.x * w, top.y, top.z + right.z * w);
    }
    rlEnd();
}

static void Part(float x, float y, float z, float w, float h, float l, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, l }, c);
}

static void DrawWheat(Vector3 pos, float growth, bool dead, bool stubble)
{
    bool ripe = growth >= 1.0f;
    Color c = dead ? (Color){ 120, 96, 60, 255 } : ripe || stubble ? COL_WHEAT_RIPE : Mix(COL_WHEAT_GREEN, COL_WHEAT_RIPE, growth * 0.4f);
    float h = stubble ? 0.12f : dead ? 0.35f : 0.15f + 0.95f * growth;

    for (int r = 0; r < 4; r++) {
        for (int k = 0; k < 4; k++) {
            float x = pos.x - 1.1f + (float)r * 0.73f, z = pos.z - 1.1f + (float)k * 0.73f;
            Part(x, 0.12f + h / 2, z, dead ? 0.16f : 0.12f, h, 0.12f, TAG_CROP(c));
            if (ripe && !stubble) Part(x, 0.12f + h + 0.1f, z, 0.16f, 0.22f, 0.16f, (Color){ 200, 160, 70, 255 });
        }
    }
}

static void DrawSapling(const Plot *p)
{
    float s = 0.25f + 0.75f * p->growth;
    float trunk = 3.0f * s;
    DrawShadedCylinder((Vector3){ p->pos.x, 0.1f, p->pos.z }, 0.08f + 0.12f * s, 0.08f + 0.12f * s, trunk, 0.8f, 6, COL_BARK);
    for (int k = 0; k < 5; k++) {
        rlPushMatrix();
        rlTranslatef(p->pos.x, 0.12f + trunk, p->pos.z);
        rlRotatef((float)k * 72.0f, 0, 1, 0);
        rlRotatef(-25.0f, 0, 0, 1);
        Part(0.8f * s, 0, 0, 1.6f * s, 0.08f, 0.4f * s, COL_PALM_LEAF);
        rlPopMatrix();
    }
}

static void DrawHen(const Wanderer *w, int i, float t)
{
    Color feather = (i % 2) ? (Color){ 170, 110, 60, 255 } : (Color){ 232, 224, 206, 255 };
    const Color yellow = { 220, 170, 50, 255 };
    float peck = (w->speed <= 0) ? fmaxf(0.0f, sinf(t * 5.0f + (float)i)) * 0.08f : 0.0f;

    rlPushMatrix();
    rlTranslatef(w->pos.x, 0, w->pos.z);
    rlRotatef(w->heading * RAD2DEG, 0, 1, 0);
    Part(-0.06f, 0.09f, 0, 0.03f, 0.18f, 0.03f, yellow);
    Part(0.06f, 0.09f, 0, 0.03f, 0.18f, 0.03f, yellow);
    DrawShadedBlob((Vector3){ 0, 0.3f, 0 }, (Vector3){ 0.15f, 0.14f, 0.2f }, feather);
    Part(0, 0.42f, -0.18f, 0.12f, 0.18f, 0.08f, feather);
    DrawShadedBlob((Vector3){ 0, 0.5f - peck, 0.17f + peck }, (Vector3){ 0.07f, 0.08f, 0.07f }, feather);
    Part(0, 0.6f - peck, 0.17f + peck, 0.04f, 0.06f, 0.08f, (Color){ 200, 40, 40, 255 });
    Part(0, 0.5f - peck, 0.25f + peck, 0.04f, 0.03f, 0.06f, yellow);
    rlPopMatrix();
}

// Crops other than yours follow the calendar: sown in spring, ripe in summer, cut in fall, bare in winter.
bool CropStage(float lead, float *growth, bool *stubble)
{
    float d = Clamp(((float)SeasonDay() - 1.0f + G.hour / 24.0f) / SEASON_DAYS + lead, 0.0f, 1.0f);
    int s = Season();

    *growth = s == SPRING ? 0.1f + 0.8f * d : s == SUMMER ? fminf(1.0f, 0.9f + 0.5f * d) : 1.0f;
    *stubble = s == FALL && d > 0.3f;
    return s != WINTER;
}

static void DrawCityCrops(void)
{
    float growth;
    bool stubble;
    if (!CropStage(0.0f, &growth, &stubble)) return;

    for (int f = 0; f < CITY_FARM_COUNT; f++) {
        Vector3 c = CityFarmSpot(f);
        if (XZDist(c, G.cam.position) > 140.0f) continue;
        LightFor(c);
        for (int j = 0; j < 6; j++)
            DrawWheat((Vector3){ c.x - 2 + 4.0f * (float)(j % 3), 0, c.z - 2 + 4.0f * (float)(j / 3) }, growth, false, stubble);
    }
}

void DrawFarm(void)
{
    float t = (float)GetTime();

    LightFor(G.home);
    for (int i = 0; i < MAX_PLOTS; i++) {
        const Plot *p = &G.plots[i];
        if (p->state == PLOT_WHEAT || p->state == PLOT_DEAD) DrawWheat(p->pos, p->growth, p->state == PLOT_DEAD, false);
        else if (p->state == PLOT_SAPLING) DrawSapling(p);
    }
    if (G.farmWater > 0) {
        float k = fminf(1.0f, G.farmWater / 2.0f);
        DrawShadedBox((Vector3){ 37.25f, 0.035f, 15.6f }, (Vector3){ 20.3f, 0.02f, 0.3f + 0.35f * k }, COL_WATER);
    }
    LightFor(G.coop);
    for (int i = 0; i < G.henCount; i++) DrawHen(&G.hens[i], i, t);
    for (int i = 0; i < G.coopEggs && i < 6; i++)
        Part(17.3f + 0.16f * (float)i, 0.06f, 29.5f, 0.1f, 0.12f, 0.1f, COL_LINEN);
    if (G.coopWater > 0) Part(19.6f, 0.33f, 33.2f, 0.9f, 0.02f, 0.2f, COL_WATER);
    DrawCityCrops();
    G.drawTorch = G.drawFog = 0;
}
