#include "game.h"
#include <stdio.h>
#include <string.h>

#define LANG_FILE  "sumer.lang"
#define HASH_SIZE  2048

static int language = LANG_EN;
static short table[HASH_SIZE];
static bool tableReady;

static unsigned Hash(const char *s)
{
    unsigned h = 2166136261u;
    while (*s) h = (h ^ (unsigned char)*s++) * 16777619u;
    return h;
}

static void BuildTable(void)
{
    for (int i = 0; i < TR_PHRASE_COUNT; i++) {
        unsigned h = Hash(TR_PHRASES[i].en) & (HASH_SIZE - 1);
        while (table[h]) h = (h + 1) & (HASH_SIZE - 1);
        table[h] = (short)(i + 1);
    }
    tableReady = true;
}

const char *L(const char *en)
{
    if (language == LANG_EN || !en || !en[0]) return en;
    // Words meant for another player's screen go out in English; their game translates them.
    if (G.role == NET_HOST && G.actor != G.self) return en;
    if (!tableReady) BuildTable();
    for (unsigned h = Hash(en) & (HASH_SIZE - 1); table[h]; h = (h + 1) & (HASH_SIZE - 1))
        if (strcmp(TR_PHRASES[table[h] - 1].en, en) == 0) return TR_PHRASES[table[h] - 1].tr;
    return en;
}

bool HasTranslation(const char *en)
{
    if (!tableReady) BuildTable();
    for (unsigned h = Hash(en) & (HASH_SIZE - 1); table[h]; h = (h + 1) & (HASH_SIZE - 1))
        if (strcmp(TR_PHRASES[table[h] - 1].en, en) == 0) return true;
    return false;
}

int Language(void)
{
    return language;
}

void SetLanguage(int lang)
{
    language = lang == LANG_TR ? LANG_TR : LANG_EN;
}

void SaveLanguage(void)
{
    FILE *f = fopen(DataPath(LANG_FILE), "w");
    if (!f) return;
    fputs(language == LANG_TR ? "tr\n" : "en\n", f);
    fclose(f);
    SyncStorage();
}

void LoadLanguage(void)
{
    char buf[8] = "";
    FILE *f = fopen(DataPath(LANG_FILE), "r");
    if (!f) return;
    if (fgets(buf, sizeof(buf), f)) SetLanguage(strncmp(buf, "tr", 2) == 0 ? LANG_TR : LANG_EN);
    fclose(f);
}

const char *ItemName(int item)
{
    return L(ITEMS[item].name);
}

// Text is set in two CC0 typefaces by GGBotNet (assets/fonts): OSerif for titles, Jupiteroid for the
// rest, both with the Turkish letters. Each is drawn into a texture at a few sizes as it is first needed,
// and text takes the smallest at least as big as it shows on the screen (its size in UI units times the
// UI's scale), so it stays sharp on a phone and on a large monitor alike; mipmaps smooth it in between.
// A face is drawn a little bigger than the size asked and lifted (FACE_GROW, FACE_LIFT), so that its
// capitals sit where the old pixel font's did and layouts made for that one still fit.
#define FACES       2
#define FACE_SIZES  3
#define TITLE_SIZE  26   // text this big or bigger is a title
static const char *const FACE_FILE[FACES] = { "assets/fonts/Jupiteroid-Regular.ttf", "assets/fonts/OSerif-Regular.ttf" };
static const int FACE_PX[FACES][FACE_SIZES] = { { 24, 40, 64 }, { 40, 64, 100 } };
static const float FACE_GROW[FACES] = { 1.08f, 1.15f }, FACE_LIFT[FACES] = { 0.07f, 0.12f };
static unsigned char *faceData[FACES];
static int faceBytes[FACES];
static bool faceRead[FACES];
static Font faces[FACES][FACE_SIZES];
static bool faceTried[FACES][FACE_SIZES];
static float textScale = 1.0f;

