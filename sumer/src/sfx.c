// Sound effects, made when the game starts by the same kind of arithmetic as the music, so there are
// still no sound files: a swish for a swing (a heavier one for the war mace), a thud for a blow that
// lands (a deeper crunch for the mace), a ringing clang for a blow turned aside, a dull thump for one
// you take, a crack for an arrow striking home. Their volume is its own setting, apart from the music's.
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SFX_FILE  "sumer.sfx"
#define RATE      44100
#define VOICES    3              // the same sound can play this many times over itself

static Sound sounds[SFX_COUNT][VOICES];
static int nextVoice[SFX_COUNT];
static bool built, failed;
static int level = 2;
static const float LEVELS[4] = { 0.0f, 0.35f, 0.7f, 1.0f };
static unsigned noiseSeed = 12345u;

static float Noise(void)
{
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return (float)(noiseSeed >> 8) / 8388608.0f - 1.0f;
}

// A two-pole band-pass (state-variable filter), retuned every sample.
typedef struct { float low, band; } Svf;

static float BandPass(Svf *f, float in, float hz, float q)
{
    float k = 2.0f * sinf(PI * fminf(hz, RATE * 0.2f) / RATE);
    f->low += k * f->band;
    float high = in - f->low - f->band / q;
    f->band += k * high;
    return f->band;
}

static void Swish(float *out, int n, float from, float peak, float to, float q)
{
    Svf f = { 0 };
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float hz = t < 0.5f ? Lerp(from, peak, t * 2.0f) : Lerp(peak, to, (t - 0.5f) * 2.0f);
        float env = sinf(PI * powf(t, 0.8f));
        out[i] = BandPass(&f, Noise(), hz, q) * env * env;
    }
}

// A body struck: a falling tone with an overtone (so small speakers carry it too) and a short crunch of
// noise around 'bright' hertz.
static void Thud(float *out, int n, float f0, float f1, float decay, float crunch, float bright)
{
    float phase = 0;
    Svf f = { 0 };
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE;
        float hz = f1 + (f0 - f1) * expf(-t * 18.0f);
        phase += 2.0f * PI * hz / RATE;
        float tone = (sinf(phase) + 0.45f * sinf(2.3f * phase)) * expf(-t * decay);
        float hit = BandPass(&f, Noise(), bright, 0.8f) * expf(-t * 30.0f) * crunch;
        out[i] = tone + hit;
    }
}

// Bronze on bronze: a few partials that are not in tune with each other, each dying at its own rate.
static void Clang(float *out, int n)
{
    static const float freq[5] = { 523.0f, 1307.0f, 2093.0f, 2960.0f, 3720.0f };
    static const float amp[5] = { 1.0f, 0.6f, 0.45f, 0.3f, 0.2f };
    static const float decay[5] = { 5.0f, 8.0f, 11.0f, 15.0f, 22.0f };
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE, s = 0;
        for (int k = 0; k < 5; k++) s += amp[k] * sinf(2.0f * PI * freq[k] * t * (1.0f + 0.002f * sinf(t * 31.0f))) * expf(-t * decay[k]);
        out[i] = s + (i < 200 ? Noise() * 0.8f * (1.0f - (float)i / 200.0f) : 0.0f);
    }
}

static void Crack(float *out, int n)
{
    float phase = 0, prev = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE, w = Noise();
        float click = (w - prev) * expf(-t * 120.0f);
        prev = w;
        phase += 2.0f * PI * (420.0f - 200.0f * t * 8.0f) / RATE;
        out[i] = click * 0.8f + sinf(phase) * expf(-t * 40.0f) * 0.6f;
    }
}

