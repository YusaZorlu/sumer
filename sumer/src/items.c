#include "game.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const ItemDef ITEMS[IT_COUNT] = {
    [IT_NONE]    = { "",            0,  0,  { 0, 0, 0, 0 } },
    [IT_DATES]   = { "Dates",       20, 8,  { 150,  72,  30, 255 } },
    [IT_REEDS]   = { "Reeds",       20, 0,  { 170, 176,  88, 255 } },
    [IT_SEEDS]   = { "Wheat seeds", 20, 0,  { 200, 170,  90, 255 } },
    [IT_WHEAT]   = { "Wheat",       20, 0,  { 222, 188,  92, 255 } },
    [IT_FLOUR]   = { "Flour",       10, 0,  { 236, 228, 206, 255 } },
    [IT_MILK]    = { "Milk",         4, 12, { 244, 244, 236, 255 } },
    [IT_DOUGH]   = { "Dough",        6, 0,  { 226, 204, 160, 255 } },
    [IT_BREAD]   = { "Bread",       10, 30, { 184, 120,  60, 255 } },
    [IT_EGG]     = { "Eggs",        12, 6,  { 240, 230, 210, 255 } },
    [IT_ROAST]   = { "Roast meat",   8, 35, { 140,  70,  40, 255 } },
    [IT_ARROWS]  = { "Arrows",      10, 0,  { 120,  92,  60, 255 } },
    [IT_GAZELLE] = { "Gazelle",      1, 0,  { 192, 142,  84, 255 } },
    [IT_ONAGER]  = { "Onager",       1, 0,  { 170, 150, 128, 255 } },
    [IT_BOAR]    = { "Boar",         1, 0,  {  82,  66,  56, 255 } },
    [IT_GOOSE]   = { "Goose",        1, 0,  { 200, 196, 186, 255 } },
    [IT_WOLF]    = { "Wolf pelt",    5, 0,  { 128, 122, 112, 255 } },
    [IT_CLAY]    = { "Clay",        20, 0,  { 122, 100,  82, 255 } },
    [IT_POT]     = { "Clay pots",    6, 0,  { 178, 104,  64, 255 } },
    [IT_WOOD]    = { "Wood",        20, 0,  { 112,  80,  50, 255 } },
    [IT_COPPER]  = { "Copper ore",  20, 0,  {  64, 140, 118, 255 } },
    [IT_TIN]     = { "Tin",         10, 0,  { 170, 172, 176, 255 } },
    [IT_BRONZE]  = { "Bronze",      10, 0,  { 190, 130,  60, 255 } },
    [IT_FISH]    = { "Fish",        10, 5,  { 150, 164, 170, 255 } },
    [IT_GRILLED] = { "Grilled fish",10, 20, { 196, 140,  80, 255 } },
    [IT_COWHIDE] = { "Cowhide",      5, 0,  { 130,  92,  60, 255 } },
    [IT_SHEEPSKIN] = { "Sheepskin",  5, 0,  { 222, 214, 196, 255 } },
    [IT_BUCKET]  = { "Bucket",       2, 0,  { 120,  96,  70, 255 } },
    [IT_WATER]   = { "Water bucket", 2, 0,  {  70, 130, 160, 255 } },
    [IT_WASTE]   = { "Plant waste", 10, 0,  { 140, 120,  70, 255 } },
    [IT_HEN]     = { "Hen",          2, 0,  { 220, 200, 170, 255 } },
    [IT_HERB]    = { "Herbs",       10, 0,  { 120, 170,  96, 255 } },
    [IT_SHEEP]   = { "Sheep",        1, 0,  { 226, 220, 204, 255 } },
};

const char *const BOW_NAMES[3] = { "Hunting bow", "Recurve bow", "Composite bow" };

const char *ArmorName(int armor)
{
    switch (armor) {
    case ARMOR_WOLF:    return L("Wolf-pelt cloak");
    case ARMOR_LEATHER: return L("Leather armor");
    case ARMOR_BRONZE:  return L("Bronze armor");
    default:            return L("No armor");
    }
}

float ArmorReduction(int armor)
{
    switch (armor) {
    case ARMOR_WOLF:    return 0.15f;
    case ARMOR_LEATHER: return 0.25f;
    case ARMOR_BRONZE:  return 0.40f;
    default:            return 0.0f;
    }
}

// A message for the acting player: on this screen, or sent to the guest the host is acting for.
void Say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (G.role == NET_HOST && G.actor != G.self) {
        if (PlayerIn(G.actor)) NetSayV(G.actor, fmt, ap);
    } else {
        vsnprintf(G.msg, sizeof(G.msg), L(fmt), ap);
        G.msgT = 3.0f;
    }
    va_end(ap);
}

void Fmt(char *buf, int size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)size, L(fmt), ap);
    va_end(ap);
}

int SlotsCount(const Slot *s, int n, int item)
{
    int total = 0;
    for (int i = 0; i < n; i++)
        if (s[i].item == item) total += s[i].count;
    return total;
}