// The letters loaded: printable ASCII, Latin-1, the Turkish ones beyond it and a few marks.
static int Codepoints(int *cp)
{
    static const int MORE[] = { 0x011E, 0x011F, 0x0130, 0x0131, 0x015E, 0x015F, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026 };
    int n = 0;
    for (int c = 32; c < 127; c++) cp[n++] = c;
    for (int c = 160; c < 256; c++) cp[n++] = c;
    for (int i = 0; i < (int)(sizeof(MORE) / sizeof(MORE[0])); i++) cp[n++] = MORE[i];
    return n;
}

static Font Rasterise(int face, int px)
{
    Font f = { 0 };
    int cp[256];
    int n = Codepoints(cp);
    GlyphInfo *g = LoadFontData(faceData[face], faceBytes[face], px, cp, n, FONT_DEFAULT);
    if (!g) return f;
    int kept = 0;   // a letter the face lacks is left out, and shows as its '?'
    for (int i = 0; i < n; i++) {
        if (!g[i].image.data && g[i].value != 32) continue;
        g[kept++] = g[i];
    }
    f.baseSize = px;
    f.glyphCount = kept;
    f.glyphPadding = 4;
    f.glyphs = g;
    Image atlas = GenImageFontAtlas(g, &f.recs, kept, px, f.glyphPadding, 0);
    f.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    for (int i = 0; i < kept; i++) {
        UnloadImage(g[i].image);
        g[i].image = (Image){ 0 };
    }
    if (f.texture.id) {
        GenTextureMipmaps(&f.texture);
        SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
    }
    return f;
}

// The face's texture best for text 'px' screen pixels high; false while none can be had.
static bool FaceFor(int face, float px, Font *out)
{
    if (!IsWindowReady()) return false;
    if (!faceRead[face]) {
        faceRead[face] = true;
        faceData[face] = LoadFileData(FACE_FILE[face], &faceBytes[face]);
    }
    if (!faceData[face]) return false;
    int k = 0;
    while (k < FACE_SIZES - 1 && (float)FACE_PX[face][k] < px * 0.9f) k++;
    for (int tries = 0; tries < FACE_SIZES; tries++) {
        if (!faceTried[face][k]) {
            faceTried[face][k] = true;
            faces[face][k] = Rasterise(face, FACE_PX[face][k]);
        }
        if (faces[face][k].texture.id) {
            *out = faces[face][k];
            return true;
        }
        k = (k + 1) % FACE_SIZES;
    }
    return false;
}

// How many screen pixels each unit of text size takes: UiBegin sets it to the UI's scale.
void SetTextScale(float s)
{
    textScale = s > 0.0f ? s : 1.0f;
}

typedef struct {
    Font font;
    float size, spacing, lift;
} Setting;

static Setting SettingFor(float size)
{
    int face = size >= (float)TITLE_SIZE ? 1 : 0;
    Setting t;
    if (FaceFor(face, size * FACE_GROW[face] * textScale, &t.font)) {
        t.size = size * FACE_GROW[face];
        t.spacing = face ? 0.0f : size * 0.02f;
        t.lift = size * FACE_LIFT[face];
    } else {   // no font file: raylib's own pixel font
        t.font = GetFontDefault();
        t.size = size;
        t.spacing = (float)((int)size / 10);
        t.lift = 0.0f;
    }
    return t;
}

void DrawGameText(const char *s, float x, float y, float size, Color c)
{
    Setting t = SettingFor(size);
    DrawTextEx(t.font, s, (Vector2){ x, y - t.lift }, t.size, t.spacing, c);
}

float GameTextWidth(const char *s, float size)
{
    Setting t = SettingFor(size);
    return MeasureTextEx(t.font, s, t.size, t.spacing).x;
}

Font GameFont(void)
{
    Font f;
    return FaceFor(0, 20.0f * textScale, &f) ? f : GetFontDefault();
}
