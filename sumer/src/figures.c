// Figures: people's bodies, faces, hair, beards, clothes, jewellery and armor, all made in code like the
// rest of the world. A body is a few smooth shapes: a torso and skirt shaped ring by ring, limbs that
// bend at the knee and elbow, a head with the large eyes and strong nose of Sumerian statues. The
// clothes are those of the time: the tufted fleece skirt (kaunakes), a short linen kilt, a fringed
// shawl wrapped over the left shoulder, a long robe. A hand reaches for whatever it holds (GripHere).
#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>

#define LOOK_FILE "sumer.look"

enum { LOD_FINE, LOD_MID, LOD_FAR };

typedef struct { float y, rx, rz, z, ruffle; Color c; } Ring;

// A garment painted onto the torso: what is cloth and what is skin, by where a point is.
typedef struct { const Look *look; bool woman; } Dress;

// What a color is, in its alpha, for the shaders' photo grain (see shaders.c): woven cloth, fleece or fur,
// leather, metal. Skin and everything else stay at 255.
enum { T_CLOTH = 1, T_FLEECE, T_LEATHER, T_METAL };

static Color Tag(Color c, int t)
{
    c.a = (unsigned char)(255 - t);
    return c;
}

static const Color P_DARK = { 26, 20, 18, 255 }, P_EYE = { 226, 220, 206, 255 }, P_GOLD = { 222, 178, 70, 255 - T_METAL },
                   P_LAPIS = { 40, 70, 168, 255 }, P_CARNELIAN = { 188, 70, 40, 255 }, P_SANDAL = { 96, 66, 44, 255 - T_LEATHER };
static const Color P_BRONZE = { 190, 130, 60, 255 - T_METAL };   // COL_BRONZE, as metal

// ---- Shapes ----

// Rings stacked bottom to top, each an ellipse (rx across, rz front to back, moved forward by z), joined
// into one smooth surface from angle a0 to a1 (all the way round: 0 to 2 PI). 'paint', when given,
// colors each point (a garment's edges); otherwise each ring has its own color. A ring's 'ruffle' puts
// every other point in or out, the folds or tufts of cloth.
static void Loft(const Ring *r, int n, int sides, float a0, float a1, Color (*paint)(const Dress *, Vector3), const Dress *d)
{
    float slope[16];
    for (int j = 0; j < n && j < 16; j++) {
        int lo = j > 0 ? j - 1 : j, hi = j + 1 < n ? j + 1 : j;
        float dy = r[hi].y - r[lo].y;
        slope[j] = dy > 1e-4f ? -((r[hi].rx + r[hi].rz) - (r[lo].rx + r[lo].rz)) * 0.5f / dy : 0.0f;
    }
    float cs[33], sn[33];
    if (sides > 32) sides = 32;
    for (int i = 0; i <= sides; i++) {
        float a = a0 + (a1 - a0) * (float)i / (float)sides;
        cs[i] = cosf(a);
        sn[i] = sinf(a);
    }
    rlCheckRenderBatchLimit((n - 1) * sides * 4);
    VBegin(RL_QUADS);
    for (int j = 0; j + 1 < n; j++)
        for (int i = 0; i < sides; i++)
            for (int k = 0; k < 4; k++) {
                int jj = j + (k == 1 || k == 2), ii = i + (k >= 2);
                const Ring *g = &r[jj];
                float ca = cs[ii], sa = sn[ii];
                float f = 1.0f + ((ii & 1) ? g->ruffle : -g->ruffle);
                Vector3 p = { g->rx * f * ca, g->y, g->rz * f * sa + g->z };
                Vector3 nrm = Vector3Normalize((Vector3){ ca * g->rz, 0, sa * g->rx });
                nrm.y = slope[jj];
                VPaint(paint ? paint(d, p) : g->c, Vector3Normalize(nrm));
                VVertex(p.x, p.y, p.z);
            }
    VEnd();
}

// A limb: a tapered tube from a to b, rounded in the light.
static void Tube(Vector3 a, Vector3 b, float ra, float rb, int sides, Color c)
{
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 1e-4f) return;
    d = Vector3Scale(d, 1.0f / len);
    Vector3 helper = fabsf(d.y) < 0.9f ? (Vector3){ 0, 1, 0 } : (Vector3){ 1, 0, 0 };
    Vector3 u = Vector3Normalize(Vector3CrossProduct(d, helper)), v = Vector3CrossProduct(u, d);
    float slope = (ra - rb) / len, cs[17], sn[17];
    if (sides > 16) sides = 16;
    for (int i = 0; i <= sides; i++) {
        cs[i] = cosf(2.0f * PI * (float)i / (float)sides);
        sn[i] = sinf(2.0f * PI * (float)i / (float)sides);
    }
    rlCheckRenderBatchLimit(sides * 4);
    VBegin(RL_QUADS);
    for (int i = 0; i < sides; i++)
        for (int k = 0; k < 4; k++) {
            bool top = k == 1 || k == 2;
            int ii = i + (k >= 2);
            float r = top ? rb : ra;
            Vector3 out = Vector3Add(Vector3Scale(u, cs[ii]), Vector3Scale(v, sn[ii]));
            Vector3 p = Vector3Add(top ? b : a, Vector3Scale(out, r));
            VPaint(c, Vector3Normalize(Vector3Add(out, Vector3Scale(d, slope))));
            VVertex(p.x, p.y, p.z);
        }
    VEnd();
}

static void Ball(Vector3 c, Vector3 r, int lod, Color col)
{
    if (lod == LOD_FINE) DrawShadedBlobFine(c, r, col);
    else if (lod == LOD_MID) DrawShadedBlob(c, r, col);
    else DrawShadedBlobFar(c, r, col);
}

static void Bit(float x, float y, float z, float w, float h, float d, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, d }, c);
}

static Vector3 V3(float x, float y, float z)
{
    return (Vector3){ x, y, z };
}

// ---- Looks ----

// The test tools draw extra people with this (a line-up to look at); the game leaves it empty.
void (*ExtraFigures)(void);

// The player's own look, two bits each for hair, beard, dress and jewellery: short hair, a full beard and
// the fleece skirt until they choose.
static unsigned chosen = HAIR_SHORT | BEARD_FULL << 2 | DRESS_FLEECE << 4, chosenSet;
static bool chosenLoaded;

