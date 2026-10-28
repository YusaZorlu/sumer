// The game's music: three tracks (the morning theme, the fight, the night) played by a small
// synthesizer, so there are no sound files to ship and nothing to license. Every note is made here:
// plucked lyre strings, reed pipes, animal horns, frame drums and drones, with a little reverb.
//
// The tunes are written for this game in the spirit of what is known of Mesopotamian music: the
// Old Babylonian tuning tablets (about 1800 BC) describe lyres tuned in pure fifths and fourths to give
// seven-note scales, so the scales here are built the same way (whole tone 9/8, half tone 256/243),
// and the sound is the lyre of Ur, the reed pipe, the frame drum and a long drone. See README.md.
//
// How it fits together: a track is a few "parts" (an instrument and a score in a tiny text notation,
// see ParseScore); each part loops on its own length, so the whole tune takes a long time to repeat.
// MusicUpdate() picks the mood and feeds the audio stream from MusicMix().
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR 44100
#define BLOCK 32           // the sequencer is checked every 32 samples (0.7 ms)
#define STREAM_FRAMES 16384 // frames per stream buffer (0.37 s): big, so a slow frame does not starve it
#define SLICE 2048         // frames made per game frame while the stream is full, a multiple of BLOCK
#define MAX_PARTS 10
#define MAX_NOTES 4096
#define MUSIC_FILE "sumer.music"

enum { MOOD_DAY, MOOD_COMBAT, MOOD_NIGHT, MOODS };
enum { I_PLUCK, I_PIPE, I_PAD, I_HORN, I_DRUM };
enum { WT_FLUTE, WT_REED, WT_HORN, WT_PAD, WT_COUNT };
#define WT_N 2048

typedef struct {
    int inst;
    const char *score;
    float gain, pan, send;   // level, stereo place (-1..1) and how much goes to the reverb
    float p1, p2, p3;        // instrument knobs, see the table below
} PartDef;

// Knobs by instrument:
//   I_PLUCK  p1 ring time (s), p2 brightness 0..1
//   I_PIPE   p1 table (WT_FLUTE or WT_REED), p2 breath noise, p3 attack (s)
//   I_PAD    p1 attack (s), p2 release (s), p3 table
//   I_HORN   p1 drive, p2 brightness (Hz), p3 attack (s)
//   I_DRUM   p1 pitch multiplier
typedef struct {
    const char *name;
    float bpm, tonic;
    const float *scale;
    int scaleLen;
    const PartDef *parts;
    int partCount;
} TrackDef;

typedef struct {
    float beat, dur, vel, jit;
    float freq[4];
    float grace;          // pitch of a quick grace note before the main one (0: none)
    unsigned char nf;     // how many pitches (a chord has several)
    char kind;            // 'n' note, 'r' rest, or the drum letter
    bool slide;
} Note;

typedef struct {
    const PartDef *def;
    Note *notes;
    int count, next;
    float length, base, lastFreq;
} Part;

// ---------------------------------------------------------------------------------------------
// Scales: pure fifths and fourths, the way the tuning tablets build them.

#define TONE (9.0f / 8.0f)
#define LIMMA (256.0f / 243.0f)
static const float SCALE_DORIAN[7] = { 1.0f, TONE, 32.0f / 27.0f, 4.0f / 3.0f, 3.0f / 2.0f, 27.0f / 16.0f, 16.0f / 9.0f };
// The fight: flat second and flat sixth with a major third in between (an augmented second above the second).
static const float SCALE_FIGHT[7] = { 1.0f, LIMMA, 81.0f / 64.0f, 4.0f / 3.0f, 3.0f / 2.0f, 128.0f / 81.0f, 16.0f / 9.0f };
// The night: five notes with no thirds, all made of fifths.
static const float SCALE_NIGHT[5] = { 1.0f, TONE, 4.0f / 3.0f, 3.0f / 2.0f, 27.0f / 16.0f };

// ---------------------------------------------------------------------------------------------
// The tunes. Notation (ParseScore): a note is a scale degree 1-9, ' for an octave up, , for an octave
// down, then ! (loud) or . (soft), then :beats. 1+5 sounds a chord, - is a rest, ^ in front gives a
// quick grace note from above, _ from below, ~ slides up to the note. Drums: D dum, T tek, k ghost tek,
// B kettle drum, S rattle, C clap. A missing :beats repeats the last length.
#define X2(s) s s
#define X4(s) X2(X2(s))
#define X8(s) X4(X2(s))

// --- Morning in Ur: 72 beats a minute, the Dorian-like scale on D, a drone, a lyre and a reed pipe.
#define LYRE_A "1:0.5 5 1' 5 3' 5 1' 5  1 5 7 5 2' 5 7 5  1 5 1' 5 3' 5 4' 5  1 5 2' 5 1' 5 7 5 "
#define LYRE_B "1+5+1':4 7+5+3':4 4+1'+4':4 1+5+1':4 "
#define LYRE_C "1':0.5 3' 5' 3' 1' 3' 2' 1'  7 2' 4' 2' 7 2' 1' 7  1' 3' 5' 6' 5' 3' 2' 1'  7 1' 2' 3' 2' 1' 7 5 "
#define PIPE_A  "5':1.5 4':0.5 3':1 2':1  3':2 1':1 -:1  2':1 3':1 4':1.5 3':0.5  2':3 -:1 "
#define PIPE_A2 "3':1 4':1 5':2  6':1 5':1 4':1 3':1  4':1 3':1 2':1 1':1  1':3 -:1 "
#define PIPE_B  "1'':2 7':1 6':1  5':1.5 6':0.5 7':1 5':1  6':1 5':1 4':1.5 3':0.5  4':2 3':1 -:1 "
#define PIPE_B2 "5':1 7':1 1'':2  2'':1 1'':1 7':1 6':1  5':1.5 4':0.5 3':1 2':1  1':4 "
#define PIPE_AO "5':1.5 ^4':0.5 3':1 ^2':1  ~3':2 1':1 -:1  2':1 ~3':1 4':1.5 ^3':0.5  2':2.5 ^2':0.5 -:1 "
#define FRAME_DAY "D.:1 -:0.5 k.:0.5 T.:1 -:0.5 k.:0.5 "

