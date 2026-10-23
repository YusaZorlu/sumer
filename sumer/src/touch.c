// Touch controls for phones and tablets: a stick under the left thumb (pushed to its edge, a run), a drag
// anywhere else on the screen to look around, and see-through buttons for the main keys: attack (held to
// draw the bow), parry, use (E), crouch, first person or behind you, the baskets and the menu (which, with a
// panel open, closes it). Shown on a phone from the start, anywhere else once a finger touches the game.
#include "game.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#define FINGERS  10
#define STICK_R  62.0f

enum { R_NONE, R_STICK, R_LOOK, R_BUTTON };
typedef struct { int id, role, button; Vector2 start, at; bool seen; } Finger;
static Finger fingers[FINGERS];
static int fingerCount;
static bool down[TB_COUNT], pressed[TB_COUNT], seenTouch;
static Vector2 stick, look;
static const char *const labels[TB_COUNT] = { "Attack", "Parry", "Use", "Crouch", "View", "Bag", "Menu" };

bool TouchOn(void)
{
    static int phone = -1;
    if (phone < 0) phone = OnWeb() && OnPhone();
    return phone || seenTouch;
}

// Where each button sits, in the UI's units, and how big it is: the main ones under the right thumb, the
// view switch below the map, the baskets and the menu at the top in the middle.
static bool Place(int b, Vector2 *c, float *r)
{
    float sw = UiWidth(), sh = UiHeight();
    static const float spot[TB_COUNT][3] = {
        { -100, -110, 50 }, { -225, -78, 36 }, { -205, -196, 36 }, { -86, -232, 32 }, { -40, 260, 28 }, { -48, 36, 24 }, { 48, 36, 24 },
    };
    if (G.panel != PANEL_NONE && b != TB_MENU) return false;
    c->x = b >= TB_BAG ? sw / 2 + spot[b][0] : sw + spot[b][0];
    c->y = spot[b][1] < 0 ? sh + spot[b][1] : spot[b][1];
    *r = spot[b][2];
    return true;
}

#if defined(__EMSCRIPTEN__)
static int Assign(Vector2 p)
{
    for (int b = 0; b < TB_COUNT; b++) {
        Vector2 c;
        float r;
        if (Place(b, &c, &r) && Vector2Distance(p, c) <= r + 10) return R_BUTTON + b;
    }
    if (G.panel != PANEL_NONE) return R_NONE;
    for (int i = 0; i < fingerCount; i++)
        if (fingers[i].role == R_STICK) return R_LOOK;
    return p.x < UiWidth() * 0.4f && p.y > UiHeight() * 0.35f ? R_STICK : R_LOOK;
}
#endif