unsigned short PackStyle(int hair, int beard, int dress, int jewel)
{
    return (unsigned short)((hair & 3) | (beard & 3) << 2 | (dress & 3) << 4 | (jewel & 3) << 6);
}

static Look Styled(Look l, unsigned short style)
{
    l.woman = (style & STYLE_WOMAN) != 0;
    l.hairStyle = style & 3;
    l.beard = style >> 2 & 3;
    l.dress = style >> 4 & 3;
    l.jewel = style >> 6 & 3;
    if (l.woman) l.beard = BEARD_NONE;
    return l;
}

// A player: their color on what they wear, the look they chose, the armor they have on.
Look PlayerLook(int slot, unsigned short style, int armor)
{
    Color own = PlayerColor(slot);
    Look l = { COL_SKIN, own, Mix(own, COL_LINEN, 0.55f), COL_HAIR, 0, 0, 0, 0, HEAD_NONE, (unsigned char)armor, false };
    return Styled(l, style);
}

static float Pick(unsigned seed, int salt)
{
    return Hash2((int)seed * 7 + 3, salt * 131 + 17);
}

static int PickOf(unsigned seed, int salt, int n)
{
    int k = (int)(Pick(seed, salt) * (float)n);
    return k < n ? k : n - 1;
}

// Skin and hair a little different from one person to the next, some grey with age.
static void Person(Look *l, unsigned seed)
{
    static const Color skins[4] = { { 152, 100, 68, 255 }, { 138, 90, 60, 255 }, { 164, 112, 76, 255 }, { 126, 82, 56, 255 } };
    static const Color hairs[3] = { { 40, 30, 24, 255 }, { 28, 22, 20, 255 }, { 56, 40, 28, 255 } };
    l->skin = skins[PickOf(seed, 1, 4)];
    l->hair = hairs[PickOf(seed, 2, 3)];
    if (Pick(seed, 3) < 0.18f) l->hair = (Color){ 128, 122, 114, 255 };
}

// Undyed wool and linen, with now and then a dyed one.
static Color Cloth(unsigned seed, Color robe)
{
    static const Color plain[5] = { { 222, 212, 186, 255 }, { 196, 176, 140, 255 }, { 232, 226, 210, 255 }, { 168, 140, 104, 255 }, { 210, 196, 160, 255 } };
    float p = Pick(seed, 4);
    return p < 0.45f ? robe : plain[PickOf(seed, 5, 5)];
}

// Everyone else's look, by what they do and who they are (the same 'seed' always gives the same person).
Look PersonLook(int role, unsigned seed, Color robe)
{
    Look l = { COL_SKIN, robe, Mix(robe, P_DARK, 0.35f), COL_HAIR, HAIR_SHORT, BEARD_FULL, DRESS_FLEECE, JEWEL_NONE, HEAD_NONE, ARMOR_NONE, false };
    Person(&l, seed);
    switch (role) {
    case LOOK_TOWN:
    case LOOK_VILLAGER:
        l.woman = Pick(seed, 6) < (role == LOOK_TOWN ? 0.42f : 0.3f);
        l.cloth = Cloth(seed, robe);
        if (l.woman) {
            l.dress = Pick(seed, 7) < 0.6f ? DRESS_SHAWL : DRESS_ROBE;
            l.hairStyle = Pick(seed, 8) < 0.55f ? HAIR_BUN : HAIR_LONG;
            l.beard = BEARD_NONE;
            l.jewel = Pick(seed, 9) < 0.5f ? JEWEL_BEADS : Pick(seed, 10) < 0.3f ? JEWEL_GOLD : JEWEL_NONE;
        } else {
            static const int dresses[4] = { DRESS_FLEECE, DRESS_KILT, DRESS_KILT, DRESS_SHAWL };
            l.dress = dresses[PickOf(seed, 7, 4)];
            l.hairStyle = PickOf(seed, 8, 4);
            l.beard = PickOf(seed, 9, 4);
            l.jewel = Pick(seed, 10) < 0.15f ? JEWEL_BANDS : JEWEL_NONE;
        }
        if (role == LOOK_VILLAGER && l.dress == DRESS_ROBE) l.dress = DRESS_SHAWL;
        break;
    case LOOK_TRAVELLER:
        l.dress = DRESS_SHAWL;
        l.cloth = Mix(robe, (Color){ 170, 150, 120, 255 }, 0.4f);
        l.beard = Pick(seed, 9) < 0.5f ? BEARD_LONG : BEARD_FULL;
        break;
    case LOOK_MERCHANT:
        l.dress = DRESS_ROBE;
        l.hairStyle = HAIR_BUN;
        l.beard = BEARD_LONG;
        l.jewel = JEWEL_GOLD;
        break;
    case LOOK_WORKER:
        l.dress = Pick(seed, 7) < 0.6f ? DRESS_KILT : DRESS_FLEECE;
        l.cloth = Mix(robe, COL_LINEN, 0.3f);
        l.hairStyle = PickOf(seed, 8, 3);
        l.beard = PickOf(seed, 9, 3);
        break;
    case LOOK_PRIEST:
        // Priests shaved head and face and went bare-chested in a fleece skirt; a priestess wore a robe and a tall headdress.
        l.woman = Pick(seed, 6) < 0.3f;
        l.hairStyle = l.woman ? HAIR_BUN : HAIR_SHAVED;
        l.beard = BEARD_NONE;
        l.dress = l.woman ? DRESS_ROBE : DRESS_FLEECE;
        l.cloth = l.woman ? robe : (Color){ 236, 230, 214, 255 };
        l.head = l.woman ? HEAD_TALL : HEAD_NONE;
        l.jewel = l.woman ? JEWEL_GOLD : JEWEL_NONE;
        break;
    case LOOK_GUARD:
        l.dress = DRESS_KILT;
        l.armor = ARMOR_BRONZE;
        l.head = HEAD_HELMET;
        l.beard = PickOf(seed, 9, 3) + 1;
        break;
    case LOOK_WARDEN:
        l.dress = DRESS_KILT;
        l.armor = ARMOR_LEATHER;
        l.head = HEAD_CAP;
        break;
    case LOOK_SOLDIER:
        l.dress = DRESS_KILT;
        l.head = HEAD_HELMET;
        l.beard = PickOf(seed, 9, 3) + 1;
        break;
    case LOOK_RAIDER:
        l.skin = (Color){ 150, 102, 70, 255 };
        l.dress = DRESS_FLEECE;
        l.armor = ARMOR_LEATHER;
        l.head = HEAD_STONECAP;
        l.hairStyle = HAIR_LONG;
        l.beard = BEARD_FULL;
        break;
    case LOOK_BARBARIAN:
        l.skin = (Color){ 150, 102, 70, 255 };
        l.dress = DRESS_KILT;
        l.armor = (seed & 1) ? ARMOR_WOLF : ARMOR_LEATHER;
        l.head = HEAD_HORNS;
        l.hairStyle = HAIR_LONG;
        l.beard = BEARD_LONG;
        break;
    }
    return l;
}