static const PartDef DAY_PARTS[] = {
    { I_PAD, "1,+5,+1:64", 0.27f, 0.0f, 0.5f, 4.0f, 6.0f, WT_PAD },
    { I_PLUCK, LYRE_A X2(LYRE_A) LYRE_B LYRE_C LYRE_C LYRE_A LYRE_B, 0.70f, -0.25f, 0.45f, 3.2f, 0.45f, 0 },
    { I_PIPE, "-:16 " PIPE_A PIPE_A2 "-:16 " PIPE_B PIPE_B2 PIPE_AO PIPE_A2, 0.26f, 0.3f, 0.55f, WT_REED, 0.06f, 0.07f },
    { I_DRUM, "-:16 " X8(FRAME_DAY) X8(FRAME_DAY) X8(FRAME_DAY) X4(FRAME_DAY), 0.60f, 0.0f, 0.25f, 1.0f, 0, 0 },
};

// --- Raiders at the Gate: 148 beats a minute, the scale with the flat second, drums, a horn and a shrill pipe.
#define FIGHT_BASS "B!:1 -:0.5 B:0.5 B:1 -:0.5 B:0.5  B!:1 -:0.5 B:0.5 B:0.5 B:0.5 B:1 "
#define FIGHT_FRAME "D:0.5 k.:0.5 T:0.5 k.:0.5 D:0.5 k.:0.5 T:0.5 T.:0.5 "
#define FIGHT_RATTLE "S!:0.25 S.:0.25 S.:0.25 S.:0.25 S:0.25 S.:0.25 S.:0.25 S.:0.25 "
#define FIGHT_RIFF1 "1,:0.5 1, 1 1, 2 1, 3 2 "
#define FIGHT_RIFF2 "1,:0.5 1, 1 1, 4 3 2 1 "
#define FIGHT_L1 "5':0.5 6' 5' 3' 2':1 3':0.5 2':0.5  1':1 2':0.5 3':0.5 5':1 3':1  5':0.5 6' 5' 3' 2':1 3':0.5 2':0.5  1':3 -:1 "
#define FIGHT_L2 "1'':1 2'':0.5 1'':0.5 7':1 6':1  5':1.5 6':0.5 5':1 3':1  1'':1 2'':0.5 1'':0.5 7':1 6':1  5':2 3':1 2':1 "
#define FIGHT_L3 "3'':0.5 2'' 1'' 7' 6':1 5':1  6':0.5 5' 3' 2' 3':2  3'':0.5 2'' 1'' 7' 6':1 5':1  5':1 3':1 2':1 1':1 "
#define FIGHT_HORN "1:3 2:0.5 1:0.5  5:3 4:1  3:2 2:1 1:1  1:3 -:1  1:3 2:0.5 1:0.5  5:2 6:1 5:1  4:1 3:1 2:1 1:1  1:4 "

static const PartDef FIGHT_PARTS[] = {
    { I_DRUM, X8(FIGHT_BASS), 0.24f, 0.0f, 0.12f, 1.0f, 0, 0 },
    { I_DRUM, X8(X2(FIGHT_FRAME)), 0.40f, -0.2f, 0.10f, 1.0f, 0, 0 },
    { I_DRUM, X8(X4(FIGHT_RATTLE)), 0.07f, 0.35f, 0.05f, 1.0f, 0, 0 },
    { I_DRUM, X8(X2("-:1 C:1 -:1 C:1 ")), 0.34f, 0.2f, 0.18f, 1.0f, 0, 0 },
    { I_PLUCK, X4(FIGHT_RIFF1 FIGHT_RIFF1 FIGHT_RIFF1 FIGHT_RIFF2), 0.55f, -0.3f, 0.12f, 1.1f, 0.55f, 0 },
    { I_HORN, FIGHT_HORN FIGHT_HORN, 0.15f, 0.0f, 0.2f, 2.2f, 900.0f, 0.04f },
    { I_PIPE, FIGHT_L1 FIGHT_L2 FIGHT_L3 FIGHT_L1, 0.22f, 0.25f, 0.2f, WT_REED, 0.12f, 0.03f },
    { I_PAD, "1,+5,+1:32", 0.34f, 0.0f, 0.2f, 1.5f, 3.0f, WT_HORN },
};

// --- Stars over the Euphrates: 50 beats a minute, five notes made of fifths, a slow pad, a soft harp.
#define NIGHT_H1 "1'':1.5 -:0.5 4'':1 3'':1.5 -:0.5 2'':2 -:1  1'':1 2'':1 3'':2 -:2 5':2 "
#define NIGHT_H2 "5'':2 4'':1 3'':1 2'':2 -:2 1'':1.5 5':0.5 4':2 -:4 "
#define NIGHT_H3 "3'':1 -:1 4'':1 3'':1 2'':2 1'':2  5':1.5 -:0.5 4':2 3':2 -:2 "
#define NIGHT_H4 "1'':2 2'':1 3'':1 4'':1.5 3'':0.5 2'':2 1'':3 -:1 5':2 -:2 "

static const PartDef NIGHT_PARTS[] = {
    { I_PAD, "1+4+1':16 3+1'+3':16 2+5+2':16 4+2'+4':16", 0.36f, 0.0f, 0.6f, 6.0f, 8.0f, WT_PAD },
    { I_PLUCK, NIGHT_H1 NIGHT_H2 NIGHT_H3 NIGHT_H4, 0.85f, -0.2f, 0.7f, 5.0f, 0.2f, 0 },
    { I_PLUCK, "-:20 5''':1 -:7 3''':1 -:7 4''':1 -:6 2''':1 -:8 5''':1 -:11 ", 0.60f, 0.5f, 0.8f, 6.0f, 0.5f, 0 },
    { I_PIPE, "-:32 5'':6 -:2 4'':4 -:4 3'':5 2'':3 -:8 ", 0.17f, 0.3f, 0.8f, WT_FLUTE, 0.04f, 0.5f },
};

static const TrackDef TRACKS[MOODS] = {
    { "Morning in Ur", 72.0f, 146.83f, SCALE_DORIAN, 7, DAY_PARTS, (int)(sizeof(DAY_PARTS) / sizeof(DAY_PARTS[0])) },
    { "Raiders at the Gate", 148.0f, 146.83f, SCALE_FIGHT, 7, FIGHT_PARTS, (int)(sizeof(FIGHT_PARTS) / sizeof(FIGHT_PARTS[0])) },
    { "Stars over the Euphrates", 50.0f, 98.0f, SCALE_NIGHT, 5, NIGHT_PARTS, (int)(sizeof(NIGHT_PARTS) / sizeof(NIGHT_PARTS[0])) },
};

// ---------------------------------------------------------------------------------------------
// Small helpers

static unsigned rngState = 2463534242u;

static float Rnd(void)   // -1..1
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (float)(int)rngState * (1.0f / 2147483648.0f);
}

static float Rnd01(void)
{
    return Rnd() * 0.5f + 0.5f;
}

typedef struct { float lp, bp; } Svf;

