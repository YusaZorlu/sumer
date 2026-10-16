// What differs in a browser: saves live in IndexedDB, the mouse is captured on a click, and the page's
// address says which server and room to use. Natively these are plain files and no-ops.
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

// Saves, the language and the last slot sit in /persist in a browser, which is kept in IndexedDB.
const char *DataPath(const char *name)
{
#if defined(__EMSCRIPTEN__)
    static char paths[4][96];
    static int next;
    char *p = paths[next++ & 3];
    snprintf(p, sizeof(paths[0]), "/persist/%s", name);
    return p;
#else
    return name;
#endif
}

void MountStorage(void)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        Module.sumerStorage = 0;
        try { FS.mkdir('/persist'); } catch (e) {}
        FS.mount(IDBFS, {}, '/persist');
        FS.syncfs(true, function (err) { Module.sumerStorage = err ? 2 : 1; });
    });
#endif
}

bool StorageReady(void)
{
#if defined(__EMSCRIPTEN__)
    return EM_ASM_INT({ return Module.sumerStorage | 0; }) != 0;
#else
    return true;
#endif
}

// Files written in /persist reach IndexedDB only when synced. A sync asked for while one runs
// is done right after it.
void SyncStorage(void)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        if (Module.sumerSyncing) { Module.sumerSyncAgain = 1; return; }
        Module.sumerSyncing = 1;
        var done = function () {
            if (Module.sumerSyncAgain) {
                Module.sumerSyncAgain = 0;
                FS.syncfs(false, done);
            } else {
                Module.sumerSyncing = 0;
            }
        };
        FS.syncfs(false, done);
    });
#endif
}

// A browser grants the mouse only during a click, so the page asks for it on the next one.
void WantPointer(bool captured)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        Module.sumerWantLock = $0;
        if (!$0 && document.pointerLockElement) document.exitPointerLock();
    }, captured ? 1 : 0);
#else
    (void)captured;
#endif
}

bool PointerCaptured(void)
{
#if defined(__EMSCRIPTEN__)
    return EM_ASM_INT({ return document.pointerLockElement ? 1 : 0; }) != 0;
#else
    return G.cursorLocked;
#endif
}

// A value from the page address (?room=...&name=...), or "" when it has none.
const char *PageParam(const char *key)
{
    static char value[64];
    value[0] = '\0';
#if defined(__EMSCRIPTEN__)
    char *got = (char *)EM_ASM_PTR({
        var v = new URLSearchParams(window.location.search).get(UTF8ToString($0)) || "";
        return stringToNewUTF8(v.substring(0, 60));
    }, key);
    if (got) {
        snprintf(value, sizeof(value), "%s", got);
        free(got);
    }
#else
    (void)key;
#endif
    return value;
}

// The relay's address: the page's own host, over wss when the page came over https.
void ServerAddress(char *buf, int size, const char *query)
{
#if defined(__EMSCRIPTEN__)
    char *got = (char *)EM_ASM_PTR({
        var l = window.location;
        return stringToNewUTF8((l.protocol === 'https:' ? 'wss://' : 'ws://') + l.host + '/ws?' + UTF8ToString($0));
    }, query);
    snprintf(buf, (size_t)size, "%s", got ? got : "");
    if (got) free(got);
#else
    snprintf(buf, (size_t)size, "ws://localhost:8000/ws?%s", query);
#endif
}

static int presses, taps[16], tapCount;
#if defined(__EMSCRIPTEN__)
static Vector2 look;
#endif

// The page counts every mouse press on the game (for MousePressed) and adds up every movement of
// the captured mouse (for MouseLook). A single movement of hundreds of pixels is a known browser
// glitch when the mouse is captured, and is left out.
void WatchClicks(void)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        Module.sumerPresses = 0;
        Module.sumerLookX = 0;
        Module.sumerLookY = 0;
        Module.canvas.addEventListener('mousedown', function (e) {
            Module.sumerPresses |= e.button === 0 ? 1 : e.button === 2 ? 2 : e.button === 1 ? 4 : 0;
        }, true);
        document.addEventListener('mousemove', function (e) {
            var x = e.movementX || 0, y = e.movementY || 0;
            if (document.pointerLockElement !== Module.canvas || Math.abs(x) > 300 || Math.abs(y) > 300) return;
            Module.sumerLookX += x;
            Module.sumerLookY += y;
        }, true);
    });
#endif
}

// Once at the start of every frame. raylib sizes the canvas to the browser window itself, but
// Emscripten's GLFW keeps the size the window was opened with (1280x720) and scales every mouse
// position by the difference, so in any other window the pointer the game sees is not where the
// mouse is. Telling GLFW the real size whenever it changes keeps the two together.
void PageFrame(void)
{
#if defined(__EMSCRIPTEN__)
    static int w, h;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (sw > 0 && sh > 0 && (sw != w || sh != h)) {
        w = sw;
        h = sh;
        SetWindowSize(sw, sh);
    }
    // Every key pressed since the last frame, even one already let go again.
    tapCount = 0;
    for (int k; tapCount < (int)(sizeof(taps) / sizeof(taps[0])) && (k = GetKeyPressed()) != 0;) taps[tapCount++] = k;
    presses = EM_ASM_INT({
        var b = Module.sumerPresses | 0;
        Module.sumerPresses = 0;
        return b;
    });
    look.x = (float)EM_ASM_DOUBLE({
        var x = Module.sumerLookX;
        Module.sumerLookX = 0;
        return x;
    });
    look.y = (float)EM_ASM_DOUBLE({
        var y = Module.sumerLookY;
        Module.sumerLookY = 0;
        return y;
    });
#endif
}

// How far the captured mouse moved since the last frame. In a browser this is every movement the
// page saw (raylib keeps only the last one of each frame, which makes the view slow and jumpy).
Vector2 MouseLook(void)
{
#if defined(__EMSCRIPTEN__)
    return look;
#else
    return GetMouseDelta();
#endif
}

// A click raylib saw, or one the page saw: a quick tap on a touchpad presses and lets go of the
// button between two frames, and raylib alone would miss it.
bool MousePressed(int button)
{
    return IsMouseButtonPressed(button) || ((presses >> button) & 1);
}

// A key raylib saw go down this frame, or one pressed and let go again since the last frame: on a slow
// page a quick tap can fall between two frames.
bool KeyPressed(int key)
{
    if (IsKeyPressed(key)) return true;
    for (int i = 0; i < tapCount; i++)
        if (taps[i] == key) return true;
    return false;
}

bool OnWeb(void)
{
#if defined(__EMSCRIPTEN__)
    return true;
#else
    return false;
#endif
}