// ---- The player's chosen look, kept on this computer or in this browser ----

static void LoadChosen(void)
{
    unsigned v;
    chosenLoaded = true;
    FILE *f = fopen(DataPath(LOOK_FILE), "r");
    if (!f) return;
    if (fscanf(f, "%u", &v) == 1 && v < 512) {
        chosen = v;
        chosenSet = true;
    }
    fclose(f);
}

unsigned short ChosenStyle(void)
{
    if (!chosenLoaded) LoadChosen();
    return (unsigned short)chosen;
}

bool StyleChosen(void)
{
    if (!chosenLoaded) LoadChosen();
    return chosenSet;
}

void ChooseStyle(unsigned short style)
{
    chosen = style;
    chosenSet = chosenLoaded = true;
    FILE *f = fopen(DataPath(LOOK_FILE), "w");
    if (!f) return;
    fprintf(f, "%u\n", (unsigned)style);
    fclose(f);
    SyncStorage();
}

const char *HairName(int k)
{
    static const char *const n[HAIR_KINDS] = { "Shaved", "Short", "Long, with a band", "Bun at the nape" };
    return L(n[k & 3]);
}

const char *BeardName(int k)
{
    static const char *const n[BEARD_KINDS] = { "None", "Short", "Full", "Long, waved" };
    return L(n[k & 3]);
}

const char *DressName(int k)
{
    static const char *const n[DRESS_KINDS] = { "Linen kilt", "Fleece skirt", "Fringed shawl", "Long robe" };
    return L(n[k & 3]);
}

const char *JewelName(int k)
{
    static const char *const n[JEWEL_KINDS] = { "None", "Lapis and carnelian beads", "Gold band and earrings", "Arm rings" };
    return L(n[k & 3]);
}

// ---- Hands: what they hold ----

static struct { Vector3 at; bool set, local; } grips[2];

// The hand 'hand' (HAND_WEAPON or HAND_OTHER) of the next person drawn holds this point, given where the
// drawing now is (inside the push and turns that draw what it holds).
void GripHere(int hand, float x, float y, float z)
{
    grips[hand & 1].at = Vector3Transform(V3(x, y, z), rlGetMatrixTransform());
    grips[hand & 1].set = true;
    grips[hand & 1].local = false;
}

// The same, as a point on the body itself (x to the weapon side, z forward, from the feet).
void GripAt(int hand, float x, float y, float z)
{
    grips[hand & 1].at = V3(x, y, z);
    grips[hand & 1].set = true;
    grips[hand & 1].local = true;
}

// Shoulder to elbow to wrist, the elbow bent toward 'hint', the wrist as near the target as the arm reaches.
static void Reach(Vector3 shoulder, Vector3 target, Vector3 hint, float upper, float lower, Vector3 *elbow, Vector3 *wrist)
{
    Vector3 d = Vector3Subtract(target, shoulder);
    float dist = Clamp(Vector3Length(d), 0.08f, upper + lower - 0.002f);
    Vector3 dir = Vector3Normalize(d);
    if (Vector3Length(d) < 1e-4f) dir = V3(0, -1, 0);
    *wrist = Vector3Add(shoulder, Vector3Scale(dir, dist));
    float ca = Clamp((upper * upper + dist * dist - lower * lower) / (2.0f * upper * dist), -1.0f, 1.0f);
    Vector3 perp = Vector3Subtract(hint, Vector3Scale(dir, Vector3DotProduct(hint, dir)));
    if (Vector3Length(perp) < 1e-4f) perp = V3(0, -1, 0);
    perp = Vector3Normalize(perp);
    *elbow = Vector3Add(shoulder, Vector3Add(Vector3Scale(dir, upper * ca), Vector3Scale(perp, upper * sqrtf(1.0f - ca * ca))));
}

// ---- Clothes on the torso ----

static Color TorsoPaint(const Dress *d, Vector3 p)
{
    const Look *l = d->look;
    Color skin = l->skin, cloth = l->cloth, trim = l->trim;
    if (p.y < 1.035f) return cloth;
    if (p.y < 1.075f && l->dress != DRESS_ROBE) return trim;   // the belt
    switch (l->dress) {
    case DRESS_SHAWL: {
        // Wrapped round the body and up over the left shoulder, the right one bare, its edge fringed.
        float edge = p.x < -0.04f ? 1.62f : 1.50f - (p.x + 0.04f) * 1.25f;
        if (p.y < edge - 0.035f) return cloth;
        if (p.y < edge) return trim;
        return skin;
    }
    case DRESS_ROBE:
        if (p.y < 1.075f && p.y > 1.035f) return Mix(cloth, trim, 0.6f);
        if (p.y < 1.505f) return cloth;
        return p.y < 1.535f ? trim : skin;
    default:
        if (d->woman) return p.y < 1.36f ? cloth : skin;
        return skin;
    }
}

// ---- The body ----

typedef struct {
    int lod, sides, limb;
    bool woman;
    float lift;                     // how far the whole body is raised or lowered by how the legs stand
} Build;

