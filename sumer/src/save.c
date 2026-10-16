#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC    0x524D5553u
#define SAVE_VERSION  4
#define MASK_BYTES    ((MAX_ANIMALS + 7) / 8)
#define SAVE_MAX      8192
#define OLD_PLOTS     9
#define OLD_HENS      6
#define SLOT_FILE     "sumer.slot"

typedef struct { unsigned char d[SAVE_MAX]; int n; } Writer;
typedef struct { const unsigned char *d; int n, at; bool bad; } Reader;

typedef struct {
    int version, day, bagSlots, quiver, quiverMax, bow, tools, weapon, hens, eggs, regrowCount;
    int equip, armor, armorOwned, temple, prayDay, seasonSeen;
    float hour, yaw, pitch, food, health, feed, lay;
    float farmWater, coopWater, coopDry, blessT, rainStart, rainEnd;
    bool rainToday, rainDone;
    Vector3 pos;
    Slot bag[BAG_MAX], store[STORE_SLOTS];
    int plotState[MAX_PLOTS];
    float plotGrowth[MAX_PLOTS], plotDry[MAX_PLOTS];
    float henBorn[MAX_HENS], henMeal[MAX_HENS];
    int regrowIndex[MAX_RES];
    float regrow[MAX_RES];
    unsigned char animals[MASK_BYTES];
    int animalBytes;
    bool mace;
} SaveData;

static int slot;

int SaveSlot(void)
{
    return slot;
}

void SetSaveSlot(int s)
{
    if (s >= 0 && s < SAVE_SLOTS) slot = s;
}

const char *SaveFileName(int s)
{
    static const char *const names[SAVE_SLOTS] = { "sumer.sav", "sumer2.sav", "sumer3.sav" };
    return names[s >= 0 && s < SAVE_SLOTS ? s : 0];
}

void RememberSlot(void)
{
    FILE *f = fopen(SLOT_FILE, "w");
    if (!f) return;
    fprintf(f, "%d\n", slot + 1);
    fclose(f);
}

int LastSlot(void)
{
    FILE *f = fopen(SLOT_FILE, "r");
    int n = 1;
    if (!f) return 0;
    if (fscanf(f, "%d", &n) != 1) n = 1;
    fclose(f);
    return n >= 1 && n <= SAVE_SLOTS ? n - 1 : 0;
}

// Little-endian on every platform, so a save moves between Mac and Linux.
static void W8(Writer *w, unsigned v) { if (w->n < SAVE_MAX) w->d[w->n++] = (unsigned char)(v & 0xff); }
static void W16(Writer *w, unsigned v) { W8(w, v); W8(w, v >> 8); }
static void W32(Writer *w, unsigned v) { W16(w, v & 0xffff); W16(w, v >> 16); }
static void WF(Writer *w, float f) { unsigned u; memcpy(&u, &f, 4); W32(w, u); }

static unsigned R8(Reader *r) { if (r->at >= r->n) { r->bad = true; return 0; } return r->d[r->at++]; }
static unsigned R16(Reader *r) { unsigned lo = R8(r); return lo | (R8(r) << 8); }
static unsigned R32(Reader *r) { unsigned lo = R16(r); return lo | (R16(r) << 16); }
static float RF(Reader *r)
{
    unsigned u = R32(r);
    float f;
    memcpy(&f, &u, 4);
    if (!isfinite(f)) r->bad = true;
    return f;
}

static void WSlots(Writer *w, const Slot *s, int n)
{
    for (int i = 0; i < n; i++) {
        W8(w, (unsigned)s[i].item);
        W8(w, (unsigned)s[i].count);
    }
}

static void RSlots(Reader *r, Slot *s, int n)
{
    for (int i = 0; i < n; i++) {
        s[i].item = (int)R8(r);
        s[i].count = (int)R8(r);
        if (s[i].item >= IT_COUNT || s[i].count > ITEMS[s[i].item].stack || (s[i].item == IT_NONE) != (s[i].count == 0))
            r->bad = true;
    }
}

