#!/bin/sh
# Linux: GL21=1 sh run.sh builds for OpenGL 2.1 (older GPUs, VMs).
set -e
cd "$(dirname "$0")"

RAYLIB_TAG=5.5

build_raylib_from_source() {
    dir=$1
    graphics=$2
    if [ -f "$dir/src/libraylib.a" ] && [ "$(cat "$dir/.graphics" 2>/dev/null)" = "$graphics" ]; then
        return
    fi
    [ -d "$dir" ] || git clone --depth 1 --branch "$RAYLIB_TAG" https://github.com/raysan5/raylib.git "$dir"
    make -C "$dir/src" clean >/dev/null 2>&1 || true
    make -C "$dir/src" PLATFORM=PLATFORM_DESKTOP GRAPHICS="$graphics" -j"$3"
    echo "$graphics" > "$dir/.graphics"
}

case "$(uname -s)" in
Darwin)
    if ! xcode-select -p >/dev/null 2>&1; then
        echo "Xcode Command Line Tools are missing. Installing them now; run this script again when that finishes."
        xcode-select --install
        exit 1
    fi
    FRAMEWORKS="-framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio -framework OpenGL"
    if command -v brew >/dev/null 2>&1; then
        brew list raylib >/dev/null 2>&1 || brew install raylib
        RL="$(brew --prefix raylib)"
        INCLUDES="-I$RL/include"
        LIBS="-L$RL/lib -lraylib"
    else
        build_raylib_from_source raylib GRAPHICS_API_OPENGL_33 "$(sysctl -n hw.ncpu)"
        INCLUDES="-Iraylib/src"
        LIBS="raylib/src/libraylib.a"
    fi
    cc -std=c99 -O2 -Wall -Wextra -o sumer src/*.c $INCLUDES $LIBS $FRAMEWORKS
    ;;
Linux)
    PACKAGES="build-essential git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev"
    MISSING=""
    for p in $PACKAGES; do
        dpkg -s "$p" >/dev/null 2>&1 || MISSING="$MISSING $p"
    done
    if [ -n "$MISSING" ]; then
        SUDO=""
        [ "$(id -u)" -ne 0 ] && SUDO=sudo
        echo "Installing build packages:$MISSING"
        $SUDO apt-get update
        $SUDO apt-get install -y $MISSING
    fi
    GRAPHICS=GRAPHICS_API_OPENGL_33
    [ -n "$GL21" ] && GRAPHICS=GRAPHICS_API_OPENGL_21
    build_raylib_from_source raylib-linux "$GRAPHICS" "$(nproc)"
    cc -std=c99 -O2 -Wall -Wextra -o sumer src/*.c -Iraylib-linux/src raylib-linux/src/libraylib.a \
       -lGL -lm -lpthread -ldl -lrt -lX11
    ;;
*)
    echo "Unsupported system: $(uname -s)"
    exit 1
    ;;
esac

exec ./sumer
