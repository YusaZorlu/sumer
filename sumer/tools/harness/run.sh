#!/bin/sh
# Builds and runs the logic tests (tools/harness/tests.c): sh tools/harness/run.sh
# The harness opens a window, so without a display this starts a virtual one (Xvfb, apt install xvfb).
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
sh "$ROOT/tools/harness/build.sh"
if [ -z "$DISPLAY" ]; then
    Xvfb :77 -screen 0 1280x720x24 >/dev/null 2>&1 &
    XVFB=$!
    trap 'kill $XVFB 2>/dev/null' EXIT
    export DISPLAY=:77
    sleep 1
fi
"$ROOT/tools/out/harness" test | tee "$ROOT/tools/out/tests.log" | grep -E "FAIL|passed"