int SlotsSpace(const Slot *s, int n, int item)
{
    int room = 0;
    for (int i = 0; i < n; i++) {
        if (s[i].item == IT_NONE) room += ITEMS[item].stack;
        else if (s[i].item == item) room += ITEMS[item].stack - s[i].count;
    }
    return room;
}

// Tops up partial stacks before opening a new basket.
int SlotsAdd(Slot *s, int n, int item, int count)
{
    int added = 0;

    for (int i = 0; i < n && added < count; i++) {
        if (s[i].item != item) continue;
        int take = ITEMS[item].stack - s[i].count;
        if (take > count - added) take = count - added;
        s[i].count += take;
        added += take;
    }
    for (int i = 0; i < n && added < count; i++) {
        if (s[i].item != IT_NONE) continue;
        int take = ITEMS[item].stack;
        if (take > count - added) take = count - added;
        s[i] = (Slot){ item, take };
        added += take;
    }
    return added;
}

int SlotsRemove(Slot *s, int n, int item, int count)
{
    int removed = 0;

    for (int i = n - 1; i >= 0 && removed < count; i--) {
        if (s[i].item != item) continue;
        int take = s[i].count;
        if (take > count - removed) take = count - removed;
        s[i].count -= take;
        removed += take;
        if (s[i].count == 0) s[i].item = IT_NONE;
    }
    return removed;
}

int MoveStack(Slot *src, Slot *dst, int dn)
{
    if (src->item == IT_NONE) return 0;
    int moved = SlotsAdd(dst, dn, src->item, src->count);
    src->count -= moved;
    if (src->count == 0) src->item = IT_NONE;
    return moved;
}

int BagCount(int item)
{
    return SlotsCount(G.bag, G.bagSlots, item);
}

int BagAdd(int item, int count)
{
    return SlotsAdd(G.bag, G.bagSlots, item, count);
}

int BagRemove(int item, int count)
{
    return SlotsRemove(G.bag, G.bagSlots, item, count);
}

int GiveArrows(int n)
{
    int toQuiver = G.quiverMax - G.quiver;
    if (toQuiver > n) toQuiver = n;
    G.quiver += toQuiver;
    return toQuiver + BagAdd(IT_ARROWS, n - toQuiver);
}

void RefillQuiver(void)
{
    int need = G.quiverMax - G.quiver;
    if (need <= 0) {
        Say("Quiver is already full.");
        return;
    }
    int got = BagRemove(IT_ARROWS, need);
    G.quiver += got;
    if (got) Say("Quiver refilled: %d/%d arrows.", G.quiver, G.quiverMax);
    else Say("No spare arrows in your baskets. The storeroom at home has more.");
}

void RefillQuiverFromStore(void)
{
    int got = SlotsRemove(G.store, STORE_SLOTS, IT_ARROWS, G.quiverMax - G.quiver);
    G.quiver += got;
    if (got) Say("Quiver refilled: %d/%d arrows.", G.quiver, G.quiverMax);
    else if (G.quiver == G.quiverMax) Say("Quiver is already full.");
    else Say("The storeroom is out of arrows. The bowyer sells more.");
}

bool EatSlot(int slot)
{
    Slot *s = &G.bag[slot];
    int food = ITEMS[s->item].food;

    if (s->item == IT_NONE || food <= 0) return false;
    if (G.food >= FOOD_MAX - 0.5f) {
        Say("You are not hungry.");
        return false;
    }
    G.food = fminf(FOOD_MAX, G.food + (float)food);
    if (s->item == IT_MILK) Say("Drank %s. Food %d/%d", ItemName(s->item), (int)G.food, (int)FOOD_MAX);
    else Say("Ate %s. Food %d/%d", ItemName(s->item), (int)G.food, (int)FOOD_MAX);
    if (--s->count == 0) s->item = IT_NONE;
    return true;
}

// A mouthful of herbs heals a little at once and then keeps healing for a while.
bool UseHerb(void)
{
    if (BagCount(IT_HERB) == 0) return false;
    if (G.health >= HEALTH_MAX - 0.5f) {
        Say("You are not hurt.");
        return false;
    }
    BagRemove(IT_HERB, 1);
    G.health = fminf(HEALTH_MAX, G.health + HERB_BURST);
    G.herbT = HERB_SECONDS;
    Say("You chew the herbs. Your wounds close quickly for a while.");
    return true;
}

