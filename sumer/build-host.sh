#!/bin/sh
# Builds the game's server program, host/sumer-host, which server.py runs for each room (see host/main.c).
# Needs a C compiler and the native raylib in raylib-linux/ (built here if it is missing). The program never
# opens a window, so it runs on a server with no screen: it needs nothing but the C library.
set -e
cd "$(dirname "$0")"
RL=${RAYLIB:-raylib-linux}
if [ ! -f "$RL/src/libraylib.a" ]; then
    [ -d "$RL" ] || git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$RL"
    make -C "$RL/src" PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_33 -j"$(nproc)"
fi
# tools/same-world.flags: the same world as the browser builds (tools/worldcheck.sh compares them).
cc -std=gnu99 -O2 -s -Wall -Wextra @tools/same-world.flags $CFLAGS -o host/sumer-host host/main.c $(ls src/*.c | grep -v '/main.c$') -Isrc -I"$RL/src" \
    "$RL/src/libraylib.a" -lm -lpthread -ldl
echo "Built host/sumer-host"
