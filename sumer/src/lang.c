#include "game.h"
#include <stdio.h>
#include <string.h>

#define LANG_FILE  "sumer.lang"
#define HASH_SIZE  2048
#define EXTRA_GLYPHS 6

static int language = LANG_EN;
static short table[HASH_SIZE];
static bool tableReady;
static Font font;
static bool fontReady;

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
    FILE *f = fopen(LANG_FILE, "w");
    if (!f) return;
    fputs(language == LANG_TR ? "tr\n" : "en\n", f);
    fclose(f);
}

void LoadLanguage(void)
{
    char buf[8] = "";
    FILE *f = fopen(LANG_FILE, "r");
    if (!f) return;
    if (fgets(buf, sizeof(buf), f)) SetLanguage(strncmp(buf, "tr", 2) == 0 ? LANG_TR : LANG_EN);
    fclose(f);
}

const char *ItemName(int item)
{
    return L(ITEMS[item].name);
}

// The built-in pixel font stops at Latin-1; these rows add the Turkish letters it lacks.
static const struct { int codepoint; const char *rows[10]; } EXTRA[EXTRA_GLYPHS] = {
    { 0x011E, { ".#..#.", "..##..", "######", "#....#", "#.....", "#...##", "#....#", "######", "......", "......" } },
    { 0x011F, { "#...#", ".###.", ".....", "#####", "#...#", "#...#", "#...#", "#####", "....#", "#####" } },
    { 0x0130, { ".#.", "...", "###", ".#.", ".#.", ".#.", ".#.", "###", "...", "..." } },
    { 0x0131, { ".", ".", ".", "#", "#", "#", "#", "#", ".", "." } },
    { 0x015E, { "......", "######", "#.....", "#.....", "######", ".....#", ".....#", "######", "...#..", "..#..." } },
    { 0x015F, { ".....", ".....", ".....", "#####", "#....", "#####", "....#", "#####", "..#..", ".#..." } },
};

static Image GlyphFromRows(const char *const rows[10])
{
    int w = (int)strlen(rows[0]);
    Image im = GenImageColor(w, 10, BLANK);
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < w; x++)
            if (rows[y][x] == '#') ImageDrawPixel(&im, x, y, WHITE);
    return im;
}

// Packs the default glyphs plus the extra ones into a new atlas, keeping the pixel look.
static void BuildFont(void)
{
    Font base = GetFontDefault();
    int count = base.glyphCount + EXTRA_GLYPHS;
    Image atlas = GenImageColor(256, 128, BLANK);
    int x = 1, y = 1;

    font.baseSize = base.baseSize;
    font.glyphCount = count;
    font.glyphPadding = 0;
    font.glyphs = (GlyphInfo *)MemAlloc((unsigned int)(count * (int)sizeof(GlyphInfo)));
    font.recs = (Rectangle *)MemAlloc((unsigned int)(count * (int)sizeof(Rectangle)));
    for (int i = 0; i < count; i++) {
        bool extra = i >= base.glyphCount;
        Image im = extra ? GlyphFromRows(EXTRA[i - base.glyphCount].rows) : ImageCopy(base.glyphs[i].image);
        ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        if (x + im.width + 1 > atlas.width) {
            x = 1;
            y += base.baseSize + 1;
        }
        ImageDraw(&atlas, im, (Rectangle){ 0, 0, (float)im.width, (float)im.height },
                  (Rectangle){ (float)x, (float)y, (float)im.width, (float)im.height }, WHITE);
        font.recs[i] = (Rectangle){ (float)x, (float)y, (float)im.width, (float)im.height };
        font.glyphs[i] = extra ? (GlyphInfo){ EXTRA[i - base.glyphCount].codepoint, 0, 0, 0, im } : base.glyphs[i];
        font.glyphs[i].image = im;
        x += im.width + 1;
    }
    font.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    fontReady = font.texture.id != 0;
}

Font GameFont(void)
{
    if (!fontReady && IsWindowReady()) BuildFont();
    return fontReady ? font : GetFontDefault();
}