// A state variable filter: f = 2 sin(pi fc / SR), q = 1 / resonance.
static void SvfRun(Svf *s, float x, float f, float q, float *lp, float *bp, float *hp)
{
    float h = x - s->lp - q * s->bp;
    s->bp += f * h;
    s->lp += f * s->bp;
    if (lp) *lp = s->lp;
    if (bp) *bp = s->bp;
    if (hp) *hp = h;
}

static float SvfCoef(float fc)
{
    return 2.0f * sinf(PI * fc / (float)SR);
}

static float wave[WT_COUNT][WT_N + 1];

static void BuildTable(int k, const float *amps, int n)
{
    float peak = 0;
    for (int i = 0; i < WT_N; i++) {
        float v = 0, ph = 2.0f * PI * (float)i / (float)WT_N;
        for (int h = 0; h < n; h++) v += amps[h] * sinf(ph * (float)(h + 1));
        wave[k][i] = v;
        if (fabsf(v) > peak) peak = fabsf(v);
    }
    for (int i = 0; i < WT_N; i++) wave[k][i] /= peak;
    wave[k][WT_N] = wave[k][0];
}

static void BuildTables(void)
{
    // A flute: nearly a sine. A reed pipe: odd harmonics for a hollow, nasal tone. A horn: a rich buzz.
    static const float flute[4] = { 1.0f, 0.12f, 0.05f, 0.02f };
    static const float reed[9] = { 1.0f, 0.22f, 0.55f, 0.10f, 0.30f, 0.06f, 0.14f, 0.03f, 0.07f };
    static const float horn[16] = { 1.0f, 0.8f, 0.7f, 0.55f, 0.45f, 0.38f, 0.30f, 0.25f, 0.2f, 0.16f, 0.13f, 0.1f, 0.08f, 0.06f, 0.05f, 0.04f };
    static const float pad[3] = { 1.0f, 0.35f, 0.12f };
    BuildTable(WT_FLUTE, flute, 4);
    BuildTable(WT_REED, reed, 9);
    BuildTable(WT_HORN, horn, 16);
    BuildTable(WT_PAD, pad, 3);
}

static float Wt(int k, float phase)   // phase 0..1
{
    float x = phase * (float)WT_N;
    int i = (int)x;
    float fr = x - (float)i;
    return wave[k][i] + (wave[k][i + 1] - wave[k][i]) * fr;
}

static void Pan(float pan, float *l, float *r)
{
    float a = (pan * 0.5f + 0.5f) * (PI * 0.5f);
    *l = cosf(a);
    *r = sinf(a);
}

// ---------------------------------------------------------------------------------------------
// Voices

// A plucked string (the lyre): a delay line with a gentle low-pass in the loop, tuned with an all-pass.
#define PL_MAX 1100
typedef struct {
    bool on;
    int n, i, wait, life;
    float d[PL_MAX];
    float prev, s, g, ax, ay, a, amp, l, r, send;
} Pluck;

static void PluckStart(Pluck *v, float f, float amp, float ring, float bright, int wait, float pan, float send)
{
    float s = 0.5f - 0.3f * bright, total = (float)SR / f;
    int n = (int)(total - s - 0.5f);
    if (n < 4) n = 4;
    if (n > PL_MAX) n = PL_MAX;
    float dly = total - s - (float)n;
    float tmp[PL_MAX], lp = 0, k = 0.15f + 0.8f * bright, mean = 0, peak = 0;
    int off = (int)((float)n * 0.22f);
    if (off < 1) off = 1;

    v->n = n;
    v->i = 0;
    v->s = s;
    v->a = (1.0f - dly) / (1.0f + dly);
    v->prev = v->ax = v->ay = 0;
    // Plucked at about a fifth of its length: noise, softened, with the notch that position gives.
    for (int j = 0; j < n; j++) {
        lp += k * (Rnd() - lp);
        tmp[j] = lp;
    }
    for (int j = 0; j < n; j++) v->d[j] = tmp[j] - tmp[(j + off) % n];
    for (int j = 0; j < n; j++) mean += v->d[j];
    mean /= (float)n;
    for (int j = 0; j < n; j++) {
        v->d[j] -= mean;
        if (fabsf(v->d[j]) > peak) peak = fabsf(v->d[j]);
    }
    for (int j = 0; j < n; j++) v->d[j] /= peak > 0 ? peak : 1.0f;
    v->g = powf(10.0f, -3.0f * ((float)n / (float)SR) / ring);
    v->life = (int)(ring * 1.3f * (float)SR);
    v->wait = wait;
    v->amp = amp;
    Pan(pan, &v->l, &v->r);
    v->send = send;
    v->on = true;
}

static float PluckTick(Pluck *v)
{
    if (v->wait > 0) {
        v->wait--;
        return 0;
    }
    float x = v->d[v->i], avg = (1.0f - v->s) * x + v->s * v->prev, y;
    v->prev = x;
    y = v->a * avg + v->ax - v->a * v->ay;
    v->ax = avg;
    v->ay = y;
    v->d[v->i] = y * v->g;
    if (++v->i >= v->n) v->i = 0;
    if (--v->life <= 0) v->on = false;
    return x * v->amp;
}

// A reed pipe or flute: a wavetable with vibrato, a little breath noise and glides between notes.
typedef struct {
    bool on;
    int table, hold;     // hold: samples until the breath is let go
    float ph, f, ft, glide, env, atk, rel, vibT, amp, breath, l, r, send, nlp, nlp2;
    bool released;
} Pipe;

static float PipeTick(Pipe *v)
{
    float vib = 1.0f + 0.0045f * sinf(2.0f * PI * 5.3f * v->vibT) * fminf(1.0f, v->vibT * 1.2f);
    float n, out;

    v->vibT += 1.0f / (float)SR;
    v->f += (v->ft - v->f) * v->glide;
    v->ph += v->f * vib / (float)SR;
    if (v->ph >= 1.0f) v->ph -= 1.0f;
    if (!v->released) {
        if (--v->hold <= 0) v->released = true;
        else if (v->env < 1.0f) {
            v->env += v->atk;
            if (v->env > 1.0f) v->env = 1.0f;
        }
    } else {
        v->env -= v->rel;
        if (v->env <= 0) {
            v->on = false;
            return 0;
        }
    }
    n = Rnd();
    v->nlp += 0.30f * (n - v->nlp);
    v->nlp2 += 0.02f * (v->nlp - v->nlp2);
    out = Wt(v->table, v->ph) * (0.85f + 0.15f * v->env) + (v->nlp - v->nlp2) * v->breath * 1.4f;
    return out * v->env * v->amp;
}

