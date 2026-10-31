#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Two sets of colours. Over the game (the HUD) light letters sit on dark see-through boxes; the windows
// are clay tablets written in dark ink, framed with the inlay of the Standard of Ur. The C_ names follow
// whichever set is in use: DrawUI switches to the tablet's while it draws a window.
typedef struct {
    Color panel, edge, slot, slotHi, locked, text, dim, gold, bad, good, title, row, rowAlt, rowHi;
    Color button, buttonHi, buttonOff, label, labelOff;
} Palette;
static const Palette HUD = {
    { 30, 24, 20, 238 }, { 150, 120, 80, 255 }, { 58, 48, 38, 255 }, { 86, 70, 52, 255 }, { 40, 34, 30, 255 },
    { 240, 230, 210, 255 }, { 150, 140, 125, 255 }, { 240, 200, 120, 255 }, { 230, 110, 90, 255 }, { 150, 210, 120, 255 },
    { 240, 200, 120, 255 }, { 48, 40, 33, 255 }, { 40, 33, 28, 255 }, { 64, 52, 38, 255 },
    { 112, 82, 48, 255 }, { 150, 110, 60, 255 }, { 60, 54, 48, 255 }, { 240, 230, 210, 255 }, { 150, 140, 125, 255 },
};
static const Palette CLAY = {
    { 214, 186, 144, 255 }, { 96, 62, 40, 255 }, { 196, 138, 94, 255 }, { 212, 158, 110, 255 }, { 170, 142, 108, 255 },
    { 40, 25, 16, 255 }, { 88, 60, 40, 255 }, { 28, 62, 146, 255 }, { 156, 34, 20, 255 }, { 30, 100, 38, 255 },
    { 62, 34, 20, 255 }, { 188, 156, 114, 255 }, { 178, 146, 106, 255 }, { 214, 184, 138, 255 },
    { 190, 128, 84, 255 }, { 212, 152, 104, 255 }, { 176, 152, 120, 255 }, { 44, 28, 18, 255 }, { 120, 96, 72, 255 },
};
static const Palette *pal = &HUD;
#define C_PANEL    (pal->panel)
#define C_EDGE     (pal->edge)
#define C_SLOT     (pal->slot)
#define C_SLOT_HI  (pal->slotHi)
#define C_TEXT     (pal->text)
#define C_DIM      (pal->dim)
#define C_GOLD     (pal->gold)
#define C_BAD      (pal->bad)
#define C_GOOD     (pal->good)
#define C_TITLE    (pal->title)
#define B_GOLD     CLITERAL(Color){ 240, 200, 120, 255 }   // what glows on a game board, whatever the window
// The inlay's stones.
static const Color BITUMEN = { 34, 24, 18, 255 }, LAPIS = { 34, 70, 156, 255 }, SHELL = { 240, 230, 208, 255 }, CARNELIAN = { 182, 62, 34, 255 };

enum { CLICK_NONE, CLICK_LEFT, CLICK_RIGHT };

static char hoverText[256];
static bool boardRules;   // the board panel shows the rules in place of the board

void SetPanel(Panel p)
{
    if (p == PANEL_BOARD && G.panel != PANEL_BOARD) boardRules = false;
    G.panel = p;
    G.player.aiming = false;
    G.player.draw = 0;
    if (p == PANEL_NONE) {
        LockMouse(true);
        WantPointer(true);
        G.cursorLocked = true;
        G.ignoreClick = true;
    } else if (OnWeb() && p != PANEL_PAUSE && PointerCaptured()) {
        // In a browser only Esc gives the mouse back: a panel over the game is worked with the game's own
        // cursor while the page keeps the mouse, so closing it goes straight back to play (a browser hands
        // the mouse over again only on a click).
        KeepPointer();
    } else {
        LockMouse(false);
        WantPointer(false);
        G.cursorLocked = false;
    }
}

// The UI is laid out for a screen of at least 960 x 540. A smaller one, a phone's above all, gets it
// all shrunk to fit, as if it were that big: everything here works in those units (UiWidth() wide),
// and the mouse and points on the screen are turned into them. On a screen with several pixels to
// each of the page's (PixelRatio) the UI is drawn that much bigger, so it keeps its size, sharper.
float UiScale(void)
{
    float s = fminf(fminf((float)GetScreenHeight() / 540.0f, (float)GetScreenWidth() / 960.0f), PixelRatio());
    return s > 0.0f ? s : 1.0f;
}

float UiWidth(void)
{
    return (float)GetScreenWidth() / UiScale();
}

float UiHeight(void)
{
    return (float)GetScreenHeight() / UiScale();
}

Vector2 UiMouse(void)
{
    return Vector2Scale(PointerPosition(), 1.0f / UiScale());
}

static Vector2 OnScreen(Vector3 p)
{
    return Vector2Scale(GetWorldToScreen(p, G.cam), 1.0f / UiScale());
}

void UiBegin(void)
{
    float s = UiScale();
    SetTextScale(s);   // text picks its letters' size for the pixels it will cover
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlScalef(s, s, 1.0f);
}

void UiEnd(void)
{
    rlDrawRenderBatchActive();
    rlPopMatrix();
    SetTextScale(1.0f);
}

static float FontSize(int size)
{
    return (float)(size < 10 ? 10 : size);
}

static int Measure(const char *s, int size)
{
    return (int)ceilf(GameTextWidth(s, FontSize(size)));
}

// Labels are translated here, so every literal passed in follows the language setting.
static void Text(const char *s, float x, float y, int size, Color c)
{
    DrawGameText(L(s), (float)(int)x, (float)(int)y, FontSize(size), c);
}

static void CenterText(const char *s, float cx, float y, int size, Color c)
{
    s = L(s);
    Text(s, cx - (float)Measure(s, size) / 2, y, size, c);
}

// Breaks the translated text into lines no wider than 'width'; returns the height used.
static bool paragraphDry;   // measuring only: nothing is drawn
static const Color *paragraphColor;   // the text's colour when not the usual
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
            if (!paragraphDry) Text(line, x, y + used, size, paragraphColor ? *paragraphColor : C_TEXT);
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
        if (!paragraphDry) Text(line, x, y + used, size, paragraphColor ? *paragraphColor : C_TEXT);
        used += lineH;
    }
    return used;
}

// A triangle whichever way round its corners are given.
static void Tri(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

// ---- The windows' clay and inlay ----

static Texture2D ClayTexture(void)
{
    static Texture2D t;
    static bool tried;
    if (!tried && IsWindowReady()) {
        tried = true;
        t = LoadTexture("assets/clay.png");   // Poly Haven's clay_plaster (CC0), in grey: tinted when drawn
        if (t.id) {
            GenTextureMipmaps(&t);
            SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
        }
    }
    return t;
}

// Clay of colour c over r, its grain fixed to the screen so that windows side by side match.
static void Clay(Rectangle r, Color c)
{
    Texture2D t = ClayTexture();
    if (!t.id) {
        DrawRectangleRec(r, c);
        return;
    }
    DrawTexturePro(t, (Rectangle){ r.x * 1.5f, r.y * 1.5f, r.width * 1.5f, r.height * 1.5f }, r, (Vector2){ 0, 0 }, 0, c);
}

// A rosette of the Royal Game of Ur's board: eight petals of lapis and red stone round a shell eye.
static void Rosette(Vector2 c, float rad)
{
    DrawCircleV((Vector2){ c.x + 1.5f, c.y + 2.0f }, rad, Fade(BLACK, 0.35f));
    DrawCircleV(c, rad, BITUMEN);
    DrawCircleV(c, rad - 1.6f, SHELL);
    for (int k = 0; k < 8; k++) {
        float t = (float)k * PI / 4.0f, co = cosf(t), si = sinf(t), w = rad * 0.22f;
        Vector2 tip = { c.x + co * rad * 0.8f, c.y + si * rad * 0.8f }, mid = { c.x + co * rad * 0.42f, c.y + si * rad * 0.42f };
        Vector2 l = { mid.x - si * w, mid.y + co * w }, r = { mid.x + si * w, mid.y - co * w };
        Color petal = k & 1 ? CARNELIAN : LAPIS;
        Tri(c, l, tip, petal);
        Tri(c, tip, r, petal);
    }
    DrawCircleV(c, rad * 0.26f, SHELL);
    DrawCircleV(c, rad * 0.17f, CARNELIAN);
}

// A band of little squares, lapis, shell, red stone, shell, from a to b, set in bitumen 'w' wide.
static void InlayBand(Vector2 a, Vector2 b, float w)
{
    Vector2 d = Vector2Subtract(b, a);
    float len = Vector2Length(d);
    if (len < 1.0f) return;
    static const Color *const STONES[4] = { &LAPIS, &SHELL, &CARNELIAN, &SHELL };
    bool across = fabsf(d.x) > fabsf(d.y);
    float tile = w - 4.0f, step = tile + 2.0f;
    int n = (int)(len / step);
    float start = (len - (float)n * step) / 2.0f + 1.0f;
    for (int i = 0; i < n; i++) {
        float t = start + (float)i * step;
        Rectangle q = across ? (Rectangle){ fminf(a.x, b.x) + t, a.y - tile / 2, tile, tile }
                             : (Rectangle){ a.x - tile / 2, fminf(a.y, b.y) + t, tile, tile };
        DrawRectangleRec(q, *STONES[i % 4]);
    }
}

// A window: a clay tablet in an inlaid frame with rosettes at its corners (and halfway along a long edge).
static void Tablet(Rectangle r)
{
    const float band = 10.0f;
    DrawRectangleRec((Rectangle){ r.x + 5, r.y + 7, r.width, r.height }, Fade(BLACK, 0.35f));
    Clay(r, CLAY.panel);
    // A little darker towards the rim, as a tablet pressed flat in the middle.
    DrawRectangleGradientV((int)r.x, (int)r.y + (int)band, (int)r.width, 18, Fade((Color){ 90, 60, 36, 255 }, 0.22f), Fade((Color){ 90, 60, 36, 255 }, 0.0f));
    DrawRectangleGradientV((int)r.x, (int)(r.y + r.height - band - 18), (int)r.width, 18, Fade((Color){ 90, 60, 36, 255 }, 0.0f), Fade((Color){ 90, 60, 36, 255 }, 0.22f));
    DrawRectangleLinesEx(r, band, BITUMEN);
    float h = band / 2;
    InlayBand((Vector2){ r.x + band, r.y + h }, (Vector2){ r.x + r.width - band, r.y + h }, band);
    InlayBand((Vector2){ r.x + band, r.y + r.height - h }, (Vector2){ r.x + r.width - band, r.y + r.height - h }, band);
    InlayBand((Vector2){ r.x + h, r.y + band }, (Vector2){ r.x + h, r.y + r.height - band }, band);
    InlayBand((Vector2){ r.x + r.width - h, r.y + band }, (Vector2){ r.x + r.width - h, r.y + r.height - band }, band);
    DrawRectangleLinesEx((Rectangle){ r.x + band, r.y + band, r.width - 2 * band, r.height - 2 * band }, 1, Fade(BITUMEN, 0.5f));
    float rad = 13.0f;
    Rosette((Vector2){ r.x + h, r.y + h }, rad);
    Rosette((Vector2){ r.x + r.width - h, r.y + h }, rad);
    Rosette((Vector2){ r.x + h, r.y + r.height - h }, rad);
    Rosette((Vector2){ r.x + r.width - h, r.y + r.height - h }, rad);
    if (r.width > 500) {
        Rosette((Vector2){ r.x + r.width / 2, r.y + h }, rad);
        Rosette((Vector2){ r.x + r.width / 2, r.y + r.height - h }, rad);
    }
}

// A button: on a tablet a raised tile of fired clay, over the game a dark box. Key fallback: a click
// pressed and released within one frame is lost.
static bool ButtonSized(Rectangle r, const char *label, bool enabled, int key, int size)
{
    bool hover = enabled && CheckCollisionPointRec(UiMouse(), r);
    Color fill = !enabled ? pal->buttonOff : hover ? pal->buttonHi : pal->button;
    if (pal == &CLAY) {
        DrawRectangleRounded((Rectangle){ r.x, r.y + 2, r.width, r.height }, 0.18f, 4, Fade(BITUMEN, enabled ? 0.55f : 0.25f));
        DrawRectangleRounded(r, 0.18f, 4, fill);
        if (enabled) DrawRectangleRounded((Rectangle){ r.x + 3, r.y + 2, r.width - 6, r.height * 0.4f }, 0.3f, 4, Fade(WHITE, hover ? 0.16f : 0.1f));
        DrawRectangleRoundedLinesEx(r, 0.18f, 4, 1.5f, Fade(BITUMEN, enabled ? 0.9f : 0.4f));
    } else {
        DrawRectangleRec(r, fill);
        DrawRectangleLinesEx(r, 1, C_EDGE);
    }
    CenterText(label, r.x + r.width / 2, r.y + (r.height - (float)size) / 2, size, enabled ? pal->label : pal->labelOff);
    return (hover && MousePressed(MOUSE_BUTTON_LEFT)) || (enabled && key && KeyPressed(key));
}

static bool Button(Rectangle r, const char *label, bool enabled, int key)
{
    return ButtonSized(r, label, enabled, key, 18);
}

// The choice in use, among buttons for the others: lapis with shell letters on a tablet.
static void ButtonOnSized(Rectangle r, const char *label, int size)
{
    if (pal == &CLAY) {
        DrawRectangleRounded((Rectangle){ r.x, r.y + 2, r.width, r.height }, 0.18f, 4, Fade(BITUMEN, 0.55f));
        DrawRectangleRounded(r, 0.18f, 4, LAPIS);
        DrawRectangleRounded((Rectangle){ r.x + 3, r.y + 2, r.width - 6, r.height * 0.4f }, 0.3f, 4, Fade(WHITE, 0.12f));
        DrawRectangleRoundedLinesEx(r, 0.18f, 4, 1.5f, BITUMEN);
        CenterText(label, r.x + r.width / 2, r.y + (r.height - (float)size) / 2, size, SHELL);
    } else {
        DrawRectangleRec(r, pal->buttonHi);
        DrawRectangleLinesEx(r, 2, C_GOLD);
        CenterText(label, r.x + r.width / 2, r.y + (r.height - (float)size) / 2, size, C_TEXT);
    }
}

static void ButtonOn(Rectangle r, const char *label)
{
    ButtonOnSized(r, label, 18);
}

// The title screen (lobby.c) draws its window and buttons like the game's.
void DrawTablet(Rectangle r)
{
    Tablet(r);
}

bool TabletButton(Rectangle r, const char *label, int size, bool enabled, bool on)
{
    const Palette *was = pal;
    pal = &CLAY;
    bool hit = false;
    if (on) ButtonOnSized(r, label, size);
    else hit = ButtonSized(r, label, enabled, 0, size);
    pal = was;
    return hit;
}

// The game's name as the title screen shows it: in lapis, between two rosettes.
void DrawLogo(float cx, float y, float size)
{
    const char *name = "SUMER";
    float w = GameTextWidth(name, size), rad = size * 0.26f;
    DrawGameText(name, cx - w / 2 + 1.5f, y + 2.5f, size, Fade(BITUMEN, 0.55f));
    DrawGameText(name, cx - w / 2, y, size, LAPIS);
    Rosette((Vector2){ cx - w / 2 - rad - 12, y + size * 0.5f }, rad);
    Rosette((Vector2){ cx + w / 2 + rad + 12, y + size * 0.5f }, rad);
}

static int SlotKey(int i)
{
    static const int keys[BAG_MAX] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX,
                                       KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO, KEY_MINUS, KEY_EQUAL };
    return (i >= 0 && i < BAG_MAX) ? keys[i] : 0;
}

static Rectangle PanelRect(float w, float h)
{
    w = fminf(w, UiWidth() - 16.0f);   // a narrow screen (a phone's) keeps the whole frame in sight
    h = fminf(h, UiHeight() - 16.0f);
    Rectangle r = { (UiWidth() - w) / 2, (UiHeight() - h) / 2, w, h };
    DrawRectangle(0, 0, (int)UiWidth() + 1, (int)UiHeight() + 1, Fade(BLACK, 0.35f));
    Tablet(r);
    return r;
}

