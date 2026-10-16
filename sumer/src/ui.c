#include "game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_PANEL    CLITERAL(Color){ 30, 24, 20, 238 }
#define C_EDGE     CLITERAL(Color){ 150, 120, 80, 255 }
#define C_SLOT     CLITERAL(Color){ 58, 48, 38, 255 }
#define C_SLOT_HI  CLITERAL(Color){ 86, 70, 52, 255 }
#define C_TEXT     CLITERAL(Color){ 240, 230, 210, 255 }
#define C_DIM      CLITERAL(Color){ 150, 140, 125, 255 }
#define C_GOLD     CLITERAL(Color){ 240, 200, 120, 255 }
#define C_BAD      CLITERAL(Color){ 230, 110, 90, 255 }
#define C_GOOD     CLITERAL(Color){ 150, 210, 120, 255 }

enum { CLICK_NONE, CLICK_LEFT, CLICK_RIGHT };

static char hoverText[256];

void SetPanel(Panel p)
{
    G.panel = p;
    G.player.aiming = false;
    G.player.draw = 0;
    if (p == PANEL_NONE) {
        DisableCursor();
        WantPointer(true);
        G.cursorLocked = true;
        G.ignoreClick = true;
    } else {
        EnableCursor();
        WantPointer(false);
        G.cursorLocked = false;
    }
}

static float FontSize(int size)
{
    return (float)(size < 10 ? 10 : size);
}

static int Measure(const char *s, int size)
{
    return (int)MeasureTextEx(GameFont(), s, FontSize(size), (float)((int)FontSize(size) / 10)).x;
}

// Labels are translated here, so every literal passed in follows the language setting.
static void Text(const char *s, float x, float y, int size, Color c)
{
    DrawTextEx(GameFont(), L(s), (Vector2){ (float)(int)x, (float)(int)y }, FontSize(size), (float)((int)FontSize(size) / 10), c);
}

static void CenterText(const char *s, float cx, float y, int size, Color c)
{
    s = L(s);
    Text(s, cx - (float)Measure(s, size) / 2, y, size, c);
}

// Breaks the translated text into lines no wider than 'width'; returns the height used.
static float Paragraph(const char *s, float x, float y, float width, int size, float lineH)
{
    char line[512], trial[512];
    int len = 0;
    float used = 0;

    s = L(s);
    line[0] = '\0';
    while (*s) {
        const char *word = s;
        while (*s && *s != ' ') s++;
        int wl = (int)(s - word), tl = len;
        if (len + 1 + wl >= (int)sizeof(trial)) break;
        memcpy(trial, line, (size_t)len);
        if (len) trial[tl++] = ' ';
        memcpy(trial + tl, word, (size_t)wl);
        tl += wl;
        trial[tl] = '\0';
        if (len && Measure(trial, size) > (int)width) {
            Text(line, x, y + used, size, C_TEXT);
            used += lineH;
            memcpy(line, word, (size_t)wl);
            len = wl;
        } else {
            memcpy(line, trial, (size_t)tl);
            len = tl;
        }
        line[len] = '\0';
        while (*s == ' ') s++;
    }
    if (len) {
        Text(line, x, y + used, size, C_TEXT);
        used += lineH;
    }
    return used;
}

// Key fallback: a click pressed and released within one frame is lost.
static bool Button(Rectangle r, const char *label, bool enabled, int key)
{
    bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, enabled ? (hover ? (Color){ 150, 110, 60, 255 } : (Color){ 112, 82, 48, 255 }) : (Color){ 60, 54, 48, 255 });
    DrawRectangleLinesEx(r, 1, C_EDGE);
    CenterText(label, r.x + r.width / 2, r.y + (r.height - 18) / 2, 18, enabled ? C_TEXT : C_DIM);
    return (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || (enabled && key && IsKeyPressed(key));
}

static int SlotKey(int i)
{
    static const int keys[BAG_MAX] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX,
                                       KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO, KEY_MINUS, KEY_EQUAL };
    return (i >= 0 && i < BAG_MAX) ? keys[i] : 0;
}

static Rectangle PanelRect(float w, float h)
{
    Rectangle r = { ((float)GetScreenWidth() - w) / 2, ((float)GetScreenHeight() - h) / 2, w, h };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.35f));
    DrawRectangleRec(r, C_PANEL);
    DrawRectangleLinesEx(r, 2, C_EDGE);
    return r;
}

static int SlotWidget(Rectangle r, Slot s, bool locked, int index)
{
    static const char *labels[BAG_MAX] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=" };
    bool hover = !locked && CheckCollisionPointRec(GetMousePosition(), r);

    DrawRectangleRec(r, locked ? (Color){ 40, 34, 30, 255 } : (hover ? C_SLOT_HI : C_SLOT));
    DrawRectangleLinesEx(r, 1, hover ? C_GOLD : (Color){ 90, 74, 56, 255 });
    if (locked) {
        CenterText("locked", r.x + r.width / 2, r.y + r.height / 2 - 7, 14, (Color){ 90, 80, 70, 255 });
        return CLICK_NONE;
    }
    if (index >= 0) Text(labels[index], r.x + r.width - 16, r.y + 6, 14, C_DIM);
    if (s.item != IT_NONE) {
        const ItemDef *d = &ITEMS[s.item];
        DrawRectangle((int)r.x + 8, (int)r.y + 8, 18, 18, d->color);
        DrawRectangleLines((int)r.x + 8, (int)r.y + 8, 18, 18, Fade(BLACK, 0.5f));
        Text(d->name, r.x + 8, r.y + 32, 14, C_TEXT);
        const char *n = TextFormat(L("%d/%d"), s.count, d->stack);
        Text(n, r.x + r.width - 8 - (float)Measure(n, 16), r.y + r.height - 22, 16, s.count == d->stack ? C_GOLD : C_DIM);
        if (hover) {
            if (d->food > 0)
                Fmt(hoverText, sizeof(hoverText), s.item == IT_MILK ? "%s: click to drink (+%d food), right-click or X to drop"
                    : "%s: click to eat (+%d food), right-click or X to drop", ItemName(s.item), d->food);
            else if (s.item == IT_ARROWS)
                Fmt(hoverText, sizeof(hoverText), "Arrows: click to refill your quiver, right-click or X to drop");
            else if (s.item == IT_HERB)
                Fmt(hoverText, sizeof(hoverText), "Herbs: click to chew one (heals now and for %d seconds), right-click or X to drop", (int)HERB_SECONDS);
            else
                Fmt(hoverText, sizeof(hoverText), "%s: right-click or X to drop", ItemName(s.item));
        }
    } else {
        CenterText("empty", r.x + r.width / 2, r.y + r.height / 2 - 7, 14, (Color){ 100, 88, 74, 255 });
    }
    if (index >= 0 && IsKeyPressed(SlotKey(index))) return CLICK_LEFT;
    if (!hover) return CLICK_NONE;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return CLICK_LEFT;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_X)) return CLICK_RIGHT;
    return CLICK_NONE;
}