bool SaveGame(void)
{
    static Writer w;
    const Player *p = &G.player;
    int regrowing = 0;

    w.n = 0;
    W32(&w, SAVE_MAGIC);
    W8(&w, SAVE_VERSION);
    W16(&w, (unsigned)G.day);
    WF(&w, G.hour);
    WF(&w, p->pos.x); WF(&w, p->pos.y); WF(&w, p->pos.z);
    WF(&w, p->yaw); WF(&w, p->pitch);
    WF(&w, G.food); WF(&w, G.health);
    W8(&w, (unsigned)G.bagSlots); W8(&w, (unsigned)G.quiver); W8(&w, (unsigned)G.quiverMax);
    W8(&w, (unsigned)G.bow); W8(&w, (unsigned)G.tools); W8(&w, (unsigned)G.weapon);
    WSlots(&w, G.bag, BAG_MAX);
    WSlots(&w, G.store, STORE_SLOTS);
    W8(&w, (unsigned)G.henCount); W8(&w, (unsigned)G.coopEggs);
    WF(&w, G.coopFeed); WF(&w, G.layClock);
    W8(&w, MAX_PLOTS);
    for (int i = 0; i < MAX_PLOTS; i++) {
        W8(&w, (unsigned)G.plots[i].state);
        WF(&w, G.plots[i].growth);
        WF(&w, G.plots[i].dry);
    }
    for (int i = 0; i < G.resCount; i++) regrowing += G.res[i].regrow > 0;
    W16(&w, (unsigned)regrowing);
    for (int i = 0; i < G.resCount; i++) {
        if (G.res[i].regrow <= 0) continue;
        W16(&w, (unsigned)i);
        W16(&w, (unsigned)Clamp(G.res[i].regrow * 100.0f, 1.0f, 65535.0f));
    }
    W8(&w, (unsigned)G.equip); W8(&w, (unsigned)G.armor); W8(&w, (unsigned)G.armorOwned);
    W8(&w, G.templeAccess ? 1u : 0u); W16(&w, (unsigned)G.prayDay);
    WF(&w, G.farmWater); WF(&w, G.coopWater); WF(&w, G.coopDry); WF(&w, G.blessT);
    W16(&w, (unsigned)G.seasonSeen);
    W8(&w, G.rainToday ? 1u : 0u); W8(&w, G.rainDone ? 1u : 0u);
    WF(&w, G.rainStart); WF(&w, G.rainEnd);
    for (int i = 0; i < G.henCount; i++) {
        WF(&w, G.henBorn[i]);
        WF(&w, G.henMeal[i]);
    }
    unsigned char animals[MASK_BYTES];
    AnimalsAwaitingSpring(animals, MASK_BYTES);
    W8(&w, MASK_BYTES);
    for (int i = 0; i < MASK_BYTES; i++) W8(&w, animals[i]);
    W8(&w, G.hasMace ? 1u : 0u);

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s.tmp", SaveFileName(slot));
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(w.d, 1, (size_t)w.n, f) == (size_t)w.n;
    ok = (fclose(f) == 0) && ok;
    if (ok) ok = rename(tmp, SaveFileName(slot)) == 0;
    if (!ok) remove(tmp);
    return ok;
}