static void Torso(const Look *l, const Build *b)
{
    Dress d = { l, b->woman };
    float y = b->lift;
    Ring man[9] = {
        { 0.86f, 0.165f, 0.120f, 0.00f, 0, { 0 } }, { 0.95f, 0.185f, 0.130f, -0.01f, 0, { 0 } }, { 1.05f, 0.168f, 0.120f, 0.0f, 0, { 0 } },
        { 1.15f, 0.176f, 0.125f, 0.01f, 0, { 0 } }, { 1.25f, 0.196f, 0.132f, 0.015f, 0, { 0 } }, { 1.35f, 0.214f, 0.134f, 0.015f, 0, { 0 } },
        { 1.44f, 0.222f, 0.124f, 0.0f, 0, { 0 } }, { 1.52f, 0.180f, 0.108f, -0.01f, 0, { 0 } }, { 1.585f, 0.090f, 0.076f, 0.0f, 0, { 0 } },
    };
    Ring woman[9] = {
        { 0.86f, 0.175f, 0.125f, 0.00f, 0, { 0 } }, { 0.95f, 0.200f, 0.135f, -0.01f, 0, { 0 } }, { 1.05f, 0.150f, 0.110f, 0.0f, 0, { 0 } },
        { 1.15f, 0.150f, 0.112f, 0.01f, 0, { 0 } }, { 1.25f, 0.170f, 0.140f, 0.03f, 0, { 0 } }, { 1.33f, 0.182f, 0.140f, 0.03f, 0, { 0 } },
        { 1.43f, 0.192f, 0.115f, 0.0f, 0, { 0 } }, { 1.51f, 0.160f, 0.098f, -0.01f, 0, { 0 } }, { 1.575f, 0.082f, 0.070f, 0.0f, 0, { 0 } },
    };
    Ring *r = b->woman ? woman : man;
    Ring far[5] = { r[0], r[2], r[4], r[6], r[8] };
    for (int j = 0; j < 9; j++) r[j].y += y;
    for (int j = 0; j < 5; j++) far[j].y += y;
    if (b->lod == LOD_FAR) Loft(far, 5, 6, 0, 2 * PI, TorsoPaint, &d);
    else Loft(r, 9, b->lod == LOD_FINE ? 16 : 8, 0, 2 * PI, TorsoPaint, &d);
}

// The skirt, from the belt down: a kilt to the knee, the fleece skirt in tiers of tufts, a shawl or robe
// to the ankle.
static void Skirt(const Look *l, const Build *b)
{
    Ring r[14];
    int n = 0, sides = b->lod == LOD_FINE ? 18 : b->lod == LOD_MID ? 10 : 6;
    float top = 1.075f + b->lift, hips = b->woman ? 0.205f : 0.19f;
    Color c = l->cloth, dark = Mix(l->cloth, P_DARK, 0.3f), fringe = l->trim;
    bool fine = b->lod == LOD_FINE;

    switch (l->dress) {
    case DRESS_FLEECE: {
        // Tiers of hanging tufts, each tier a little wider, shaded darker at its root.
        const int tiers = b->lod == LOD_FAR ? 1 : 4;
        float bottom = 0.40f + b->lift, h = (top - bottom) / (float)tiers;
        for (int t = 0; t < tiers; t++) {
            float y0 = bottom + h * (float)t, w = hips + 0.03f + 0.03f * (float)(tiers - 1 - t) / (float)tiers;
            r[n++] = (Ring){ y0, w + 0.03f, w * 0.78f + 0.03f, 0.02f, fine ? 0.06f : 0.0f, Mix(c, COL_LINEN, 0.15f) };
            r[n++] = (Ring){ y0 + h * 0.97f, w, w * 0.74f, 0.01f, fine ? 0.03f : 0.0f, dark };
        }
        r[n++] = (Ring){ top + 0.005f, hips - 0.005f, hips * 0.68f, 0.0f, 0, dark };
        break;
    }
    case DRESS_KILT: {
        float hem = 0.50f + b->lift;
        r[n++] = (Ring){ hem, hips + 0.07f, hips * 0.82f + 0.05f, 0.025f, fine ? 0.04f : 0.0f, fringe };
        r[n++] = (Ring){ hem + 0.04f, hips + 0.065f, hips * 0.82f + 0.045f, 0.025f, fine ? 0.04f : 0.0f, c };
        r[n++] = (Ring){ 0.80f + b->lift, hips + 0.035f, hips * 0.76f + 0.03f, 0.01f, fine ? 0.02f : 0.0f, c };
        r[n++] = (Ring){ top, hips - 0.005f, hips * 0.68f, 0.0f, 0, c };
        break;
    }
    default: {
        // Down to the ankles, wide enough at the hem for the legs to walk in it; the shawl's hem is fringed.
        float hem = 0.10f;
        bool shawl = l->dress == DRESS_SHAWL;
        r[n++] = (Ring){ hem, hips + 0.10f, hips + 0.03f, 0.04f, fine ? 0.05f : 0.0f, shawl ? fringe : dark };
        r[n++] = (Ring){ hem + 0.06f, hips + 0.095f, hips + 0.025f, 0.04f, fine ? 0.04f : 0.0f, c };
        if (b->lod != LOD_FAR) r[n++] = (Ring){ 0.55f + b->lift * 0.5f, hips + 0.06f, hips * 0.85f + 0.035f, 0.02f, fine ? 0.025f : 0.0f, c };
        r[n++] = (Ring){ 0.86f + b->lift, hips + 0.02f, hips * 0.72f + 0.015f, 0.0f, 0, c };
        r[n++] = (Ring){ top, hips - 0.012f, hips * 0.68f, 0.0f, 0, c };
        break;
    }
    }
    Loft(r, n, sides, 0, 2 * PI, NULL, NULL);
}

// A long skirt keeps the steps short.
static bool LongDress(const Look *l)
{
    return l->dress == DRESS_SHAWL || l->dress == DRESS_ROBE;
}

static void Leg(Vector3 hip, float thigh, float bend, const Look *l, const Build *b)
{
    const float upper = 0.44f, lower = 0.43f;
    Vector3 knee = { hip.x, hip.y - upper * cosf(thigh), hip.z + upper * sinf(thigh) };
    float s = thigh - bend;
    Vector3 ankle = { hip.x, knee.y - lower * cosf(s), knee.z + lower * sinf(s) };
    if (b->lod == LOD_FAR) {
        Tube(hip, ankle, 0.075f, 0.045f, 4, l->skin);
        Bit(ankle.x, ankle.y - 0.02f, ankle.z + 0.05f, 0.09f, 0.06f, 0.2f, P_SANDAL);
        return;
    }
    Tube(hip, knee, 0.078f, 0.056f, b->limb, l->skin);
    Tube(knee, ankle, 0.056f, 0.04f, b->limb, l->skin);
    if (b->lod == LOD_FINE) Ball(knee, V3(0.056f, 0.056f, 0.056f), LOD_FAR, l->skin);
    Ball(V3(ankle.x, ankle.y - 0.02f, ankle.z + 0.055f), V3(0.048f, 0.035f, 0.11f), b->lod == LOD_FINE ? LOD_MID : LOD_FAR, P_SANDAL);
}