// A drone or pad: two slightly apart oscillators with a slow swell.
typedef struct {
    bool on, released;
    int table, hold;
    float ph1, ph2, f, env, atk, rel, amp, lfo, l, r, send;
} Pad;

static float PadTick(Pad *v)
{
    float out;

    v->ph1 += v->f * 0.9992f / (float)SR;
    v->ph2 += v->f * 1.0009f / (float)SR;
    if (v->ph1 >= 1.0f) v->ph1 -= 1.0f;
    if (v->ph2 >= 1.0f) v->ph2 -= 1.0f;
    v->lfo += 0.13f / (float)SR;
    if (!v->released) {
        if (--v->hold <= 0) v->released = true;
        else if (v->env < 1.0f) {
            v->env += v->atk;
            if (v->env > 1.0f) v->env = 1.0f;
        }
    } else {
        v->env -= v->rel;
        if (v->env <= 0) {
            v->on = false;
            return 0;
        }
    }
    out = (Wt(v->table, v->ph1) + Wt(v->table, v->ph2)) * 0.5f * (0.85f + 0.15f * sinf(2.0f * PI * v->lfo));
    return out * v->env * v->amp;
}

// An animal horn: a buzzing wavetable through a low-pass that opens at the start of the note, scooped
// up to pitch and driven a little.
typedef struct {
    bool on, released;
    int hold;
    float ph, f, t, env, atk, rel, amp, drive, bright, l, r, send;
    Svf filter;
} Horn;

static float HornTick(Horn *v)
{
    float scoop = powf(2.0f, -0.9f / 12.0f * expf(-v->t * 14.0f)), vib = 1.0f + 0.004f * sinf(2.0f * PI * 5.0f * v->t) * fminf(1.0f, v->t);
    float cut = v->bright * (0.35f + 0.65f * expf(-v->t * 2.5f)) + 200.0f, lp, x;

    v->t += 1.0f / (float)SR;
    v->ph += v->f * scoop * vib / (float)SR;
    if (v->ph >= 1.0f) v->ph -= 1.0f;
    if (!v->released) {
        if (--v->hold <= 0) v->released = true;
        else if (v->env < 1.0f) {
            v->env += v->atk;
            if (v->env > 1.0f) v->env = 1.0f;
        }
    } else {
        v->env -= v->rel;
        if (v->env <= 0) {
            v->on = false;
            return 0;
        }
    }
    SvfRun(&v->filter, Wt(WT_HORN, v->ph), SvfCoef(cut), 0.7f, &lp, NULL, NULL);
    x = tanhf(lp * v->drive) / tanhf(v->drive);
    return x * v->env * v->amp;
}

// Drums: dum (centre of a frame drum), tek (rim), k (a ghost tek), B (a big kettle drum), S (rattle), C (clap).
typedef struct {
    bool on;
    char kind;
    float t, amp, ph, tune, l, r, send, nlp;
    Svf f1;
} Drum;

static float DrumTick(Drum *v)
{
    float dt = 1.0f / (float)SR, t = v->t, n = Rnd(), out = 0, bp, hp, lp;

    v->t += dt;
    switch (v->kind) {
    case 'D': {
        float fr = (78.0f + 90.0f * expf(-t / 0.028f)) * v->tune;
        v->ph += fr * dt;
        v->nlp += 0.25f * (n - v->nlp);
        out = sinf(2.0f * PI * v->ph) * expf(-t / 0.20f) + v->nlp * 0.30f * expf(-t / 0.012f);
        if (t > 1.2f) v->on = false;
        break;
    }
    case 'B': {
        float fr = (52.0f + 46.0f * expf(-t / 0.07f)) * v->tune;
        v->ph += fr * dt;
        v->nlp += 0.12f * (n - v->nlp);
        out = (sinf(2.0f * PI * v->ph) * expf(-t / 0.45f) + 0.25f * sinf(4.0f * PI * v->ph) * expf(-t / 0.10f)) * 1.1f + v->nlp * 0.2f * expf(-t / 0.015f);
        if (t > 1.8f) v->on = false;
        break;
    }
    case 'T':
    case 'k': {
        SvfRun(&v->f1, n, SvfCoef(2300.0f), 0.8f, NULL, &bp, NULL);
        v->ph += 520.0f * v->tune * dt;
        out = bp * 1.6f * expf(-t / 0.05f) + sinf(2.0f * PI * v->ph) * 0.35f * expf(-t / 0.035f);
        if (t > 0.4f) v->on = false;
        break;
    }
    case 'S': {
        SvfRun(&v->f1, n, SvfCoef(6200.0f), 1.2f, NULL, NULL, &hp);
        out = hp * (expf(-t / 0.05f) + (t > 0.022f ? 0.6f * expf(-(t - 0.022f) / 0.06f) : 0.0f)) * 0.9f;
        if (t > 0.5f) v->on = false;
        break;
    }
    case 'C': {
        float e = expf(-t / 0.02f) + (t > 0.011f ? 0.8f * expf(-(t - 0.011f) / 0.02f) : 0.0f) + (t > 0.023f ? 1.1f * expf(-(t - 0.023f) / 0.05f) : 0.0f);
        SvfRun(&v->f1, n, SvfCoef(1500.0f), 1.0f, &lp, &bp, NULL);
        out = bp * 2.2f * e;
        (void)lp;
        if (t > 0.5f) v->on = false;
        break;
    }
    default:
        v->on = false;
    }
    return out * v->amp;
}

// ---------------------------------------------------------------------------------------------
// The reverb: four combs and two all-passes a side, which is enough to make the instruments sound like
// they are in a room of mud brick instead of a box.
static const int COMB_L[4] = { 1557, 1617, 1491, 1422 };
static const int COMB_R[4] = { 1580, 1640, 1514, 1445 };
static const int ALL_L[2] = { 556, 441 };
static const int ALL_R[2] = { 579, 464 };

typedef struct {
    float comb[2][4][1700], cl[2][4];
    int ci[2][4];
    float all[2][2][600];
    int ai[2][2];
} Reverb;

static Reverb reverb;

