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
if [ ! -f raylib-web/src/libraylib.a ]; then
    [ -d raylib-web ] || git clone --depth 1 --branch "$RAYLIB_TAG" https://github.com/raysan5/raylib.git raylib-web
    make -C raylib-web/src PLATFORM=PLATFORM_WEB -j4
fi

mkdir -p web
emcc -std=gnu99 -O2 -Wall -Wextra -o web/index.html src/*.c -Iraylib-web/src raylib-web/src/libraylib.a \
    -DPLATFORM_WEB -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sFORCE_FILESYSTEM=1 \
    -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$stringToNewUTF8,$UTF8ToString' \
    -sEXPORTED_RUNTIME_METHODS=HEAPF32 \
    -lidbfs.js -lwebsocket.js --shell-file src/shell.html
echo "Built web/. Start the server with: python3 server.py"
