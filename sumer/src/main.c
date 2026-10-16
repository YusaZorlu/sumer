#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include "rlgl.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

#if defined(__EMSCRIPTEN__)
// In a browser the page drives the frames: first the saves are read from IndexedDB, then the title
// screen runs until a game starts, and leaving a game goes back to it.
static void Frame(void)
{
    static bool ready;

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
        OpenLobby("");
    }
    if (InLobby()) {
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
    emscripten_set_main_loop(Frame, 0, 1);
    return 0;
}
#else
// raylib 5.5 carries on after a failed window and crashes in GL setup.
static void WindowFailed(int sig)
{
    static const char msg[] =
        "Could not open an OpenGL 3.3 window. Run from a desktop session; "
        "on an old GPU or a VM rebuild with: GL21=1 sh run.sh\n";
    (void)sig;
    if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) _exit(2);
    _exit(1);
}

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    signal(SIGSEGV, WindowFailed);
    signal(SIGABRT, WindowFailed);
    InitWindow(1280, 720, "Sumer");
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    if (!IsWindowReady()) WindowFailed(0);

    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    rlSetClipPlanes(0.1, 1200.0);

    LoadLanguage();
    SetSaveSlot(LastSlot());
    GameInit();
    if (LoadGame()) {
        Say("Welcome back. Day %d. Slot %d was loaded from %s.", G.day, SaveSlot() + 1, SaveFileName(SaveSlot()));
        G.msgT = 6.0f;
    }
    SetPanel(PANEL_NONE);
    G.ignoreClick = false;

    while (!WindowShouldClose() && !G.quit) {
        GameUpdate();
        BeginDrawing();
        GameDraw();
        EndDrawing();
    }

    SaveGame();
    CloseWindow();
    return 0;
}
#endif
