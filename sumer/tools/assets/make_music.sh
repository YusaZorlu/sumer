#!/bin/sh
# Builds assets/music/*.ogg, the music of places, from CC0 tracks on OpenGameArt (needs curl and ffmpeg):
#   city.ogg     "Darbuka Delight" (blow_the_day_with_a_darbuka) by Adiutorium  opengameart.org/content/darbuka-delight
#   village.ogg  "Hyyoshi Arabian Style Background Music RPG" by Tozan        opengameart.org/content/hyyoshi-arabian-syle-backgound-music-rpg
#   wild.ogg     "Caught in the Desert Plains" by Tozan                       opengameart.org/content/caught-in-the-desert-plains
#   temple.ogg   "Egyptian meditation music" by HaelDB                         opengameart.org/content/egyptian-meditation-music
# Each is brought to the same loudness, faded in and out (they loop) and made a small Ogg Vorbis file.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TMP=$(mktemp -d)
OUT="$ROOT/assets/music"
mkdir -p "$OUT"
get() { curl -sSfL -A "Mozilla/5.0" -o "$TMP/$2" "https://opengameart.org/sites/default/files/$1"; }
enc() {
    d=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$TMP/$1")
    st=$(awk "BEGIN { print ($d > 3) ? $d - 3 : 0 }")
    ffmpeg -v error -y -i "$TMP/$1" -af "loudnorm=I=-20:TP=-2:LRA=11,afade=t=in:d=1.5,afade=t=out:st=$st:d=3" \
        -ar "$3" -c:a libvorbis -q:a 2 "$OUT/$2"
}
get blow_the_day_with_a_darbuka.mp3 city.mp3 && enc city.mp3 city.ogg 44100
get Hyoshi2.ogg village.src.ogg && enc village.src.ogg village.ogg 22050
get telosdesert_0.ogg wild.src.ogg && enc wild.src.ogg wild.ogg 22050
get v312.ogg temple.src.ogg && enc temple.src.ogg temple.ogg 44100
rm -rf "$TMP"
echo "wrote assets/music/city.ogg, village.ogg, wild.ogg and temple.ogg"
