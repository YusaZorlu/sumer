// Renders a track with one part at a time and prints each part's level and spectral balance: parts <mood> <seconds>
#include "stubs.h"
#include <complex.h>
int main(int argc, char **argv)
{
    int m = atoi(argv[1]);
    int secs = argc > 2 ? atoi(argv[2]) : 20, frames = secs * SR / 32 * 32;
    float *buf = malloc((size_t)frames * 2 * sizeof(float));
    static int counts[MAX_PARTS];
    Prepare();
    for (int i = 0; i < TRACKS[m].partCount; i++) counts[i] = parts[m][i].count;
    for (int k = 0; k < TRACKS[m].partCount; k++) {
        for (int i = 0; i < TRACKS[m].partCount; i++) parts[m][i].count = i == k ? counts[i] : 0;
        MusicRender(m, buf, frames);
        double e = 0, band[5] = {0}; 
        // crude band energies via one-pole splits
        float lp1 = 0, lp2 = 0, lp3 = 0, lp4 = 0;
        for (int i = 0; i < frames; i++) {
            float x = buf[2 * i];
            e += (double)x * x;
            lp1 += 0.0278f * (x - lp1);   // ~200 Hz
            lp2 += 0.0800f * (x - lp2);   // ~600
            lp3 += 0.1500f * (x - lp3);   // ~1200
            lp4 += 0.3200f * (x - lp4);   // ~3000
            band[0] += (double)lp1 * lp1;
            band[1] += (double)(lp2 - lp1) * (lp2 - lp1);
            band[2] += (double)(lp3 - lp2) * (lp3 - lp2);
            band[3] += (double)(lp4 - lp3) * (lp4 - lp3);
            band[4] += (double)(x - lp4) * (x - lp4);
        }
        printf("part %d (inst %d, gain %.2f): rms %.1f dB | <200 %.0f%% 200-600 %.0f%% 600-1200 %.0f%% 1.2-3k %.0f%% >3k %.0f%%\n", k, TRACKS[m].parts[k].inst, TRACKS[m].parts[k].gain,
               10 * log10(e / frames + 1e-12), 100 * band[0] / e, 100 * band[1] / e, 100 * band[2] / e, 100 * band[3] / e, 100 * band[4] / e);
    }
    return 0;
}
