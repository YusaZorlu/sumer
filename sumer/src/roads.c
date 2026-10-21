#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stddef.h>

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define STRIP_STEP    2.0f
#define STRIP_LIFT    0.1f
#define STRIP_SHOWN   320.0f
#define MAX_SAMPLES   1800
#define MAX_PROFILE   4096

typedef struct { const Vector2 *p; int n; float width; } RoadDef;

// Out of the south gate to the edge of the world: the caravans come this way.
static const Vector2 SOUTH[] = { { 0, 80 }, { 0, 532 } };
// Out of the east gate, over the canal bridge and through the hills to the hunters.
static const Vector2 EAST[] = {
    { 96, 9 }, { 128, 9 }, { 160, 6 }, { 200, -14 }, { 236, -24 }, { 272, -6 }, { 308, 28 }, { 350, 36 }, { 395, 30 },
    { HUNTERS_X - 14, HUNTERS_Z + 2 },
};
// Down the trade road, then south-west over the downs to the herders.
static const Vector2 HERDERS[] = { { 0, 80 }, { 0, 172 }, { -24, 228 }, { -46, 296 }, { -58, 360 }, { HERDERS_X, HERDERS_Z - 14 } };
// Out of the north gate, between the barley fields, up to the ford.
static const Vector2 NORTH[] = { { -37, -86 }, { -37, -165 }, { -50, -250 }, { -75, -340 }, { -100, -430 }, { -120, -490 }, { -128, -512 } };
// From the west gate along the bank and over the harbor bridge to the old landing stage.
static const Vector2 FERRY[] = { { -96, 26 }, { -112, 22 }, { -115, 4 }, { -114, -24 }, { -112, -40 }, { -119, -56 }, { -124, -62 } };
// Tracks on the far bank, from the ford down to the old landing stage and on to the south.
static const Vector2 WEST_NORTH[] = { { -152, -512 }, { -176, -470 }, { -186, -380 }, { -182, -280 }, { -172, -170 }, { -163, -100 }, { -153, -66 } };
static const Vector2 WEST_SOUTH[] = { { -153, -58 }, { -170, -10 }, { -188, 80 }, { -205, 180 }, { -228, 280 }, { -262, 400 }, { -290, 520 } };

static const RoadDef ROADS_DEF[ROADS] = {
    [ROAD_SOUTH] = { SOUTH, COUNT(SOUTH), 6.0f },
    [ROAD_EAST] = { EAST, COUNT(EAST), 5.0f },
    [ROAD_HERDERS] = { HERDERS, COUNT(HERDERS), 4.0f },
    [ROAD_NORTH] = { NORTH, COUNT(NORTH), 4.0f },
    [ROAD_FERRY] = { FERRY, COUNT(FERRY), 4.0f },
    [ROAD_WEST_NORTH] = { WEST_NORTH, COUNT(WEST_NORTH), 3.5f },
    [ROAD_WEST_SOUTH] = { WEST_SOUTH, COUNT(WEST_SOUTH), 3.5f },
};

// The main streets inside the walls, shown on the map only.
static const Vector2 MAIN_STREET[] = { { 0, 80 }, { 0, -6 } };
static const Vector2 CROSS_STREET[] = { { -96, 9 }, { 96, 9 } };
static const Vector2 WEST_STREET[] = { { -96, 26 }, { -40, 26 } };
static const Vector2 NORTH_STREET[] = { { -37, -86 }, { -37, 9 } };
static const RoadDef STREETS[] = {
    { MAIN_STREET, COUNT(MAIN_STREET), 7.0f }, { CROSS_STREET, COUNT(CROSS_STREET), 5.0f },
    { WEST_STREET, COUNT(WEST_STREET), 5.0f }, { NORTH_STREET, COUNT(NORTH_STREET), 5.0f },
};

// Strip cross-sections: left and right edge, and whether this one is over water or inside the walls.
typedef struct { Vector3 l, r; bool skip; } Sample;

static Sample samples[MAX_SAMPLES];
static int sampleFirst[ROADS], sampleCount[ROADS];
static float roadLen[ROADS];
static float profile[MAX_PROFILE];
static int profileFirst[ROADS], profileCount[ROADS];