static void FoodBar(float x, float y, float w)
{
    DrawRectangle((int)x, (int)y, (int)w, 14, Fade(BLACK, 0.5f));
    DrawRectangle((int)x, (int)y, (int)(w * G.food / FOOD_MAX), 14, G.food < 20 ? C_BAD : (Color){ 210, 160, 70, 255 });
    DrawRectangleLines((int)x, (int)y, (int)w, 14, Fade(BLACK, 0.6f));
}

static void DrawBagPanel(void)
{
    Rectangle r = PanelRect(900, 460);
    float x0 = r.x + 24, y0 = r.y + 78, sw = 115, shh = 80, gap = 8;

    hoverText[0] = '\0';
    Text(TextFormat(L("Baskets  %d/%d"), G.bagSlots, BAG_MAX), r.x + 24, r.y + 18, 26, C_GOLD);
    Text("Each basket holds one kind of thing: 20 dates, 10 arrows, 4 jugs of milk, or a single carcass.", r.x + 24, r.y + 50, 15, C_DIM);

    for (int i = 0; i < BAG_MAX; i++) {
        Rectangle s = { x0 + (float)(i % 6) * (sw + gap), y0 + (float)(i / 6) * (shh + gap), sw, shh };
        int click = SlotWidget(s, G.bag[i], i >= G.bagSlots, i);
        if (click == CLICK_LEFT) {
            if (G.bag[i].item == IT_ARROWS) RefillQuiver();
            else if (G.bag[i].item == IT_HERB) UseHerb();
            else EatSlot(i);
        } else if (click == CLICK_RIGHT && G.bag[i].item != IT_NONE) {
            Say("Dropped %d %s.", G.bag[i].count, ItemName(G.bag[i].item));
            G.bag[i] = (Slot){ IT_NONE, 0 };
        }
    }

    float y = y0 + 2 * (shh + gap) + 14;
    float lw = fmaxf(60.0f, (float)Measure(L("Food"), 18) + 12.0f);
    Text("Food", x0, y, 18, C_TEXT);
    FoodBar(x0 + lw, y + 3, 320.0f - lw);
    Text(TextFormat(L("%d/%d"), (int)G.food, (int)FOOD_MAX), x0 + 330, y, 18, C_DIM);
    Text(TextFormat(L("Quiver %d/%d    arrow speed %d m/s"), G.quiver, G.quiverMax, (int)BowSpeed()), x0 + 420, y, 18, C_TEXT);
    y += 30;

    static const int eqs[4] = { EQ_BOW, EQ_MELEE, EQ_MACE, EQ_NONE };
    Text("In hand", x0, y + 8, 18, C_TEXT);
    for (int k = 0; k < 4; k++) {
        Rectangle b = { x0 + 90 + 176.0f * (float)k, y, 168, 32 };
        bool owned = eqs[k] != EQ_MACE || G.hasMace;
        const char *label = owned ? EquipName(eqs[k]) : TextFormat(L("(%s)"), MaceName());
        if (G.equip == eqs[k]) DrawRectangleLinesEx((Rectangle){ b.x - 3, b.y - 3, b.width + 6, b.height + 6 }, 2, C_GOLD);
        if (Button(b, label, owned && G.equip != eqs[k], 0)) SetEquip(eqs[k]);
    }
    y += 40;
    static const int armors[4] = { ARMOR_BRONZE, ARMOR_LEATHER, ARMOR_WOLF, ARMOR_NONE };
    static const char *const shortName[4] = { "Bronze", "Leather", "Wolf cloak", "No armor" };
    Text("Armor", x0, y + 8, 18, C_TEXT);
    for (int k = 0; k < 4; k++) {
        Rectangle b = { x0 + 90 + 146.0f * (float)k, y, 138, 32 };
        bool owned = armors[k] == ARMOR_NONE || (G.armorOwned & (1 << armors[k]));
        if (G.armor == armors[k]) DrawRectangleLinesEx((Rectangle){ b.x - 3, b.y - 3, b.width + 6, b.height + 6 }, 2, C_GOLD);
        if (Button(b, owned ? shortName[k] : TextFormat(L("(%s)"), L(shortName[k])), owned && G.armor != armors[k], 0)) {
            G.armor = armors[k];
            Say(G.armor == ARMOR_NONE ? "You take off your armor." : "You put on the %s.", ArmorName(G.armor));
        }
    }
    Text(TextFormat(L("-%d%% damage"), (int)(ArmorReduction(G.armor) * 100)), x0 + 690, y + 8, 16, C_DIM);
    y += 44;
    Text(TextFormat(L("Coop: %d hens, feed %d, %d eggs waiting, water %.1f days    Farm water %.1f days"), G.henCount, (int)G.coopFeed,
                    G.coopEggs, G.coopWater, G.farmWater), x0, y, 18, C_TEXT);
    Text(hoverText[0] ? hoverText : "Click or 1-6: eat / drink / chew herbs / refill quiver    Right-click or X: drop    Tab: close",
         x0, r.y + r.height - 34, 16, hoverText[0] ? C_GOLD : C_DIM);
}

static void DrawStoragePanel(void)
{
    Rectangle r = PanelRect(1000, 480);
    float sw = 96, shh = 70, gap = 7;
    float bx = r.x + 24, sx = r.x + 24 + 3 * (sw + gap) + 30, y0 = r.y + 98;

    hoverText[0] = '\0';
    Text("Storeroom", r.x + 24, r.y + 18, 26, C_GOLD);
    Text("Click a basket to move it between what you carry and the storage jars. Number keys store a carried basket.", r.x + 24, r.y + 50, 15, C_DIM);
    Text("Carried", bx, y0 - 22, 16, C_TEXT);
    Text("Storage jars", sx, y0 - 22, 16, C_TEXT);

    for (int i = 0; i < BAG_MAX; i++) {
        Rectangle s = { bx + (float)(i % 3) * (sw + gap), y0 + (float)(i / 3) * (shh + gap), sw, shh };
        if (SlotWidget(s, G.bag[i], i >= G.bagSlots, i) == CLICK_LEFT && G.bag[i].item != IT_NONE)
            if (MoveStack(&G.bag[i], G.store, STORE_SLOTS) == 0) Say("The storage jars are full.");
    }
    for (int i = 0; i < STORE_SLOTS; i++) {
        Rectangle s = { sx + (float)(i % 6) * (sw + gap), y0 + (float)(i / 6) * (shh + gap), sw, shh };
        if (SlotWidget(s, G.store[i], false, -1) == CLICK_LEFT && G.store[i].item != IT_NONE)
            if (MoveStack(&G.store[i], G.bag, G.bagSlots) == 0) Say("No room in your baskets.");
    }

    float y = r.y + r.height - 50;
    if (Button((Rectangle){ r.x + 24, y, 230, 34 }, TextFormat(L("[R] Refill quiver %d/%d"), G.quiver, G.quiverMax), G.quiver < G.quiverMax, KEY_R))
        RefillQuiverFromStore();
    Text(TextFormat(L("Arrows in storage: %d"), SlotsCount(G.store, STORE_SLOTS, IT_ARROWS)), r.x + 272, y + 8, 18, C_TEXT);
    if (hoverText[0]) Text(hoverText, r.x + 500, y + 8, 15, C_GOLD);
}