static void ReverbTick(float in, float *l, float *r)
{
    const float fb = 0.83f, damp = 0.32f;
    float out[2] = { 0, 0 };

    for (int ch = 0; ch < 2; ch++) {
        const int *len = ch ? COMB_R : COMB_L, *alen = ch ? ALL_R : ALL_L;
        for (int k = 0; k < 4; k++) {
            float *d = reverb.comb[ch][k], x = d[reverb.ci[ch][k]];
            reverb.cl[ch][k] += (x - reverb.cl[ch][k]) * (1.0f - damp);
            d[reverb.ci[ch][k]] = in * 0.25f + reverb.cl[ch][k] * fb;
            if (++reverb.ci[ch][k] >= len[k]) reverb.ci[ch][k] = 0;
            out[ch] += x;
        }
        for (int k = 0; k < 2; k++) {
            float *d = reverb.all[ch][k], x = d[reverb.ai[ch][k]], y = -out[ch] + x;
            d[reverb.ai[ch][k]] = out[ch] + x * 0.5f;
            if (++reverb.ai[ch][k] >= alen[k]) reverb.ai[ch][k] = 0;
            out[ch] = y;
        }
    }
    *l = out[0];
    *r = out[1];
}

// ---------------------------------------------------------------------------------------------
// Reading a score

static Note notePool[MAX_NOTES];
static int notesUsed;
static Part parts[MOODS][MAX_PARTS];

static bool IsDrum(char c)
{
    return c == 'D' || c == 'T' || c == 'k' || c == 'B' || c == 'S' || c == 'C';
}

static float DegreeFreq(const float *scale, int ns, float tonic, int deg, int oct)
{
    int idx = deg - 1;
    return tonic * scale[idx % ns] * powf(2.0f, (float)(oct + idx / ns));
}

// Notes are read left to right; each one starts where the last one ended.
static int ParseScore(const char *s, const float *scale, int ns, float tonic, Note *out, int max, float *length)
{
    int count = 0;
    float at = 0, lastDur = 1.0f;
    const char *p = s;

    while (*p) {
        while (*p == ' ' || *p == '|' || *p == '\n' || *p == '\t') p++;
        if (!*p) break;
        if (count >= max) break;
        Note *n = &out[count];
        int deg[4], oct[4], nd = -1, graceDir = 0;
        float vel = 0.75f, dur = lastDur;

        memset(n, 0, sizeof(*n));
        n->kind = 'n';
        while (*p && *p != ' ' && *p != '|' && *p != '\n' && *p != '\t') {
            char c = *p++;
            if (c == '^') graceDir = 1;
            else if (c == '_') graceDir = -1;
            else if (c == '~') n->slide = true;
            else if (c == '-') n->kind = 'r';
            else if (IsDrum(c)) n->kind = c;
            else if (c >= '1' && c <= '9') {
                if (nd < 3) nd++;
                deg[nd] = c - '0';
                oct[nd] = 0;
            } else if (c == '\'' && nd >= 0) oct[nd]++;
            else if (c == ',' && nd >= 0) oct[nd]--;
            else if (c == '!') vel = 1.0f;
            else if (c == '.') vel = 0.5f;
            else if (c == ':') {
                char *end;
                dur = strtof(p, &end);
                p = end;
            }
        }
        n->beat = at;
        n->dur = dur;
        n->vel = vel;
        lastDur = dur;
        if (n->kind == 'n') {
            n->nf = (unsigned char)(nd + 1);
            for (int k = 0; k <= nd; k++) n->freq[k] = DegreeFreq(scale, ns, tonic, deg[k], oct[k]);
            if (graceDir && nd >= 0) n->grace = DegreeFreq(scale, ns, tonic, deg[0] + graceDir, oct[0]);
            if (nd < 0) n->kind = 'r';
        }
        at += dur;
        count++;
    }
    *length = at;
    return count;
}

// ---------------------------------------------------------------------------------------------
// Playing a track

#define MAX_PLUCK 18
#define MAX_PIPE 6
#define MAX_PAD 8
#define MAX_HORN 4
#define MAX_DRUM 16

typedef struct {
    float beat, gain, target;
    bool awake;
    Pluck pluck[MAX_PLUCK];
    Pipe pipe[MAX_PIPE];
    Pad pad[MAX_PAD];
    Horn horn[MAX_HORN];
    Drum drum[MAX_DRUM];
} Track;

static Track tracks[MOODS];
static bool ready;

static float Seconds(const TrackDef *t, float beats)
{
    return beats * 60.0f / t->bpm;
}

static void PlayNote(Track *tr, const TrackDef *td, Part *pt, const Note *n)
{
    const PartDef *d = pt->def;
    float vel = n->vel * (0.9f + 0.2f * Rnd01()), secs = Seconds(td, n->dur), l, r;

    Pan(d->pan, &l, &r);
    switch (d->inst) {
    case I_PLUCK:
        for (int k = 0; k < n->nf; k++) {
            Pluck *v = NULL;
            for (int i = 0; i < MAX_PLUCK && !v; i++)
                if (!tr->pluck[i].on) v = &tr->pluck[i];
            if (!v) v = &tr->pluck[(int)(Rnd01() * (MAX_PLUCK - 1))];
            PluckStart(v, n->freq[k], d->gain * vel * 0.9f, d->p1, d->p2, (int)((float)k * 0.011f * (float)SR), d->pan, d->send);
        }
        break;
    case I_PIPE: {
        Pipe *v = NULL;
        for (int i = 0; i < MAX_PIPE && !v; i++)
            if (!tr->pipe[i].on) v = &tr->pipe[i];
        if (!v) v = &tr->pipe[0];
        float from = pt->lastFreq > 0 && n->slide ? pt->lastFreq : n->freq[0];
        if (n->grace > 0) {
            // Play the grace note first, then glide quickly to the main note.
            from = n->grace;
        }
        memset(v, 0, sizeof(*v));
        v->on = true;
        v->table = (int)d->p1;
        v->f = from;
        v->ft = n->freq[0];
        v->glide = n->slide || n->grace > 0 ? 1.0f - expf(-1.0f / (0.06f * (float)SR)) : 1.0f;
        v->atk = 1.0f / (fmaxf(d->p3, 0.005f) * (float)SR);
        v->rel = 1.0f / (0.12f * (float)SR);
        v->hold = (int)(fmaxf(secs - 0.05f, 0.1f) * (float)SR);
        v->amp = d->gain * (0.55f + 0.45f * vel);
        v->breath = d->p2;
        v->l = l;
        v->r = r;
        v->send = d->send;
        pt->lastFreq = n->freq[0];
        break;
    }
    case I_PAD:
        for (int k = 0; k < n->nf; k++) {
            Pad *v = NULL;
            for (int i = 0; i < MAX_PAD && !v; i++)
                if (!tr->pad[i].on) v = &tr->pad[i];
            if (!v) v = &tr->pad[0];
            memset(v, 0, sizeof(*v));
            v->on = true;
            v->table = (int)d->p3;
            v->f = n->freq[k];
            v->ph1 = Rnd01();
            v->ph2 = Rnd01();
            v->atk = 1.0f / (d->p1 * (float)SR);
            v->rel = 1.0f / (d->p2 * (float)SR);
            v->hold = (int)(secs * (float)SR);
            v->amp = d->gain * 0.6f / (float)n->nf * 1.5f;
            v->l = l;
            v->r = r;
            v->send = d->send;
        }
        break;
    case I_HORN: {
        Horn *v = NULL;
        for (int i = 0; i < MAX_HORN && !v; i++)
            if (!tr->horn[i].on) v = &tr->horn[i];
        if (!v) v = &tr->horn[0];
        memset(v, 0, sizeof(*v));
        v->on = true;
        v->f = n->freq[0];
        v->atk = 1.0f / (d->p3 * (float)SR);
        v->rel = 1.0f / (0.25f * (float)SR);
        v->hold = (int)(fmaxf(secs - 0.04f, 0.1f) * (float)SR);
        v->amp = d->gain * (0.6f + 0.4f * vel);
        v->drive = d->p1;
        v->bright = d->p2;
        v->l = l;
        v->r = r;
        v->send = d->send;
        break;
    }
    case I_DRUM: {
        Drum *v = NULL;
        for (int i = 0; i < MAX_DRUM && !v; i++)
            if (!tr->drum[i].on) v = &tr->drum[i];
        if (!v) v = &tr->drum[(int)(Rnd01() * (MAX_DRUM - 1))];
        memset(v, 0, sizeof(*v));
        v->on = true;
        v->kind = n->kind;
        v->amp = d->gain * vel * (n->kind == 'k' ? 0.5f : 1.0f);
        v->tune = d->p1;
        v->l = l;
        v->r = r;
        v->send = d->send;
        break;
    }
    }
}