// Where a leg's ankle is below its hip, for keeping the lower foot on the ground.
static float AnkleDrop(float thigh, float bend)
{
    return 0.44f * cosf(thigh) + 0.43f * cosf(thigh - bend);
}

static void Arm(Vector3 shoulder, Vector3 elbow, Vector3 wrist, Color upperC, Color skin, const Look *l, const Build *b, bool ring)
{
    Vector3 fore = Vector3Normalize(Vector3Subtract(wrist, elbow));
    Vector3 hand = Vector3Add(wrist, Vector3Scale(fore, 0.055f));
    if (b->lod == LOD_FAR) {
        Tube(shoulder, wrist, 0.06f, 0.04f, 4, upperC);
        return;
    }
    Tube(shoulder, elbow, 0.06f, 0.047f, b->limb, upperC);
    Tube(elbow, wrist, 0.047f, 0.034f, b->limb, skin);
    Ball(elbow, V3(0.047f, 0.047f, 0.047f), LOD_FAR, upperC.r == skin.r && upperC.g == skin.g ? skin : Mix(upperC, skin, 0.5f));
    Ball(hand, V3(0.042f, 0.042f, 0.042f), b->lod == LOD_FINE ? LOD_MID : LOD_FAR, skin);
    Ball(shoulder, V3(0.07f, 0.065f, 0.068f), b->lod == LOD_FINE ? LOD_MID : LOD_FAR, upperC);
    if (ring && b->lod == LOD_FINE) {
        Vector3 up = Vector3Normalize(Vector3Subtract(elbow, shoulder));
        Vector3 a = Vector3Add(shoulder, Vector3Scale(up, 0.12f));
        Tube(a, Vector3Add(a, Vector3Scale(up, 0.035f)), 0.064f, 0.062f, 10, P_GOLD);
        Tube(Vector3Subtract(wrist, Vector3Scale(fore, 0.05f)), wrist, 0.042f, 0.04f, 10, P_GOLD);
    }
    (void)l;
}

// ---- The head ----

static void Hair(const Look *l, const Build *b, float y)
{
    int lod = b->lod == LOD_FINE ? LOD_FINE : b->lod;
    Color band = l->jewel == JEWEL_GOLD ? P_GOLD : Mix(l->trim, l->hair, 0.3f);
    if (l->head != HEAD_NONE && l->head != HEAD_TALL) {
        // Under a helmet or cap only the hair at the back shows.
        if (l->hairStyle == HAIR_LONG) Ball(V3(0, y + 1.66f, -0.09f), V3(0.11f, 0.13f, 0.05f), lod == LOD_FINE ? LOD_MID : LOD_FAR, l->hair);
        return;
    }
    if (l->hairStyle == HAIR_SHAVED) {
        if (l->jewel == JEWEL_GOLD) {
            Ring r[2] = { { y + 1.835f, 0.117f, 0.133f, -0.005f, 0, P_GOLD }, { y + 1.862f, 0.112f, 0.128f, -0.008f, 0, P_GOLD } };
            Loft(r, 2, lod == LOD_FINE ? 18 : 8, 0, 2 * PI, NULL, NULL);
        }
        return;
    }
    Ball(V3(0, y + 1.848f, -0.02f), V3(0.121f, 0.108f, 0.132f), lod, l->hair);
    if (l->hairStyle == HAIR_LONG) {
        Ball(V3(0, y + 1.67f, -0.085f), V3(0.118f, 0.16f, 0.06f), lod == LOD_FINE ? LOD_MID : LOD_FAR, l->hair);
        if (b->woman) Ball(V3(0, y + 1.56f, -0.07f), V3(0.1f, 0.12f, 0.05f), LOD_FAR, l->hair);
    }
    if (l->hairStyle == HAIR_BUN) Ball(V3(0, y + 1.755f, -0.15f), V3(0.075f, 0.062f, 0.06f), lod == LOD_FINE ? LOD_MID : LOD_FAR, l->hair);
    if ((l->hairStyle == HAIR_LONG || l->hairStyle == HAIR_BUN) && b->lod != LOD_FAR) {
        // The band round the head that held long hair in place (gold for the rich).
        Ring r[2] = { { y + 1.828f, 0.124f, 0.139f, -0.012f, 0, band }, { y + 1.852f, 0.123f, 0.137f, -0.016f, 0, band } };
        Loft(r, 2, lod == LOD_FINE ? 18 : 8, 0, 2 * PI, NULL, NULL);
    }
}

static void Beard(const Look *l, const Build *b, float y)
{
    if (l->beard == BEARD_NONE || b->woman) return;
    int lod = b->lod == LOD_FINE ? LOD_MID : LOD_FAR;
    Color h = l->hair, ripple = Mix(l->hair, (Color){ 120, 96, 74, 255 }, 0.35f);
    switch (l->beard) {
    case BEARD_SHORT:
        Ball(V3(0, y + 1.698f, 0.038f), V3(0.1f, 0.062f, 0.092f), lod, h);
        break;
    case BEARD_FULL:
        Ball(V3(0, y + 1.676f, 0.045f), V3(0.106f, 0.088f, 0.094f), lod, h);
        break;
    default: {
        // The long beard of the statues, combed into waves: rings of alternating shade down to the chest.
        if (b->lod == LOD_FAR) {
            Ball(V3(0, y + 1.62f, 0.06f), V3(0.1f, 0.13f, 0.08f), LOD_FAR, h);
            break;
        }
        Ring r[9];
        int n = 0;
        for (int k = 0; k < 9; k++) {
            float t = (float)k / 8.0f;
            float ry = 1.5f + 0.24f * t, w = Lerp(0.07f, 0.098f, sqrtf(t));
            r[n++] = (Ring){ y + ry, w, Lerp(0.035f, 0.09f, t * t), Lerp(0.105f, 0.04f, t), 0, (k & 1) ? ripple : h };
        }
        Loft(r, n, b->lod == LOD_FINE ? 14 : 8, 0, 2 * PI, NULL, NULL);
        break;
    }
    }
    if (b->lod == LOD_FINE) Tube(V3(-0.045f, y + 1.736f, 0.11f), V3(0.045f, y + 1.736f, 0.11f), 0.013f, 0.013f, 6, h);
}