static float SegmentDistance(Vector2 p, Vector2 a, Vector2 b)
{
    Vector2 ab = Vector2Subtract(b, a);
    float len2 = Vector2DotProduct(ab, ab);
    float t = len2 > 0 ? Clamp(Vector2DotProduct(Vector2Subtract(p, a), ab) / len2, 0.0f, 1.0f) : 0.0f;
    return Vector2Distance(p, Vector2Add(a, Vector2Scale(ab, t)));
}

// How far a point is from the edge of the nearest road; 0 on the road itself.
float RoadDistance(float x, float z)
{
    Vector2 p = { x, z };
    float best = 1e9f;

    for (int r = 0; r < ROADS; r++) {
        const RoadDef *d = &ROADS_DEF[r];
        for (int i = 0; i + 1 < d->n; i++) {
            const Vector2 a = d->p[i], b = d->p[i + 1];
            // Cheap box test first: most segments are far away.
            if (x < fminf(a.x, b.x) - best - d->width || x > fmaxf(a.x, b.x) + best + d->width ||
                z < fminf(a.y, b.y) - best - d->width || z > fmaxf(a.y, b.y) + best + d->width)
                continue;
            float e = SegmentDistance(p, a, b) - d->width / 2;
            if (e < best) best = e;
        }
    }
    return fmaxf(best, 0.0f);
}

float RoadLength(int road)
{
    return road >= 0 && road < ROADS ? roadLen[road] : 0.0f;
}

// The point 's' metres along a road, standing on the ground or on a bridge deck.
Vector3 RoadPoint(int road, float s, Vector3 *dir)
{
    const RoadDef *d = &ROADS_DEF[road];
    Vector2 at = d->p[0], heading = { 0, 1 };

    s = Clamp(s, 0.0f, roadLen[road]);
    float left = s;
    for (int i = 0; i + 1 < d->n; i++) {
        Vector2 a = d->p[i], b = d->p[i + 1];
        float seg = Vector2Distance(a, b);
        heading = Vector2Normalize(Vector2Subtract(b, a));
        if (left <= seg || i + 2 == d->n) {
            at = Vector2Lerp(a, b, seg > 0 ? Clamp(left / seg, 0.0f, 1.0f) : 0.0f);
            break;
        }
        left -= seg;
    }
    if (dir) *dir = (Vector3){ heading.x, 0, heading.y };

    float y = TerrainHeight(at.x, at.y);
    if (profileCount[road] > 1) {
        float f = Clamp(s, 0.0f, (float)(profileCount[road] - 1) - 0.001f);
        int k = (int)f;
        y = Lerp(profile[profileFirst[road] + k], profile[profileFirst[road] + k + 1], f - (float)k);
    }
    return (Vector3){ at.x, y, at.y };
}

static Vector2 Normal(Vector2 a, Vector2 b)
{
    Vector2 t = Vector2Normalize(Vector2Subtract(b, a));
    return (Vector2){ -t.y, t.x };
}

static void SetSample(Sample *s, Vector2 c, Vector2 n, float half)
{
    Vector2 l = Vector2Add(c, Vector2Scale(n, half)), r = Vector2Subtract(c, Vector2Scale(n, half));
    s->l = (Vector3){ l.x, GroundSurface(l.x, l.y) + STRIP_LIFT, l.y };
    s->r = (Vector3){ r.x, GroundSurface(r.x, r.y) + STRIP_LIFT, r.y };
    s->skip = InWater(c.x, c.y) || InWater(l.x, l.y) || InWater(r.x, r.y) || InsideWalls(c.x, c.y, -3.0f);
}