// On a touch screen a slot answers when the finger lifts: a short tap is a click, and a finger held on a
// basket for two seconds empties it (the long press fills a bar along the slot's foot meanwhile).
#define HOLD_TO_DROP 2.0f
static int SlotTouch(Rectangle r, Slot s, int index)
{
    static Rectangle held;
    static float heldT;
    static bool done;
    bool same = held.x == r.x && held.y == r.y;
    bool on = GetTouchPointCount() > 0 && CheckCollisionPointRec(UiMouse(), r);
    if (on) {
        if (!same) {
            held = r;
            heldT = 0;
            done = false;
        } else if (!done) {
            heldT += GetFrameTime();
        }
        if (index >= 0 && s.item != IT_NONE && !done && heldT > 0.3f) {
            DrawRectangle((int)r.x + 2, (int)(r.y + r.height) - 6, (int)((r.width - 4) * fminf(1.0f, heldT / HOLD_TO_DROP)), 4, C_BAD);
            Fmt(hoverText, sizeof(hoverText), "Keep holding to drop the %s", ItemName(s.item));
        }
        if (index >= 0 && s.item != IT_NONE && !done && heldT >= HOLD_TO_DROP) {
            done = true;
            return CLICK_RIGHT;
        }
        return CLICK_NONE;
    }
    if (same) {
        bool tap = !done && heldT < 0.6f;
        held = (Rectangle){ -1, -1, 0, 0 };
        if (tap) return CLICK_LEFT;
    }
    return CLICK_NONE;
}

// A basket's place: on a tablet a tile of terracotta set into the clay (sunk and pale when locked).
static void SlotBox(Rectangle r, bool locked, bool hover)
{
    if (pal != &CLAY) {
        DrawRectangleRec(r, locked ? pal->locked : (hover ? C_SLOT_HI : C_SLOT));
        DrawRectangleLinesEx(r, 1, hover ? C_GOLD : (Color){ 90, 74, 56, 255 });
        return;
    }
    if (locked) {
        DrawRectangleRounded(r, 0.1f, 4, Fade(BITUMEN, 0.12f));
        DrawRectangleRoundedLinesEx(r, 0.1f, 4, 1, Fade(BITUMEN, 0.25f));
        return;
    }
    DrawRectangleRounded((Rectangle){ r.x - 1, r.y - 1, r.width + 2, r.height + 3 }, 0.1f, 4, Fade(BITUMEN, 0.35f));
    DrawRectangleRounded(r, 0.1f, 4, hover ? C_SLOT_HI : C_SLOT);
    DrawRectangleGradientV((int)r.x + 2, (int)r.y + 2, (int)r.width - 4, (int)(r.height * 0.45f), Fade(WHITE, 0.12f), Fade(WHITE, 0.0f));
    DrawRectangleRoundedLinesEx(r, 0.1f, 4, hover ? 2.0f : 1.0f, hover ? C_GOLD : Fade(BITUMEN, 0.8f));
}

static int SlotWidget(Rectangle r, Slot s, bool locked, int index)
{
    static const char *labels[BAG_MAX] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=" };
    bool hover = !locked && CheckCollisionPointRec(UiMouse(), r);

    SlotBox(r, locked, hover);
    if (locked) {
        CenterText("locked", r.x + r.width / 2, r.y + r.height / 2 - 7, 14, Fade(C_DIM, 0.8f));
        return CLICK_NONE;
    }
    if (index >= 0) Text(labels[index], r.x + r.width - 16, r.y + 6, 14, C_DIM);
    if (s.item != IT_NONE) {
        const ItemDef *d = &ITEMS[s.item];
        // Its picture at the left, how many at the right, its name along the foot.
        float pic = fminf(r.height - 22, r.width - 46);   // room for the count beside it in a narrow slot
        if (!DrawItemIcon(s.item, (Rectangle){ r.x + 4, r.y + 2, pic, pic }, WHITE)) {
            DrawRectangle((int)r.x + 8, (int)r.y + 8, 18, 18, d->color);
            DrawRectangleLines((int)r.x + 8, (int)r.y + 8, 18, 18, Fade(BLACK, 0.5f));
        }
        Text(d->name, r.x + 7, r.y + r.height - 19, 14, C_TEXT);
        const char *n = TextFormat(L("%d/%d"), s.count, d->stack);
        Text(n, r.x + r.width - 7 - (float)Measure(n, 16), r.y + r.height / 2 - 12, 16, s.count == d->stack ? C_GOLD : C_DIM);
        if (hover && !TouchOn()) {
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
        CenterText("empty", r.x + r.width / 2, r.y + r.height / 2 - 7, 14, Fade(C_DIM, 0.8f));
    }
    if (index >= 0 && KeyPressed(SlotKey(index))) return CLICK_LEFT;
    if (TouchOn()) return SlotTouch(r, s, index);
    if (!hover) return CLICK_NONE;
    if (MousePressed(MOUSE_BUTTON_LEFT)) return CLICK_LEFT;
    if (MousePressed(MOUSE_BUTTON_RIGHT) || KeyPressed(KEY_X)) return CLICK_RIGHT;
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
    Text(TextFormat(L("Baskets  %d/%d"), G.bagSlots, BAG_MAX), r.x + 24, r.y + 18, 26, C_TITLE);
    Text("Each basket holds one kind of thing: 20 dates, 10 arrows, 4 jugs of milk, or a single carcass.", r.x + 24, r.y + 50, 17, C_DIM);

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
    if (UnlimitedArrows()) Text(TextFormat(L("Unlimited arrows    arrow speed %d m/s"), (int)BowSpeed()), x0 + 420, y, 18, C_TEXT);
    else Text(TextFormat(L("Quiver %d/%d    arrow speed %d m/s"), G.quiver, G.quiverMax, (int)BowSpeed()), x0 + 420, y, 18, C_TEXT);
    y += 30;

    static const int eqs[4] = { EQ_BOW, EQ_MELEE, EQ_MACE, EQ_NONE };
    Text("In hand", x0, y + 8, 18, C_TEXT);
    for (int k = 0; k < 4; k++) {
        Rectangle b = { x0 + 90 + 176.0f * (float)k, y, 168, 32 };
        bool owned = eqs[k] != EQ_MACE || UseMace();
        const char *label = owned ? EquipName(eqs[k]) : TextFormat(L("(%s)"), MaceName());
        if (G.equip == eqs[k]) ButtonOn(b, label);
        else if (Button(b, label, owned, 0)) SetEquip(eqs[k]);
    }
    y += 40;
    static const int armors[4] = { ARMOR_BRONZE, ARMOR_LEATHER, ARMOR_WOLF, ARMOR_NONE };
    static const char *const shortName[4] = { "Bronze", "Leather", "Wolf cloak", "No armor" };
    Text("Armor", x0, y + 8, 18, C_TEXT);
    // In combat mode everyone wears bronze.
    for (int k = 0; k < 4; k++) {
        Rectangle b = { x0 + 90 + 146.0f * (float)k, y, 138, 32 };
        bool owned = armors[k] == ARMOR_NONE || (G.armorOwned & (1 << armors[k])) || G.combat;
        if (UseArmor() == armors[k]) ButtonOn(b, shortName[k]);
        else if (Button(b, owned ? shortName[k] : TextFormat(L("(%s)"), L(shortName[k])), owned && !G.combat, 0)) {
            G.armor = armors[k];
            Say(G.armor == ARMOR_NONE ? "You take off your armor." : "You put on the %s.", ArmorName(G.armor));
        }
    }
    Text(TextFormat(L("-%d%% damage"), (int)(ArmorReduction(UseArmor()) * 100)), x0 + 690, y + 8, 16, C_DIM);
    y += 44;
    Text(TextFormat(L("Coop: %d hens, feed %d, %d eggs waiting, water %.1f days    Farm water %.1f days"), G.henCount, (int)G.coopFeed,
                    G.coopEggs, G.coopWater, G.farmWater), x0, y, 18, C_TEXT);
    Text(hoverText[0] ? hoverText : TouchOn() ? "Tap: eat / drink / chew herbs / refill quiver    Hold 2 seconds: drop" : "Click or 1-6: eat / drink / chew herbs / refill quiver    Right-click or X: drop    Tab: close",
         x0, r.y + r.height - 36, 17, hoverText[0] ? C_GOLD : C_DIM);
    if (Button((Rectangle){ r.x + r.width - 344, r.y + 14, 170, 30 }, G.torchOn ? "Torch: on (B)" : "Torch: off (B)", true, 0)) {
        G.torchOn = !G.torchOn;
        Say(G.torchOn ? "You take up your torch (B). It burns from the evening on." : "You put your torch away (B).");
    }
}

static void DrawStoragePanel(void)
{
    Rectangle r = PanelRect(1000, 480);
    float gap = 7, sw = fminf(96.0f, (r.width - 48 - 30 - 9 * gap) / 9), shh = 70;
    float bx = r.x + 24, sx = r.x + 24 + 3 * (sw + gap) + 30, y0 = r.y + 98;

    hoverText[0] = '\0';
    Text("Storeroom", r.x + 24, r.y + 18, 26, C_TITLE);
    Text("Click a basket to move it between what you carry and the storage jars. Number keys store a carried basket.", r.x + 24, r.y + 50, 17, C_DIM);
    Text("Carried", bx, y0 - 22, 16, C_TEXT);
    Text("Storage jars", sx, y0 - 22, 16, C_TEXT);

    for (int i = 0; i < BAG_MAX; i++) {
        Rectangle s = { bx + (float)(i % 3) * (sw + gap), y0 + (float)(i / 3) * (shh + gap), sw, shh };
        if (SlotWidget(s, G.bag[i], i >= G.bagSlots, i) == CLICK_LEFT && G.bag[i].item != IT_NONE)
            if (MoveStack(&G.bag[i], G.store, STORE_SLOTS) == 0) Say("The storage jars are full.");
    }
    for (int i = 0; i < STORE_SLOTS; i++) {
        Rectangle s = { sx + (float)(i % 6) * (sw + gap), y0 + (float)(i / 6) * (shh + gap), sw, shh };
        if (SlotWidget(s, G.store[i], false, -1) == CLICK_LEFT && G.store[i].item != IT_NONE) {
            int item = G.store[i].item, moved = MoveStack(&G.store[i], G.bag, G.bagSlots);
            // Out of storage, it is the player's own: not lost if they are brought down today.
            G.safe[item] += moved;
            if (moved == 0) Say("No room in your baskets.");
        }
    }

    float y = r.y + r.height - 50;
    if (Button((Rectangle){ r.x + 24, y, 230, 34 }, TextFormat(L("[R] Refill quiver %d/%d"), G.quiver, G.quiverMax), G.quiver < G.quiverMax, KEY_R))
        RefillQuiverFromStore();
    Text(TextFormat(L("Arrows in storage: %d"), SlotsCount(G.store, STORE_SLOTS, IT_ARROWS)), r.x + 272, y + 8, 18, C_TEXT);
    if (hoverText[0]) Text(hoverText, r.x + 500, y + 8, 16, C_GOLD);
}

// What an offer asks for, each thing with its picture: 2 [dates] Dates + 1 [reeds] Reeds. The letters
// shrink when it is too long for 'width'.
static void OfferRow(const Offer *o, float x, float y, float width)
{
    const int in[3] = { o->in1, o->in2, o->in3 }, n[3] = { o->n1, o->n2, o->n3 };
    int count = o->in3 ? 3 : o->in2 ? 2 : 1, size = 17;
    float pic = 34;
    for (; size > 13; size--, pic -= 2) {
        float w = 0;
        for (int k = 0; k < count; k++) w += (k ? (float)Measure("+", size) + 12 : 0) + pic + 2 + (float)Measure(TextFormat("%d %s", n[k], ItemName(in[k])), size) + 4;
        if (w <= width) break;
    }
    for (int k = 0; k < count; k++) {
        if (k) {
            Text("+", x + 4, y + 10, size, C_DIM);
            x += (float)Measure("+", size) + 12;
        }
        if (DrawItemIcon(in[k], (Rectangle){ x, y + 19 - pic / 2, pic, pic }, WHITE)) x += pic + 2;
        const char *t = TextFormat("%d %s", n[k], ItemName(in[k]));
        Text(t, x, y + 10 + (float)(17 - size) / 2, size, C_TEXT);
        x += (float)Measure(t, size) + 4;
    }
}

static void DrawTradePanel(void)
{
    const Trader *t = &G.traders[G.trader];
    const float row = 44.0f;
    float h = 140.0f + row * (float)t->offerCount;
    Rectangle r = PanelRect(1000, h);

    Text(t->name, r.x + 24, r.y + 18, 26, C_TITLE);
    Text(TextFormat(L("\"%s\""), L(t->greeting)), r.x + 24, r.y + 52, 17, C_DIM);

    for (int i = 0; i < t->offerCount; i++) {
        const Offer *o = &t->offers[i];
        const char *why;
        bool ok = OfferCheck(o, &why);
        float y = r.y + 84 + row * (float)i;

        DrawRectangle((int)r.x + 16, (int)y, (int)r.width - 32, (int)row - 6, (i & 1) ? pal->rowAlt : pal->row);
        OfferRow(o, r.x + 24, y, 350.0f);
        Text("->", r.x + 385, y + 10, 17, C_DIM);
        int got = o->kind == OUT_ITEM ? o->out : o->kind == OUT_HEN ? IT_HEN : o->kind == OUT_BASKET ? IT_REEDS : IT_NONE;
        float ox = r.x + 410;
        if (got != IT_NONE && DrawItemIcon(got, (Rectangle){ ox, y + 1, 36, 36 }, WHITE)) ox += 38;
        Text(OfferOutput(o), ox, y + 10, 17, C_GOLD);
        const char *have = o->in2 ? TextFormat(L("have %d / %d"), BagCount(o->in1), BagCount(o->in2)) : TextFormat(L("have %d"), BagCount(o->in1));
        const char *note = ok ? have : why;   // kept clear of the button on a narrow screen
        Text(note, fminf(r.x + 680, r.x + r.width - 146 - (float)Measure(L(note), 14)), y + 12, 14, ok ? C_GOOD : C_BAD);

        static const int extraKeys[3] = { KEY_ZERO, KEY_MINUS, KEY_EQUAL };
        static const char *const extraLabels[3] = { "[0] Barter", "[-] Barter", "[=] Barter" };
        int key = i < 9 ? KEY_ONE + i : i < 12 ? extraKeys[i - 9] : 0;
        const char *label = i < 9 ? TextFormat(L("[%d] Barter"), i + 1) : i < 12 ? extraLabels[i - 9] : "Barter";
        if (Button((Rectangle){ r.x + r.width - 136, y + 4, 112, 30 }, label, ok, key)) Barter(G.trader, i);
    }
    Text("Click Barter or press the offer's number    E or Esc: close", r.x + 24, r.y + r.height - 36, 17, C_DIM);
}

// Playing with others the world goes on while the menu is open, and there are no save slots: the server
// keeps the world, and each player's things go with their account.
static void DrawNetPausePanel(void)
{
    Rectangle r = PanelRect(620, 530);
    float bx = r.x + 50, bw = 240, gap = r.width - 100 - 2 * bw, y = r.y + 128;

    CenterText("Paused", r.x + r.width / 2, r.y + 20, 30, C_TITLE);
    CenterText("The world goes on while this menu is open. H: controls.", r.x + r.width / 2, r.y + 64, 17, C_TEXT);
    CenterText(TextFormat(L("Server \"%s\". Your things are kept with your account."), NetRoom()), r.x + r.width / 2, r.y + 92, 16, C_DIM);
    // A battle royale's crowd goes in two narrow columns, with each player's kills and whether they are out.
    if (PlayerCount() > 6) {
        int row = 0;
        for (int s = 0; s < MAX_PLAYERS; s++) {
            const Peer *p = &G.peers[s];
            if (!p->active) continue;
            float cx = bx + (float)(row / 10) * 265.0f, cy = y + (float)(row % 10) * 14.0f;
            DrawRectangle((int)cx, (int)cy + 2, 10, 10, PlayerColor(s));
            DrawGameText(p->name, cx + 16, cy, 13, s == G.self ? C_GOLD : (p->flags & PF_OUT) ? C_DIM : C_TEXT);
            Text(TextFormat(L("%d kills%s"), p->kills, (p->flags & PF_OUT) ? L(", out") : ""), cx + 150, cy, 12, C_DIM);
            row++;
        }
    }
    for (int s = 0; s < MAX_PLAYERS && PlayerCount() <= 6; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active) continue;
        DrawRectangle((int)bx, (int)y + 2, 16, 16, PlayerColor(s));
        DrawRectangleLines((int)bx, (int)y + 2, 16, 16, Fade(BLACK, 0.6f));
        DrawGameText(p->name, bx + 26, y, 18, s == G.self ? C_GOLD : C_TEXT);
        if (s == G.self) Text(TextFormat("(%s)", L("you")), bx + 160, y + 2, 15, C_DIM);
        Text(PlayerColorName(s), bx + 250, y + 2, 15, C_DIM);
        if (G.pvp) Text(KillDeath(s), bx + 320, y + 2, 15, C_TEXT);
        if (p->wanted > 0) Text(TextFormat(L("wanted %d:%02d"), (int)p->wanted / 60, (int)p->wanted % 60), bx + 440, y + 2, 15, C_BAD);
        y += 24;
    }

    // A battle royale or a party, waiting to begin: ready, and in a party the vote.
    if (LobbyWaiting()) {
        float rw = Party() ? bw : r.width - 100;
        if (Button((Rectangle){ bx, r.y + 274, rw, 34 }, MatchReady() ? "Ready: yes (Enter)" : "Ready: no (Enter)", true, KEY_ENTER)) ToggleReady();
        if (Party() && Button((Rectangle){ bx + bw + gap, r.y + 274, bw, 34 }, "Vote a game out", true, KEY_V)) SetPanel(PANEL_PARTY);
    } else if (Party()) {
        if (Button((Rectangle){ bx, r.y + 274, r.width - 100, 34 }, "The party's games", true, KEY_V)) SetPanel(PANEL_PARTY);
    }
    if (Button((Rectangle){ bx, r.y + 316, r.width - 100, 34 }, "Game settings (S)", true, KEY_S)) SetPanel(PANEL_SETTINGS);
    if (Button((Rectangle){ bx, r.y + 358, r.width - 100, 34 }, "Your look", true, 0)) SetPanel(PANEL_LOOK);
    CenterText(RulesText(), r.x + r.width / 2, r.y + 397, 16, C_GOLD);
    y = r.y + r.height - 96;
    if (Button((Rectangle){ bx, y, bw, 40 }, "Resume (Esc)", true, 0)) SetPanel(PANEL_NONE);
    if (Button((Rectangle){ bx + bw + gap, y, bw, 40 }, "Hand in my things (F5)", true, 0)) SaveNow();
    if (Button((Rectangle){ bx, y + 54, r.width - 100, 40 }, "Leave the game (Q)", true, 0)) G.quit = true;
}