static void OfferInputs(const Offer *o, char *buf, int size)
{
    if (o->in3)
        Fmt(buf, size, "%d %s + %d %s + %d %s", o->n1, ItemName(o->in1), o->n2, ItemName(o->in2), o->n3, ItemName(o->in3));
    else if (o->in2)
        Fmt(buf, size, "%d %s + %d %s", o->n1, ItemName(o->in1), o->n2, ItemName(o->in2));
    else
        Fmt(buf, size, "%d %s", o->n1, ItemName(o->in1));
}

static void DrawTradePanel(void)
{
    const Trader *t = &G.traders[G.trader];
    const float row = 44.0f;
    float h = 140.0f + row * (float)t->offerCount;
    Rectangle r = PanelRect(1000, h);
    char inputs[160];

    Text(t->name, r.x + 24, r.y + 18, 26, C_GOLD);
    Text(TextFormat(L("\"%s\""), L(t->greeting)), r.x + 24, r.y + 52, 16, C_DIM);

    for (int i = 0; i < t->offerCount; i++) {
        const Offer *o = &t->offers[i];
        const char *why;
        bool ok = OfferCheck(o, &why);
        float y = r.y + 84 + row * (float)i;

        DrawRectangle((int)r.x + 16, (int)y, (int)r.width - 32, (int)row - 6, (i & 1) ? (Color){ 40, 33, 28, 255 } : (Color){ 48, 40, 33, 255 });
        OfferInputs(o, inputs, sizeof(inputs));
        Text(inputs, r.x + 28, y + 10, 17, C_TEXT);
        Text("->", r.x + 385, y + 10, 17, C_DIM);
        Text(OfferOutput(o), r.x + 412, y + 10, 17, C_GOLD);
        const char *have = o->in2 ? TextFormat(L("have %d / %d"), BagCount(o->in1), BagCount(o->in2)) : TextFormat(L("have %d"), BagCount(o->in1));
        Text(ok ? have : why, r.x + 680, y + 12, 14, ok ? C_GOOD : C_BAD);

        static const int extraKeys[3] = { KEY_ZERO, KEY_MINUS, KEY_EQUAL };
        static const char *const extraLabels[3] = { "[0] Barter", "[-] Barter", "[=] Barter" };
        int key = i < 9 ? KEY_ONE + i : i < 12 ? extraKeys[i - 9] : 0;
        const char *label = i < 9 ? TextFormat(L("[%d] Barter"), i + 1) : i < 12 ? extraLabels[i - 9] : "Barter";
        if (Button((Rectangle){ r.x + r.width - 136, y + 4, 112, 30 }, label, ok, key)) Barter(G.trader, i);
    }
    Text("Click Barter or press the offer's number    Esc: close", r.x + 24, r.y + r.height - 34, 16, C_DIM);
}

// Playing with others the world goes on while the menu is open, and there are no save slots: the
// host's browser keeps the game.
static void DrawNetPausePanel(void)
{
    Rectangle r = PanelRect(620, 470);
    float bx = r.x + 50, bw = 240, gap = r.width - 100 - 2 * bw, y = r.y + 128;
    bool host = G.role == NET_HOST;

    CenterText("Paused", r.x + r.width / 2, r.y + 20, 30, C_GOLD);
    CenterText("The world goes on while this menu is open. H: controls.", r.x + r.width / 2, r.y + 64, 16, C_TEXT);
    CenterText(TextFormat(L("Room \"%s\". The host's browser keeps the game and everyone's things."), NetRoom()), r.x + r.width / 2, r.y + 92, 15, C_DIM);
    for (int s = 0; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active) continue;
        DrawRectangle((int)bx, (int)y + 2, 16, 16, PlayerColor(s));
        DrawRectangleLines((int)bx, (int)y + 2, 16, 16, Fade(BLACK, 0.6f));
        DrawTextEx(GameFont(), p->name, (Vector2){ bx + 26, y }, 18, 1, s == G.self ? C_GOLD : C_TEXT);
        const char *tag = s == 0 ? (s == G.self ? "host, you" : "host") : s == G.self ? "you" : "";
        if (tag[0]) Text(TextFormat("(%s)", L(tag)), bx + 210, y + 2, 15, C_DIM);
        Text(PlayerColorName(s), bx + 330, y + 2, 15, C_DIM);
        y += 24;
    }

    const char *lang = Language() == LANG_TR ? "Dil / Language: Türkçe" : "Language / Dil: English";
    if (Button((Rectangle){ r.x + r.width / 2 - 150, r.y + 274, 300, 34 }, lang, true, KEY_L)) {
        SetLanguage(Language() == LANG_TR ? LANG_EN : LANG_TR);
        SaveLanguage();
    }
    y = r.y + r.height - 130;
    if (Button((Rectangle){ bx, y, bw, 40 }, "Resume (Esc)", true, 0)) SetPanel(PANEL_NONE);
    if (Button((Rectangle){ bx + bw + gap, y, bw, 40 }, host ? "Save (F5)" : "Hand in my things (F5)", true, 0)) SaveNow();
    if (Button((Rectangle){ bx, y + 54, r.width - 100, 40 }, host ? "Save and end the game for everyone (Q)" : "Leave the game (Q)", true, 0)) G.quit = true;
}