// Strips follow the drawn ground (not the smooth height field), so hills never poke through them.
void BuildRoads(void)
{
    int profiles = 0, total = 0;

    for (int r = 0; r < ROADS; r++) {
        const RoadDef *d = &ROADS_DEF[r];
        float half = d->width / 2;

        roadLen[r] = 0;
        for (int i = 0; i + 1 < d->n; i++) roadLen[r] += Vector2Distance(d->p[i], d->p[i + 1]);

        sampleFirst[r] = total;
        sampleCount[r] = 0;
        for (int i = 0; i + 1 < d->n; i++) {
            Vector2 a = d->p[i], b = d->p[i + 1];
            Vector2 n = Normal(a, b);
            int steps = (int)ceilf(Vector2Distance(a, b) / STRIP_STEP);
            for (int k = 0; (k < steps || (k == steps && i + 2 == d->n)) && total < MAX_SAMPLES; k++) {
                Vector2 c = Vector2Lerp(a, b, (float)k / (float)steps), m = n;
                float w = half;
                // A corner gets a mitred cross-section so the strip neither gaps nor folds.
                if (k == 0 && i > 0) {
                    m = Vector2Normalize(Vector2Add(Normal(d->p[i - 1], a), n));
                    w = half / fmaxf(0.6f, Vector2DotProduct(m, n));
                }
                SetSample(&samples[total++], c, m, w);
                sampleCount[r]++;
            }
        }

        // Walkers keep to bridge decks: the standing height every metre. Until it is filled,
        // RoadPoint falls back to the bare ground.
        int n = (int)ceilf(roadLen[r]) + 2;
        if (n > MAX_PROFILE - profiles) n = MAX_PROFILE - profiles;
        profileFirst[r] = profiles;
        profileCount[r] = 0;
        for (int k = 0; k < n; k++) {
            Vector3 p = RoadPoint(r, (float)k, NULL);
            profile[profiles + k] = GroundHeight(p.x, p.z, fmaxf(p.y, 0.0f) + 0.6f);
        }
        profiles += n;
        profileCount[r] = n;
    }
}

void DrawRoads(void)
{
    const Color dirt = { 168, 148, 110, 255 };
    Vector3 eye = G.cam.position, fwd = Vector3Normalize(Vector3Subtract(G.cam.target, eye));
    bool all = DrawingAll();

    for (int r = 0; r < ROADS; r++) {
        rlCheckRenderBatchLimit(4 * sampleCount[r]);
        VBegin(RL_QUADS);
        for (int k = 0; k + 1 < sampleCount[r]; k++) {
            const Sample *a = &samples[sampleFirst[r] + k], *b = a + 1;
            if (a->skip || b->skip) continue;
            Vector3 mid = Vector3Lerp(a->l, b->r, 0.5f), v = Vector3Subtract(mid, eye);
            if (!all && (v.x * v.x + v.z * v.z > STRIP_SHOWN * STRIP_SHOWN || Vector3DotProduct(v, fwd) < -8.0f)) continue;
            // Counter-clockwise seen from above, so the strip's top is its front face.
            const Vector3 *pts[4] = { &a->l, &b->l, &b->r, &a->r };
            for (int q = 0; q < 4; q++) {
                const Vector3 *p = pts[q];
                G.drawTorch = GroundTorch(p->x, p->z);
                G.drawFog = FogAt(*p) * (1.0f - 0.6f * G.drawTorch);
                VColor(Lit(dirt, 0.97f));
                VVertex(p->x, p->y, p->z);
            }
        }
        VEnd();
    }
    G.drawTorch = G.drawFog = 0;
}

// Lines in one pass and the round joints at the bends in another: alternating the two would
// cost a draw call per segment.
static void MapLine(const RoadDef *d, float mx, float mz, float s, Color c, bool joints)
{
    float thick = Clamp(d->width * s * 0.8f, 2.0f, 6.0f);
    for (int i = 0; i + 1 < d->n; i++) {
        Vector2 a = { mx + d->p[i].x * s, mz + d->p[i].y * s }, b = { mx + d->p[i + 1].x * s, mz + d->p[i + 1].y * s };
        if (!joints) DrawLineEx(a, b, thick, c);
        else if (i + 2 < d->n) DrawCircleV(b, thick / 2, c);
    }
}

// Roads on the minimap: world x to the right, world z down.
void DrawRoadMap(float mx, float mz, float s)
{
    const Color road = { 236, 214, 152, 235 }, street = { 214, 192, 140, 200 };
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < COUNT(STREETS); i++) MapLine(&STREETS[i], mx, mz, s, street, pass == 1);
        for (int r = 0; r < ROADS; r++) MapLine(&ROADS_DEF[r], mx, mz, s, road, pass == 1);
    }
}