// The party's five games: while everyone waits, a vote to drop one; during the party, the order played.
// ---- A board game at the alehouse (boards.c) ----

static const Color PIECE_UI[2] = { { 40, 40, 54, 255 }, { 236, 230, 214, 255 } };
static const Color NOTE_BAD = { 245, 140, 115, 255 }, NOTE_GOOD = { 160, 225, 135, 255 };   // on a note's dark label

// A piece on the panel's board: its disc, a ring if it can move (with its number), a faint one if it moved last.
// True if it was clicked.
#if defined(SUMER_TEST)
// Test builds: where on the board panel to tap (UI units; x < 0 when not shown): Throw, Play a townsman,
// Get up, the pieces that can move by their number, then Rules.
#define BOARD_TAPS (4 + UR_PIECES)
static Vector2 boardTap[BOARD_TAPS];
#define BOARD_TAP(k, v) (boardTap[k] = (v))

bool BoardRulesShown(void)
{
    return boardRules;
}

bool BoardTapAt(int k, Vector2 *c)
{
    if (k < 0 || k >= BOARD_TAPS || boardTap[k].x < 0) return false;
    *c = Vector2Scale(boardTap[k], UiScale() / PixelRatio());
    return true;
}
#else
#define BOARD_TAP(k, v) ((void)0)
#endif

static int boardSel = -1;   // on a touch screen: the piece tapped first (its way shown); a second tap moves it

// A small arrowhead at 'at' pointing along 'dir'.
static void Chevron(Vector2 at, Vector2 dir, float size, Color col)
{
    float len = Vector2Length(dir);
    if (len < 0.001f) return;
    Vector2 d = Vector2Scale(dir, 1.0f / len), n = { -d.y, d.x };
    Vector2 tip = Vector2Add(at, Vector2Scale(d, size * 0.6f)), back = Vector2Subtract(at, Vector2Scale(d, size * 0.4f));
    Tri(tip, Vector2Add(back, Vector2Scale(n, size * 0.55f)), Vector2Subtract(back, Vector2Scale(n, size * 0.55f)), col);
}

// A piece as the panel shows it: a disc with the five dots of the old pieces. 'glow' (0-1) rings one that
// can move, brighter for the one pointed at; 'alpha' below 1 for the ghost showing where one would land.
static void BoardPiece(Vector2 c, float rad, int side, float glow, float alpha)
{
    if (glow > 0) {
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f);
        DrawRing(c, rad + 2, rad + 4 + 3 * glow * pulse, 0, 360, 36, Fade(B_GOLD, 0.45f + 0.5f * glow));
    }
    DrawCircleV((Vector2){ c.x + 2, c.y + 3 }, rad, Fade(BLACK, 0.35f * alpha));
    DrawCircleV(c, rad, Fade(PIECE_UI[side], alpha));
    DrawCircleLines((int)c.x, (int)c.y, rad, Fade(side ? (Color){ 150, 140, 120, 255 } : (Color){ 10, 10, 16, 255 }, alpha));
    Color dots = Fade(side ? (Color){ 60, 50, 40, 255 } : (Color){ 220, 214, 200, 255 }, alpha);
    for (int k = 0; k < 5; k++) {
        float t = (float)k * 2.0f * PI / 5.0f;
        DrawCircleV((Vector2){ c.x + cosf(t) * rad * 0.5f, c.y + sinf(t) * rad * 0.5f }, rad * 0.11f, dots);
    }
}

// The last move: an arrow from where the piece stood to where it went, in the mover's colour.
static void MoveArrow(Vector2 a, Vector2 b, int side, float alpha)
{
    Vector2 d = Vector2Subtract(b, a);
    float len = Vector2Length(d);
    if (len < 6) return;
    d = Vector2Scale(d, 1.0f / len);
    Vector2 n = { -d.y, d.x }, base = Vector2Subtract(b, Vector2Scale(d, 14));
    Color edge = Fade(B_GOLD, alpha), fill = Fade(PIECE_UI[side], alpha);
    DrawLineEx(a, base, 7, edge);
    DrawLineEx(a, base, 3, fill);
    Tri(b, Vector2Add(base, Vector2Scale(n, 10)), Vector2Subtract(base, Vector2Scale(n, 10)), edge);
}

// A die as the panel shows it: an ivory cube face with its number. 'roll' (1 down to 0) while it is still
// tumbling, turning over and showing any number.
static void DieFace(Vector2 c, float size, int value, float roll, int seed)
{
    const Color ivory = { 242, 234, 214, 255 }, ink = { 52, 36, 26, 255 }, edge = { 128, 104, 78, 255 };
    if (roll > 0) {
        Vector2 at = { c.x, c.y - sinf(roll * PI) * 16.0f };
        float turn = roll * 560.0f + (float)seed * 33.0f;
        DrawRectanglePro((Rectangle){ at.x + 3, at.y + 4, size, size }, (Vector2){ size / 2, size / 2 }, turn, Fade(BLACK, 0.3f));
        DrawRectanglePro((Rectangle){ at.x, at.y, size, size }, (Vector2){ size / 2, size / 2 }, turn, ivory);
        CenterText(TextFormat("%d", 1 + ((int)(roll * 23.0f) + seed) % 6), at.x, at.y - size * 0.32f, (int)(size * 0.6f), Fade(ink, 0.6f));
        return;
    }
    Rectangle q = { c.x - size / 2, c.y - size / 2, size, size };
    DrawRectangleRounded((Rectangle){ q.x + 3, q.y + 4, size, size }, 0.22f, 6, Fade(BLACK, 0.35f));
    DrawRectangleRounded(q, 0.22f, 6, ivory);
    DrawRectangleRoundedLinesEx(q, 0.22f, 6, 2, edge);
    CenterText(TextFormat("%d", value), c.x, c.y - size * 0.32f, (int)(size * 0.62f), ink);
}

// The throw: one die for Ur, two (and their sum) for the holes; empty outlines before anyone throws.
static void DrawThrow(int bi, const Board *b, float x, float y)
{
    int dice = b->kind == BG_UR ? 1 : 2;
    float roll = BoardTumble(bi);
    for (int k = 0; k < dice; k++) {
        Vector2 c = { x + 26 + (float)k * 62, y + 26 };
        if (b->roll > 0 || roll > 0) DieFace(c, 48, b->dice[k], roll, bi * 3 + k);
        else DrawRectangleRoundedLinesEx((Rectangle){ c.x - 24, c.y - 24, 48, 48 }, 0.22f, 6, 2, Fade(C_DIM, 0.6f));
    }
    if (dice == 2 && b->roll > 0 && roll <= 0) Text(TextFormat("= %d", b->roll), x + 132, y + 12, 28, C_GOLD);
}

// What happens to a piece pointed at: the way it would go and the ghost of it where it lands.
typedef struct {
    Vector2 at[40];
    int n;
    Vector2 land;
    bool line;          // a line on the holes board carries it on from 'land'
} Way;

static void DrawWay(const Way *w, int side, float rad, const char *note, Color noteCol)
{
    for (int k = 0; k + 1 < w->n; k++) {
        DrawCircleV(w->at[k], 5, Fade(BITUMEN, 0.5f));
        DrawCircleV(w->at[k], 4, Fade(B_GOLD, 0.95f));
    }
    if (w->line && w->n > 0) DrawLineEx(w->at[w->n - 1], w->land, 4, Fade(B_GOLD, 0.8f));
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f);
    BoardPiece(w->land, rad, side, 0, 0.32f);   // a ghost of the piece, faint beside the real ones
    for (int k = 0; k < 12; k += 2)   // ringed by a dashed circle
        DrawRing(w->land, rad + 3, rad + 5 + 2 * pulse, (float)k * 30.0f, (float)k * 30.0f + 22.0f, 6, Fade(B_GOLD, 0.95f));
    if (note) {   // on a dark label, to be read over the board's light squares and the clay alike
        note = L(note);
        float nw = (float)Measure(note, 15) + 14, ny = w->land.y - rad - 25;
        DrawRectangleRounded((Rectangle){ w->land.x - nw / 2, ny, nw, 22 }, 0.5f, 6, Fade(BITUMEN, 0.85f));
        CenterText(note, w->land.x, ny + 3, 15, noteCol);
    }
}

typedef struct { float ox, oy, S, rad; int view; } UrGeo;

static Vector2 UrSpot(const UrGeo *g, int side, int p, int slot)
{
    float y = side == g->view ? g->oy + 3 * g->S + 26 : g->oy - 26;
    if (p <= 0) return (Vector2){ g->ox + g->rad + (float)slot * g->rad * 1.35f, y };
    if (p >= UR_HOME) return (Vector2){ g->ox + 8 * g->S - g->rad - (float)slot * g->rad * 1.35f, y };
    int col, row;
    UrSquare(g->view, side, p, &col, &row);
    return (Vector2){ g->ox + ((float)col + 0.5f) * g->S, g->oy + ((float)row + 0.5f) * g->S };
}

// Pieces of both sides where the screen shows them (walking ones between squares), which of 'me' can move,
// and the one pointed at. Shared by both boards: 'spot' places position p of a side, with its slot in the
// start or home row.
typedef Vector2 (*SpotFn)(const void *geo, int side, int p, int slot);

typedef struct {
    Vector2 c[2][UR_PIECES];
    int slot[2][UR_PIECES];     // each piece's place in the start or home row
    bool moving[2][UR_PIECES], can[UR_PIECES];
    int waiting[2], home[2], focus, hover, order[UR_PIECES], orders;
} Pieces;

static void PlacePieces(int bi, const Board *b, SpotFn spot, const void *geo, float lift, int me, const int *moves, int nm, Pieces *out)
{
    int n = BoardPieces(b->kind), homeAt = BoardHome(b->kind);
    memset(out, 0, sizeof(*out));
    out->focus = out->hover = -1;
    for (int s = 0; s < 2; s++) {
        int waitSlot[UR_PIECES], homeSlot[UR_PIECES];
        for (int i = 0; i < n; i++) {
            waitSlot[i] = b->pos[s][i] == 0 ? out->waiting[s]++ : -1;
            homeSlot[i] = b->pos[s][i] >= homeAt ? out->home[s]++ : -1;
        }
        for (int i = 0; i < n; i++) {
            int pa, pb;
            float k, up;
            BoardAnim(bi, s, i, &pa, &pb, &k, &up);
            int slot = waitSlot[i] >= 0 ? waitSlot[i] : homeSlot[i] >= 0 ? homeSlot[i] : (pa == 0 ? out->waiting[s] : out->home[s]);
            out->slot[s][i] = slot;
            Vector2 c = Vector2Lerp(spot(geo, s, pa, slot), spot(geo, s, pb, slot), k);
            c.y -= up * lift;
            out->c[s][i] = c;
            out->moving[s][i] = BoardMoving(bi, s, i);
        }
    }
    if (me < 0) return;
    for (int m = 0; m < nm; m++) out->can[moves[m]] = true;
    // The keys 1-7 and the order of taps: the pieces that can move, left to right.
    for (int i = 0; i < n; i++)
        if (out->can[i]) {
            int at = out->orders++;
            while (at > 0 && out->c[me][out->order[at - 1]].x > out->c[me][i].x) {
                out->order[at] = out->order[at - 1];
                at--;
            }
            out->order[at] = i;
        }
}

// The panel's routine for both boards: the way of the piece in focus and every other one's landing faintly,
// then the pieces (walking ones on top). Returns the piece clicked or tapped; *confirm when the landing
// of the piece already picked was tapped.
static int DrawPiecesAndWays(const Board *b, const Pieces *pc, int me, float rad, float homeRad, const Way *ways, const char *const *notes,
                             const Color *noteCols, bool *confirm)
{
    int n = BoardPieces(b->kind), homeAt = BoardHome(b->kind), clicked = -1;
    *confirm = false;
    if (me >= 0)
        for (int i = 0; i < n; i++)
            if (pc->can[i] && i != pc->focus) DrawRing(ways[i].land, rad * 0.35f, rad * 0.5f, 0, 360, 24, Fade(B_GOLD, 0.7f));
    if (me >= 0 && pc->focus >= 0) DrawWay(&ways[pc->focus], me, rad, notes[pc->focus], noteCols[pc->focus]);
    for (int pass = 0; pass < 2; pass++)
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < n; i++) {
                if (pc->moving[s][i] != (pass == 1)) continue;
                int p = b->pos[s][i];
                bool can = s == me && pc->can[i];
                BoardPiece(pc->c[s][i], (p == 0 || p >= homeAt) && !pc->moving[s][i] ? homeRad : rad, s,
                           can ? (i == pc->focus ? 1.0f : 0.5f) : 0, 1.0f);
            }
    if (me < 0 || !MousePressed(MOUSE_BUTTON_LEFT)) return -1;
    Vector2 m = UiMouse();
    if (pc->focus >= 0 && CheckCollisionPointCircle(m, ways[pc->focus].land, rad + 6)) {
        *confirm = true;
        return pc->focus;
    }
    for (int i = 0; i < n; i++)
        if (pc->can[i] && CheckCollisionPointCircle(m, pc->c[me][i], rad + 5)) clicked = i;
    return clicked;
}