static void DrawPausePanel(void)
{
    if (Online()) {
        DrawNetPausePanel();
        return;
    }
    Rectangle r = PanelRect(620, 470);
    float bx = r.x + 50, bw = 240, gap = r.width - 100 - 2 * bw;
    int active = SaveSlot();

    CenterText("Paused", r.x + r.width / 2, r.y + 20, 30, C_GOLD);
    CenterText("Press H for the controls and tips.", r.x + r.width / 2, r.y + 64, 18, C_TEXT);
    CenterText("Progress is saved to the slot in play when you sleep and when you quit.", r.x + r.width / 2, r.y + 94, 15, C_DIM);

    for (int k = 0; k < SAVE_SLOTS; k++) {
        float y = r.y + 126 + 46.0f * (float)k;
        int day;
        float hour;
        bool has = SaveSummary(k, &day, &hour);
        const char *info = has ? TextFormat(L("Day %d  %02d:%02d"), day, (int)hour, (int)((hour - floorf(hour)) * 60.0f)) : "empty";
        DrawRectangle((int)bx, (int)y, (int)(r.width - 100), 40, k == active ? (Color){ 64, 52, 38, 255 } : (Color){ 44, 36, 30, 255 });
        Text(TextFormat(L("Slot %d"), k + 1), bx + 12, y + 10, 20, k == active ? C_GOLD : C_TEXT);
        Text(k == active ? "playing" : info, bx + 100, y + 12, 17, k == active ? C_GOOD : has ? C_TEXT : C_DIM);
        if (k == active) continue;
        if (Button((Rectangle){ bx + r.width - 100 - 190, y + 5, 180, 30 }, has ? "Switch to this" : "Start a game here", true, 0))
            G.requestSlot = k + 1;
    }

    const char *lang = Language() == LANG_TR ? "Dil / Language: Türkçe" : "Language / Dil: English";
    if (Button((Rectangle){ r.x + r.width / 2 - 150, r.y + 274, 300, 34 }, lang, true, KEY_L)) {
        SetLanguage(Language() == LANG_TR ? LANG_EN : LANG_TR);
        SaveLanguage();
    }

    float y = r.y + r.height - 130;
    if (Button((Rectangle){ bx, y, bw, 40 }, "Resume (Esc)", true, 0)) SetPanel(PANEL_NONE);
    if (Button((Rectangle){ bx + bw + gap, y, bw, 40 }, "Save (F5)", true, 0)) {
        if (SaveGame()) Say("Game saved to %s.", SaveFileName(active));
        else Say("Could not write %s in this folder.", SaveFileName(active));
    }
    const char *erase = G.confirmNew ? TextFormat(L("Click again: erase slot %d"), active + 1) : TextFormat(L("New game in slot %d"), active + 1);
    if (Button((Rectangle){ bx, y + 54, bw, 40 }, erase, true, 0)) {
        if (G.confirmNew) G.requestNew = true;
        else G.confirmNew = true;
    }
    if (Button((Rectangle){ bx + bw + gap, y + 54, bw, 40 }, "Save & quit (Q)", true, 0)) G.quit = true;
}

static void DrawCoopPanel(void)
{
    static const char *const ask[3] = { "Slaughter", "Sure? (1/2)", "Really? (2/2)" };
    Rectangle r = PanelRect(820, 480);
    float x0 = r.x + 24, y = r.y + 18, now = NowDays();
    bool canSeeds = BagCount(IT_SEEDS) > 0 && G.coopFeed <= FEED_MAX - 1.0f;
    bool canWheat = BagCount(IT_WHEAT) > 0 && G.coopFeed <= FEED_MAX - 2.0f;
    bool canWater = BagCount(IT_WATER) > 0 && G.coopWater < WATER_DAYS - 0.05f;

    Text(TextFormat(L("Hen coop  %d/%d hens"), G.henCount, MAX_HENS), x0, y, 26, C_GOLD);
    y += 38;
    Text(TextFormat(L("Water %.1f of %d days"), G.coopWater, (int)WATER_DAYS), x0, y, 18, G.coopWater <= 2.0f ? C_BAD : C_TEXT);
    Text(TextFormat(L("Feed %d/%d"), (int)G.coopFeed, (int)FEED_MAX), x0 + 250, y, 18, G.coopFeed < (float)G.henCount ? C_BAD : C_TEXT);
    Text(TextFormat(L("Eggs waiting %d"), G.coopEggs), x0 + 420, y, 18, C_TEXT);
    if (G.coopDry > 0) Text(TextFormat(L("dry for %.1f days"), G.coopDry), x0 + 600, y, 18, C_BAD);
    y += 26;
    Text("Each hen eats 1 feed every 3 days and lays an egg a day while fed and watered. Seeds give 1 feed, wheat 2.", x0, y, 14, C_DIM);
    y += 28;

    float bw = 182;
    if (Button((Rectangle){ x0, y, bw, 32 }, "[1] Collect eggs", G.coopEggs > 0, KEY_ONE)) CollectEggs();
    if (Button((Rectangle){ x0 + 194, y, bw, 32 }, "[2] Scatter seeds", canSeeds, KEY_TWO)) FeedSeeds();
    if (Button((Rectangle){ x0 + 388, y, bw, 32 }, "[3] Feed wheat", canWheat, KEY_THREE)) FeedWheat();
    if (Button((Rectangle){ x0 + 582, y, bw, 32 }, "[4] Pour water", canWater, KEY_FOUR)) PourCoop();
    y += 40;
    Text(TextFormat(L("You carry: seeds %d, wheat %d, water buckets %d"), BagCount(IT_SEEDS), BagCount(IT_WHEAT), BagCount(IT_WATER)),
         x0, y, 15, C_DIM);
    y += 30;

    if (G.henCount == 0) {
        Text("No hens yet. The farmer sells them.", x0, y + 10, 18, C_TEXT);
    }
    for (int i = 0; i < G.henCount; i++) {
        float age = now - G.henBorn[i], left = HEN_LIFE_DAYS - age, meal = G.henMeal[i] - now;
        bool ready = age >= SEASON_DAYS, asking = G.slaughterHen == i && G.slaughterStep > 0;
        DrawRectangle((int)x0, (int)y, (int)r.width - 48, 40, (i & 1) ? (Color){ 40, 33, 28, 255 } : (Color){ 48, 40, 33, 255 });
        Text(TextFormat(L("Hen %d"), i + 1), x0 + 12, y + 11, 18, C_GOLD);
        Text(TextFormat(L("age %.1f days"), age), x0 + 104, y + 12, 16, C_TEXT);
        Text(TextFormat(L("life left %d days (%.1f seasons)"), (int)ceilf(left), left / SEASON_DAYS), x0 + 220, y + 12, 16, left < SEASON_DAYS ? C_GOLD : C_TEXT);
        if (meal >= 0) Text(TextFormat(L("next meal in %.1f days"), meal), x0 + 470, y + 12, 16, C_TEXT);
        else Text(TextFormat(L("hungry %.1f of %d days"), -meal, (int)STARVE_DAYS), x0 + 470, y + 12, 16, C_BAD);
        const char *label = asking ? ask[G.slaughterStep] : ready ? ask[0] : TextFormat(L("in %.1f days"), SEASON_DAYS - age);
        if (Button((Rectangle){ r.x + r.width - 168, y + 5, 132, 30 }, label, ready, 0)) {
            if (!asking) {
                G.slaughterHen = i;
                G.slaughterStep = 1;
            } else if (G.slaughterStep == 1) {
                G.slaughterStep = 2;
            } else {
                G.slaughterStep = 0;
                SlaughterHen(i);
            }
        }
        y += 44;
    }
    const char *foot = G.slaughterStep > 0 ? "Click the hen's button again to confirm, Esc to cancel" : "Click or 1-4    Esc: close";
    Text(foot, x0, r.y + r.height - 34, 16, G.slaughterStep > 0 ? C_GOLD : C_DIM);
}

