// The game's server program: server.py starts one for each room that has players in it and talks to it
// through its standard input and output (net.c). It runs the room's world, sixty steps a second, the way
// a player's browser would, with no player of its own. When server.py closes the pipe it saves the
// world (a survival or PvP room's world carries on next time) and stops.
//
//     sumer-host survival|pvp|royale|party ROOM-ID
//
// It is started in the room's own folder, where its save (sumer-mp.sav) is kept. Built by build-host.sh.
#include "game.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static double Now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static volatile sig_atomic_t stop;

static void Stop(int sig)
{
    (void)sig;
    stop = 1;
}

int main(int argc, char **argv)
{
    int mode = argc > 1 && strcmp(argv[1], "pvp") == 0 ? ROOM_PVP : argc > 1 && strcmp(argv[1], "royale") == 0 ? ROOM_ROYALE
             : argc > 1 && strcmp(argv[1], "party") == 0 ? ROOM_PARTY : ROOM_SURVIVAL;
    if (argc < 3 || (mode == ROOM_SURVIVAL && strcmp(argv[1], "survival") != 0)) {
        fprintf(stderr, "usage: sumer-host survival|pvp|royale|party ROOM-ID (server.py starts it)\n");
        return 2;
    }
    SetTraceLogLevel(LOG_WARNING);
    signal(SIGTERM, Stop);
    signal(SIGINT, Stop);
    signal(SIGPIPE, SIG_IGN);
    ServeRoom(mode, argv[2]);
    fprintf(stderr, "sumer-host: room %s (%s) is running\n", argv[2], argv[1]);
    const double step = 1.0 / 60.0;
    double last = Now();
    while (!stop && NetLink() == LINK_OPEN) {
        double wait = last + step - Now();
        if (wait > 0) NetWait(wait);
        double now = Now();
        if (now - last < step) continue;
        ServerTick((float)fmin(now - last, 0.05));
        last = now;
    }
    SaveNetGame();
    fprintf(stderr, "sumer-host: room %s stopped\n", argv[2]);
    return 0;
}
