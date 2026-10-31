#!/bin/sh
# Builds the browser version into web/ (index.html, index.js, index.wasm), which server.py serves.
# Needs Emscripten: https://emscripten.org/docs/getting_started/downloads.html (emcc on the PATH).
set -e
cd "$(dirname "$0")"

RAYLIB_TAG=5.5
if ! command -v emcc >/dev/null 2>&1; then
    echo "emcc not found. Install the Emscripten SDK and run: source ./emsdk_env.sh"
    exit 1
fi
# The game draws with WebGL 2 (OpenGL ES 3.0); a raylib built earlier for WebGL 1 is rebuilt.
if [ ! -f raylib-web/src/libraylib.a ] || [ "$(cat raylib-web/.graphics 2>/dev/null)" != GRAPHICS_API_OPENGL_ES3 ]; then
    [ -d raylib-web ] || git clone --depth 1 --branch "$RAYLIB_TAG" https://github.com/raysan5/raylib.git raylib-web
    make -C raylib-web/src clean >/dev/null 2>&1 || true
    make -C raylib-web/src PLATFORM=PLATFORM_WEB GRAPHICS=GRAPHICS_API_OPENGL_ES3 -j4
    echo GRAPHICS_API_OPENGL_ES3 > raylib-web/.graphics
fi

mkdir -p web
# tools/same-world.flags: the server program (build-host.sh) must build the very same world as the browser.
emcc -std=gnu99 -O2 -Wall -Wextra @tools/same-world.flags -o web/index.html src/*.c -Iraylib-web/src raylib-web/src/libraylib.a \
    -DPLATFORM_WEB -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sFORCE_FILESYSTEM=1 \
    -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$stringToNewUTF8,$UTF8ToString' \
    -sEXPORTED_RUNTIME_METHODS=HEAPF32 \
    --embed-file assets/detail.png --embed-file assets/detail2.png --embed-file assets/detail3.png --embed-file assets/flame.png --embed-file assets/board.png --embed-file assets/clay.png --embed-file assets/sky_day.jpg --embed-file assets/sky_dusk.jpg --embed-file assets/sky_grey.jpg --embed-file assets/fonts/OSerif-Regular.ttf --embed-file assets/fonts/Jupiteroid-Regular.ttf --embed-file assets/music -lidbfs.js -lwebsocket.js --shell-file src/shell.html
echo "Built web/. Start the server with: python3 server.py"
