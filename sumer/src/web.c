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

bool OnWeb(void)
{
#if defined(__EMSCRIPTEN__)
    return true;
#else
    return false;
#endif
}
