#!/bin/sh
# Builds the native test harness: every file in src/ except main.c, plus harness.c, into tools/out/harness.
# Needs the native raylib in raylib-linux/ (sh run.sh builds it; this script does the same if it is missing).
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
RL=${RAYLIB:-$ROOT/raylib-linux}
OUT=$ROOT/tools/out
mkdir -p "$OUT"
if [ ! -f "$RL/src/libraylib.a" ]; then
    [ -d "$RL" ] || git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$RL"
    make -C "$RL/src" PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_33 -j"$(nproc)"
fi
cc -std=c99 -O2 -Wall -Wextra -DSUMER_TEST -o "$OUT/harness" $(ls "$ROOT"/src/*.c | grep -v '/main.c$') \
    "$ROOT/tools/harness/harness.c" -I"$ROOT/src" -I"$RL/src" "$RL/src/libraylib.a" -lGL -lm -lpthread -ldl -lrt -lX11
echo "Built $OUT/harness"