static void TrackReset(int m)
{
    Track *tr = &tracks[m];
    memset(tr->pluck, 0, sizeof(tr->pluck));
    memset(tr->pipe, 0, sizeof(tr->pipe));
    memset(tr->pad, 0, sizeof(tr->pad));
    memset(tr->horn, 0, sizeof(tr->horn));
    memset(tr->drum, 0, sizeof(tr->drum));
    tr->beat = 0;
    for (int i = 0; i < TRACKS[m].partCount; i++) {
        parts[m][i].next = 0;
        parts[m][i].base = 0;
        parts[m][i].lastFreq = 0;
    }
}

// Renders one block of a track into the dry and reverb buses (already scaled by 'gain').
static void TrackBlock(int m, float gain, float *dl, float *dr, float *snd)
{
    Track *tr = &tracks[m];
    const TrackDef *td = &TRACKS[m];
    float bpb = td->bpm / 60.0f * (float)BLOCK / (float)SR;

    tr->beat += bpb;
    for (int i = 0; i < td->partCount; i++) {
        Part *pt = &parts[m][i];
        const bool human = pt->def->inst == I_PLUCK || pt->def->inst == I_PIPE;
        if (pt->count == 0 || pt->length <= 0) continue;
        for (;;) {
            if (pt->next >= pt->count) {
                if (tr->beat < pt->base + pt->length) break;
                pt->base += pt->length;
                pt->next = 0;
            }
            Note *n = &pt->notes[pt->next];
            if (pt->base + n->beat + n->jit > tr->beat) break;
            pt->next++;
            n->jit = human ? Rnd() * 0.012f * td->bpm / 60.0f : 0;
            if (n->kind != 'r') PlayNote(tr, td, pt, n);
        }
    }

    for (int i = 0; i < MAX_PLUCK; i++) {
        Pluck *v = &tr->pluck[i];
        if (!v->on) continue;
        for (int j = 0; j < BLOCK; j++) {
            float x = PluckTick(v) * gain;
            dl[j] += x * v->l;
            dr[j] += x * v->r;
            snd[j] += x * v->send;
        }
    }
    for (int i = 0; i < MAX_PIPE; i++) {
        Pipe *v = &tr->pipe[i];
        if (!v->on) continue;
        for (int j = 0; j < BLOCK; j++) {
            float x = PipeTick(v) * gain;
            dl[j] += x * v->l;
            dr[j] += x * v->r;
            snd[j] += x * v->send;
        }
    }
    for (int i = 0; i < MAX_PAD; i++) {
        Pad *v = &tr->pad[i];
        if (!v->on) continue;
        for (int j = 0; j < BLOCK; j++) {
            float x = PadTick(v) * gain;
            dl[j] += x * v->l;
            dr[j] += x * v->r;
            snd[j] += x * v->send;
        }
    }
    for (int i = 0; i < MAX_HORN; i++) {
        Horn *v = &tr->horn[i];
        if (!v->on) continue;
        for (int j = 0; j < BLOCK; j++) {
            float x = HornTick(v) * gain;
            dl[j] += x * v->l;
            dr[j] += x * v->r;
            snd[j] += x * v->send;
        }
    }
    for (int i = 0; i < MAX_DRUM; i++) {
        Drum *v = &tr->drum[i];
        if (!v->on) continue;
        for (int j = 0; j < BLOCK; j++) {
            float x = DrumTick(v) * gain;
            dl[j] += x * v->l;
            dr[j] += x * v->r;
            snd[j] += x * v->send;
        }
    }
}

static float level = 0.7f;       // chosen volume (0 is off), applied to the stream
static float volume;             // the volume the stream has now, easing toward 'level'
static float dcL, dcR, dcXL, dcXR, toneL, toneR;   // DC blocker and tone filter state
#define MIX_GAIN 0.96f           // the synthesizer's own level: the loudest setting leaves room above the peaks
static float fadeUp[MOODS] = { 1.5f, 0.6f, 2.5f }, fadeDown[MOODS] = { 3.0f, 4.0f, 3.0f };

