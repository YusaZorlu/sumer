// The browser game's entry point. The desktop program (run.sh, sumer-prebuilt) is kept on the desktop
// branch; this branch is built for the browser only.
#include "game.h"
#include "rlgl.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#if defined(__EMSCRIPTEN__)
// In a browser the page drives the frames: first the saves are read from IndexedDB, then the title
// screen runs until a game starts, and leaving a game goes back to it.
static void Frame(void)
{
    static bool ready;

    PageFrame();
    if (!ready) {
        if (!StorageReady()) {
            BeginDrawing();
            ClearBackground((Color){ 30, 24, 20, 255 });
            DrawText("Loading...", GetScreenWidth() / 2 - 60, GetScreenHeight() / 2 - 10, 20, RAYWHITE);
            EndDrawing();
            return;
        }
        ready = true;
        LoadLanguage();
        GfxLoadSetting();
        SfxLoadSetting();
        MusicInit();
        OpenLobby("");
    }
    if (InLobby()) {
        MusicUpdate(GetFrameTime(), false);
        LobbyFrame();
        return;
    }
    GameUpdate();
    if (InLobby()) return;
    BeginDrawing();
    GameDraw();
    EndDrawing();
    if (G.quit) {
        G.quit = false;
        if (Online()) NetLeave();
        else SaveGame();
        OpenLobby("");
    }
}

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Sumer");
    SetExitKey(KEY_NULL);
    rlSetClipPlanes(0.1, 1200.0);
    MountStorage();
    WatchClicks();
    emscripten_set_main_loop(Frame, 0, 1);
    return 0;
}
#else
#error "The game is built for the browser (sh build-web.sh); the desktop version lives on the desktop branch."
#endif