static Vector2 UrSpotFn(const void *geo, int side, int p, int slot)
{
    return UrSpot((const UrGeo *)geo, side, p, slot);
}

// The Royal Game of Ur from above, 'view' nearest, with your way round numbered.
static int DrawUrPanel(Rectangle a, int bi, const Board *b, int view, int me, const int *moves, int nm, Pieces *pc, bool *confirm)
{
    float S = fminf(a.width / 9.0f, (a.height - 110) / 3.0f);
    UrGeo g = { a.x + (a.width - 8 * S) / 2, a.y + 56, S, S * 0.32f, view };
    float ox = g.ox, oy = g.oy;
    const Color lapis = { 40, 62, 128, 255 }, shell = { 230, 218, 190, 255 }, red = { 186, 70, 54, 255 }, ink = { 92, 62, 40, 255 };
    Texture2D wood = BoardTexture();   // the board's box of wood under the squares
    Rectangle frame = { ox - 12, oy - 12, 8 * S + 24, 3 * S + 24 };
    DrawTexturePro(wood, (Rectangle){ 0, 0, frame.width, frame.height }, frame, (Vector2){ 0, 0 }, 0, WHITE);
    DrawRectangleLinesEx(frame, 2, (Color){ 30, 20, 14, 255 });
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 8; col++) {
            if (!UrSquareExists(col, row)) continue;
            Rectangle q = { ox + (float)col * S, oy + (float)row * S, S, S };
            DrawRectangleRec(q, lapis);
            DrawRectangle((int)(q.x + 3), (int)(q.y + 3), (int)(S - 6), (int)(S - 6), shell);
            Vector2 c = { q.x + S / 2, q.y + S / 2 };
            if (UrRosetteSquare(col, row)) {
                for (int k = 0; k < 8; k++) {
                    float t = (float)k * PI / 4;
                    DrawCircleV((Vector2){ c.x + cosf(t) * S * 0.24f, c.y + sinf(t) * S * 0.24f }, S * 0.11f, k & 1 ? lapis : red);
                }
                DrawCircleV(c, S * 0.1f, red);
            } else if ((col + row) % 3 == 0) {
                for (int k = 0; k < 5; k++) DrawCircleV((Vector2){ c.x + (k == 4 ? 0 : (k & 1 ? 1 : -1) * S * 0.22f), c.y + (k == 4 ? 0 : (k & 2 ? 1 : -1) * S * 0.22f) }, S * 0.07f, Fade(lapis, 0.75f));
            } else if ((col + row) % 3 == 1) {
                DrawCircleV(c, S * 0.26f, Fade(lapis, 0.75f));
                DrawCircleV(c, S * 0.17f, shell);
                DrawCircleV(c, S * 0.07f, red);
            } else {
                for (int k = 0; k < 4; k++) DrawCircleV((Vector2){ c.x + ((float)k - 1.5f) * S * 0.18f, c.y }, S * 0.06f, Fade(lapis, 0.75f));
                DrawLineEx((Vector2){ c.x - S * 0.32f, c.y - S * 0.18f }, (Vector2){ c.x + S * 0.32f, c.y - S * 0.18f }, 2, Fade(lapis, 0.75f));
                DrawLineEx((Vector2){ c.x - S * 0.32f, c.y + S * 0.18f }, (Vector2){ c.x + S * 0.32f, c.y + S * 0.18f }, 2, Fade(lapis, 0.75f));
            }
        }
    // Your way round: each square numbered, arrows from one to the next, in from the start row and off at 15.
    for (int p = 0; p <= 14; p++) {
        Vector2 c0 = UrSpot(&g, view, p, 0), c1 = UrSpot(&g, view, p + 1, 0);
        Chevron(Vector2Lerp(c0, c1, 0.5f), Vector2Subtract(c1, c0), S * 0.16f, Fade((Color){ 200, 150, 60, 255 }, 0.9f));
        if (p >= 1) {
            DrawRectangle((int)(c0.x - S / 2 + 4), (int)(c0.y - S / 2 + 4), p >= 10 ? 19 : 12, 15, Fade(shell, 0.85f));
            Text(TextFormat("%d", p), c0.x - S / 2 + 5, c0.y - S / 2 + 4, 14, ink);
        }
    }
    PlacePieces(bi, b, UrSpotFn, &g, S * 0.35f, me, moves, nm, pc);
    // The last move, as an arrow a few seconds long.
    const BoardEvent *e = BoardLastEvent(bi);
    if (e && e->age < 5.0f)
        MoveArrow(UrSpot(&g, e->side, e->from, pc->waiting[e->side]), UrSpot(&g, e->side, e->to, pc->slot[e->side][e->piece]),
                  e->side, e->age < 4.0f ? 0.9f : 0.9f * (5.0f - e->age));
    // Where each piece of yours that can move would land.
    Way ways[UR_PIECES];
    const char *notes[UR_PIECES];
    Color noteCols[UR_PIECES];
    for (int i = 0; i < UR_PIECES; i++) {
        ways[i].n = 0;
        notes[i] = NULL;
        noteCols[i] = B_GOLD;
        if (me < 0 || !pc->can[i]) continue;
        int from = b->pos[me][i], to = BoardTarget(b, me, i);
        for (int p = from + 1; p <= to && ways[i].n < 39; p++) ways[i].at[ways[i].n++] = UrSpot(&g, me, p, pc->home[me]);
        ways[i].land = UrSpot(&g, me, to, pc->home[me]);
        ways[i].line = false;
        if (to >= UR_HOME) notes[i] = "Off";
        else if (UrRosette(to)) notes[i] = "Throws again";
        else if (to >= 5 && to <= 12)
            for (int k = 0; k < UR_PIECES; k++)
                if (b->pos[1 - me][k] == to) {
                    notes[i] = "Takes";
                    noteCols[i] = NOTE_BAD;
                }
    }
    // The piece in focus: the one pointed at, or on a touch screen the one tapped.
    if (me >= 0 && !TouchOn())
        for (int i = 0; i < UR_PIECES; i++)
            if (pc->can[i] && CheckCollisionPointCircle(UiMouse(), pc->c[me][i], g.rad + 5)) pc->hover = i;
    pc->focus = TouchOn() ? (boardSel >= 0 && boardSel < UR_PIECES && pc->can[boardSel] ? boardSel : -1) : pc->hover;
    int clicked = DrawPiecesAndWays(b, pc, me, g.rad, g.rad * 0.8f, ways, notes, noteCols, confirm);
    Text("waiting", ox, oy + 3 * S + 45, 16, C_DIM);
    Text("home", ox + 8 * S - (float)Measure(L("home"), 16), oy + 3 * S + 45, 16, C_DIM);
    return clicked;
}

typedef struct { float ox, oy, bw, bh; int view; } HoleGeo;

static Vector2 HoleSpot(const void *geo, int side, int p, int slot)
{
    const HoleGeo *g = (const HoleGeo *)geo;
    float sd = side == g->view ? -1.0f : 1.0f;
    if (p <= 0) return (Vector2){ g->ox + g->bw / 2 + sd * (g->bw / 2 + 40), g->oy + g->bh - 20 - (float)slot * 26 };
    if (p >= HOLES_HOME) return (Vector2){ g->ox + g->bw / 2 + sd * (18 + (float)slot * 14), g->oy + 0.06f * g->bh - 16 };
    Vector2 uv = HolesHole(g->view, side, p);
    return (Vector2){ g->ox + uv.x * g->bw, g->oy + (1.0f - uv.y) * g->bh };
}

// Fifty-eight holes from above, 'view' on the left: your holes numbered, arrows up your way, the lines that
// carry a pin up (green end) or down (red end).
static int DrawHolesPanel(Rectangle a, int bi, const Board *b, int view, int me, const int *moves, int nm, Pieces *pc, bool *confirm)
{
    float bh = a.height - 40, bw = fminf(a.width - 140, bh * 0.82f);
    HoleGeo g = { a.x + (a.width - bw) / 2, a.y + 30, bw, bh, view };
    float ox = g.ox, oy = g.oy;
    const Color ivory = { 226, 210, 176, 255 }, line = { 120, 80, 50, 255 }, hole = { 70, 52, 40, 255 };
    const Color up = { 70, 150, 80, 255 }, down = { 190, 70, 50, 255 };
    Texture2D wood = BoardTexture();   // a wooden board with an ivory face inlaid
    DrawTexturePro(wood, (Rectangle){ 0, 0, bw + 24, bh + 24 }, (Rectangle){ ox - 12, oy - 12, bw + 24, bh + 24 }, (Vector2){ 0, 0 }, 0, WHITE);
    DrawRectangleRounded((Rectangle){ ox, oy, bw, bh }, 0.18f, 8, ivory);
    DrawRectangleRoundedLinesEx((Rectangle){ ox, oy, bw, bh }, 0.18f, 8, 2, line);
    for (int s = 0; s < 2; s++) {
        static const int links[2][2] = { { 6, 20 }, { 8, 10 } };
        for (int k = 0; k < 2; k++) {
            Vector2 lo = HoleSpot(&g, s, links[k][0], 0), hi = HoleSpot(&g, s, links[k][1], 0);
            DrawLineEx(lo, hi, 3, line);
            Chevron(Vector2Lerp(lo, hi, 0.85f), Vector2Subtract(hi, lo), 12, up);
            Chevron(Vector2Lerp(hi, lo, 0.85f), Vector2Subtract(lo, hi), 12, down);
        }
        for (int p = 1; p <= 29; p++) {
            Vector2 c = HoleSpot(&g, s, p, 0);
            DrawCircleV(c, p == 6 || p == 8 || p == 10 || p == 20 ? 8 : 6, hole);
            if (p == 6 || p == 8) DrawRing(c, 8, 10, 0, 360, 20, up);   // land here: up the line
            if (p == 10 || p == 20) DrawRing(c, 8, 10, 0, 360, 20, down);   // land here: down it
            bool label = s == view ? (p == 1 || p % 5 == 0 || p == 29) : (p == 1 || p == 15 || p == 29);
            if (label) Text(TextFormat("%d", p), c.x + (s == view ? -28.0f : 13.0f), c.y - 8, 13, s == view ? (Color){ 92, 62, 40, 255 } : C_DIM);
            if (s == view && p < 29) {
                Vector2 c1 = HoleSpot(&g, s, p + 1, 0);
                if (p != 14) Chevron(Vector2Lerp(c, c1, 0.5f), Vector2Subtract(c1, c), 8, Fade((Color){ 200, 150, 60, 255 }, 0.8f));
            }
        }
    }
    Vector2 goal = { ox + 0.5f * bw, oy + (1.0f - 0.94f) * bh };
    DrawCircleV(goal, 12, hole);
    PlacePieces(bi, b, HoleSpot, &g, 18.0f, me, moves, nm, pc);
    const BoardEvent *e = BoardLastEvent(bi);
    if (e && e->age < 5.0f)
        MoveArrow(HoleSpot(&g, e->side, e->from, pc->waiting[e->side]), HoleSpot(&g, e->side, e->to, pc->slot[e->side][e->piece]),
                  e->side, e->age < 4.0f ? 0.9f : 0.9f * (5.0f - e->age));
    Way ways[UR_PIECES];
    const char *notes[UR_PIECES];
    Color noteCols[UR_PIECES];
    for (int i = 0; i < UR_PIECES; i++) {
        ways[i].n = 0;
        notes[i] = NULL;
        noteCols[i] = B_GOLD;
        if (me < 0 || i >= HOLES_PIECES || !pc->can[i]) continue;
        int from = b->pos[me][i], to = BoardTarget(b, me, i), land = from + b->roll;
        if (land > HOLES_HOME) land = HOLES_HOME;
        for (int p = from + 1; p <= land && ways[i].n < 39; p++) ways[i].at[ways[i].n++] = HoleSpot(&g, me, p, pc->home[me]);
        ways[i].land = HoleSpot(&g, me, to, pc->home[me]);
        ways[i].line = to != land;
        if (to >= HOLES_HOME) notes[i] = "Home";
        else if (to > land) {
            notes[i] = "Up the line";
            noteCols[i] = NOTE_GOOD;
        } else if (to < land) {
            notes[i] = "Down the line";
            noteCols[i] = NOTE_BAD;
        }
    }
    if (me >= 0 && !TouchOn())
        for (int i = 0; i < HOLES_PIECES; i++)
            if (pc->can[i] && CheckCollisionPointCircle(UiMouse(), pc->c[me][i], 15)) pc->hover = i;
    pc->focus = TouchOn() ? (boardSel >= 0 && boardSel < HOLES_PIECES && pc->can[boardSel] ? boardSel : -1) : pc->hover;
    return DrawPiecesAndWays(b, pc, me, 10, 10, ways, notes, noteCols, confirm);
}

// The rules in short, shown over the board (the Rules button), for whoever has never played.
static void DrawBoardRules(Rectangle a, bool ur)
{
    static const char *const UR[] = {
        "Goal: be the first to take all seven of your pieces round the board and off it.",
        "On your turn throw the die and move one of your pieces that many squares. The numbers on the squares show your way round: in at 1 on your own row, along the whole middle row, back onto your row at 13 and 14, and off.",
        "Any throw that reaches past square 14 takes the piece off the board.",
        "You cannot land on your own piece. Land on your opponent's piece in the middle row (5 to 12) and it goes back to the start.",
        "Land on a rosette (the flower squares) and you throw again. A piece on the middle rosette (8) cannot be taken.",
        "If no piece can move, the turn passes.",
    };
    static const char *const HOLES[] = {
        "Goal: be the first to bring all five of your pins up to the shared hole at the top.",
        "On your turn throw both dice, add them, and move one of your pins that many holes up your side. Your holes are numbered 1 to 29.",
        "The lines join hole 6 with 20, and 8 with 10. Land exactly on one end and the pin goes along the line: from 6 up to 20, from 8 up to 10, from 20 down to 6, from 10 down to 8.",
        "Any throw that reaches past hole 29 brings the pin home.",
        "Two of your pins cannot share a hole. Pins never take each other.",
        "If no pin can move, the turn passes.",
    };
    const char *const *lines = ur ? UR : HOLES;
    int n = ur ? (int)(sizeof(UR) / sizeof(UR[0])) : (int)(sizeof(HOLES) / sizeof(HOLES[0]));
    DrawRectangleRounded(a, 0.03f, 4, pal->rowAlt);
    DrawRectangleRoundedLinesEx(a, 0.03f, 4, 1, Fade(C_EDGE, 0.6f));
    Text("How to play", a.x + 16, a.y + 12, 20, C_GOLD);
    float y = a.y + 42, w = a.width - 44;
    for (int i = 0; i < n; i++) {
        DrawCircleV((Vector2){ a.x + 22, y + 9 }, 3, C_GOLD);
        y += Paragraph(lines[i], a.x + 32, y, w, 16, 20) + 7;
    }
    y += 4;
    Paragraph("The winner gains 20 experience against a player, 10 against a townsman. Getting up gives the "
              "game away once twenty moves are made.", a.x + 16, y, a.width - 32, 15, 19);
}

// What the last move did, in words (the arrow on the board shows where).
static const char *MoveNews(const Board *b, const BoardEvent *e, int me, const char *mover)
{
    bool you = e->side == me, ur = b->kind == BG_UR;
    int n = e->steps;
    if (ur) {
        if (e->home) return you ? TextFormat(L("You moved a piece %d squares and off the board."), n) : TextFormat(L("%s moved a piece %d squares and off the board."), mover, n);
        if (e->took) return you ? TextFormat(L("You moved %d squares and took a piece: it goes back to the start."), n) : TextFormat(L("%s moved %d squares and took your piece: it goes back to the start."), mover, n);
        if (e->again) return you ? TextFormat(L("You moved %d squares onto a rosette: throw again."), n) : TextFormat(L("%s moved %d squares onto a rosette and throws again."), mover, n);
        return you ? TextFormat(L("You moved a piece %d squares."), n) : TextFormat(L("%s moved a piece %d squares."), mover, n);
    }
    if (e->home) return you ? TextFormat(L("You moved a pin %d holes and home."), n) : TextFormat(L("%s moved a pin %d holes and home."), mover, n);
    if (e->up) return you ? TextFormat(L("You moved %d holes onto a line: up it goes."), n) : TextFormat(L("%s moved %d holes onto a line: up it goes."), mover, n);
    if (e->down) return you ? TextFormat(L("You moved %d holes onto a line: down it goes."), n) : TextFormat(L("%s moved %d holes onto a line: down it goes."), mover, n);
    return you ? TextFormat(L("You moved a pin %d holes."), n) : TextFormat(L("%s moved a pin %d holes."), mover, n);
}