static void Headgear(const Look *l, const Build *b, float y)
{
    int lod = b->lod == LOD_FINE ? LOD_MID : LOD_FAR;
    switch (l->head) {
    case HEAD_HELMET:
        // A bronze helmet over the ears, as the soldiers of the Stele of the Vultures wore.
        Ball(V3(0, y + 1.835f, -0.012f), V3(0.138f, 0.128f, 0.152f), lod, P_BRONZE);
        Bit(0, y + 1.79f, -0.13f, 0.2f, 0.12f, 0.04f, P_BRONZE);
        if (b->lod != LOD_FAR) Bit(0, y + 1.94f, -0.01f, 0.03f, 0.05f, 0.18f, Mix(P_BRONZE, P_DARK, 0.25f));
        break;
    case HEAD_CAP:
        Ball(V3(0, y + 1.85f, -0.015f), V3(0.128f, 0.1f, 0.142f), lod, (Color){ 128, 88, 56, 255 - T_LEATHER });
        break;
    case HEAD_STONECAP:
        Ball(V3(0, y + 1.85f, -0.015f), V3(0.13f, 0.1f, 0.144f), lod, (Color){ 128, 122, 112, 255 });
        break;
    case HEAD_HORNS:
        Ball(V3(0, y + 1.85f, -0.015f), V3(0.134f, 0.11f, 0.148f), lod, P_BRONZE);
        for (int s = -1; s <= 1; s += 2) {
            Vector3 a = V3(0.11f * (float)s, y + 1.89f, -0.01f), m = V3(0.23f * (float)s, y + 1.97f, 0.0f), t = V3(0.26f * (float)s, y + 2.13f, 0.04f);
            Tube(a, m, 0.035f, 0.028f, b->lod == LOD_FINE ? 8 : 4, COL_LINEN);
            Tube(m, t, 0.028f, 0.008f, b->lod == LOD_FINE ? 8 : 4, COL_LINEN);
        }
        break;
    case HEAD_TALL:
        DrawShadedCylinder(V3(0, y + 1.83f, -0.01f), 0.125f, 0.135f, 0.26f, 1.12f, b->lod == LOD_FINE ? 14 : 6, l->cloth);
        if (b->lod != LOD_FAR) DrawShadedCylinder(V3(0, y + 1.83f, -0.01f), 0.13f, 0.14f, 0.04f, 1.0f, b->lod == LOD_FINE ? 14 : 6, P_GOLD);
        break;
    default:
        break;
    }
}

static void Head(const Look *l, const Build *b, float y)
{
    int lod = b->lod;
    Color skin = l->skin;
    Tube(V3(0, y + 1.55f, 0), V3(0, y + 1.7f, 0.012f), 0.06f, 0.054f, b->lod == LOD_FINE ? 10 : 5, skin);
    Ball(V3(0, y + 1.795f, 0), b->woman ? V3(0.106f, 0.128f, 0.122f) : V3(0.112f, 0.134f, 0.128f), lod, skin);
    if (lod != LOD_FAR) Ball(V3(0, y + 1.722f, 0.032f), b->woman ? V3(0.084f, 0.064f, 0.088f) : V3(0.092f, 0.07f, 0.094f), LOD_MID, skin);
    // The strong straight nose of the statues.
    Tube(V3(0, y + 1.808f, 0.112f), V3(0, y + 1.748f, 0.148f), 0.019f, 0.024f, lod == LOD_FINE ? 6 : 4, Mix(skin, P_DARK, 0.06f));
    if (lod == LOD_FINE) {
        // Wide open eyes under a strong brow, and ears.
        for (int s = -1; s <= 1; s += 2) {
            float x = 0.043f * (float)s;
            Ball(V3(x, y + 1.802f, 0.103f), V3(0.025f, 0.0145f, 0.014f), LOD_FAR, P_EYE);
            Bit(x, y + 1.802f, 0.116f, 0.013f, 0.013f, 0.006f, P_DARK);
            Bit(x, y + 1.826f, 0.112f, 0.052f, 0.012f, 0.02f, Mix(l->hair, P_DARK, 0.3f));
            Ball(V3(0.111f * (float)s, y + 1.785f, -0.005f), V3(0.018f, 0.034f, 0.026f), LOD_FAR, skin);
            if (l->jewel == JEWEL_GOLD) Ball(V3(0.116f * (float)s, y + 1.738f, 0.0f), V3(0.016f, 0.026f, 0.016f), LOD_FAR, P_GOLD);
        }
        if (l->beard < BEARD_FULL || b->woman) Bit(0, y + 1.712f, 0.112f, 0.04f, 0.009f, 0.01f, Mix(skin, (Color){ 120, 40, 36, 255 }, 0.45f));
    } else if (lod == LOD_MID) {
        Bit(0, y + 1.81f, 0.105f, 0.12f, 0.022f, 0.02f, Mix(skin, P_DARK, 0.6f));
    }
    Hair(l, b, y);
    Beard(l, b, y);
    Headgear(l, b, y);
}

// A string of lapis and carnelian beads lying on the chest.
static void Necklace(const Look *l, const Build *b, float y)
{
    if (l->jewel != JEWEL_BEADS || b->lod == LOD_FAR) return;
    int n = b->lod == LOD_FINE ? 26 : 6;
    float size = b->lod == LOD_FINE ? 0.019f : 0.045f;
    for (int k = 0; k < n; k++) {
        float a = 2.0f * PI * (float)k / (float)n, front = fmaxf(0.0f, sinf(a));
        float rx = b->woman ? 0.142f : 0.158f, rz = 0.104f;
        Color c = (k % 3 == 2) ? P_GOLD : (k & 1) ? P_CARNELIAN : P_LAPIS;
        Bit(rx * cosf(a), y + 1.52f - 0.07f * front, rz * sinf(a) + 0.012f + 0.012f * front, size, size, size, c);
    }
}

// ---- Armor, over the clothes ----

