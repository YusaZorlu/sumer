// Offline renderer: musicwav <mood 0-2> <seconds> <out.raw>  writes float32 stereo 44100 Hz.
// Prints how long each part of the track is, in beats (they should be whole bars).
#include "stubs.h"
int main(int argc, char **argv)
{
    if (argc < 4) return 1;
    int mood = atoi(argv[1]), frames = ((int)(atof(argv[2]) * 44100) / 32) * 32;
    float *buf = malloc((size_t)frames * 2 * sizeof(float));
    FILE *f = fopen(argv[3], "wb");
    for (int p = 0; p < MAX_PARTS; p++) {
        float b = MusicPartBeats(mood, p);
        if (b > 0) fprintf(stderr, "part %d: %.2f beats\n", p, b);
    }
    MusicRender(mood, buf, frames);
    fwrite(buf, sizeof(float), (size_t)frames * 2, f);
    fclose(f);
    return 0;
}