static void DrawBoardPanel(void)
{
    int bi = BoardViewing();
    const Board *b = BoardAt(bi);
    if (!b || !BoardsOn()) {
        SetPanel(PANEL_NONE);
        return;
    }
#if defined(SUMER_TEST)
    for (int k = 0; k < BOARD_TAPS; k++) boardTap[k] = (Vector2){ -1, -1 };
#endif
    Rectangle r = PanelRect(fminf(860.0f, UiWidth() - 20.0f), fminf(520.0f, UiHeight() - 16.0f));
    int me = BoardSide(bi, G.self), view = me >= 0 ? me : 0, other = 1 - view;
    bool ur = b->kind == BG_UR, opponent = b->seat[0] != SEAT_NONE && b->seat[1] != SEAT_NONE;
    bool mine = me >= 0 && opponent && b->turn == me && b->winner < 0;
    bool rolling = BoardTumble(bi) > 0;
    int moves[8], nm = mine && b->roll > 0 && !rolling ? BoardMoves(b, me, moves) : 0;
    if (nm == 0) boardSel = -1;
    Text(ur ? "The Royal Game of Ur" : "Fifty-eight holes", r.x + 24, r.y + 16, 26, C_TITLE);

    Rectangle area = { r.x + 16, r.y + 50, r.width - 290, r.height - 92 };   // the foot keeps a line for the keys
    int clicked = -1;
    bool confirm = false;
    Pieces pc;
    memset(&pc, 0, sizeof(pc));
    if (boardRules) DrawBoardRules((Rectangle){ area.x, area.y, area.width, area.height - 30 }, ur);
    else if (ur) clicked = DrawUrPanel(area, bi, b, view, nm ? me : -1, moves, nm, &pc, &confirm);
    else clicked = DrawHolesPanel(area, bi, b, view, nm ? me : -1, moves, nm, &pc, &confirm);
    for (int k = 0; k < pc.orders && k < 7; k++) BOARD_TAP(3 + k, pc.c[me][pc.order[k]]);

    // At the right: who plays (whose turn it is lit), the throw, what happens now, what just happened.
    float x = r.x + r.width - 262, y = r.y + 60;
    const char *names[2];
    for (int s = 0; s < 2; s++)
        names[s] = b->seat[s] >= 0 ? G.peers[b->seat[s]].name : b->seat[s] == SEAT_NPC ? L("A townsman") : L("(nobody)");
    if (opponent && b->winner < 0) DrawRectangleRounded((Rectangle){ x - 6, y - 4 + (b->turn == view ? 0 : 26), 246, 26 }, 0.4f, 6, Fade(C_GOLD, 0.22f));
    DrawCircleV((Vector2){ x + 8, y + 9 }, 7, PIECE_UI[view]);
    Text(me >= 0 ? L("You") : names[view], x + 22, y, 18, C_TEXT);
    DrawCircleV((Vector2){ x + 8, y + 35 }, 7, PIECE_UI[other]);
    Text(names[other], x + 22, y + 26, 18, C_TEXT);
    y += 62;
    DrawThrow(bi, b, x, y);
    y += 66;
    const char *status;
    bool touch = TouchOn();
    if (!opponent) status = me >= 0 ? L("Waiting for someone to sit across, or play a townsman.") : L("Nobody is playing here.");
    else if (b->winner >= 0) status = b->winner == me ? L("You won the game!") : TextFormat(L("%s won the game."), names[b->winner]);
    else if (!mine) status = me >= 0 ? TextFormat(L("%s's turn."), names[b->turn]) : TextFormat(L("%s's turn (watching)."), names[b->turn]);
    else if (b->roll < 0) status = L(ur ? (touch ? "Your turn: throw the die." : "Your turn: throw the die (Space).") : (touch ? "Your turn: throw the dice." : "Your turn: throw the dice (Space)."));
    else if (rolling) status = "";
    else if (nm == 0) status = TextFormat(L(ur ? "You threw %d, but no piece can move %d squares. The turn passes." : "You threw %d, but no pin can move %d holes. The turn passes."), b->roll, b->roll);
    else if (ur) status = TextFormat(L(touch ? "You threw %d: move one of the glowing pieces %d squares. Tap a piece to see where it lands, then tap it again."
                                              : "You threw %d: move one of the glowing pieces %d squares. Point at a piece to see where it lands."), b->roll, b->roll);
    else status = TextFormat(L(touch ? "You threw %d: move one of the glowing pins %d holes. Tap a pin to see where it lands, then tap it again."
                                      : "You threw %d: move one of the glowing pins %d holes. Point at a pin to see where it lands."), b->roll, b->roll);
    if (status[0]) y += Paragraph(status, x, y, 240, 17, 21) + 8;
    const BoardEvent *e = BoardLastEvent(bi);
    if (e && e->age < 8.0f && opponent) {
        const char *news = MoveNews(b, e, me, names[e->side]);
        Color col = Fade(C_GOLD, e->age < 7.0f ? 0.95f : 0.95f * (8.0f - e->age));
        paragraphColor = &col;
        y += Paragraph(news, x, y, 240, 16, 20) + 8;
        paragraphColor = NULL;
    }

    if (me >= 0) {
        BOARD_TAP(0, ((Vector2){ x + 120, y + 17 }));
        if (Button((Rectangle){ x, y, 240, 34 }, touch ? "Throw" : "Throw (Space)", mine && b->roll < 0, KEY_SPACE)) BoardAct(bi, BA_THROW, 0);
        y += 42;
        if (!opponent) BOARD_TAP(1, ((Vector2){ x + 120, y + 17 }));
        if (!opponent && Button((Rectangle){ x, y, 240, 34 }, touch ? "Play a townsman" : "Play a townsman (Enter)", true, KEY_ENTER)) BoardAct(bi, BA_NPC, 0);
    }
    // At the foot: the rules, and getting up.
    Rectangle rb = { x, r.y + r.height - 76, me >= 0 ? 116 : 240, 34 };
    BOARD_TAP(3 + UR_PIECES, ((Vector2){ rb.x + rb.width / 2, rb.y + 17 }));
    if (Button(rb, boardRules ? (touch ? "Board" : "Board (R)") : (touch ? "Rules" : "Rules (R)"), true, KEY_R)) boardRules = !boardRules;
    if (me >= 0) {
        Rectangle gu = { x + 124, rb.y, 116, 34 };
        BOARD_TAP(2, ((Vector2){ gu.x + 58, gu.y + 17 }));
        if (Button(gu, "Get up", true, 0)) {
            BoardAct(bi, BA_LEAVE, 0);
            SetPanel(PANEL_NONE);
            return;
        }
    }
    if (touch) {   // the Close button in the Menu button's place
        if (me >= 0) Text("Close: away from the board (you keep your place)", r.x + 24, r.y + r.height - 34, 16, C_DIM);
    } else {
        Text(me >= 0 ? "E or Esc: away from the board (you keep your place)" : "E or Esc: close", r.x + 24, r.y + r.height - 34, 16, C_DIM);
    }
    // A move: clicked (or the keys 1-7, the glowing pieces left to right); on a touch screen the first tap only
    // shows the piece's way, the second (on it or where it lands) moves it.
    if (nm > 0 && !boardRules)
        for (int k = 0; k < pc.orders && k < 7; k++)
            if (KeyPressed(KEY_ONE + k)) {
                clicked = pc.order[k];
                confirm = true;
            }
    if (clicked >= 0 && nm > 0) {
        if (touch && !confirm && boardSel != clicked) {
            boardSel = clicked;
        } else {
            boardSel = -1;
            BoardAct(bi, BA_MOVE, clicked);
        }
    }
}

static void DrawPartyPanel(void)
{
    Rectangle r = PanelRect(760, 470);
    float x = r.x + 30, y = r.y + 70;
    bool voting = LobbyWaiting();
    CenterText("Sumer Party", r.x + r.width / 2, r.y + 18, 28, C_TITLE);
    CenterText(voting ? "Vote one game out: the most voted is left out, the other four come in a random order."
                      : "The games of this party, and the one voted out.", r.x + r.width / 2, r.y + 50, 15, C_DIM);
    for (int g = 0; g < PG_COUNT; g++) {
        Text(PartyGameName(g), x, y, 20, PartyVote() == g ? C_BAD : C_GOLD);
        Paragraph(PartyGameRule(g), x, y + 24, 480, 15, 17.5f);
        if (voting) {
            Text(TextFormat(L("%d votes out"), PartyVotes(g)), x + 500, y + 4, 15, C_DIM);
            if (Button((Rectangle){ x + 590, y, 110, 30 }, PartyVote() == g ? "Take back" : "Vote out", true, 0)) SetPartyVote(g);
        }
        y += 66;
    }
    float by = r.y + r.height - 54;
    if (voting && Button((Rectangle){ x, by, 260, 38 }, MatchReady() ? "Ready: yes (Enter)" : "Ready: no (Enter)", true, KEY_ENTER)) ToggleReady();
    if (Button((Rectangle){ r.x + r.width - 30 - 200, by, 200, 38 }, "Back (Esc)", true, 0)) SetPanel(PANEL_PAUSE);
}