static bool Parse(Reader *r, SaveData *s)
{
    if (R32(r) != SAVE_MAGIC) return false;
    int version = (int)R8(r);
    if (version < 1 || version > SAVE_VERSION) return false;
    s->version = version;
    s->day = (int)R16(r);
    s->hour = RF(r);
    s->pos.x = RF(r); s->pos.y = RF(r); s->pos.z = RF(r);
    s->yaw = RF(r); s->pitch = RF(r);
    s->food = RF(r); s->health = RF(r);
    s->bagSlots = (int)R8(r); s->quiver = (int)R8(r); s->quiverMax = (int)R8(r);
    s->bow = (int)R8(r); s->tools = (int)R8(r); s->weapon = (int)R8(r);
    RSlots(r, s->bag, BAG_MAX);
    RSlots(r, s->store, STORE_SLOTS);
    s->hens = (int)R8(r); s->eggs = (int)R8(r);
    s->feed = RF(r); s->lay = RF(r);
    // The old farm had nine plots in a 3x3 square; they keep their rows in the wider field.
    int plots = version >= 3 ? (int)R8(r) : OLD_PLOTS;
    if (plots > MAX_PLOTS) return false;
    for (int k = 0; k < plots; k++) {
        int i = version >= 3 ? k : k % 3 + 5 * (k / 3);
        s->plotState[i] = (int)R8(r);
        s->plotGrowth[i] = RF(r);
        if (version >= 3) s->plotDry[i] = RF(r);
        if (s->plotState[i] > (version >= 3 ? PLOT_DEAD : PLOT_PALM) || s->plotDry[i] < 0) r->bad = true;
    }
    s->regrowCount = (int)R16(r);
    if (s->regrowCount > MAX_RES) return false;
    for (int i = 0; i < s->regrowCount; i++) {
        s->regrowIndex[i] = (int)R16(r);
        s->regrow[i] = (float)R16(r) / 100.0f;
    }
    s->equip = EQ_BOW;
    if (version >= 2) {
        s->equip = (int)R8(r); s->armor = (int)R8(r); s->armorOwned = (int)R8(r);
        s->temple = (int)R8(r); s->prayDay = (int)R16(r);
        if (s->equip > EQ_MACE || s->armor >= ARMOR_KINDS || s->armorOwned >= 1 << ARMOR_KINDS) r->bad = true;
        if (s->armor != ARMOR_NONE && !(s->armorOwned & (1 << s->armor))) r->bad = true;
    }
    s->farmWater = s->coopWater = WATER_DAYS;
    s->seasonSeen = (s->day - 1) / SEASON_DAYS;
    if (version >= 3) {
        s->farmWater = RF(r); s->coopWater = RF(r); s->coopDry = RF(r); s->blessT = RF(r);
        s->seasonSeen = (int)R16(r);
        s->rainToday = R8(r) != 0; s->rainDone = R8(r) != 0;
        s->rainStart = RF(r); s->rainEnd = RF(r);
        for (int i = 0; i < s->hens && i < MAX_HENS; i++) {
            s->henBorn[i] = RF(r);
            s->henMeal[i] = RF(r);
        }
        // Version 3 kept 64 animals of the smaller world's herds; that mask no longer matches anyone.
        if (version >= 4) {
            s->animalBytes = (int)R8(r);
            if (s->animalBytes > MASK_BYTES) return false;
            for (int i = 0; i < s->animalBytes; i++) s->animals[i] = (unsigned char)R8(r);
            s->mace = R8(r) != 0;
        } else {
            R32(r);
            R32(r);
        }
        if (s->farmWater < 0 || s->farmWater > WATER_DAYS || s->coopWater < 0 || s->coopWater > WATER_DAYS) r->bad = true;
        if (s->coopDry < 0 || s->blessT < 0 || s->blessT > 24.0f) r->bad = true;
    }

    if (s->equip == EQ_MACE && !s->mace) r->bad = true;
    if (r->bad || s->day < 1 || s->hour < 0 || s->hour >= 24) return false;
    if (s->bagSlots < BAG_START || s->bagSlots > BAG_MAX || s->quiverMax > QUIVER_BIG || s->quiver > s->quiverMax) return false;
    if (s->bow > 2 || s->weapon > 2 || s->hens > (s->version >= 3 ? MAX_HENS : OLD_HENS)) return false;
    return fabsf(s->pos.x) <= WORLD_HALF && fabsf(s->pos.z) <= WORLD_HALF;
}