static void DrawHelpPanel(void)
{
    static const char *const keys[][2] = {
        { "WASD", "move" }, { "Shift", "run" }, { "Space", "jump" }, { "C", "crouch (stealth)" },
        { "Mouse", "look" }, { "Wheel", "zoom; all the way in is first person" }, { "P", "first person on/off" },
        { "1 / 2 / 3", "bow / melee weapon / hands free" }, { "4", "war mace (the smith sells one)" },
        { "Left mouse", "draw and release (bow), strike (melee)" },
        { "Right mouse, Q", "parry with the melee weapon" }, { "V", "strike" },
        { "E", "use, talk, pick up, climb, search" }, { "F", "alternate action" },
        { "R", "refill the quiver" }, { "Z", "arrow camera on/off" }, { "Tab", "baskets and equipment" },
        { "M", "map zoom" }, { "T (hold)", "fast-forward time" }, { "F5", "save" },
        { "F11", "fullscreen" }, { "Esc", "pause, save slots, language" }, { "H", "this window" },
    };
    static const char *const tips[] = {
        "Parry: a yellow mark means a blow is coming. Parry while facing them: green = turned aside (hit back harder), "
        "red = it landed.",
        "Seasons last 9 days. Wheat needs 10 days with water, so sow by summer's 8th day. Nothing grows in winter. "
        "Harvest: 4 wheat, 2 seeds. The farm and the coop each hold 6 days of water. Fill the bucket from your storeroom "
        "at the well or the river (E), then pour it into the farm ditch or at the coop. Rain fills both; 6 dry days kill "
        "growing wheat. The shepherd takes withered plant waste, the cattle herder spare seeds.",
        "Hens live 12 seasons, eat every 3 days, and die after 6 days without food or water. E at the coop shows each hen.",
        "Home: quern, trough, oven, storage, bed. The workbench sews leather armor or a wolf-pelt cloak. The smith makes "
        "bronze armor (5 bronze, 3 sheepskin, 3 cowhide) and sells a war mace (key 4) that strikes everything in front of "
        "you. Tin comes with the caravans. Picking dates is free; all other gathering costs food, mining the most, then "
        "woodcutting. Health returns while food is at 20% or more.",
        "Ziggurat: market in the base. The priest at the top blesses you for a day: +20% running and attack speed. "
        "Guards fight raiders who follow you. Raiders camp south and north of the city; barbarians twice as strong camp "
        "across the river. The ferry west of the city crosses the river for a fare. The far north ford is wolf country.",
        "Roads on the map lead east to the hunters' village and south-west to the herders' village; travellers walk them, "
        "and wolves leave you alone in company. Only the shepherd sells sheepskin and sheep.",
        "Herbs grow in green meadows, most of all across the river: chew one from your baskets to heal fast. Snakes hide "
        "in long grass; a hiss means back off (crouch to creep by). A hurt boar charges. Arrows landing close send prey "
        "running and bring wolves down on you.",
        "Playing together (in the browser): each player has a house in their own color. The world never pauses, and the "
        "night passes when everyone lies in bed. The farm, the coop and the eggs are shared.",
    };
    const int nk = (int)(sizeof(keys) / sizeof(keys[0])), nt = (int)(sizeof(tips) / sizeof(tips[0]));
    Rectangle r = PanelRect(fminf(1200.0f, (float)GetScreenWidth() - 20.0f), fminf(690.0f, (float)GetScreenHeight() - 16.0f));
    float x0 = r.x + 24, x1 = r.x + 500, y0 = r.y + 60;

    Text("Controls", x0, r.y + 20, 26, C_GOLD);
    Text("Tips", x1, r.y + 20, 26, C_GOLD);
    for (int i = 0; i < nk; i++) {
        Text(keys[i][0], x0, y0 + 23.0f * (float)i, 16, C_GOLD);
        Text(keys[i][1], x0 + 126, y0 + 23.0f * (float)i, 16, C_TEXT);
    }
    float ty = y0;
    for (int i = 0; i < nt; i++) ty += Paragraph(tips[i], x1, ty, r.x + r.width - 24 - x1, 15, 19) + 7;
    Text("H or Esc: close", r.x + r.width - 170, r.y + r.height - 30, 16, C_DIM);
}

static void DrawAnimalBars(void)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));

    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (!a->active || a->state == ST_DEAD || AnimalHidden(a)) continue;
        float dist = XZDist(a->pos, G.player.pos);
        int max = AnimalMaxHp(a->type);
        bool hurt = a->hp < max, aware = a->state != ST_CALM;
        if (dist > (hurt || aware ? 60.0f : 30.0f)) continue;
        Vector3 head = { a->pos.x, AnimalTop(a) - 0.2f, a->pos.z };
        if (Vector3DotProduct(Vector3Subtract(head, G.cam.position), fwd) < 0) continue;

        Vector2 sp = GetWorldToScreen(head, G.cam);
        float k = Clamp((float)a->hp / (float)max, 0.0f, 1.0f);
        float w = Clamp(56.0f - dist * 0.5f, 30.0f, 56.0f);
        Color fill = k > 0.6f ? C_GOOD : k > 0.3f ? C_GOLD : C_BAD;
        DrawRectangle((int)(sp.x - w / 2) - 1, (int)sp.y - 1, (int)w + 2, 7, Fade(BLACK, 0.7f));
        DrawRectangle((int)(sp.x - w / 2), (int)sp.y, (int)(w * k), 5, fill);
        if (dist < 20.0f) CenterText(AnimalName(a->type), sp.x, sp.y - 15, 12, Fade(RAYWHITE, 0.9f));
    }
}