static void DrawPausePanel(void)
{
    if (Online()) {
        DrawNetPausePanel();
        return;
    }
    Rectangle r = PanelRect(620, 530);
    float bx = r.x + 50, bw = 240, gap = r.width - 100 - 2 * bw;
    int active = SaveSlot();

    CenterText("Paused", r.x + r.width / 2, r.y + 20, 30, C_TITLE);
    CenterText("Press H for the controls and tips.", r.x + r.width / 2, r.y + 64, 18, C_TEXT);
    CenterText("Progress is saved to the slot in play when you sleep and when you quit.", r.x + r.width / 2, r.y + 94, 16, C_DIM);

    for (int k = 0; k < SAVE_SLOTS; k++) {
        float y = r.y + 126 + 46.0f * (float)k;
        int day;
        float hour;
        bool has = SaveSummary(k, &day, &hour);
        const char *info = has ? TextFormat(L("Day %d  %02d:%02d"), day, (int)hour, (int)((hour - floorf(hour)) * 60.0f)) : "empty";
        DrawRectangle((int)bx, (int)y, (int)(r.width - 100), 40, k == active ? pal->rowHi : pal->rowAlt);
        Text(TextFormat(L("Slot %d"), k + 1), bx + 12, y + 10, 20, k == active ? C_GOLD : C_TEXT);
        Text(k == active ? "playing" : info, bx + 100, y + 12, 17, k == active ? C_GOOD : has ? C_TEXT : C_DIM);
        if (k == active) continue;
        if (Button((Rectangle){ bx + r.width - 100 - 190, y + 5, 180, 30 }, has ? "Switch to this" : "Start a game here", true, 0))
            G.requestSlot = k + 1;
    }

    if (Button((Rectangle){ bx, r.y + 316, r.width - 100, 34 }, "Game settings (S)", true, KEY_S)) SetPanel(PANEL_SETTINGS);
    if (Button((Rectangle){ bx, r.y + 358, r.width - 100, 34 }, "Your look", true, 0)) SetPanel(PANEL_LOOK);

    float y = r.y + r.height - 110;
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

// The settings that only change how the game looks and sounds to this player, not how it plays.
static void DrawSettingsPanel(void)
{
    Rectangle r = PanelRect(560, 524);
    float bx = r.x + 50, bw = r.width - 100, y = r.y + 96;

    CenterText("Game settings", r.x + r.width / 2, r.y + 20, 30, C_TITLE);
    CenterText("Kept in this browser. The keys work while you play, too.", r.x + r.width / 2, r.y + 60, 17, C_DIM);
    const char *lang = Language() == LANG_TR ? "Dil / Language: Türkçe (L)" : "Language / Dil: English (L)";
    if (Button((Rectangle){ bx, y, bw, 34 }, lang, true, KEY_L)) {
        SetLanguage(Language() == LANG_TR ? LANG_EN : LANG_TR);
        SaveLanguage();
    }
    y += 42;
    if (Button((Rectangle){ bx, y, bw, 34 }, TextFormat(L("Graphics: %s (G)"), GfxLabel()), true, KEY_G)) GfxSetChoice(GfxChoice() + 1);
    y += 42;
    if (Button((Rectangle){ bx, y, bw, 34 }, TextFormat(L("Music: %s (N)"), L(MusicLevelName())), true, KEY_N))
        MusicSetLevel((MusicLevel() + 1) % 4);
    y += 42;
    if (Button((Rectangle){ bx, y, bw, 34 }, TextFormat(L("Sounds: %s"), L(SfxLevelName())), true, 0)) SfxSetLevel((SfxLevel() + 1) % 4);
    y += 42;
    if (Button((Rectangle){ bx, y, bw, 34 }, G.arrowCam ? "Arrow camera: on (Z)" : "Arrow camera: off (Z)", true, KEY_Z)) {
        G.arrowCam = !G.arrowCam;
        SaveGamePrefs();
    }
    y += 42;
    if (Button((Rectangle){ bx, y, bw, 34 }, G.firstPerson ? "View: first person (P)" : "View: behind you (P)", true, KEY_P)) {
        G.firstPerson = !G.firstPerson;
        SaveGamePrefs();
    }
    y += 42;
    // The frame limit (a phone runs cooler at 30), and on a touch screen which side the buttons are on.
    float fw = TouchOn() ? (bw - 8) / 2 : bw;
    const char *fps = FpsLimit() ? TextFormat(L("Frame limit: %d a second"), FpsLimit()) : L("Frame limit: none");
    if (Button((Rectangle){ bx, y, fw, 34 }, fps, true, 0)) NextFpsLimit();
    if (TouchOn() && Button((Rectangle){ bx + fw + 8, y, fw, 34 }, LeftHanded() ? "Buttons: left hand" : "Buttons: right hand", true, 0))
        SetLeftHanded(!LeftHanded());
    y += 42;
    if (Button((Rectangle){ bx, y, bw, 34 }, G.showFps ? "Frame rate shown: on" : "Frame rate shown: off", true, 0)) {
        G.showFps = !G.showFps;
        SaveGamePrefs();
    }
    if (Button((Rectangle){ bx, r.y + r.height - 58, bw, 40 }, "Back", true, KEY_BACKSPACE)) SetPanel(PANEL_PAUSE);
}

// Choosing how you look: hair, beard, what you wear and your jewellery, shown on your own body
// beside the choices (the armor you have on goes over it). Each change is kept at once.
static void DrawLookPanel(void)
{
    static const char *const rows[5] = { "Body", "Hair", "Beard", "Clothes", "Jewellery" };
    static const int counts[5] = { 2, HAIR_KINDS, BEARD_KINDS, DRESS_KINDS, JEWEL_KINDS };
    unsigned short style = ChosenStyle();
    int v[5] = { (style & STYLE_WOMAN) != 0, style & 3, style >> 2 & 3, style >> 4 & 3, style >> 6 & 3 };
    float h = 450, w = 400;
    Rectangle r = { 20, (UiHeight() - h) / 2, w, h };
    Tablet(r);
    CenterText("Your look", r.x + w / 2, r.y + 16, 28, C_TITLE);
    CenterText("Everyone sees you like this, in your own color.", r.x + w / 2, r.y + 52, 16, C_DIM);

    bool changed = false;
    for (int k = 0; k < 5; k++) {
        float y = r.y + 82 + 58.0f * (float)k;
        // A woman has no beard: that row stays empty for her.
        if (k == 2 && v[0]) {
            Text(TextFormat("%d  %s", k + 1, L(rows[k])), r.x + 20, y, 16, C_DIM);
            DrawRectangle((int)r.x + 66, (int)y + 20, (int)w - 152, 30, C_SLOT);
            CenterText("None", r.x + w / 2, y + 26, 17, C_DIM);
            continue;
        }
        const char *name = k == 0 ? (v[0] ? "Woman" : "Man") : k == 1 ? HairName(v[k]) : k == 2 ? BeardName(v[k]) : k == 3 ? DressName(v[k]) : JewelName(v[k]);
        Text(TextFormat("%d  %s", k + 1, L(rows[k])), r.x + 20, y, 16, C_DIM);
        if (Button((Rectangle){ r.x + 20, y + 20, 40, 30 }, "<", true, 0)) {
            v[k] = (v[k] + counts[k] - 1) % counts[k];
            changed = true;
        }
        DrawRectangle((int)r.x + 66, (int)y + 20, (int)w - 152, 30, C_SLOT);
        CenterText(name, r.x + w / 2, y + 26, 17, C_TEXT);
        if (Button((Rectangle){ r.x + w - 80, y + 20, 40 + 20, 30 }, ">", true, KEY_ONE + k)) {
            v[k] = (v[k] + 1) % counts[k];
            changed = true;
        }
    }
    unsigned short chosen = PackStyle(v[1], v[2], v[3], v[4]) | (v[0] ? STYLE_WOMAN : 0);
    if (changed) ChooseStyle(chosen);
    CenterText("1-5: next choice    Left/Right arrows: turn the view", r.x + w / 2, r.y + h - 78, 16, C_DIM);
    if (Button((Rectangle){ r.x + 20, r.y + h - 52, w - 40, 38 }, "Done (Enter)", true, KEY_ENTER)) {
        ChooseStyle(chosen);
        SetPanel(PANEL_NONE);
    }
}

static void DrawCoopPanel(void)
{
    static const char *const ask[3] = { "Slaughter", "Sure? (1/2)", "Really? (2/2)" };
    Rectangle r = PanelRect(820, 480);
    float x0 = r.x + 24, y = r.y + 18, now = NowDays();
    bool canSeeds = BagCount(IT_SEEDS) > 0 && G.coopFeed <= FEED_MAX - 1.0f;
    bool canWheat = BagCount(IT_WHEAT) > 0 && G.coopFeed <= FEED_MAX - 2.0f;
    bool canWater = BagCount(IT_WATER) > 0 && G.coopWater < WATER_DAYS - 0.05f;

    Text(TextFormat(L("Hen coop  %d/%d hens"), G.henCount, MAX_HENS), x0, y, 26, C_TITLE);
    y += 38;
    Text(TextFormat(L("Water %.1f of %d days"), G.coopWater, (int)WATER_DAYS), x0, y, 18, G.coopWater <= 2.0f ? C_BAD : C_TEXT);
    Text(TextFormat(L("Feed %d/%d"), (int)G.coopFeed, (int)FEED_MAX), x0 + 250, y, 18, G.coopFeed < (float)G.henCount ? C_BAD : C_TEXT);
    Text(TextFormat(L("Eggs waiting %d"), G.coopEggs), x0 + 420, y, 18, C_TEXT);
    if (G.coopDry > 0) Text(TextFormat(L("dry for %.1f days"), G.coopDry), x0 + 600, y, 18, C_BAD);
    y += 26;
    Text("Each hen eats 1 feed every 3 days and lays an egg a day while fed and watered. Seeds give 1 feed, wheat 2.", x0, y, 16, C_DIM);
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
        DrawRectangle((int)x0, (int)y, (int)r.width - 48, 40, (i & 1) ? pal->rowAlt : pal->row);
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
    const char *foot = G.slaughterStep > 0 ? "Click the hen's button again to confirm, Esc to cancel" : "Click or 1-4    E or Esc: close";
    Text(foot, x0, r.y + r.height - 36, 17, G.slaughterStep > 0 ? C_GOLD : C_DIM);
}

static void DrawHelpPanel(void)
{
    static const char *const keys[][2] = {
        { "WASD", "move" }, { "Shift", "run" }, { "Space", "jump" }, { "C", "crouch (stealth)" },
        { "Mouse", "look" }, { "Wheel", "zoom; all the way in is first person" }, { "P", "first person on/off" },
        { "1 / 2", "bow / melee weapon" }, { "3 / 4", "war mace / hands free" },
        { "Left mouse", "draw and release (bow), strike (melee)" },
        { "Right mouse, Q", "parry with the melee weapon" }, { "V", "strike" },
        { "E", "use, talk, pick up, climb, search" }, { "F", "alternate action" },
        { "R", "refill the quiver" }, { "B", "torch on/off (from evening)" }, { "Z", "arrow camera on/off" }, { "Tab", "baskets and equipment" },
        { "M", "map zoom" }, { "N", "music: off, low, medium, high" }, { "G", "graphics: auto, high, low, classic" }, { "T (hold)", "fast-forward time (alone)" }, { "T", "chat (in a shared game)" }, { "F5", "save" },
        { "Enter", "ready (battle royale, party)" }, { "F11", "fullscreen" }, { "Esc", "pause, save slots, game settings, your look" }, { "H", "this window" },
    };
    static const char *const tips[] = {
        "Parry: a yellow mark means a blow is coming, over a person or a player swinging at you (a player gives you less "
        "time). Parry while facing them: green = turned aside (hit back harder), red = it landed. A red glow on the edge "
        "of the screen shows where a blow on you came from: top ahead, bottom behind, left and right.",
        "Seasons last 9 days. Wheat needs 10 days with water, so sow by summer's 8th day. Nothing grows in winter. "
        "Harvest: 4 wheat, 2 seeds. The farm and the coop each hold 6 days of water. Fill the bucket from your storeroom "
        "at the well or the river (E), then pour it into the farm ditch or at the coop. Rain fills both; 6 dry days kill "
        "growing wheat. The shepherd takes withered plant waste, the cattle herder spare seeds.",
        "Hens live 12 seasons, eat every 3 days, and die after 6 days without food or water. E at the coop shows each hen.",
        "Home: quern, trough, oven, storage, bed. The workbench sews leather armor or a wolf-pelt cloak. The smith makes "
        "bronze armor (5 bronze, 3 sheepskin, 3 cowhide) and sells a war mace (key 3) that strikes everything in front of "
        "you. Tin comes with the caravans. Picking dates is free; all other gathering costs food, mining the most, then "
        "woodcutting. Health returns while food is at 20% or more. Brought down, you lose what you gathered that day unless "
        "it is in your storage.",
        "Ziggurat: market in the base, the granary and treasury in its wings; the guards' room, the tablet house and the "
        "sanctuary on the middle terrace. The priest at the top blesses you for a day: +20% running and attack speed. "
        "Guards fight raiders who follow you. Raiders camp south and north of the city, bandits roam the eastern hills; "
        "barbarians twice as strong camp across the river. Wade the canals and the river (the canals slow you by 60%, the river's current by 85%). The far north ford is wolf country.",
        "Roads on the map lead east to the hunters' village and south-west to the herders' village; the fishers live across "
        "the river from the city and sell a fine net that doubles your catch. Each village has a guard with a bow. Travellers walk the "
        "roads, and wolves leave you alone in company. Only the shepherd sells sheepskin and sheep.",
        "Herbs grow in green meadows, most of all across the river: chew one from your baskets to heal fast. Snakes hide "
        "in long grass; a hiss means back off (crouch to creep by). Hurt a boar and its whole sounder charges. Arrows "
        "landing close send prey running and bring wolves down on you.",
        "Playing together: pick one of the game's servers on the title screen, up to six players each, every one with a house "
        "in their own color. The world never pauses, and the night passes when everyone lies in bed. The farm, the coop and the "
        "eggs are shared. On a PvP server players can hurt each other with the best gear, arrows never run out and the guards stay out of it "
        "(everyone starts somewhere in the city, the ziggurat is unguarded).",
        "The towers by the city's gates have stairs inside: the middle floor opens onto the wall walk, and archers keep watch "
        "on the roofs. Many houses have stairs or crates up to their roofs, for running across the city.",
        "Experience: 1 for a beast or a townsman brought down, 3 for a guard, raider, barbarian or bandit, 10 for another player, "
        "10 for every player in a battle royale round you win, and 10, 5 or 2 for every player in a party you finish first, "
        "second or third. Level 1 takes 100 to reach level 2, level 2 another 200, up to level 100. Your level, K/D and "
        "things are kept with your account.",
        "Battle royale and Sumer Party begin once everyone in (two or more) is ready: Enter, the Ready button at the top, or Ready in the Esc menu. "
        "A party is four of five minigames, four minutes each (vote one out in the Esc menu): floor is lava, date stashing, "
        "one-shot archers, city defence, hide and seek. Each game's top three gain +10, +6, +3; the party's top three gain experience.",
        "The alehouse has game tables (not in battle royale or a party): sit with E and play the Royal Game of Ur or fifty-eight "
        "holes against another player or a townsman (Enter). Space throws, click or 1-7 moves. The winner gains 20 experience "
        "against a player, 10 against a townsman; Rules (R) at the board explains both games. Nothing from outside can hurt "
        "you inside the alehouse.",
        "Cats live in the city's streets and round the fishers' village, dogs in the villages and by the temple gate. Give a "
        "cat a fish or a dog roast meat (E) and it follows you about for a while.",
        "In a browser the mouse stays with the game while a window is open over it (bag, barter, storeroom, coop, board): "
        "E or Tab closes it and play goes straight on. Esc gives the mouse back; a click takes it again.",
        "A name and health bar show over a person or a beast while you aim at them, while they are hurt, and while they "
        "are after you. Game settings can show the frame rate under the map.",
    };
    const int nk = (int)(sizeof(keys) / sizeof(keys[0])), nt = (int)(sizeof(tips) / sizeof(tips[0]));
    Rectangle r = PanelRect(fminf(1200.0f, UiWidth() - 20.0f), fminf(690.0f, UiHeight() - 16.0f));
    float x0 = r.x + 24, x1 = r.x + 500, y0 = r.y + 60;

    Text("Controls", x0, r.y + 20, 26, C_TITLE);
    Text("Tips", x1, r.y + 20, 26, C_TITLE);
    // The keys close up on a short screen (a phone's).
    float row = fminf(23.0f, (r.y + r.height - 40.0f - y0) / (float)nk);
    int keySize = row >= 21.0f ? 17 : row >= 18.0f ? 16 : 14;
    for (int i = 0; i < nk; i++) {
        Text(keys[i][0], x0, y0 + row * (float)i, keySize, C_GOLD);
        Text(keys[i][1], x0 + 126, y0 + row * (float)i, keySize, C_TEXT);
    }
    // The tips take the biggest letters that fit the window (Turkish runs longer than English).
    static const struct { int size; float line; } fits[] = { { 17, 20.5f }, { 16, 19.0f }, { 15, 17.5f }, { 14, 16.5f } };
    float room = r.y + r.height - 44.0f - y0, width = r.x + r.width - 30 - x1;   // the foot keeps a line for "read on"
    int fit = 0;
    for (; fit < 3; fit++) {
        float used = 0;
        paragraphDry = true;
        for (int i = 0; i < nt; i++) used += Paragraph(tips[i], x1, y0, width, fits[fit].size, fits[fit].line) + 5;
        paragraphDry = false;
        if (used <= room) break;
    }
    // When even the smallest letters do not fit (a small window), the tips scroll: the wheel, the arrow keys
    // or a finger dragged over them; a bar at the side shows where.
    static float scroll, dragY = -1;
    float total = 0;
    paragraphDry = true;
    for (int i = 0; i < nt; i++) total += Paragraph(tips[i], x1, y0, width, fits[fit].size, fits[fit].line) + 5;
    paragraphDry = false;
    float most = fmaxf(0.0f, total - room);
    Rectangle area = { x1, y0, width, room };
    if (most > 0) {
        scroll -= GetMouseWheelMove() * 60.0f;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_PAGE_DOWN)) scroll += 400.0f * GetFrameTime();
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_PAGE_UP)) scroll -= 400.0f * GetFrameTime();
        bool finger = GetTouchPointCount() > 0 && CheckCollisionPointRec(UiMouse(), area);
        if (finger && dragY >= 0) scroll -= UiMouse().y - dragY;
        dragY = finger ? UiMouse().y : -1.0f;
    } else {
        scroll = 0;
    }
    scroll = Clamp(scroll, 0.0f, most);
    float k = UiScale();
    BeginScissorMode((int)(area.x * k), (int)(area.y * k), (int)ceilf(area.width * k), (int)ceilf(area.height * k));
    float ty = y0 - scroll;
    for (int i = 0; i < nt; i++) ty += Paragraph(tips[i], x1, ty, width, fits[fit].size, fits[fit].line) + 5;
    EndScissorMode();
    if (most > 0) {
        float bar = room * room / total;
        DrawRectangle((int)(x1 + width + 8), (int)(y0 + (room - bar) * scroll / most), 4, (int)bar, Fade(C_GOLD, 0.6f));
        Text(TouchOn() ? "Drag the tips to read on" : "Wheel or arrow keys: read on", x1, r.y + r.height - 36, 16, C_DIM);
    }
    Text("H or Esc: close", r.x + r.width - 30 - (float)Measure(L("H or Esc: close"), 17), r.y + 26, 17, C_DIM);
}

// How far a point in sight is from the middle of the screen, where the crosshair is (UI units, scaled so a
// person's height counts less than their width); -1 when it is behind, too far or well off the middle.
static float AimGap(Vector3 p, float reach)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));
    if (G.panel != PANEL_NONE || Vector3DotProduct(Vector3Subtract(p, G.cam.position), fwd) < 0 || Vector3Distance(p, G.cam.position) > reach) return -1.0f;
    Vector2 sp = OnScreen(p);
    float dx = fabsf(sp.x - UiWidth() / 2) / 46.0f, dy = fabsf(sp.y - UiHeight() / 2) / 70.0f;
    return dx < 1.0f && dy < 1.0f ? dx * dx + dy * dy : -1.0f;
}

// Of those near the crosshair, the one nearest it is what you aim at (several would stack their names).
static int AimedFighter(void)
{
    int best = -1;
    float gap = 1e9f;
    for (int i = 0; i < FighterCount(); i++) {
        Vector3 pos;
        float hp, maxHp;
        const char *name;
        bool hostile, dead;
        if (!FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead) || dead) continue;
        float g = AimGap((Vector3){ pos.x, pos.y + 1.4f, pos.z }, 40.0f);
        if (g >= 0.0f && g < gap) {
            gap = g;
            best = i;
        }
    }
    return best;
}

static int AimedAnimal(void)
{
    int best = -1;
    float gap = 1e9f;
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (!a->active || a->state == ST_DEAD || AnimalHidden(a)) continue;
        float g = AimGap((Vector3){ a->pos.x, AnimalTop(a) - 0.2f, a->pos.z }, 45.0f);
        if (g >= 0.0f && g < gap) {
            gap = g;
            best = i;
        }
    }
    return best;
}

// A beast's health over it while it is hurt, hunting you or aimed at; the calm and whole ones go bare.
static void DrawAnimalBars(void)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));
    int aim = AimedAnimal();

    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (!a->active || a->state == ST_DEAD || AnimalHidden(a)) continue;
        float dist = XZDist(a->pos, G.player.pos);
        int max = AnimalMaxHp(a->type);
        Vector3 head = { a->pos.x, AnimalTop(a) - 0.2f, a->pos.z };
        bool hurt = a->hp < max, hunting = a->state == ST_HUNT, aimed = i == aim;
        if (!hurt && !hunting && !aimed) continue;
        if (dist > 60.0f) continue;
        if (Vector3DotProduct(Vector3Subtract(head, G.cam.position), fwd) < 0) continue;

        Vector2 sp = OnScreen(head);
        float k = Clamp((float)a->hp / (float)max, 0.0f, 1.0f);
        float w = Clamp(56.0f - dist * 0.5f, 30.0f, 56.0f);
        Color fill = k > 0.6f ? C_GOOD : k > 0.3f ? C_GOLD : C_BAD;
        DrawRectangle((int)(sp.x - w / 2) - 1, (int)sp.y - 1, (int)w + 2, 7, Fade(BLACK, 0.7f));
        DrawRectangle((int)(sp.x - w / 2), (int)sp.y, (int)(w * k), 5, fill);
        if (aimed || (dist < 20.0f && hunting)) CenterText(AnimalName(a->type), sp.x, sp.y - 18, 15, Fade(RAYWHITE, 0.9f));
    }
}

