#!/bin/sh
# Rebuilds ./sumer-prebuilt, the Linux x86_64 program that runs on Ubuntu 20.04 and newer. It is linked
# against glibc 2.29 with zig's C compiler, which is how one build can run on old systems.
#   pip install ziglang            (gives `python3 -m ziglang`; or set ZIG to another zig command)
#   sh tools/prebuilt.sh
# RAYLIB_ZIG points at a raylib 5.5 source tree built with zig (made in tools/out/raylib-zig if missing).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${ZIG:-python3 -m ziglang}
RLZ=${RAYLIB_ZIG:-$ROOT/tools/out/raylib-zig}
TARGET=x86_64-linux-gnu.2.29
mkdir -p "$ROOT/tools/out"
if [ ! -f "$RLZ/src/libraylib.a" ]; then
    [ -d "$RLZ" ] || git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$RLZ"
    make -C "$RLZ/src" PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_33 \
        CC="$ZIG cc -target $TARGET -idirafter /usr/include -idirafter /usr/include/x86_64-linux-gnu" AR="$ZIG ar" -j"$(nproc)"
fi
$ZIG cc -target $TARGET -std=c99 -O2 -Wall -Wextra -o "$ROOT/tools/out/sumer-prebuilt-new" "$ROOT"/src/*.c \
    -I"$RLZ/src" "$RLZ/src/libraylib.a" -lm -lpthread -ldl -lrt
strip -o "$ROOT/sumer-prebuilt" "$ROOT/tools/out/sumer-prebuilt-new"
echo "Newest glibc needed: $(objdump -T "$ROOT/sumer-prebuilt" | grep -oE 'GLIBC_[0-9.]+' | sort -V | uniq | tail -1)"
ls -l "$ROOT/sumer-prebuilt"
