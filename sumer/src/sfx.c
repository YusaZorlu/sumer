// Sound effects, made when the game starts by the same kind of arithmetic as the music, so there are
// still no sound files: a swish for a swing (a heavier one for the war mace), a thud for a blow that
// lands (a deeper crunch for the mace), a ringing clang for a blow turned aside, a dull thump for one
// you take, a crack for an arrow striking home, footsteps on grass, earth, brick and in water; and two
// looping sounds of places, a city's crowd and a village's quiet. Their volume is its own setting, apart
// from the music's.
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

// A foot on dry earth: a crunch of grit (noise let through in uneven grains) over a soft thump.
static void Crunch(float *out, int n)
{
    Svf f = { 0 };
    float phase = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE, w = Noise(), grain = fabsf(Noise()) > 0.75f ? 1.0f : 0.2f;
        phase += 2.0f * PI * (110.0f - 300.0f * t) / RATE;
        out[i] = BandPass(&f, w * grain, 1700.0f, 0.9f) * expf(-t * 28.0f) + 0.5f * sinf(phase) * expf(-t * 40.0f);
    }
}

// One sound as 16-bit samples at RATE, its peak at 85% and its tail faded out.
static Wave Render(int id)
{
    static const float secs[SFX_COUNT] = { 0.22f, 0.34f, 0.26f, 0.42f, 0.9f, 0.32f, 0.14f, 0.16f, 0.16f, 0.12f, 0.36f };
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
    case SFX_STEP_GRASS:  Swish(s, n, 1300.0f, 2200.0f, 900.0f, 1.2f); break;
    case SFX_STEP_EARTH:  Crunch(s, n); break;
    case SFX_STEP_HARD:   Thud(s, n, 190.0f, 90.0f, 48.0f, 1.4f, 2600.0f); break;
    case SFX_STEP_WATER:  Swish(s, n, 500.0f, 2300.0f, 800.0f, 0.9f); break;
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

static unsigned char *LoopWav(int k, int *size);

// For listening outside the game: every sound as a .wav file in 'dir' (the harness's sfx mode), and the
// two loops of places.
void SfxExport(const char *dir)
{
    for (int k = 0; k < 2; k++) {
        int size;
        unsigned char *w = LoopWav(k, &size);
        SaveFileData(TextFormat("%s/%s.wav", dir, k ? "village" : "city"), w, size);
        free(w);
    }
    static const char *names[SFX_COUNT] = { "swing", "swing-heavy", "hit", "hit-heavy", "clang", "hurt", "arrow",
                                            "step-grass", "step-earth", "step-hard", "step-water" };
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

// ---- The sound of places ----
//
// Eight seconds each, made once, played as looping streams. The city: many voices talking at once (a
// buzz shaped by two moving formants, in syllables and phrases), the shuffle of the crowd under it, a
// knock now and then. A village: a few voices, wind, a hen. The end of each loop runs into its start.

#define AMB_RATE 22050
#define AMB_N    (8 * AMB_RATE)
#define AMB_FADE (AMB_RATE / 2)

static Music amb[2];
static unsigned char *ambWav[2];
static float ambGain[2];
static bool ambTried;

// The game's own random numbers stay untouched: these come from the noise.
static float Rnd(float lo, float hi)
{
    return lo + (hi - lo) * (0.5f + 0.5f * Noise());
}

// Voices heard from a little way off: none of the words, only their rise and fall.
static void Voices(float *out, int n, int count, float level)
{
    for (int v = 0; v < count; v++) {
        Svf f1 = { 0 }, f2 = { 0 };
        float f0 = Rnd(95.0f, 230.0f), phase = 0, env = 0, syl = 0, phrase = Rnd(0.0f, 2.0f), a = 0, b = 0;
        bool talking = Noise() > 0;
        for (int i = 0; i < n; i++) {
            if ((syl -= 1.0f / AMB_RATE) <= 0) {   // the next syllable: new vowel, a little pitch movement
                syl = Rnd(0.12f, 0.28f);
                a = Rnd(300.0f, 850.0f);
                b = Rnd(950.0f, 2300.0f);
                f0 *= Rnd(0.94f, 1.06f);
                f0 = Clamp(f0, 90.0f, 250.0f);
            }
            if ((phrase -= 1.0f / AMB_RATE) <= 0) {
                talking = !talking;
                phrase = talking ? Rnd(1.0f, 3.5f) : Rnd(0.4f, 2.5f);
            }
            float want = talking ? 0.6f + 0.4f * sinf(PI * syl / 0.28f) : 0.0f;
            env += (want - env) * 0.0015f;
            phase += f0 / AMB_RATE;
            if (phase >= 1) phase -= 1;
            float buzz = (phase < 0.08f ? 1.0f : -0.087f) + 0.15f * Noise();
            float k1 = 2.0f * sinf(PI * a / AMB_RATE), k2 = 2.0f * sinf(PI * b / AMB_RATE);
            f1.low += k1 * f1.band; f1.band += k1 * (buzz - f1.low - f1.band / 4.0f);
            f2.low += k2 * f2.band; f2.band += k2 * (buzz - f2.low - f2.band / 5.0f);
            out[i] += (f1.band + 0.5f * f2.band) * env * level;
        }
    }
}

static void Place(float *out, int n, bool city)
{
    Svf hush = { 0 }, wind = { 0 };
    float low = 0, gust = 0, cluck = -1, knock = -1, kf = 0;
    noiseSeed = city ? 4242u : 777u;
    Voices(out, n, city ? 14 : 3, city ? 0.05f : 0.06f);
    for (int i = 0; i < n; i++) {
        float t = (float)i / AMB_RATE, w = Noise();
        if (city) {   // the crowd's shuffle and a far-off knock of pots or a mallet
            out[i] += BandPass(&hush, w, 900.0f, 0.5f) * 0.05f;
            if (knock < 0 && Rnd(0, 1) < 0.6f / AMB_RATE) knock = 0, kf = Rnd(180.0f, 420.0f);
            if (knock >= 0) {
                out[i] += sinf(2.0f * PI * kf * knock) * expf(-knock * 35.0f) * 0.25f;
                if ((knock += 1.0f / AMB_RATE) > 0.2f) knock = -1;
            }
        } else {      // wind rising and falling, a hen now and then
            gust += (0.5f + 0.5f * sinf(t * 0.7f) * sinf(t * 0.23f + 1.0f) - gust) * 0.0005f;
            out[i] += BandPass(&wind, w, 350.0f + 300.0f * gust, 0.7f) * 0.12f * gust;
            if (cluck < 0 && Rnd(0, 1) < 0.25f / AMB_RATE) cluck = 0;
            if (cluck >= 0) {
                float c = fmodf(cluck, 0.13f);
                out[i] += sinf(2.0f * PI * (700.0f - 900.0f * c) * c) * expf(-c * 40.0f) * 0.12f;
                if ((cluck += 1.0f / AMB_RATE) > 0.5f) cluck = -1;
            }
        }
        low += (out[i] - low) * 0.35f;   // heard through the air and round corners: a little dulled
        out[i] = low;
    }
}

// A loop as a .wav file in memory (k 0 the city, 1 a village), which the audio device streams on and on.
static unsigned char *LoopWav(int k, int *size)
{
    float *s = calloc((size_t)(AMB_N + AMB_FADE), sizeof(float)), peak = 1e-6f;
    Place(s, AMB_N + AMB_FADE, k == 0);
    for (int i = 0; i < AMB_FADE; i++) {
        float w = (float)i / AMB_FADE;
        s[i] = s[i] * w + s[AMB_N + i] * (1.0f - w);
    }
    for (int i = 0; i < AMB_N; i++) peak = fmaxf(peak, fabsf(s[i]));
    int bytes = AMB_N * 2;
    unsigned char *w = malloc((size_t)bytes + 44);
    unsigned int head[11] = { 0x46464952u, (unsigned)bytes + 36, 0x45564157u, 0x20746d66u, 16, 1u | (1u << 16), AMB_RATE, AMB_RATE * 2, 2u | (16u << 16),
                              0x61746164u, (unsigned)bytes };
    memcpy(w, head, 44);   // the header's numbers are little-endian, as on every machine this runs on
    short *pcm = (short *)(w + 44);
    for (int i = 0; i < AMB_N; i++) pcm[i] = (short)lrintf(s[i] / peak * 0.8f * 32767.0f);
    free(s);
    *size = bytes + 44;
    return w;
}

static Music LoopFor(int k)
{
    int size;
    ambWav[k] = LoopWav(k, &size);
    Music m = LoadMusicStreamFromMemory(".wav", ambWav[k], size);
    m.looping = true;
    return m;
}

// Once a frame: the city loud inside its walls and fading over the 30 m past them, quieter at night; a
// village within about 70 m of its fire.
void SfxAmbience(float dt, bool inGame)
{
    if (!IsAudioDeviceReady() || failed) return;
    if (!ambTried) {
        if (level <= 0) return;
        ambTried = true;
        for (int k = 0; k < 2; k++) {
            amb[k] = LoopFor(k);
            if (amb[k].frameCount == 0) return;
        }
        for (int k = 0; k < 2; k++) PlayMusicStream(amb[k]);
    }
    if (amb[0].frameCount == 0 || amb[1].frameCount == 0) return;
    Vector3 p = G.player.pos;
    float want[2] = { 0, 0 };
    if (inGame) {
        want[0] = (InsideWalls(p.x, p.z, 0.0f) ? 1.0f : InsideWalls(p.x, p.z, -30.0f) ? 0.4f : 0.0f) * Lerp(1.0f, 0.3f, G.night);
        float d = 1e9f;
        for (int v = 0; v < VILLAGES; v++) d = fminf(d, XZDist(p, VillageCenter(v)));
        want[1] = Clamp(1.0f - (d - 25.0f) / 45.0f, 0.0f, 1.0f) * Lerp(1.0f, 0.5f, G.night);
    }
    for (int k = 0; k < 2; k++) {
        ambGain[k] += (want[k] - ambGain[k]) * fminf(1.0f, dt * 1.5f);
        SetMusicVolume(amb[k], ambGain[k] * LEVELS[level] * 0.6f);
        UpdateMusicStream(amb[k]);
    }
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