// The warning over someone who is about to strike: yellow while the blow is coming, then green when it was
// turned aside or red when it landed.
static void DrawMarkDiamond(int mark, Vector2 c)
{
    static const Color colors[4] = { { 0 }, { 245, 205, 60, 255 }, { 90, 215, 90, 255 }, { 225, 60, 50, 255 } };
    float size = mark == MARK_WARN ? 9.0f + 2.0f * sinf((float)GetTime() * 18.0f) : 10.0f;

    DrawPoly(c, 4, size + 2, 0, Fade(BLACK, 0.7f));
    DrawPoly(c, 4, size, 0, colors[mark & 3]);
}

static void DrawPeopleBars(void)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));
    int aim = AimedFighter();

    for (int i = 0; i < FighterCount(); i++) {
        Vector3 pos;
        float hp, maxHp;
        const char *name;
        bool hostile, dead;
        if (!FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead) || dead) continue;
        // Hide and seek: a hider is drawn as one of the townsfolk with no name over them, so no one
        // peaceful gets a name or a bar, aimed at, hurt or not.
        if (PartyGame() == PG_HIDE && !hostile) continue;
        float dist = Vector3Distance(pos, G.player.pos);
        Vector3 head = { pos.x, pos.y + 2.25f, pos.z };
        bool hurt = hp < maxHp, aimed = i == aim;
        if (!hurt && !hostile && !aimed) continue;
        if (dist > 50.0f || (!hostile && fabsf(pos.y - G.player.pos.y) > 3.5f)) continue;
        if (Vector3DotProduct(Vector3Subtract(head, G.cam.position), fwd) < 0) continue;
        if (FogAt(head) > 0.6f) continue;

        Vector2 sp = OnScreen(head);
        float k = Clamp(hp / maxHp, 0.0f, 1.0f);
        float w = Clamp(60.0f - dist * 0.5f, 32.0f, 60.0f);
        Color fill = hostile ? C_BAD : k > 0.6f ? C_GOOD : C_GOLD;
        DrawRectangle((int)(sp.x - w / 2) - 1, (int)sp.y - 1, (int)w + 2, 7, Fade(BLACK, 0.7f));
        DrawRectangle((int)(sp.x - w / 2), (int)sp.y, (int)(w * k), 5, fill);
        if (aimed || hostile || dist < 16.0f) CenterText(name, sp.x, sp.y - 18, 15, hostile ? C_BAD : Fade(RAYWHITE, 0.9f));

        int mark = FighterMark(i);
        if (mark != MARK_NONE) DrawMarkDiamond(mark, (Vector2){ sp.x, sp.y - 32 });
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

// A heart, for health.
static void Heart(Vector2 c, float r, Color col)
{
    DrawCircleV((Vector2){ c.x - r * 0.48f, c.y - r * 0.2f }, r * 0.55f, col);
    DrawCircleV((Vector2){ c.x + r * 0.48f, c.y - r * 0.2f }, r * 0.55f, col);
    Tri((Vector2){ c.x - r * 1.02f, c.y - r * 0.05f }, (Vector2){ c.x + r * 1.02f, c.y - r * 0.05f }, (Vector2){ c.x, c.y + r }, col);
}

// A bar of 'k' (0-1) with a dark groove and a little shine on its top.
static void Bar(float x, float y, float w, float h, float k, Color fill)
{
    k = Clamp(k, 0.0f, 1.0f);
    DrawRectangleRounded((Rectangle){ x, y, w, h }, 0.5f, 6, Fade(BLACK, 0.55f));
    if (k > 0.0f) {
        DrawRectangleRounded((Rectangle){ x + 1, y + 1, fmaxf(h - 2, (w - 2) * k), h - 2 }, 0.5f, 6, fill);
        DrawRectangleRounded((Rectangle){ x + 3, y + 2, fmaxf(0.0f, (w - 2) * k - 4), (h - 4) * 0.4f }, 0.5f, 4, Fade(WHITE, 0.25f));
    }
}

#define STATUS_W 300.0f   // the status box's width (the news banner keeps clear of it)

static void DrawStatus(void)
{
    int hh = (int)G.hour, mm = (int)((G.hour - (float)hh) * 60.0f);

    const Peer *me = &G.peers[G.self];
    bool wolves = AnyWolfHunting(), beast = AnyBeastAttacking();
    bool alert = me->alone || wolves || beast || me->wanted > 0, kd = Online() && G.pvp;
    float h = 186.0f + (alert ? 20.0f : 0.0f) + (kd ? 20.0f : 0.0f), right = 12.0f + STATUS_W - 12.0f;

    DrawRectangleRounded((Rectangle){ 12, 12, STATUS_W, h }, 0.06f, 6, Fade(BLACK, 0.42f));
    DrawRectangleRoundedLinesEx((Rectangle){ 12, 12, STATUS_W, h }, 0.06f, 6, 1, Fade(C_GOLD, 0.25f));
    Text(TextFormat(L("Day %d  %02d:%02d  %s"), G.day, hh, mm, L(PhaseName())), 24, 18, 20, RAYWHITE);
    Text(TextFormat(L("%s, day %d of %d"), SeasonName(Season()), SeasonDay(), SEASON_DAYS), 24, 42, 16, C_GOLD);
    const char *status = G.herbT > 0 ? TextFormat(L("Herbs %ds"), (int)ceilf(G.herbT))
                       : G.blessT > 0 ? TextFormat(L("Blessed %dh"), (int)ceilf(G.blessT)) : G.raining ? L("Raining") : "";
    Text(status, right - (float)Measure(status, 16), 42, 16, G.herbT > 0 || G.blessT > 0 ? C_GOOD : (Color){ 170, 200, 230, 255 });
    if (G.raining)
        for (int k = 0; k < 3; k++)
            DrawLineEx((Vector2){ right - 26.0f + 8.0f * (float)k, 20 }, (Vector2){ right - 30.0f + 8.0f * (float)k, 34 }, 2, (Color){ 170, 200, 230, 255 });
    // Health and food as bars by their signs, then the arrows.
    float bx = 50, bw = right - bx;
    Heart((Vector2){ 34, 76 }, 9, G.health < 35 ? (Color){ 240, 70, 60, 255 } : (Color){ 214, 60, 52, 255 });
    Bar(bx, 70, bw, 13, G.health / HEALTH_MAX, G.health < 35 ? C_BAD : C_GOOD);
    if (!DrawItemIcon(IT_BREAD, (Rectangle){ 21, 86, 26, 26 }, WHITE)) Text("Food", 24, 90, 14, C_TEXT);
    Bar(bx, 93, bw, 13, G.food / FOOD_MAX, G.food < 20 ? C_BAD : (Color){ 214, 162, 68, 255 });
    DrawItemIcon(IT_ARROWS, (Rectangle){ 20, 110, 28, 28 }, WHITE);
    if (UnlimitedArrows()) Text(Royale() ? "Arrows: unlimited (battle royale)" : Party() ? "Arrows: unlimited (party)" : "Arrows: unlimited (PvP)", bx, 116, 16, C_TEXT);
    else Text(TextFormat(L("Arrows %d/%d   +%d carried"), G.quiver, G.quiverMax, BagCount(IT_ARROWS)), bx, 116, 16, G.quiver == 0 ? C_BAD : C_TEXT);
    Text(TextFormat(L("In hand: %s%s"), EquipName(G.equip), G.player.crouching ? L("  |  crouching") : ""), 24, 140, 16,
         G.player.crouching ? C_GOOD : C_TEXT);
    Text(TextFormat(L("Armor: %s"), ArmorName(UseArmor())), 24, 160, 16, C_TEXT);
    // The level, and how far into it: experience is only shown, never spent.
    long xp = AccountXp();
    int level = LevelOf(xp);
    Text(LevelText(xp), 24, 180, 16, C_GOLD);
    if (level < MAX_LEVEL) {
        float into = (float)(xp - LevelStart(level)) / (float)(LevelStart(level + 1) - LevelStart(level));
        Bar(right - 100, 185, 100, 8, into, C_GOLD);
    }
    float y = 200;
    if (alert) {
        if (me->wanted > 0) Text(TextFormat(L("Wanted by the guards  %d:%02d"), (int)me->wanted / 60, (int)me->wanted % 60), 24, y, 16, C_BAD);
        else if (wolves) Text("Wolves are hunting you!", 24, y, 16, C_BAD);
        else if (beast) Text("A wild beast is attacking you!", 24, y, 16, C_BAD);
        else Text("Alone outside the walls", 24, y, 16, C_GOLD);
        y += 20;
    }
    if (kd) Text(TextFormat(L("PvP: %s"), KillDeath(G.self)), 24, y, 16, C_TEXT);
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
    float k = UiScale();
    BeginScissorMode((int)((float)ox * k), (int)((float)oy * k), (int)ceilf((float)size * k), (int)ceilf((float)size * k));
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
            if (PartyGame() == PG_HIDE && !hostile) continue;   // the hiders have no dots, so neither do the townsfolk
            DrawCircleV((Vector2){ mx + pos.x * s, mz + pos.z * s }, 2, hostile ? RED : (Color){ 240, 236, 220, 255 });
        }
        for (int i = 0; i < G.ladderCount; i++)
            DrawRectangle((int)(mx + G.ladders[i].base.x * s) - 2, (int)(mz + G.ladders[i].base.z * s) - 2, 4, 4, COL_WOOD);
    }
    // Raider camps as red crosses; the barbarians' across the river darker and larger.
    for (int c = 0; c < (RaidersAway() ? 0 : CAMPS); c++) {
        Vector3 cc = CampCenter(c);
        Vector2 camp = { mx + cc.x * s, mz + cc.z * s };
        float k = c >= EAST_CAMPS ? 5.0f : 4.0f;
        Color col = c >= EAST_CAMPS ? (Color){ 150, 20, 20, 255 } : C_BAD;
        DrawLineEx((Vector2){ camp.x - k, camp.y - k }, (Vector2){ camp.x + k, camp.y + k }, 2.5f, col);
        DrawLineEx((Vector2){ camp.x - k, camp.y + k }, (Vector2){ camp.x + k, camp.y - k }, 2.5f, col);
    }
    DrawRoyaleMap(mx, mz, s);
    // In a battle royale the map gives no one away, except to those already out.
    for (int k = 0; k < MAX_PLAYERS; k++) {
        if (k == G.self || !G.peers[k].active || (G.peers[k].flags & PF_OUT)) continue;
        if ((Royale() || PartyGame() == PG_HIDE) && !OutOfRound()) continue;
        Vector2 q = { mx + G.peers[k].shown.x * s, mz + G.peers[k].shown.z * s };
        DrawCircleV(q, 4.5f, Fade(BLACK, 0.8f));
        DrawCircleV(q, 3.5f, PlayerColor(k));
    }
    Vector2 pp = { mx + G.player.pos.x * s, mz + G.player.pos.z * s };
    DrawLineEx(pp, (Vector2){ pp.x + sinf(G.player.facing) * 8, pp.y + cosf(G.player.facing) * 8 }, 2, BLACK);
    DrawCircleV(pp, 3, RAYWHITE);
    EndScissorMode();
    // Framed in bitumen and shell, with a rosette at each corner.
    DrawRectangleLinesEx((Rectangle){ (float)ox - 4, (float)oy - 4, (float)size + 8, (float)size + 8 }, 4, BITUMEN);
    DrawRectangleLinesEx((Rectangle){ (float)ox - 1, (float)oy - 1, (float)size + 2, (float)size + 2 }, 1, Fade(SHELL, 0.7f));
    Rosette((Vector2){ (float)ox - 2, (float)oy - 2 }, 7);
    Rosette((Vector2){ (float)(ox + size) + 2, (float)oy - 2 }, 7);
    Rosette((Vector2){ (float)ox - 2, (float)(oy + size) + 2 }, 7);
    Rosette((Vector2){ (float)(ox + size) + 2, (float)(oy + size) + 2 }, 7);
    Text(TextFormat(L("M  x%.1f"), zoom), (float)ox + 8, (float)(oy + size) - 18, 12, Fade(BLACK, 0.7f));
}

// Waiting for a battle royale round or a party to begin, while the mouse is free (as one joins, before the
// first click into the game, and always on a phone): Ready, and in a party the vote, as buttons under the
// line at the top. A click on them does not take the mouse (NoGrab).
static void DrawStartButtons(float sw)
{
    static bool shown;
    if (!LobbyWaiting() || G.panel != PANEL_NONE || (OnWeb() && PointerCaptured())) {
        if (shown) NoGrab(NULL, 0);
        shown = false;
        return;
    }
    const float w = 190, h = 32, gap = 12, y = 42;
    int n = Party() ? 2 : 1;
    float x = sw / 2 - (w * (float)n + gap * (float)(n - 1)) / 2;
    Rectangle b[2] = { { x, y, w, h }, { x + w + gap, y, w, h } };
    NoGrab(b, n);
    shown = true;
    bool over = CheckCollisionPointRec(UiMouse(), b[0]) || (n > 1 && CheckCollisionPointRec(UiMouse(), b[1]));
    if (Button(b[0], MatchReady() ? "Ready: yes (Enter)" : "Ready: no (Enter)", true, 0)) ToggleReady();
    if (n > 1 && Button(b[1], "Vote a game out", true, 0)) SetPanel(PANEL_PARTY);
    if (over) G.ignoreClick = true;
}

// The other players' names over their heads, in their colors, with their health when hurt.
static void DrawPeerTags(void)
{
    if (PartyGame() == PG_HIDE) return;   // hide and seek: no one's name gives them away
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));

    for (int s = 0; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active || s == G.self || (p->flags & PF_OUT)) continue;
        Vector3 head = { p->shown.x, p->shown.y + ((p->flags & PF_BED) ? 1.0f : 2.4f), p->shown.z };
        float dist = Vector3Distance(head, G.cam.position);
        if (dist > 90.0f || Vector3DotProduct(Vector3Subtract(head, G.cam.position), fwd) < 0 || FogAt(head) > 0.7f) continue;
        Vector2 sp = OnScreen(head);
        int w = Measure(p->name, 16);
        DrawRectangle((int)sp.x - w / 2 - 8, (int)sp.y - 3, w + 16, 22, Fade(BLACK, 0.5f));
        DrawRectangle((int)sp.x - w / 2 - 8, (int)sp.y - 3, 4, 22, PlayerColor(s));
        DrawGameText(p->name, (float)((int)sp.x - w / 2), sp.y, 16, RAYWHITE);
        if (p->health < HEALTH_MAX - 1.0f) {
            float k = Clamp(p->health / HEALTH_MAX, 0.0f, 1.0f);
            DrawRectangle((int)sp.x - 24, (int)sp.y + 21, 48, 5, Fade(BLACK, 0.6f));
            DrawRectangle((int)sp.x - 24, (int)sp.y + 21, (int)(48 * k), 5, k > 0.35f ? C_GOOD : C_BAD);
        }
        // With PvP on: their kills and deaths, and how long the guards are still after them.
        if (G.pvp && dist < 60.0f) {
            const char *line = p->wanted > 0 ? TextFormat(L("%s   wanted %d:%02d"), KillDeath(s), (int)p->wanted / 60, (int)p->wanted % 60)
                                             : KillDeath(s);
            int lw = Measure(line, 14);
            DrawRectangle((int)sp.x - lw / 2 - 6, (int)sp.y - 22, lw + 12, 18, Fade(BLACK, 0.45f));
            DrawGameText(line, (float)((int)sp.x - lw / 2), sp.y - 20, 14, p->wanted > 0 ? C_BAD : RAYWHITE);
        }
        // Swinging at someone: yellow while the blow is coming, then green (turned aside) or red (it landed).
        if (G.pvp && p->mark != MARK_NONE && dist < 60.0f) DrawMarkDiamond(p->mark, (Vector2){ sp.x, sp.y - 46.0f });
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
        Vector2 sp = OnScreen(head);
        bool flee = a->state == ST_FLEE || hunt;
        CenterText(hunt ? "!!" : flee ? "!" : "?", sp.x, sp.y - 14, 28, flee ? C_BAD : C_GOLD);
    }
}

