#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include "rlgl.h"
#include <signal.h>
#include <unistd.h>

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