const char *OfferOutput(const Offer *o)
{
    static char buf[128];

    switch (o->kind) {
    case OUT_ITEM:   Fmt(buf, sizeof(buf), "%d %s", o->nOut, ItemName(o->out)); break;
    case OUT_HEN:    Fmt(buf, sizeof(buf), "1 hen for your coop"); break;
    case OUT_BASKET: Fmt(buf, sizeof(buf), "1 reed basket (+1 slot)"); break;
    case OUT_BOW:    Fmt(buf, sizeof(buf), "%s", L(BOW_NAMES[o->out])); break;
    case OUT_QUIVER: Fmt(buf, sizeof(buf), "Large quiver (%d arrows)", QUIVER_BIG); break;
    case OUT_TOOL:
        Fmt(buf, sizeof(buf), "%s", L(o->out == TOOL_AXE ? "Bronze axe (double wood)"
                                      : o->out == TOOL_PICK ? "Bronze pick (double ore)" : "Bronze arrowheads (+10 damage)"));
        break;
    case OUT_WEAPON: Fmt(buf, sizeof(buf), "%s", WeaponName(o->out)); break;
    case OUT_ARMOR:  Fmt(buf, sizeof(buf), "%s", ArmorName(o->out)); break;
    case OUT_MACE:   Fmt(buf, sizeof(buf), "%s (hits all in front, key 4)", MaceName()); break;
    }
    return buf;
}

// Simulates on a copy of the bag so freed baskets count as room.
bool OfferCheck(const Offer *o, const char **why)
{
    static char reason[128];
    *why = reason;

    if (o->in1 && BagCount(o->in1) < o->n1) {
        Fmt(reason, sizeof(reason), "Need %d %s", o->n1, ItemName(o->in1));
        return false;
    }
    if (o->in2 && BagCount(o->in2) < o->n2) {
        Fmt(reason, sizeof(reason), "Need %d %s", o->n2, ItemName(o->in2));
        return false;
    }
    if (o->in3 && BagCount(o->in3) < o->n3) {
        Fmt(reason, sizeof(reason), "Need %d %s", o->n3, ItemName(o->in3));
        return false;
    }

    switch (o->kind) {
    case OUT_ITEM: {
        Slot tmp[BAG_MAX];
        memcpy(tmp, G.bag, sizeof(tmp));
        if (o->in1) SlotsRemove(tmp, G.bagSlots, o->in1, o->n1);
        if (o->in2) SlotsRemove(tmp, G.bagSlots, o->in2, o->n2);
        if (o->in3) SlotsRemove(tmp, G.bagSlots, o->in3, o->n3);
        int room = SlotsSpace(tmp, G.bagSlots, o->out);
        if (o->out == IT_ARROWS) room += G.quiverMax - G.quiver;
        if (room < o->nOut) {
            Fmt(reason, sizeof(reason), "No room in your baskets");
            return false;
        }
        break;
    }
    case OUT_HEN:
        if (G.henCount >= MAX_HENS) {
            Fmt(reason, sizeof(reason), "Your coop is full (%d hens)", MAX_HENS);
            return false;
        }
        break;
    case OUT_BASKET:
        if (G.bagSlots >= BAG_MAX) {
            Fmt(reason, sizeof(reason), "You can't carry more baskets");
            return false;
        }
        break;
    case OUT_BOW:
        if (G.bow >= o->out) {
            Fmt(reason, sizeof(reason), "Your bow is already as good");
            return false;
        }
        break;
    case OUT_QUIVER:
        if (G.quiverMax >= QUIVER_BIG) {
            Fmt(reason, sizeof(reason), "You already have it");
            return false;
        }
        break;
    case OUT_TOOL:
        if (G.tools & o->out) {
            Fmt(reason, sizeof(reason), "You already have it");
            return false;
        }
        break;
    case OUT_WEAPON:
        if (G.weapon >= o->out) {
            Fmt(reason, sizeof(reason), "Your weapon is already as good");
            return false;
        }
        break;
    case OUT_ARMOR:
        if (G.armorOwned & (1 << o->out)) {
            Fmt(reason, sizeof(reason), "You already have one");
            return false;
        }
        break;
    case OUT_MACE:
        if (G.hasMace) {
            Fmt(reason, sizeof(reason), "You already have one");
            return false;
        }
        break;
    }
    reason[0] = '\0';
    return true;
}

void OfferPay(const Offer *o)
{
    if (o->in1) BagRemove(o->in1, o->n1);
    if (o->in2) BagRemove(o->in2, o->n2);
    if (o->in3) BagRemove(o->in3, o->n3);
}

bool OfferApply(const Offer *o)
{
    const char *why;

    if (!OfferCheck(o, &why)) {
        Say("%s.", why);
        return false;
    }
    OfferPay(o);

    switch (o->kind) {
    case OUT_ITEM:
        if (o->out == IT_ARROWS) GiveArrows(o->nOut);
        else BagAdd(o->out, o->nOut);
        break;
    case OUT_HEN:    AddHen(); break;
    case OUT_BASKET: G.bagSlots++; break;
    case OUT_BOW:    G.bow = o->out; break;
    case OUT_QUIVER: G.quiverMax = QUIVER_BIG; break;
    case OUT_TOOL:   G.tools |= o->out; break;
    case OUT_WEAPON: G.weapon = o->out; break;
    case OUT_ARMOR:
        G.armorOwned |= 1 << o->out;
        if (ArmorReduction(o->out) > ArmorReduction(G.armor)) G.armor = o->out;
        break;
    case OUT_MACE:
        G.hasMace = true;
        break;
    }
    return true;
}