static void DrawPeopleBars(void)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));

    for (int i = 0; i < FighterCount(); i++) {
        Vector3 pos;
        float hp, maxHp;
        const char *name;
        bool hostile, dead;
        if (!FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead) || dead) continue;
        float dist = Vector3Distance(pos, G.player.pos);
        bool hurt = hp < maxHp;
        if (dist > ((hurt || hostile) ? 50.0f : 14.0f) || (!hostile && fabsf(pos.y - G.player.pos.y) > 3.5f)) continue;
        Vector3 head = { pos.x, pos.y + 2.25f, pos.z };
        if (Vector3DotProduct(Vector3Subtract(head, G.cam.position), fwd) < 0) continue;
        if (FogAt(head) > 0.6f) continue;

        Vector2 sp = GetWorldToScreen(head, G.cam);
        float k = Clamp(hp / maxHp, 0.0f, 1.0f);
        float w = Clamp(60.0f - dist * 0.5f, 32.0f, 60.0f);
        Color fill = hostile ? C_BAD : k > 0.6f ? C_GOOD : C_GOLD;
        DrawRectangle((int)(sp.x - w / 2) - 1, (int)sp.y - 1, (int)w + 2, 7, Fade(BLACK, 0.7f));
        DrawRectangle((int)(sp.x - w / 2), (int)sp.y, (int)(w * k), 5, fill);
        if (dist < 16.0f || hostile) CenterText(name, sp.x, sp.y - 15, 12, hostile ? C_BAD : Fade(RAYWHITE, 0.85f));

        int mark = FighterMark(i);
        if (mark != MARK_NONE) {
            static const Color colors[4] = { { 0 }, { 245, 205, 60, 255 }, { 90, 215, 90, 255 }, { 225, 60, 50, 255 } };
            float size = mark == MARK_WARN ? 9.0f + 2.0f * sinf((float)GetTime() * 18.0f) : 10.0f;
            Vector2 c = { sp.x, sp.y - 32 };
            DrawPoly(c, 4, size + 2, 0, Fade(BLACK, 0.7f));
            DrawPoly(c, 4, size, 0, colors[mark]);
        }
    }
}

static const char *PhaseName(void)
{
    if (G.hour < 5) return "Night";
    if (G.hour < 8) return "Dawn";
    if (G.hour < 17) return "Day";
    if (G.hour < 20) return "Dusk";
    return "Night";
}

static void DrawStatus(void)
{
    int hh = (int)G.hour, mm = (int)((G.hour - (float)hh) * 60.0f);

    const Peer *me = &G.peers[G.self];
    bool wolves = AnyWolfHunting(), beast = AnyBeastAttacking();
    bool alert = me->alone || wolves || beast || me->wanted > 0;

    DrawRectangle(12, 12, 360, alert ? 208 : 186, Fade(BLACK, 0.4f));
    Text(TextFormat(L("Day %d  %02d:%02d  %s"), G.day, hh, mm, L(PhaseName())), 24, 20, 22, RAYWHITE);
    Text(TextFormat(L("%s, day %d of %d"), SeasonName(Season()), SeasonDay(), SEASON_DAYS), 24, 46, 18, C_GOLD);
    const char *status = G.herbT > 0 ? TextFormat(L("Herbs %ds"), (int)ceilf(G.herbT))
                       : G.blessT > 0 ? TextFormat(L("Blessed %dh"), (int)ceilf(G.blessT)) : G.raining ? L("Raining") : "";
    Text(status, 360.0f - (float)Measure(status, 18), 46, 18, G.herbT > 0 || G.blessT > 0 ? C_GOOD : (Color){ 170, 200, 230, 255 });
    if (G.raining)
        for (int k = 0; k < 3; k++)
            DrawLineEx((Vector2){ 334.0f + 8.0f * (float)k, 22 }, (Vector2){ 330.0f + 8.0f * (float)k, 36 }, 2, (Color){ 170, 200, 230, 255 });
    int bx = 24 + (int)fmaxf(60.0f, (float)(fmaxf((float)Measure(L("Food"), 18), (float)Measure(L("Health"), 18)) + 12.0f));
    Text("Food", 24, 76, 18, C_TEXT);
    FoodBar((float)bx, 79, 200);
    Text("Health", 24, 98, 18, C_TEXT);
    DrawRectangle(bx, 101, 200, 14, Fade(BLACK, 0.5f));
    DrawRectangle(bx, 101, (int)(200 * Clamp(G.health, 0, HEALTH_MAX) / HEALTH_MAX), 14, G.health < 35 ? C_BAD : C_GOOD);
    DrawRectangleLines(bx, 101, 200, 14, Fade(BLACK, 0.6f));
    Text(TextFormat(L("Arrows %d/%d   +%d carried"), G.quiver, G.quiverMax, BagCount(IT_ARROWS)), 24, 124, 18, C_TEXT);
    Text(TextFormat(L("In hand: %s%s"), EquipName(G.equip), G.player.crouching ? L("  |  crouching") : ""), 24, 146, 18,
         G.player.crouching ? C_GOOD : C_TEXT);
    Text(TextFormat(L("Armor: %s"), ArmorName(G.armor)), 24, 168, 18, C_TEXT);
    if (me->wanted > 0) Text(TextFormat(L("Wanted by the guards  %d:%02d"), (int)me->wanted / 60, (int)me->wanted % 60), 24, 190, 18, C_BAD);
    else if (wolves) Text("Wolves are hunting you!", 24, 190, 18, C_BAD);
    else if (beast) Text("A wild beast is attacking you!", 24, 190, 18, C_BAD);
    else if (me->alone) Text("Alone outside the walls", 24, 190, 18, C_GOLD);
}

