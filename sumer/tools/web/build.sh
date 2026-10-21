#!/bin/bash
# Builds the browser game with the test hooks (testhooks.c) into tools/out/web-test/, ready for serve.sh.
# Needs Emscripten on the PATH and raylib-web/ built (sh build-web.sh does both once).
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$ROOT/tools/out/web-test
mkdir -p "$OUT/web"
cd "$ROOT"
emcc -std=gnu99 -O2 -Wall -Wextra -DSUMER_TEST -o "$OUT/web/index.html" src/*.c tools/web/testhooks.c -Isrc -Iraylib-web/src raylib-web/src/libraylib.a \
    -DPLATFORM_WEB -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sFORCE_FILESYSTEM=1 \
    -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$stringToNewUTF8,$UTF8ToString' \
    -sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString,HEAPF32 -sEXPORTED_FUNCTIONS=_main,_TestInfo,_TestCommand,_TestConsole \
    --embed-file assets/detail.png --embed-file assets/detail2.png --embed-file assets/detail3.png --embed-file assets/flame.png -lidbfs.js -lwebsocket.js --shell-file src/shell.html
cp server.py "$OUT/server.py"
echo "Built $OUT"