static void DrawCrosshair(void)
{
    if (!G.player.aiming) {
        if (G.firstPerson && G.panel == PANEL_NONE) DrawCircle((int)UiWidth() / 2, (int)UiHeight() / 2, 2.5f, Fade(RAYWHITE, 0.8f));
        return;
    }
    float cx = UiWidth() / 2, cy = UiHeight() / 2;
    float gap = 10.0f - 6.0f * G.player.draw;

    DrawLineEx((Vector2){ cx - gap - 8, cy }, (Vector2){ cx - gap, cy }, 2, RAYWHITE);
    DrawLineEx((Vector2){ cx + gap, cy }, (Vector2){ cx + gap + 8, cy }, 2, RAYWHITE);
    DrawLineEx((Vector2){ cx, cy - gap - 8 }, (Vector2){ cx, cy - gap }, 2, RAYWHITE);
    DrawLineEx((Vector2){ cx, cy + gap }, (Vector2){ cx, cy + gap + 8 }, 2, RAYWHITE);
    DrawRectangle((int)cx - 30, (int)cy + 26, 60, 5, Fade(BLACK, 0.5f));
    DrawRectangle((int)cx - 30, (int)cy + 26, (int)(60 * G.player.draw), 5, G.player.draw >= 1 ? C_GOOD : C_GOLD);
}

// A message too wide for the window breaks at the space nearest its middle.
// Red lights on the edges of the screen show where a blow came from: the top is ahead of you, the bottom
// behind you, then left and right.
static void DrawHitLights(int sw, int sh)
{
    const Color red = { 240, 28, 24, 255 };
    int bandY = sh / 5, bandX = sw / 7;

    if (G.hitLight[0] > 0.02f) DrawRectangleGradientV(sw / 8, 0, sw * 3 / 4, bandY, Fade(red, G.hitLight[0] * 0.9f), Fade(red, 0.0f));
    if (G.hitLight[1] > 0.02f) DrawRectangleGradientV(sw / 8, sh - bandY, sw * 3 / 4, bandY, Fade(red, 0.0f), Fade(red, G.hitLight[1] * 0.9f));
    if (G.hitLight[2] > 0.02f) DrawRectangleGradientH(0, sh / 6, bandX, sh * 2 / 3, Fade(red, G.hitLight[2] * 0.9f), Fade(red, 0.0f));
    if (G.hitLight[3] > 0.02f) DrawRectangleGradientH(sw - bandX, sh / 6, bandX, sh * 2 / 3, Fade(red, 0.0f), Fade(red, G.hitLight[3] * 0.9f));
}

// The news: a band across the top between the status box and the map, fading out. Under the party's or
// the battle royale's lines when they are there; lower down, as wide as the screen, when too long for
// three lines between the two.
static int NewsLines(const char *msg, int size, float width, char lines[3][sizeof(G.msg)], int *wide)
{
    int len = (int)strlen(msg), w = Measure(msg, size), want = (int)ceilf((float)w / fmaxf(width, 1.0f)), count = 0, from = 0;
    if (want < 1) want = 1;
    if (want > 3) return 0;
    *wide = 0;
    for (int k = 1; k <= want && from <= len; k++) {
        int cut = len;
        if (k < want) {   // at the space nearest an equal share
            int goal = len * k / want, best = -1;
            for (int i = from + 1; i < len; i++)
                if (msg[i] == ' ' && (best < 0 || abs(i - goal) < abs(best - goal))) best = i;
            if (best > from) cut = best;
        }
        snprintf(lines[count], sizeof(lines[count]), "%.*s", cut - from, msg + from);
        int lw = Measure(lines[count], size);
        if (lw > *wide) *wide = lw;
        count++;
        from = cut + 1;
        if (cut >= len) break;
    }
    return *wide <= (int)width + 8 ? count : 0;
}

static void DrawMessage(int sw)
{
    const int size = 18;
    float a = fminf(G.msgT, 1.0f), half = fminf((float)sw / 2 - (12 + STATUS_W + 14), (float)sw / 2 - 220);   // clear of the map too
    char lines[3][sizeof(G.msg)];
    int w = 0, count = NewsLines(G.msg, size, 2 * half - 36, lines, &w);
    float y = Party() || Royale() ? 84.0f : 16.0f;
    if (!count) {
        count = NewsLines(G.msg, size, (float)sw - 80, lines, &w);
        y = 250.0f;
    }
    if (!count) {
        snprintf(lines[0], sizeof(lines[0]), "%s", G.msg);
        w = Measure(G.msg, size);
        count = 1;
    }
    float bw = (float)w + 44, bh = 12.0f + 22.0f * (float)count, x = ((float)sw - bw) / 2;
    DrawRectangleRounded((Rectangle){ x, y, bw, bh }, 0.25f, 6, Fade(BLACK, 0.58f * a));
    DrawLineEx((Vector2){ x + 12, y + 2 }, (Vector2){ x + bw - 12, y + 2 }, 1.5f, Fade(B_GOLD, 0.7f * a));
    DrawLineEx((Vector2){ x + 12, y + bh - 2 }, (Vector2){ x + bw - 12, y + bh - 2 }, 1.5f, Fade(B_GOLD, 0.7f * a));
    for (int i = 0; i < count; i++) CenterText(lines[i], (float)sw / 2, y + 7.0f + 22.0f * (float)i, size, Fade(RAYWHITE, a));
}

// Chat in a shared game: the last lines said, at the bottom left for a while after they come, and all of
// them while a line is being typed (T opens it, Enter sends it, Esc puts it away).
#define CHAT_LINES 8
#define CHAT_SHOW  15.0
static struct { char name[NAME_LEN]; Color color; char text[CHAT_MAX + 1]; double at; } chat[CHAT_LINES];
static int chatCount;
static char chatLine[CHAT_MAX + 1];

void ChatAdd(int slot, const char *text)
{
    memmove(&chat[1], &chat[0], sizeof(chat[0]) * (CHAT_LINES - 1));
    snprintf(chat[0].name, sizeof(chat[0].name), "%s", G.peers[slot].name);
    chat[0].color = PlayerColor(slot);
    snprintf(chat[0].text, sizeof(chat[0].text), "%s", text);
    chat[0].at = GetTime();
    if (chatCount < CHAT_LINES) chatCount++;
}

void ChatClear(void)
{
    chatCount = 0;
    chatLine[0] = '\0';
}

#if defined(SUMER_TEST)
const char *ChatLast(void)
{
    return chatCount ? TextFormat("%s: %s", chat[0].name, chat[0].text) : "";
}
#endif

// How much of s goes on a row w wide: up to the last space that fits, or the last letter if none does.
static int ChatFits(const char *s, float w)
{
    int at = 0, space = 0;
    while (s[at]) {
        int len, cp = GetCodepointNext(s + at, &len);
        if (GameTextWidth(TextFormat("%.*s", at + len, s), 16) > w && at > 0) return space > 0 ? space : at;
        if (cp == ' ') space = at;
        at += len;
    }
    return at;
}

static void DrawChat(int sh)
{
    bool typing = G.panel == PANEL_CHAT;
    float size = FontSize(16), w = fminf(460.0f, UiWidth() * 0.45f), y = (float)sh - 54;
    if (typing) {
        int c, n = (int)strlen(chatLine), len;
        while ((c = GetCharPressed()) > 0) {
            const char *u = CodepointToUTF8(c, &len);
            if (c >= 32 && c != 127 && n + len <= CHAT_MAX) {
                memcpy(chatLine + n, u, (size_t)len);
                chatLine[n += len] = '\0';
            }
        }
        if ((KeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && n > 0) {
            while (n > 1 && ((unsigned char)chatLine[n - 1] & 0xC0) == 0x80) n--;
            chatLine[--n] = '\0';
        }
        if (KeyPressed(KEY_ENTER) || KeyPressed(KEY_KP_ENTER)) {
            if (chatLine[0]) NetChat(chatLine);
            chatLine[0] = '\0';
            SetPanel(PANEL_NONE);
        }
        // The end of a line too long for the box.
        float lw = (float)Measure(L("Say:"), 16) + 8;
        const char *tail = chatLine;
        while (*tail && GameTextWidth(tail, size) > w - lw - 24) {
            GetCodepointNext(tail, &len);
            tail += len;
        }
        DrawRectangleRec((Rectangle){ 8, y, w + 8, 26 }, Fade(BLACK, 0.6f));
        DrawRectangleLinesEx((Rectangle){ 8, y, w + 8, 26 }, 1, C_EDGE);
        Text("Say:", 14, y + 5, 16, C_GOLD);
        DrawGameText(TextFormat("%s%s", tail, fmodf((float)GetTime(), 1.0f) < 0.5f ? "_" : ""), 14 + lw, y + 5, size, RAYWHITE);
        y -= 4;
    }
    // Newest at the bottom, each on as many rows as it needs, the name in the player's colour.
    for (int i = 0; i < chatCount && y > 260; i++) {
        double age = GetTime() - chat[i].at;
        if (!typing && age > CHAT_SHOW) break;
        float a = typing ? 1.0f : Clamp((float)(CHAT_SHOW - age), 0.0f, 1.0f);
        char head[NAME_LEN + 2];
        const char *row[4];
        snprintf(head, sizeof(head), "%s:", chat[i].name);
        float hw = 8 + GameTextWidth(head, size);
        int len[4], rows = 0;
        for (const char *s = chat[i].text; *s && rows < 4; rows++) {
            len[rows] = ChatFits(s, rows ? w : w - hw);
            row[rows] = s;
            s += len[rows];
            while (*s == ' ') s++;
        }
        y -= 20.0f * (float)rows + 2;
        DrawRectangle(8, (int)y - 2, (int)w + 8, 20 * rows + 2, Fade(BLACK, 0.4f * a));
        DrawGameText(head, 14, y, size, Fade(ColorBrightness(chat[i].color, 0.4f), a));
        for (int k = 0; k < rows; k++)
            DrawGameText(TextFormat("%.*s", len[k], row[k]), k ? 14 : 14 + hw, y + 20.0f * (float)k, size, Fade(RAYWHITE, a));
    }
}

static void DrawPrompts(void)
{
    float cx = UiWidth() / 2, y = UiHeight() - 128;
    const char *lines[2] = { G.promptE, G.promptF };
    bool ok[2] = { G.okE, G.okF };

    for (int i = 0; i < 2; i++) {
        if (!lines[i][0]) continue;
        // "[E] Sit down": the key drawn as a key, the rest beside it.
        const char *text = lines[i];
        char key[2] = { 0 };
        if (text[0] == '[' && text[1] && text[2] == ']') {
            key[0] = text[1];
            text += text[3] == ' ' ? 4 : 3;
        }
        float kw = key[0] ? 30.0f : 0.0f, w = (float)Measure(text, 20) + kw, x = cx - w / 2;
        DrawRectangleRounded((Rectangle){ x - 12, y - 5, w + 24, 32 }, 0.3f, 6, Fade(BLACK, 0.55f));
        if (key[0]) {
            Rectangle k = { x - 4, y - 1, 24, 24 };
            DrawRectangleRounded((Rectangle){ k.x, k.y + 2, k.width, k.height }, 0.25f, 4, Fade(BLACK, 0.6f));
            DrawRectangleRounded(k, 0.25f, 4, ok[i] ? SHELL : Fade(SHELL, 0.5f));
            DrawGameText(key, k.x + 12 - GameTextWidth(key, 16) / 2, k.y + 4, 16, BITUMEN);
        }
        Text(text, x + kw, y, 20, ok[i] ? RAYWHITE : C_DIM);
        y += 36;
    }
}

// The cursor of a panel opened while the page keeps the mouse (SetPanel): an arrow where the mouse's
// movements have taken it.
static void DrawGameCursor(void)
{
    Vector2 m = UiMouse();
    Vector2 a = m, b = { m.x, m.y + 20 }, c = { m.x + 14, m.y + 14 };
    DrawTriangle((Vector2){ a.x + 1, a.y + 2 }, (Vector2){ b.x + 1, b.y + 2 }, (Vector2){ c.x + 1, c.y + 2 }, Fade(BLACK, 0.4f));
    DrawTriangle(a, b, c, RAYWHITE);
    DrawTriangleLines(a, b, c, (Color){ 40, 30, 20, 255 });
}

void DrawUI(void)
{
    IconsPrepare();
    UiBegin();
    int sw = (int)ceilf(UiWidth()), sh = (int)ceilf(UiHeight());

    DrawStatus();
    DrawMinimap(sw - 202, 12, 190);
    if (G.showFps) Text(TextFormat("%d FPS", GetFPS()), (float)sw - 200, 208, 14, Fade(RAYWHITE, 0.8f));
    DrawAnimalBars();
    DrawPeopleBars();
    DrawPeerTags();
    DrawAlerts();
    if (G.impactT > 0) {
        Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));
        if (Vector3DotProduct(Vector3Subtract(G.impact, G.cam.position), fwd) > 0) {
            Vector2 sp = OnScreen((Vector3){ G.impact.x, G.impact.y + 0.4f, G.impact.z });
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
    if (NetSilence() > 3.0f) CenterText("The server is not answering. Check your connection.", (float)sw / 2, 210, 18, C_BAD);
    // Experience just gained, rising and fading over the crosshair.
    if (G.xpGainT > 0) {
        float k = G.xpGainT / 2.0f;
        CenterText(TextFormat("+%d XP", G.xpGain), (float)sw / 2, (float)sh / 2 - 60 - 30 * (1.0f - k), 22, Fade(C_GOLD, fminf(1.0f, k * 2.0f)));
    }
    if (OnWeb() && G.panel == PANEL_NONE && !PointerCaptured() && !TouchOn()) {
        const char *click = L("Click the game to look around with the mouse");
        float cw = (float)Measure(click, 20);
        DrawRectangleRounded((Rectangle){ (float)sw / 2 - cw / 2 - 14, (float)sh / 2 + 64, cw + 28, 32 }, 0.3f, 6, Fade(BLACK, 0.5f));
        CenterText(click, (float)sw / 2, (float)sh / 2 + 70, 20, C_GOLD);
    }

    DrawRoyaleHUD((float)sw, (float)sh);
    DrawPartyHUD((float)sw, (float)sh);
    DrawStartButtons((float)sw);
    if (G.msgT > 0) DrawMessage(sw);

    DrawChat(sh);
    if (!TouchOn()) Text(Online() ? "H  controls    T  chat" : "H  controls", 12, (float)sh - 26, 16, Fade(RAYWHITE, 0.75f));

    pal = &CLAY;
    switch (G.panel) {
    case PANEL_BAG:     DrawBagPanel(); break;
    case PANEL_STORAGE: DrawStoragePanel(); break;
    case PANEL_TRADE:   DrawTradePanel(); break;
    case PANEL_PAUSE:   DrawPausePanel(); break;
    case PANEL_PARTY:   DrawPartyPanel(); break;
    case PANEL_BOARD:   DrawBoardPanel(); break;
    case PANEL_HELP:    DrawHelpPanel(); break;
    case PANEL_COOP:    DrawCoopPanel(); break;
    case PANEL_LOOK:    DrawLookPanel(); break;
    case PANEL_SETTINGS: DrawSettingsPanel(); break;
    default: break;
    }
    pal = &HUD;

    DrawTouch();
    if (G.panel != PANEL_NONE && OnWeb() && PointerCaptured()) DrawGameCursor();
    if (G.hurtT > 0) DrawRectangle(0, 0, sw, sh, Fade(RED, G.hurtT * 0.6f));
    DrawHitLights(sw, sh);
    if (G.sleeping) {
        float fade = Clamp(1.0f - fabsf(G.sleepT - 1.1f) / 1.1f, 0.0f, 1.0f);
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, fade));
    }
    UiEnd();
}