// Mixes 'frames' frames (a multiple of BLOCK) of interleaved stereo float into 'out'.
static void MusicMix(float *out, int frames)
{
    for (int b = 0; b < frames; b += BLOCK) {
        float dl[BLOCK] = { 0 }, dr[BLOCK] = { 0 }, snd[BLOCK] = { 0 };
        float blockT = (float)BLOCK / (float)SR;

        for (int m = 0; m < MOODS; m++) {
            Track *tr = &tracks[m];
            float rate = tr->target > tr->gain ? 1.0f / fadeUp[m] : 1.0f / fadeDown[m];
            if (tr->gain < tr->target) tr->gain = fminf(tr->target, tr->gain + rate * blockT);
            else if (tr->gain > tr->target) tr->gain = fmaxf(tr->target, tr->gain - rate * blockT);
            if (tr->gain <= 0.0005f && tr->target <= 0) {
                tr->awake = false;
                continue;
            }
            if (!tr->awake) {
                tr->awake = true;
                if (m == MOOD_COMBAT) TrackReset(m);   // the fight always begins with its drums
            }
            // A square-root curve keeps the loudness even through a crossfade.
            TrackBlock(m, sqrtf(tr->gain), dl, dr, snd);
        }
        for (int j = 0; j < BLOCK; j++) {
            float wl, wr, l, r;
            ReverbTick(snd[j], &wl, &wr);
            l = (dl[j] + wl * 0.55f) * MIX_GAIN;
            r = (dr[j] + wr * 0.55f) * MIX_GAIN;
            // Take out any steady offset (horns and drive make an uneven wave), then round off the peaks.
            dcL = l - dcXL + 0.9990f * dcL;
            dcR = r - dcXR + 0.9990f * dcR;
            dcXL = l;
            dcXR = r;
            // A soft roll-off above about 7 kHz: warmer, and no fizz from the noisy drums.
            toneL += 0.65f * (dcL - toneL);
            toneR += 0.65f * (dcR - toneR);
            out[2 * (b + j)] = tanhf(toneL);
            out[2 * (b + j) + 1] = tanhf(toneR);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Choosing the mood

static AudioStream stream;
static bool streaming, started;
static float combatT, mixBuf[STREAM_FRAMES * 2];
static int staged;   // frames of mixBuf already made, waiting for the stream to want them
static int mood = MOOD_DAY;
static bool nightNow;
static float lastHealth;

int MusicMood(void)
{
    return mood;
}

const char *MusicTitle(int m)
{
    return m >= 0 && m < MOODS ? TRACKS[m].name : "";
}

// Is something out to hurt this player? Anything hostile to them nearby: a pack or a boar on their
// trail, guards, villagers, raiders and caravan guards that have turned on them. In a game with PvP,
// another player nearby who is fighting (or anyone in a battle royale) counts too. The facts used are the
// ones every player's game holds, so guests hear the same.
static bool Threatened(void)
{
    Vector3 me = G.player.pos;

    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (a->active && a->state == ST_HUNT && a->target == G.self && XZDist(a->pos, me) < 50.0f) return true;
    }
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (!v->vit.dead && v->vit.hostile > 0 && v->vit.target == G.self && XZDist(VillagerPos(v), me) < 50.0f) return true;
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        const Raider *r = &G.raiders[i];
        if (!r->vit.dead && r->vit.hostile > 0 && r->vit.target == G.self && XZDist(r->pos, me) < 50.0f) return true;
    }
    for (int c = 0; c < MAX_CARAVANS; c++) {
        for (int k = 0; k <= CARAVAN_GUARDS; k++) {
            Vector3 pos;
            float face;
            const Vitals *v = &G.caravans[c].vit[k];
            if (!v->dead && v->hostile > 0 && v->target == G.self && CaravanMember(c, k, &pos, &face) && XZDist(pos, me) < 50.0f) return true;
        }
    }
    if (G.pvp) {
        for (int s = 0; s < MAX_PLAYERS; s++) {
            const Peer *p = &G.peers[s];
            float d;
            if (s == G.self || !PlayerAwake(s)) continue;
            d = XZDist(p->pos, me);
            if ((G.combat && d < 60.0f) || (d < 25.0f && (p->swingT > 0 || (p->flags & PF_AIM)))) return true;
        }
    }
    return false;
}

static void ChooseMood(float dt)
{
    // Darkness has to settle before the night tune starts or ends, so dusk does not flicker between them.
    if (G.night > 0.55f) nightNow = true;
    else if (G.night < 0.35f) nightNow = false;

    if (G.health < lastHealth - 0.01f && G.hurtT > 0.2f) combatT = 12.0f;
    lastHealth = G.health;
    if (Threatened()) combatT = fmaxf(combatT, 12.0f);
    // Hide and seek: the fight's drums would tell the seeker a hider is near.
    if (G.sleeping || PartyGame() == PG_HIDE) combatT = 0;
    combatT = fmaxf(0, combatT - dt);
    mood = combatT > 0 ? MOOD_COMBAT : nightNow ? MOOD_NIGHT : MOOD_DAY;
}

// ---------------------------------------------------------------------------------------------
// The game's side

static const float LEVELS[4] = { 0.0f, 0.35f, 0.7f, 1.0f };

static void LoadLevel(void)
{
    FILE *f = fopen(DataPath(MUSIC_FILE), "r");
    int v = 0;
    if (!f) return;
    if (fscanf(f, "%d", &v) == 1 && v >= 0 && v <= 3) level = LEVELS[v];
    fclose(f);
}

int MusicLevel(void)
{
    return level <= 0.01f ? 0 : level < 0.5f ? 1 : level < 0.85f ? 2 : 3;
}

void MusicSetLevel(int l)
{
    FILE *f;

    level = LEVELS[l < 0 ? 0 : l > 3 ? 3 : l];
    f = fopen(DataPath(MUSIC_FILE), "w");
    if (f) {
        fprintf(f, "%d\n", MusicLevel());
        fclose(f);
        SyncStorage();
    }
}

const char *MusicLevelName(void)
{
    static const char *const names[4] = { "off", "low", "medium", "high" };
    return names[MusicLevel()];
}

static void Prepare(void)
{
    BuildTables();
    for (int m = 0; m < MOODS; m++) {
        const TrackDef *td = &TRACKS[m];
        for (int i = 0; i < td->partCount; i++) {
            Part *pt = &parts[m][i];
            pt->def = &td->parts[i];
            pt->notes = &notePool[notesUsed];
            pt->count = ParseScore(pt->def->score, td->scale, td->scaleLen, td->tonic, pt->notes, MAX_NOTES - notesUsed, &pt->length);
            notesUsed += pt->count;
        }
        tracks[m].gain = tracks[m].target = 0;
        TrackReset(m);
    }
    ready = true;
}

// Opens the sound device. If there is none (a server, a headless test), the game simply has no music.
void MusicInit(void)
{
    LoadLevel();
    Prepare();
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    SetAudioStreamBufferSizeDefault(STREAM_FRAMES);
    stream = LoadAudioStream(SR, 32, 2);
    streaming = true;
}

// ---- The music of places ----
// Recorded tracks (CC0, in assets/music/, made by tools/assets/make_music.sh): the city, a village, the
// open land and the ziggurat's halls. In the game they play instead of the made tunes, faded across as
// the player goes from place to place; a fight brings in the made fight tune instead.
enum { PLACE_CITY, PLACE_VILLAGE, PLACE_WILD, PLACE_TEMPLE, PLACES };
static const char *const PLACE_FILES[PLACES] = { "city.ogg", "village.ogg", "wild.ogg", "temple.ogg" };
static Music placeMusic[PLACES];
static float placeGain[PLACES];
static bool placesTried, placeStarted[PLACES];
static int placeNow = -1;

#ifdef SUMER_TEST
// For tests: the made tunes' gains (day, fight, night) and the places' (city, village, open land, ziggurat).
void MusicGains(float *out)
{
    for (int m = 0; m < MOODS; m++) out[m] = tracks[m].gain;
    for (int k = 0; k < PLACES; k++) out[MOODS + k] = IsMusicStreamPlaying(placeMusic[k]) ? placeGain[k] : -placeGain[k];
}
#endif

// Which place's track is being heard (0 city, 1 village, 2 open land, 3 ziggurat), or -1: for tests.
int MusicPlace(void)
{
    return placeNow;
}

static int PlaceNow(void)
{
    Vector3 p = G.player.pos;
    if (InTempleInterior(p)) return PLACE_TEMPLE;
    if (InsideWalls(p.x, p.z, 0.0f)) return PLACE_CITY;
    if (VillageAt(p) >= 0) return PLACE_VILLAGE;
    // The open land's tune is a tense one, for dusk to dawn; by day the made main theme plays out there.
    return G.hour >= 17.0f || G.hour < 8.0f ? PLACE_WILD : -1;
}

static void LoadPlaces(void)
{
    placesTried = true;
    // A second of sound in each buffer, so a slow frame (or a browser tab drawing a few frames a second)
    // does not run them dry.
    SetAudioStreamBufferSizeDefault(44100);
    for (int k = 0; k < PLACES; k++) {
        const char *path = TextFormat("assets/music/%s", PLACE_FILES[k]);
        if (!FileExists(path)) path = TextFormat("%s../../assets/music/%s", GetApplicationDirectory(), PLACE_FILES[k]);
        placeMusic[k] = LoadMusicStream(path);
        placeMusic[k].looping = true;
    }
    SetAudioStreamBufferSizeDefault(STREAM_FRAMES);
}

// Fades this place's track in and the others out; true while a place's track is what should be heard.
static bool UpdatePlaces(float dt, bool inGame)
{
    if (!placesTried) LoadPlaces();
    int want = inGame && mood != MOOD_COMBAT ? PlaceNow() : -1;
    if (want >= 0 && placeMusic[want].frameCount == 0) want = -1;   // a missing file: the made tunes play on
    placeNow = want;
    for (int k = 0; k < PLACES; k++) {
        if (placeMusic[k].frameCount == 0) continue;
        float target = k == want ? 1.0f : 0.0f;
        placeGain[k] += (target - placeGain[k]) * fminf(1.0f, dt * 0.8f);
        if (target == 0 && placeGain[k] < 0.003f) placeGain[k] = 0;
        if (placeGain[k] == 0 || volume < 0.001f) {   // faded out, or the music turned off: not playing at all
            placeGain[k] = 0;
            if (IsMusicStreamPlaying(placeMusic[k])) PauseMusicStream(placeMusic[k]);
            continue;
        }
        if (!placeStarted[k]) PlayMusicStream(placeMusic[k]);
        else if (!IsMusicStreamPlaying(placeMusic[k])) ResumeMusicStream(placeMusic[k]);
        placeStarted[k] = true;
        SetMusicVolume(placeMusic[k], placeGain[k] * volume);
        UpdateMusicStream(placeMusic[k]);
    }
    return want >= 0;
}

void MusicClose(void)
{
    if (!streaming) return;
    for (int k = 0; k < PLACES; k++)
        if (placeMusic[k].frameCount) UnloadMusicStream(placeMusic[k]);
    UnloadAudioStream(stream);
    CloseAudioDevice();
    streaming = false;
}

// Once per frame: picks the mood and keeps the stream fed. 'inGame' is false on the title screen,
// which plays the morning tune. The next buffer is made a slice at a time, a few milliseconds a frame, and
// only when the stream runs dry does a frame make it all at once, so a slow frame costs no gap in the sound.
void MusicUpdate(float dt, bool inGame)
{
    if (!ready) return;
    if (inGame) {
        ChooseMood(dt);
    } else {
        // Back at the title the fight is over: a game entered afterwards starts in peace, not with the old
        // fight tune still running under the title's and the place's.
        mood = MOOD_DAY;
        combatT = 0;
        lastHealth = 0;
    }
    for (int m = 0; m < MOODS; m++) tracks[m].target = m == mood ? 1.0f : 0.0f;
    if (!streaming) return;
    volume += (level - volume) * fminf(1.0f, dt * 8.0f);
    if (fabsf(level - volume) < 0.002f) volume = level;
    SetAudioStreamVolume(stream, volume);
    if (UpdatePlaces(dt, inGame))
        for (int m = 0; m < MOODS; m++) tracks[m].target = 0.0f;
    if (!started) {
        started = true;
        PlayAudioStream(stream);
    }
    for (int round = 0; round < 2; round++) {
        bool want = IsAudioStreamProcessed(stream);
        int slices = want ? STREAM_FRAMES / SLICE : 1;
        // With the music off and faded out, or a place's track playing and the made tunes faded, there is
        // nothing to make: silence, and no time spent on it.
        bool idle = true;
        for (int m = 0; m < MOODS; m++) idle = idle && tracks[m].target <= 0.0f && tracks[m].gain <= 0.0f;
        bool silent = (level <= 0 && volume <= 0) || idle;
        while (staged < STREAM_FRAMES && slices-- > 0) {
            if (silent) memset(mixBuf + 2 * staged, 0, (size_t)SLICE * 2 * sizeof(float));
            else MusicMix(mixBuf + 2 * staged, SLICE);
            staged += SLICE;
        }
        if (!want || staged < STREAM_FRAMES) break;
        UpdateAudioStream(stream, mixBuf, STREAM_FRAMES);
        staged = 0;
    }
}

// For tests: renders 'frames' frames (a multiple of 32) of one track alone, from its beginning.
void MusicRender(int m, float *out, int frames)
{
    if (!ready) Prepare();
    for (int i = 0; i < MOODS; i++) {
        tracks[i].gain = tracks[i].target = i == m ? 1.0f : 0.0f;
        tracks[i].awake = false;
        TrackReset(i);
    }
    memset(&reverb, 0, sizeof(reverb));
    dcL = dcR = dcXL = dcXR = toneL = toneR = 0;
    MusicMix(out, frames);
}

// For tests: how many beats a part lasts (0 when there is no such part).
float MusicPartBeats(int m, int part)
{
    if (!ready) Prepare();
    return part < TRACKS[m].partCount ? parts[m][part].length : 0;
}