// Rebuilds the world from the seed, then lays the saved progress over it.
bool LoadGame(void)
{
    static unsigned char data[SAVE_MAX];
    static SaveData s;
    FILE *f = fopen(SaveFileName(slot), "rb");
    if (!f) return false;
    int n = (int)fread(data, 1, sizeof(data), f);
    fclose(f);

    Reader r = { data, n, 0, false };
    memset(&s, 0, sizeof(s));
    if (!Parse(&r, &s)) return false;

    Player *p = &G.player;
    G.day = s.day;
    G.hour = s.hour;
    p->pos = s.pos;
    p->yaw = s.yaw;
    p->pitch = Clamp(s.pitch, -0.45f, 1.35f);
    p->facing = s.yaw + PI;
    G.food = Clamp(s.food, 0.0f, FOOD_MAX);
    G.health = Clamp(s.health, 1.0f, HEALTH_MAX);
    G.bagSlots = s.bagSlots;
    G.quiver = s.quiver;
    G.quiverMax = s.quiverMax;
    G.bow = s.bow;
    G.tools = s.tools;
    G.weapon = s.weapon;
    memcpy(G.bag, s.bag, sizeof(G.bag));
    memcpy(G.store, s.store, sizeof(G.store));
    G.coopEggs = s.eggs;
    G.coopFeed = Clamp(s.feed, 0.0f, FEED_MAX);
    G.layClock = s.lay;
    G.henCount = 0;
    for (int i = 0; i < s.hens && i < MAX_HENS; i++) {
        AddHen();
        if (s.version < 3) continue;
        G.henBorn[i] = s.henBorn[i];
        G.henMeal[i] = s.henMeal[i];
    }
    for (int i = 0; i < MAX_PLOTS; i++) {
        G.plots[i].state = (PlotState)s.plotState[i];
        G.plots[i].growth = s.plotGrowth[i];
        G.plots[i].dry = s.plotDry[i];
        if (G.plots[i].state != PLOT_PALM) continue;
        int k = AddResource(G.plots[i].pos.x, G.plots[i].pos.z, RES_PALM);
        if (k >= 0) G.res[k].farm = true;
    }
    // Older saves index trees and pits of a smaller map, so their regrowth is dropped.
    for (int i = 0; i < s.regrowCount && s.version >= SAVE_VERSION; i++)
        if (s.regrowIndex[i] < G.resCount) G.res[s.regrowIndex[i]].regrow = s.regrow[i];
    G.farmWater = s.farmWater;
    G.coopWater = s.coopWater;
    G.coopDry = s.coopDry;
    G.blessT = s.blessT;
    G.seasonSeen = s.seasonSeen;
    if (s.version >= 3) {
        G.rainToday = s.rainToday;
        G.rainDone = s.rainDone;
        G.rainStart = s.rainStart;
        G.rainEnd = s.rainEnd;
    } else {
        NewDay();
    }
    HoldAnimalsForSpring(s.animals, s.animalBytes);

    G.equip = s.equip;
    G.hasMace = s.mace;
    G.armor = s.armor;
    G.armorOwned = s.armorOwned;
    G.templeAccess = s.temple != 0;
    G.prayDay = s.prayDay;
    G.dawnDay = G.hour >= 6.0f ? G.day : G.day - 1;

    // A save from an older layout may stand inside a wall that moved.
    if (PointInSolid((Vector3){ p->pos.x, p->pos.y + 1.0f, p->pos.z })) p->pos = Vector3Add(G.stations[STN_BED], (Vector3){ -1.6f, 0, 0.5f });
    p->pos.y = GroundHeight(p->pos.x, p->pos.z, p->pos.y + 0.5f);
    UpdateSky();
    FollowCamera(1.0f);
    return true;
}

void DeleteSave(void)
{
    remove(SaveFileName(slot));
}

bool SaveSummary(int s, int *day, float *hour)
{
    unsigned char head[11];
    FILE *f = fopen(SaveFileName(s), "rb");
    if (!f) return false;
    int n = (int)fread(head, 1, sizeof(head), f);
    fclose(f);

    Reader r = { head, n, 0, false };
    if (R32(&r) != SAVE_MAGIC) return false;
    unsigned version = R8(&r);
    *day = (int)R16(&r);
    *hour = RF(&r);
    return !r.bad && version >= 1 && version <= SAVE_VERSION;
}
