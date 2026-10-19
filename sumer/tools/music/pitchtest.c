// Checks the tuning of the plucked string and the pipe against the pitch asked for (cents error).
#include "stubs.h"

// Pitch by autocorrelation with parabolic refinement over a window.
static double Detect(const float *x, int n, double lo, double hi)
{
    int minLag = (int)(SR / hi), maxLag = (int)(SR / lo);
    double best = -1e30; int bl = minLag;
    for (int lag = minLag; lag <= maxLag; lag++) {
        double s = 0;
        for (int i = 0; i + lag < n; i++) s += (double)x[i] * x[i + lag];
        if (s > best) { best = s; bl = lag; }
    }
    // refine
    double a = 0, b = 0, c = 0;
    for (int i = 0; i + bl + 1 < n; i++) { a += (double)x[i] * x[i + bl - 1]; b += (double)x[i] * x[i + bl]; c += (double)x[i] * x[i + bl + 1]; }
    double d = (a - 2 * b + c);
    double off = d != 0 ? 0.5 * (a - c) / d : 0;
    return SR / (bl + off);
}
int main(void)
{
    BuildTables();
    static float buf[SR];
    double fs[] = { 73.4, 110.0, 146.83, 196.0, 293.66, 440.0, 587.3, 880.0, 1323.0 };
    printf("pluck (string tuning, cents error):\n");
    for (int k = 0; k < 9; k++) {
        static Pluck v;
        PluckStart(&v, (float)fs[k], 1.0f, 3.0f, 0.5f, 0, 0, 0);
        for (int i = 0; i < SR / 2; i++) buf[i] = PluckTick(&v);
        double f = Detect(buf + 4000, SR / 4, fs[k] * 0.8, fs[k] * 1.25);
        printf("  %7.1f Hz -> %8.2f Hz  %+6.2f cents\n", fs[k], f, 1200 * log2(f / fs[k]));
    }
    printf("pipe:\n");
    for (int k = 2; k < 6; k++) {
        static Pipe v;
        memset(&v, 0, sizeof v);
        v.on = true; v.table = WT_REED; v.f = v.ft = (float)fs[k]; v.glide = 1; v.atk = 1.0f / (0.05f * SR); v.rel = 0.001f; v.hold = SR; v.amp = 1;
        for (int i = 0; i < SR / 2; i++) buf[i] = PipeTick(&v);
        double f = Detect(buf + 4000, SR / 4, fs[k] * 0.8, fs[k] * 1.25);
        printf("  %7.1f Hz -> %8.2f Hz  %+6.2f cents (vibrato on)\n", fs[k], f, 1200 * log2(f / fs[k]));
    }
    return 0;
}