static void DrawMinimap(int ox, int oy, int size)
{
    static const float zooms[3] = { 1.0f, 2.5f, 6.0f };
    float zoom = zooms[G.mapZoom < 0 || G.mapZoom > 2 ? 0 : G.mapZoom];
    float s = (float)size / (2 * WORLD_HALF) * zoom, view = WORLD_HALF / zoom;
    float cx = Clamp(G.player.pos.x, -WORLD_HALF + view, WORLD_HALF - view);
    float cz = Clamp(G.player.pos.z, -WORLD_HALF + view, WORLD_HALF - view);
    float mx = (float)ox + (float)size / 2 - cx * s, mz = (float)oy + (float)size / 2 - cz * s;

    DrawRectangle(ox, oy, size, size, Fade((Color){ 170, 160, 110, 255 }, 0.88f));
    BeginScissorMode(ox, oy, size, size);
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (!b->solid || (b->size.x * s < 1.0f && b->size.z * s < 1.0f)) continue;
        if (fabsf(b->pos.x - cx) > view + b->size.x || fabsf(b->pos.z - cz) > view + b->size.z) continue;
        if (!b->water && b->pos.y - b->size.y / 2 > TerrainHeight(b->pos.x, b->pos.z) + 0.5f) continue;
        Rectangle r = { mx + (b->pos.x - b->size.x / 2) * s, mz + (b->pos.z - b->size.z / 2) * s, fmaxf(b->size.x * s, 1), fmaxf(b->size.z * s, 1) };
        if (b->water && b->shape == SHAPE_CYL) DrawEllipse((int)(mx + b->pos.x * s), (int)(mz + b->pos.z * s), b->size.x * s * 0.4f, b->size.z * s * 0.4f, COL_WATER);
        else DrawRectangleRec(r, b->water ? COL_WATER : COL_MUD_DARK);
    }
    DrawRoadMap(mx, mz, s);
    DrawRectangleLinesEx((Rectangle){ mx + (G.home.x - 6) * s, mz + (G.home.z - 6) * s, 12 * s, 12 * s }, 2, COL_LAPIS);
    for (int v = 0; v < VILLAGES; v++) {
        Vector3 c = VillageCenter(v);
        Vector2 at = { mx + c.x * s, mz + c.z * s };
        DrawRectangleRec((Rectangle){ at.x - 4, at.y - 2, 8, 6 }, (Color){ 240, 226, 190, 255 });
        DrawTriangle((Vector2){ at.x - 5, at.y - 2 }, (Vector2){ at.x + 5, at.y - 2 }, (Vector2){ at.x, at.y - 7 }, COL_BLANKET);
        DrawRectangleLinesEx((Rectangle){ at.x - 4, at.y - 2, 8, 6 }, 1, Fade(BLACK, 0.6f));
        if (zoom > 1.0f) Text(VillageName(v), at.x + 7, at.y - 6, 10, Fade(BLACK, 0.85f));
    }
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (!a->active || AnimalHidden(a) || XZDist(a->pos, G.player.pos) > 60) continue;
        Color c = a->state == ST_DEAD ? BLACK : a->state == ST_HUNT ? RED : COL_BLANKET;
        DrawCircleV((Vector2){ mx + a->pos.x * s, mz + a->pos.z * s }, 2, c);
    }
    for (int i = 0; i < TR_COUNT; i++) {
        const Trader *t = &G.traders[i];
        if (t->roaming && !t->away) DrawCircleV((Vector2){ mx + t->pos.x * s, mz + t->pos.z * s }, 3, C_GOLD);
    }
    if (zoom > 1.0f) {
        for (int i = 0; i < FighterCount(); i++) {
            Vector3 pos;
            float hp, maxHp;
            const char *name;
            bool hostile, dead;
            if (!FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead) || dead) continue;
            DrawCircleV((Vector2){ mx + pos.x * s, mz + pos.z * s }, 2, hostile ? RED : (Color){ 240, 236, 220, 255 });
        }
        for (int i = 0; i < G.ladderCount; i++)
            DrawRectangle((int)(mx + G.ladders[i].base.x * s) - 2, (int)(mz + G.ladders[i].base.z * s) - 2, 4, 4, COL_WOOD);
    }
    // Raider camps as red crosses; the barbarians' across the river darker and larger.
    for (int c = 0; c < CAMPS; c++) {
        Vector3 cc = CampCenter(c);
        Vector2 camp = { mx + cc.x * s, mz + cc.z * s };
        float k = c >= EAST_CAMPS ? 5.0f : 4.0f;
        Color col = c >= EAST_CAMPS ? (Color){ 150, 20, 20, 255 } : C_BAD;
        DrawLineEx((Vector2){ camp.x - k, camp.y - k }, (Vector2){ camp.x + k, camp.y + k }, 2.5f, col);
        DrawLineEx((Vector2){ camp.x - k, camp.y + k }, (Vector2){ camp.x + k, camp.y - k }, 2.5f, col);
    }
    Vector3 fe = G.stations[STN_FERRY_E], fw = G.stations[STN_FERRY_W];
    DrawLineEx((Vector2){ mx + fe.x * s, mz + fe.z * s }, (Vector2){ mx + fw.x * s, mz + fw.z * s }, 2, C_GOLD);
    for (int k = 0; k < MAX_PLAYERS; k++) {
        if (k == G.self || !G.peers[k].active) continue;
        Vector2 q = { mx + G.peers[k].shown.x * s, mz + G.peers[k].shown.z * s };
        DrawCircleV(q, 4.5f, Fade(BLACK, 0.8f));
        DrawCircleV(q, 3.5f, PlayerColor(k));
    }
    Vector2 pp = { mx + G.player.pos.x * s, mz + G.player.pos.z * s };
    DrawLineEx(pp, (Vector2){ pp.x + sinf(G.player.facing) * 8, pp.y + cosf(G.player.facing) * 8 }, 2, BLACK);
    DrawCircleV(pp, 3, RAYWHITE);
    EndScissorMode();
    DrawRectangleLines(ox, oy, size, size, Fade(BLACK, 0.6f));
    Text(TextFormat(L("M  x%.1f"), zoom), (float)ox + 4, (float)(oy + size) - 16, 12, Fade(BLACK, 0.7f));
}

// The other players' names over their heads, in their colors, with their health when hurt.
static void DrawPeerTags(void)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));

    for (int s = 0; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active || s == G.self) continue;
        Vector3 head = { p->shown.x, p->shown.y + ((p->flags & PF_BED) ? 1.0f : 2.4f), p->shown.z };
        float dist = Vector3Distance(head, G.cam.position);
        if (dist > 90.0f || Vector3DotProduct(Vector3Subtract(head, G.cam.position), fwd) < 0 || FogAt(head) > 0.7f) continue;
        Vector2 sp = GetWorldToScreen(head, G.cam);
        int w = Measure(p->name, 16);
        DrawRectangle((int)sp.x - w / 2 - 8, (int)sp.y - 3, w + 16, 22, Fade(BLACK, 0.5f));
        DrawRectangle((int)sp.x - w / 2 - 8, (int)sp.y - 3, 4, 22, PlayerColor(s));
        DrawTextEx(GameFont(), p->name, (Vector2){ (float)((int)sp.x - w / 2), sp.y }, 16, 1, RAYWHITE);
        if (p->health < HEALTH_MAX - 1.0f) {
            float k = Clamp(p->health / HEALTH_MAX, 0.0f, 1.0f);
            DrawRectangle((int)sp.x - 24, (int)sp.y + 21, 48, 5, Fade(BLACK, 0.6f));
            DrawRectangle((int)sp.x - 24, (int)sp.y + 21, (int)(48 * k), 5, k > 0.35f ? C_GOOD : C_BAD);
        }
    }
}

static void DrawAlerts(void)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));

    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        bool hunt = a->state == ST_HUNT;
        if (!a->active || (a->state != ST_ALERT && a->state != ST_FLEE && !hunt)) continue;
        if (a->type == AN_WOLF && !hunt) continue;
        Vector3 head = { a->pos.x, AnimalTop(a), a->pos.z };
        if (Vector3Distance(head, G.player.pos) > 60 || Vector3DotProduct(Vector3Subtract(head, G.cam.position), fwd) < 0) continue;
        Vector2 sp = GetWorldToScreen(head, G.cam);
        bool flee = a->state == ST_FLEE || hunt;
        CenterText(hunt ? "!!" : flee ? "!" : "?", sp.x, sp.y - 14, 28, flee ? C_BAD : C_GOLD);
    }
}