static void Armor(const Look *l, const Build *b, float y)
{
    int sides = b->lod == LOD_FINE ? 16 : b->lod == LOD_MID ? 8 : 5;
    switch (l->armor) {
    case ARMOR_WOLF: {
        // A wolf's pelt over the shoulders and down the back, its head worn as a hood.
        const Color fur = { 168, 160, 146, 255 - T_FLEECE }, dark = { 112, 106, 96, 255 - T_FLEECE };
        Ring r[4] = {
            { y + 0.72f, 0.25f, 0.2f, -0.02f, b->lod == LOD_FINE ? 0.05f : 0, dark }, { y + 1.1f, 0.22f, 0.16f, -0.01f, 0, fur },
            { y + 1.42f, 0.25f, 0.155f, 0.0f, 0, fur }, { y + 1.56f, 0.13f, 0.1f, -0.01f, 0, fur },
        };
        Loft(r, 4, sides, PI - 0.5f, 2 * PI + 0.5f, NULL, NULL);
        // Its forelegs knotted over the chest.
        if (b->lod != LOD_FAR)
            for (int s = -1; s <= 1; s += 2)
                Tube(V3(0.2f * (float)s, y + 1.5f, 0.02f), V3(0.03f * (float)s, y + 1.3f, 0.15f), 0.04f, 0.03f, b->limb, fur);
        if (l->head == HEAD_NONE) {
            Ball(V3(0, y + 1.9f, -0.03f), V3(0.142f, 0.085f, 0.16f), b->lod == LOD_FINE ? LOD_MID : LOD_FAR, dark);
            Ball(V3(0, y + 1.9f, 0.13f), V3(0.06f, 0.045f, 0.08f), LOD_FAR, dark);
            if (b->lod != LOD_FAR)
                for (int s = -1; s <= 1; s += 2) Bit(0.08f * (float)s, y + 1.99f, 0.0f, 0.05f, 0.08f, 0.04f, dark);
        }
        break;
    }
    case ARMOR_LEATHER: {
        // Bands of thick hide laced one over the next.
        const Color hide = { 96, 62, 40, 255 - T_LEATHER }, strap = { 58, 38, 26, 255 - T_LEATHER }, lace = { 196, 170, 120, 255 - T_LEATHER };
        Ring r[8];
        for (int k = 0; k < 8; k++) {
            float t = (float)k / 7.0f, yy = y + 1.02f + 0.48f * t;
            float rx = t < 0.85f ? Lerp(0.19f, 0.238f, t / 0.85f) : Lerp(0.238f, 0.18f, (t - 0.85f) / 0.15f);
            r[k] = (Ring){ yy, rx, Lerp(0.142f, 0.142f, t) - (t > 0.85f ? 0.03f : 0.0f), 0.01f, 0, k == 0 || k == 7 ? strap : (k & 1) ? lace : hide };
        }
        if (b->lod == LOD_FAR) r[1] = r[7];
        Loft(r, b->lod == LOD_FAR ? 2 : 8, sides, 0, 2 * PI, NULL, NULL);
        for (int s = -1; s <= 1; s += 2) Ball(V3(0.21f * (float)s, y + 1.47f, 0), V3(0.095f, 0.065f, 0.095f), LOD_FAR, strap);
        break;
    }
    case ARMOR_BRONZE: {
        // A thick cloak over the shoulders and back, studded with bronze discs.
        const Color felt = { 92, 74, 56, 255 - T_FLEECE };
        Ring r[4] = {
            { y + 0.62f, 0.27f, 0.21f, -0.015f, b->lod == LOD_FINE ? 0.03f : 0, Mix(felt, P_DARK, 0.2f) }, { y + 1.0f, 0.245f, 0.18f, -0.01f, 0, felt },
            { y + 1.43f, 0.258f, 0.162f, 0.0f, 0, felt }, { y + 1.56f, 0.12f, 0.1f, -0.005f, 0, felt },
        };
        Loft(r, 4, sides, PI - 0.85f, 2 * PI + 0.85f, NULL, NULL);
        if (b->lod == LOD_FINE)
            for (int row = 0; row < 3; row++)
                for (int k = 0; k < 5; k++) {
                    float a = PI + 0.35f + (PI - 0.7f) * (float)k / 4.0f, yy = y + 0.8f + 0.22f * (float)row;
                    float rx = Lerp(0.27f, 0.25f, (yy - y - 0.62f) / 0.8f), rz = Lerp(0.21f, 0.17f, (yy - y - 0.62f) / 0.8f);
                    Bit(rx * cosf(a), yy, rz * sinf(a) - 0.015f, 0.05f, 0.05f, 0.05f, P_BRONZE);
                }
        break;
    }
    default:
        break;
    }
}

// ---- Bows ----

static void BowPart(float x, float y, float z, float w, float h, float d, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, d }, c);
}

// A bow held ready to draw, in front of the body, tilted, the other hand on its grip and the hand that
// draws resting on the string: an archer between shots, or walking. With a torch in the other hand the bow
// hangs from the drawing hand instead. The hands sway a little as the body breathes.
void HoldBowFor(Vector3 pos, float facing, bool crouch, bool torch)
{
    float t = (float)GetTime() + pos.x * 0.37f, sway = 0.012f * sinf(t * 1.7f);
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + (crouch ? 0.92f : 1.3f), pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    if (torch) {
        rlTranslatef(0.4f, 0, 0.12f);
        rlRotatef(-12.0f, 1, 0, 0);
        GripHere(HAND_WEAPON, 0, 0, 0);
    } else {
        rlTranslatef(-0.1f, -0.02f + sway, 0.36f);
        rlRotatef(-28.0f, 0, 0, 1);
        rlRotatef(-8.0f, 1, 0, 0);
        GripHere(HAND_OTHER, 0, 0, 0);
        GripHere(HAND_WEAPON, 0, 0.1f, -0.09f);
    }
    BowPart(0, 0, 0, 0.05f, 0.9f, 0.06f, COL_WOOD);
    BowPart(0, 0.5f, 0.05f, 0.04f, 0.16f, 0.05f, COL_WOOD);
    BowPart(0, -0.5f, 0.05f, 0.04f, 0.16f, 0.05f, COL_WOOD);
    BowPart(0, 0, -0.07f, 0.015f, 1.05f, 0.015f, COL_LINEN);
    rlPopMatrix();
}

void HoldBow(Vector3 pos, float facing, bool crouch)
{
    HoldBowFor(pos, facing, crouch, false);
}