// Once a frame, from what the page saw: every finger on the game (id, where), and those lifted since.
void TouchFrame(void)
{
#if defined(__EMSCRIPTEN__)
    static float buf[FINGERS * 2 * 6];
    static bool listening;
    if (!listening) {
        listening = true;
        EM_ASM({
            var T = Module.sumerTouch = ({ live: {}, ended: [] });
            var at = function (t) {
                var r = Module.canvas.getBoundingClientRect();
                return ({ x: (t.clientX - r.left) * Module.canvas.width / r.width, y: (t.clientY - r.top) * Module.canvas.height / r.height });
            };
            var moved = function (e) {
                for (var i = 0; i < e.changedTouches.length; i++) {
                    var t = e.changedTouches[i];
                    var f = at(t);
                    var was = T.live[t.identifier];
                    f.sx = was ? was.sx : f.x;
                    f.sy = was ? was.sy : f.y;
                    T.live[t.identifier] = f;
                }
            };
            var up = function (e) {
                for (var i = 0; i < e.changedTouches.length; i++) {
                    var t = e.changedTouches[i];
                    var f = at(t);
                    var was = T.live[t.identifier];
                    f.id = t.identifier;
                    f.sx = was ? was.sx : f.x;
                    f.sy = was ? was.sy : f.y;
                    T.ended.push(f);
                    delete T.live[t.identifier];
                }
            };
            Module.canvas.addEventListener('touchstart', moved, true);
            Module.canvas.addEventListener('touchmove', moved, true);
            Module.canvas.addEventListener('touchend', up, true);
            Module.canvas.addEventListener('touchcancel', up, true);
        });
    }
    int n = EM_ASM_INT({
        var T = Module.sumerTouch;
        var b = $0 >> 2;
        var k = 0;
        var put = function (id, f, gone) {
            if (k >= $1) return;
            HEAPF32[b + 6 * k] = id;
            HEAPF32[b + 6 * k + 1] = f.x;
            HEAPF32[b + 6 * k + 2] = f.y;
            HEAPF32[b + 6 * k + 3] = gone;
            HEAPF32[b + 6 * k + 4] = f.sx;
            HEAPF32[b + 6 * k + 5] = f.sy;
            k++;
        };
        for (var id in T.live) put(+id, T.live[id], 0);
        for (var i = 0; i < T.ended.length; i++) put(T.ended[i].id, T.ended[i], 1);
        T.ended = [];
        return k;
    }, buf, FINGERS * 2);
    if (n > 0) seenTouch = true;

    bool tapped = false;
    look = (Vector2){ 0 };
    for (int b = 0; b < TB_COUNT; b++) pressed[b] = down[b] = false;
    for (int i = 0; i < fingerCount; i++) fingers[i].seen = false;
    for (int k = 0; k < n; k++) {
        const float *e = &buf[6 * k];
        int id = (int)e[0];
        Vector2 p = Vector2Scale((Vector2){ e[1], e[2] }, 1.0f / UiScale()), from = Vector2Scale((Vector2){ e[4], e[5] }, 1.0f / UiScale());
        bool gone = e[3] != 0;
        Finger *f = NULL;
        for (int i = 0; i < fingerCount; i++)
            if (fingers[i].id == id) f = &fingers[i];
        if (!f && fingerCount < FINGERS) {
            f = &fingers[fingerCount++];
            // Placed where it came down, which may be a little way from where it is by now.
            int role = Assign(from);
            *f = (Finger){ id, role >= R_BUTTON ? R_BUTTON : role, role - R_BUTTON, from, from, false };
            if (f->role == R_BUTTON) {
                pressed[f->button] = tapped = true;
            }
        }
        if (!f) continue;
        if (f->role == R_LOOK) look = Vector2Add(look, Vector2Subtract(p, f->at));
        f->at = p;
        f->seen = !gone;
        if (f->role == R_BUTTON && !gone) down[f->button] = true;
    }
    // Fingers lifted (or lost) are forgotten.
    stick = (Vector2){ 0 };
    for (int i = 0; i < fingerCount;) {
        if (!fingers[i].seen) {
            fingers[i] = fingers[--fingerCount];
            continue;
        }
        if (fingers[i].role == R_STICK) {
            Vector2 d = Vector2Scale(Vector2Subtract(fingers[i].at, fingers[i].start), 1.0f / STICK_R);
            if (Vector2Length(d) > 1.0f) d = Vector2Normalize(d);
            stick = d;
        }
        i++;
    }
    // A tap on one of these buttons is not also a click on whatever menu it opens.
    if (tapped) ForgetClick();
#endif
}

bool TouchDown(int b)
{
    return down[b];
}

bool TouchPressed(int b)
{
    return pressed[b];
}

// The stick: x to the right, y forward, each -1 to 1; nothing within a small dead zone.
Vector2 TouchStick(void)
{
    return Vector2Length(stick) < 0.2f ? (Vector2){ 0 } : (Vector2){ stick.x, -stick.y };
}

bool TouchRun(void)
{
    return Vector2Length(stick) > 0.9f;
}

// How far the looking finger moved since the last frame, in the UI's units.
Vector2 TouchLook(void)
{
    return look;
}

#if defined(SUMER_TEST)
// Test builds: where button b is on the page, in CSS pixels, and its radius; false when it is not shown.
bool TouchButtonAt(int b, Vector2 *c, float *r)
{
    if (!Place(b, c, r)) return false;
    float k = UiScale() / PixelRatio();
    *c = Vector2Scale(*c, k);
    *r *= k;
    return true;
}
#endif

void DrawTouch(void)
{
    if (!TouchOn()) return;
    float gap = 1;
    for (int b = 0; b < TB_COUNT; b++) {
        Vector2 c;
        float r;
        if (!Place(b, &c, &r)) continue;
        DrawCircleV(c, r, Fade(down[b] ? WHITE : BLACK, down[b] ? 0.28f : 0.16f));
        DrawRing(c, r - 2, r, 0, 360, 48, Fade(WHITE, 0.4f));
        const char *s = L(b == TB_MENU && G.panel != PANEL_NONE ? "Close" : labels[b]);
        int size = r < 30 ? 14 : 16;
        Vector2 w = MeasureTextEx(GameFont(), s, (float)size, gap);
        DrawTextEx(GameFont(), s, (Vector2){ (float)(int)(c.x - w.x / 2), (float)(int)(c.y - w.y / 2) }, (float)size, gap, Fade(WHITE, 0.75f));
    }
    if (G.panel != PANEL_NONE) return;
    // The stick: where the thumb came down, or where it usually does.
    Vector2 base = { 130, UiHeight() - 130 }, knob = base;
    for (int i = 0; i < fingerCount; i++)
        if (fingers[i].role == R_STICK) {
            base = fingers[i].start;
            knob = Vector2Add(base, Vector2Scale(stick, STICK_R));
        }
    DrawCircleV(base, STICK_R, Fade(BLACK, 0.14f));
    DrawRing(base, STICK_R - 2, STICK_R, 0, 360, 48, Fade(WHITE, 0.35f));
    DrawCircleV(knob, 26, Fade(WHITE, 0.25f));
}