static void DrawCrosshair(void)
{
    if (!G.player.aiming) {
        if (G.firstPerson && G.panel == PANEL_NONE) DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2, 2.5f, Fade(RAYWHITE, 0.8f));
        return;
    }
    float cx = (float)GetScreenWidth() / 2, cy = (float)GetScreenHeight() / 2;
    float gap = 10.0f - 6.0f * G.player.draw;

    DrawLineEx((Vector2){ cx - gap - 8, cy }, (Vector2){ cx - gap, cy }, 2, RAYWHITE);
    DrawLineEx((Vector2){ cx + gap, cy }, (Vector2){ cx + gap + 8, cy }, 2, RAYWHITE);
    DrawLineEx((Vector2){ cx, cy - gap - 8 }, (Vector2){ cx, cy - gap }, 2, RAYWHITE);
    DrawLineEx((Vector2){ cx, cy + gap }, (Vector2){ cx, cy + gap + 8 }, 2, RAYWHITE);
    DrawRectangle((int)cx - 30, (int)cy + 26, 60, 5, Fade(BLACK, 0.5f));
    DrawRectangle((int)cx - 30, (int)cy + 26, (int)(60 * G.player.draw), 5, G.player.draw >= 1 ? C_GOOD : C_GOLD);
}

// A message too wide for the window breaks at the space nearest its middle.
static void DrawMessage(int sw)
{
    float a = fminf(G.msgT, 1.0f);
    char lines[2][sizeof(G.msg)];
    int count = 1, w = Measure(G.msg, 22), len = (int)strlen(G.msg);

    snprintf(lines[0], sizeof(lines[0]), "%s", G.msg);
    if (w > sw - 80) {
        int cut = -1;
        for (int i = 0; i < len; i++)
            if (G.msg[i] == ' ' && (cut < 0 || abs(i - len / 2) < abs(cut - len / 2))) cut = i;
        if (cut > 0) {
            snprintf(lines[0], sizeof(lines[0]), "%.*s", cut, G.msg);
            snprintf(lines[1], sizeof(lines[1]), "%s", G.msg + cut + 1);
            count = 2;
            w = Measure(lines[0], 22) > Measure(lines[1], 22) ? Measure(lines[0], 22) : Measure(lines[1], 22);
        }
    }
    DrawRectangle((sw - w) / 2 - 12, 236, w + 24, 6 + 28 * count, Fade(BLACK, 0.5f * a));
    for (int i = 0; i < count; i++) CenterText(lines[i], (float)sw / 2, 241.0f + 28.0f * (float)i, 22, Fade(RAYWHITE, a));
}

static void DrawPrompts(void)
{
    float cx = (float)GetScreenWidth() / 2, y = (float)GetScreenHeight() - 128;
    const char *lines[2] = { G.promptE, G.promptF };
    bool ok[2] = { G.okE, G.okF };

    for (int i = 0; i < 2; i++) {
        if (!lines[i][0]) continue;
        int w = Measure(lines[i], 22);
        DrawRectangle((int)cx - w / 2 - 10, (int)y - 4, w + 20, 30, Fade(BLACK, 0.5f));
        CenterText(lines[i], cx, y, 22, ok[i] ? RAYWHITE : C_DIM);
        y += 34;
    }
}

void DrawUI(void)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    DrawStatus();
    DrawMinimap(sw - 202, 12, 190);
    DrawFPS(sw - 202, 208);
    DrawAnimalBars();
    DrawPeopleBars();
    DrawPeerTags();
    DrawAlerts();
    if (G.impactT > 0) {
        Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));
        if (Vector3DotProduct(Vector3Subtract(G.impact, G.cam.position), fwd) > 0) {
            Vector2 sp = GetWorldToScreen((Vector3){ G.impact.x, G.impact.y + 0.4f, G.impact.z }, G.cam);
            Color c = Fade((Color){ 255, 196, 100, 255 }, Clamp(G.impactT, 0.0f, 1.0f));
            DrawPoly(sp, 4, 7, 45, c);
            DrawPolyLines(sp, 4, 9, 45, Fade(BLACK, Clamp(G.impactT, 0.0f, 1.0f) * 0.6f));
            CenterText(TextFormat(L("%d m"), (int)XZDist(G.impact, G.player.pos)), sp.x, sp.y - 24, 14, c);
        }
    }
    DrawCrosshair();
    if (G.hitMarkT > 0) {
        float cx = (float)sw / 2, cy = (float)sh / 2, k = 6.0f + 10.0f * (0.3f - G.hitMarkT);
        Color c = Fade(C_BAD, G.hitMarkT / 0.3f);
        DrawLineEx((Vector2){ cx - k, cy - k }, (Vector2){ cx - k / 2, cy - k / 2 }, 3, c);
        DrawLineEx((Vector2){ cx + k, cy - k }, (Vector2){ cx + k / 2, cy - k / 2 }, 3, c);
        DrawLineEx((Vector2){ cx - k, cy + k }, (Vector2){ cx - k / 2, cy + k / 2 }, 3, c);
        DrawLineEx((Vector2){ cx + k, cy + k }, (Vector2){ cx + k / 2, cy + k / 2 }, 3, c);
    }
    if (G.panel == PANEL_NONE && !G.sleeping && !G.waitBed) DrawPrompts();
    if (G.waitBed) {
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.45f));
        CenterText(TextFormat(L("In bed: %d of %d players. The night passes when everyone lies down."), PlayersInBed(), PlayerCount()),
                   (float)sw / 2, (float)sh - 140, 22, RAYWHITE);
        CenterText("E, Space or a step gets you up.", (float)sw / 2, (float)sh - 106, 18, C_DIM);
    }
    if (NetSilence() > 3.0f)
        CenterText("The host's game is not answering. Is its browser tab hidden or closed?", (float)sw / 2, 210, 18, C_BAD);
    if (OnWeb() && G.panel == PANEL_NONE && !PointerCaptured())
        CenterText("Click the game to look around with the mouse", (float)sw / 2, (float)sh / 2 + 70, 20, C_GOLD);

    if (G.msgT > 0) DrawMessage(sw);

    Text("H  controls", 12, (float)sh - 22, 14, Fade(RAYWHITE, 0.6f));

    switch (G.panel) {
    case PANEL_BAG:     DrawBagPanel(); break;
    case PANEL_STORAGE: DrawStoragePanel(); break;
    case PANEL_TRADE:   DrawTradePanel(); break;
    case PANEL_PAUSE:   DrawPausePanel(); break;
    case PANEL_HELP:    DrawHelpPanel(); break;
    case PANEL_COOP:    DrawCoopPanel(); break;
    default: break;
    }

    if (G.hurtT > 0) DrawRectangle(0, 0, sw, sh, Fade(RED, G.hurtT * 0.6f));
    if (G.sleeping) {
        float fade = Clamp(1.0f - fabsf(G.sleepT - 1.1f) / 1.1f, 0.0f, 1.0f);
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, fade));
    }
}