// One sound as 16-bit samples at RATE, its peak at 85% and its tail faded out.
static Wave Render(int id)
{
    static const float secs[SFX_COUNT] = { 0.22f, 0.34f, 0.26f, 0.42f, 0.9f, 0.32f, 0.14f };
    int n = (int)(secs[id] * RATE);
    float *s = calloc((size_t)n, sizeof(float)), peak = 1e-6f;
    noiseSeed = 12345u + (unsigned)id * 977u;
    switch (id) {
    case SFX_SWING:       Swish(s, n, 700.0f, 2200.0f, 1100.0f, 2.2f); break;
    case SFX_SWING_HEAVY: Swish(s, n, 300.0f, 1100.0f, 500.0f, 1.6f); break;
    case SFX_HIT:         Thud(s, n, 260.0f, 130.0f, 20.0f, 2.5f, 1800.0f); break;
    case SFX_HIT_HEAVY:   Thud(s, n, 240.0f, 118.0f, 12.0f, 3.6f, 1400.0f); break;
    case SFX_CLANG:       Clang(s, n); break;
    case SFX_HURT:        Thud(s, n, 220.0f, 120.0f, 13.0f, 2.4f, 1000.0f); break;
    default:              Crack(s, n); break;
    }
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(s[i]));
    short *pcm = malloc((size_t)n * sizeof(short));
    for (int i = 0; i < n; i++) {
        float fade = i > n - 400 ? (float)(n - i) / 400.0f : 1.0f;
        pcm[i] = (short)lrintf(s[i] / peak * 0.85f * fade * 32767.0f);
    }
    free(s);
    return (Wave){ (unsigned int)n, RATE, 16, 1, pcm };
}

static void Build(void)
{
    built = true;
    for (int id = 0; id < SFX_COUNT; id++) {
        Wave w = Render(id);
        sounds[id][0] = LoadSoundFromWave(w);
        UnloadWave(w);
        if (sounds[id][0].frameCount == 0) failed = true;
        for (int v = 1; v < VOICES && !failed; v++) sounds[id][v] = LoadSoundAlias(sounds[id][0]);
    }
}

// For listening outside the game: every sound as a .wav file in 'dir' (the harness's sfx mode).
void SfxExport(const char *dir)
{
    static const char *names[SFX_COUNT] = { "swing", "swing-heavy", "hit", "hit-heavy", "clang", "hurt", "arrow" };
    for (int id = 0; id < SFX_COUNT; id++) {
        Wave w = Render(id);
        ExportWave(w, TextFormat("%s/%s.wav", dir, names[id]));
        UnloadWave(w);
    }
}

// Plays a sound at 'volume' (0-1) of the setting, a little higher or lower each time so a run of blows
// does not sound like a machine.
void SfxPlay(int id, float volume)
{
    if (id < 0 || id >= SFX_COUNT || level <= 0 || failed || !IsAudioDeviceReady()) return;
    if (!built) Build();
    if (failed) return;
    Sound *s = &sounds[id][nextVoice[id]];
    nextVoice[id] = (nextVoice[id] + 1) % VOICES;
    SetSoundVolume(*s, Clamp(volume, 0.0f, 1.0f) * LEVELS[level]);
    SetSoundPitch(*s, RandF(0.93f, 1.07f));
    PlaySound(*s);
}

// A sound somewhere in the world: quieter with distance from the camera, silent past 'reach' metres.
void SfxPlayAt(int id, Vector3 at, float volume, float reach)
{
    float d = Vector3Distance(at, G.cam.position);
    if (d < reach) SfxPlay(id, volume * (1.0f - d / reach));
}

int SfxLevel(void)
{
    return level;
}

void SfxSetLevel(int l)
{
    level = l < 0 ? 0 : l > 3 ? 3 : l;
    FILE *f = fopen(DataPath(SFX_FILE), "w");
    if (!f) return;
    fprintf(f, "%d\n", level);
    fclose(f);
    SyncStorage();
}

void SfxLoadSetting(void)
{
    int v;
    FILE *f = fopen(DataPath(SFX_FILE), "r");
    if (!f) return;
    if (fscanf(f, "%d", &v) == 1 && v >= 0 && v <= 3) level = v;
    fclose(f);
}

const char *SfxLevelName(void)
{
    static const char *const names[4] = { "off", "low", "medium", "high" };
    return names[level];
}
