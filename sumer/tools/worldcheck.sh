#!/bin/sh
# The game's server program (native) and the browser (WebAssembly) must build the very same world: the
# server sends only where things move, never the world itself, and a browser whose world differs is
# turned away ("a different version of the game"). This builds the world both ways and compares them.
# Run it after changing anything that builds the world. Needs Emscripten (emcc) and Node.js.
#
# Two rules keep the worlds the same: never draw two random numbers in one expression (C leaves the
# order of a call's arguments open, and compilers differ: write them into variables first), and build
# with tools/same-world.flags. Positions may still differ in the last digits (the math libraries differ
# that little); counts, kinds and the world's signature must match.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/tools/out/worldcheck
mkdir -p "$OUT"
cd "$ROOT"
SRC=$(ls src/*.c | grep -v '/main.c$')
cc -std=gnu99 -O2 @tools/same-world.flags -o "$OUT/native" tools/worldcheck.c $SRC -Isrc -Iraylib-linux/src raylib-linux/src/libraylib.a -lm -lpthread -ldl
emcc -std=gnu99 -O2 @tools/same-world.flags -o "$OUT/web.js" tools/worldcheck.c $SRC -Isrc -Iraylib-web/src raylib-web/src/libraylib.a -DPLATFORM_WEB \
    -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sENVIRONMENT=node -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$stringToNewUTF8,$UTF8ToString' -lwebsocket.js
status=0
for mode in survival royale; do
    "$OUT/native" $mode > "$OUT/native-$mode.txt"
    node -e "global.window = { location: { search: '' } }; global.navigator = { userAgent: '' }; require('$OUT/web.js')" x $mode > "$OUT/web-$mode.txt"
    python3 - "$OUT/native-$mode.txt" "$OUT/web-$mode.txt" $mode <<'PY' || status=1
import sys
a, b = open(sys.argv[1]).read().split("\n"), open(sys.argv[2]).read().split("\n")
worst, bad = 0.0, []
if a[0] != b[0]:
    bad.append("native: %s / browser: %s" % (a[0], b[0]))
for x, y in zip(a[1:], b[1:]):
    if x == y:
        continue
    xs, ys = x.split(), y.split()
    if xs[:2] != ys[:2] or len(xs) != len(ys):
        bad.append("%s / %s" % (x, y))
        continue
    worst = max([worst] + [abs(float(u) - float(v)) for u, v in zip(xs[2:], ys[2:])])
if worst > 0.001:
    bad.append("something stands %.3f m apart" % worst)
print("%s: %s (largest difference %.6f m)" % (sys.argv[3], "the same world" if not bad else "DIFFERENT WORLDS: " + "; ".join(bad[:5]), worst))
sys.exit(1 if bad else 0)
PY
done
exit $status
