#!/bin/sh
# Renders the three tracks to tools/out/music/ (raw float32, wav and spectrogram pngs) and prints their levels.
# Needs the native raylib (see tools/harness/build.sh), ffmpeg and python3 with numpy.   sh tools/music/render.sh
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
RL=${RAYLIB:-$ROOT/raylib-linux}
OUT=$ROOT/tools/out/music
mkdir -p "$OUT"
build() { cc -std=c99 -O2 -Wall -Wextra -o "$OUT/$1" "$ROOT/tools/music/$1.c" -I"$ROOT/src" -I"$RL/src" "$RL/src/libraylib.a" -lGL -lm -lpthread -ldl -lrt -lX11; }
for t in musicwav notes parts pitchtest; do build $t 2>&1 | grep -E "error" || true; done
echo "-- tuning"; "$OUT/pitchtest"
cd "$OUT"
for m in 0 1 2; do
    s=120; [ $m = 1 ] && s=60
    ./musicwav $m $s m$m.raw 2>/dev/null
    ffmpeg -y -loglevel error -f f32le -ar 44100 -ac 2 -i m$m.raw m$m.wav
    ffmpeg -y -loglevel error -i m$m.wav -lavfi "showspectrumpic=s=1800x500:legend=1:scale=log:fscale=log:start=60:stop=8000" spec$m.png
done
echo "-- levels"; python3 "$ROOT/tools/music/analyze.py"