// A bow carried across the back, string out, for someone whose hands are full.
void SlungBow(Vector3 pos, float facing)
{
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + 1.3f, pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    rlTranslatef(0, 0, -0.24f);
    rlRotatef(28.0f, 0, 0, 1);
    BowPart(0, 0, 0, 0.05f, 1.0f, 0.05f, COL_WOOD);
    BowPart(0, 0.55f, 0.04f, 0.04f, 0.14f, 0.05f, COL_WOOD);
    BowPart(0, -0.55f, 0.04f, 0.04f, 0.14f, 0.05f, COL_WOOD);
    BowPart(0, 0, -0.06f, 0.012f, 1.15f, 0.012f, COL_LINEN);
    rlPopMatrix();
}

// ---- A whole person ----

static float flashWas;

static void Body(Vector3 at, float stride, Look look, bool crouch, bool bow, float draw, int lod)
{
    Build b = { lod, lod == LOD_FINE ? 16 : lod == LOD_MID ? 8 : 4, lod == LOD_FINE ? 10 : 6, look.woman, 0 };
    bool longDress = LongDress(&look);
    float swing = sinf(stride);

    if (!(look.trim.r == look.skin.r && look.trim.g == look.skin.g)) look.trim = Tag(Clothe(look.trim, look.skin), T_CLOTH);
    look.cloth = Tag(Clothe(look.cloth, look.skin), look.dress == DRESS_FLEECE ? T_FLEECE : T_CLOTH);

    // The legs: a step's thigh swing and knee bend, or a crouch on bent knees.
    float thigh[2], bend[2];
    for (int s = 0; s < 2; s++) {
        float ph = stride + (s ? PI : 0.0f);
        if (crouch) {
            thigh[s] = 58.0f * DEG2RAD;
            bend[s] = 116.0f * DEG2RAD;
        } else {
            float amp = longDress ? 15.0f : 25.0f;
            thigh[s] = amp * DEG2RAD * sinf(ph);
            bend[s] = (longDress ? 22.0f : 38.0f) * DEG2RAD * fmaxf(0.0f, cosf(ph)) * (stride != 0.0f);
        }
    }
    float hipY = 0.92f, reach = fmaxf(AnkleDrop(thigh[0], bend[0]), AnkleDrop(thigh[1], bend[1]));
    b.lift = (0.05f + reach) - hipY;
    float y = b.lift;
    float sit = crouch ? 0.06f : 0.0f;

    for (int s = 0; s < 2; s++) Leg(V3(s ? 0.095f : -0.095f, hipY + y, sit), thigh[s], bend[s], &look, &b);

    rlPushMatrix();
    rlTranslatef(0, 0, sit);
    Skirt(&look, &b);
    Torso(&look, &b);
    Necklace(&look, &b, y);
    Armor(&look, &b, y);
    Head(&look, &b, y - 0.025f);
    rlPopMatrix();

    // The arms: swinging as they walk, drawing a bow, or reaching for what the hands hold.
    float shoulderX = look.woman ? 0.19f : 0.215f, upper = 0.29f, lower = 0.26f;
    for (int s = 0; s < 2; s++) {
        float side = s == HAND_WEAPON ? 1.0f : -1.0f;
        Vector3 sh = { side * shoulderX, 1.455f + y, sit }, target, hint = { side * 0.6f, -0.4f, -1.0f };
        if (bow) {
            float string = fmaxf(0.6f - 0.42f * draw, 0.16f);
            target = s == HAND_WEAPON ? V3(0.03f, 1.48f + y, sit + string) : V3(-0.2f, 1.48f + y, sit + 0.58f);
            hint = s == HAND_WEAPON ? V3(1.0f, 0.1f, -0.6f) : V3(-1.0f, -0.4f, 0.0f);
        } else if (grips[s].set) {
            target = grips[s].at;
            hint = V3(side * 0.9f, -0.7f, -0.4f);
        } else {
            float a = (s == HAND_WEAPON ? -swing : swing) * (longDress ? 0.12f : 0.2f) * (stride != 0.0f);
            target = V3(side * (shoulderX + 0.065f), 0.93f + y + fabsf(a) * 0.2f, sit + 0.04f + a);
        }
        Vector3 elbow, wrist;
        Reach(sh, target, hint, upper, lower, &elbow, &wrist);
        bool sleeve = look.dress == DRESS_ROBE || (look.dress == DRESS_SHAWL && s != HAND_WEAPON);
        Color upperC = sleeve ? look.cloth : look.skin;
        if (look.armor == ARMOR_LEATHER) upperC = Tag((Color){ 96, 62, 40, 255 }, T_LEATHER);
        Arm(sh, elbow, wrist, upperC, look.skin, &look, &b, look.jewel == JEWEL_BANDS);
    }
    (void)at;
}

static int LodFor(Vector3 p)
{
    float d = Vector3Distance(p, G.cam.position);
    return d < 26.0f ? LOD_FINE : d < 80.0f ? LOD_MID : LOD_FAR;
}

// The grips set for this person become points on their body (where the body is drawn from now).
static void LocalGrips(Vector3 pos)
{
    Matrix inv = MatrixInvert(rlGetMatrixTransform());
    for (int s = 0; s < 2; s++) {
        if (!grips[s].set || grips[s].local) continue;
        if (Vector3Distance(grips[s].at, pos) > 2.5f) {
            grips[s].set = false;
            continue;
        }
        grips[s].at = Vector3Transform(grips[s].at, inv);
        grips[s].local = true;
    }
}

void DrawHuman(Vector3 pos, float facing, float stride, Look look, bool crouch, bool bow, float draw)
{
    flashWas = DrawFlash();
    SetDrawFlash(fmaxf(flashWas, FlashAt(pos)));
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    LocalGrips(pos);
    Body(pos, stride, look, crouch, bow, draw, LodFor(pos));
    rlPopMatrix();
    grips[0].set = grips[1].set = false;
    SetDrawFlash(flashWas);
}

void DrawPersonBody(Vector3 pos, float facing, Look look)
{
    grips[0].set = grips[1].set = false;
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + 0.16f, pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    rlRotatef(-90.0f, 1, 0, 0);
    Body(pos, 0, look, false, false, 0, LodFor(pos));
    rlPopMatrix();
}
