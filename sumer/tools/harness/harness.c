// Test harness: links the game without main.c. Usage: harness <mode> [args]
//   stats            counts boxes, resources, animals, fighters; times the update loop
//   topo <out.png>   top-down map of terrain height, water, solids, roads
//   shot <out.png> x z yaw pitch dist hour [frames] [rain]   screenshot from a viewpoint (rain 1: in the rain);
//   shotin: the same standing on the floor indoors
#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include "rlgl.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double Now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void Stats(void)
{
    int solid = 0, water = 0, blobs = 0;
    for (int i = 0; i < G.boxCount; i++) {
        solid += G.boxes[i].solid;
        water += G.boxes[i].water;
        blobs += G.boxes[i].shape == SHAPE_BLOB;
    }
    int kinds[RES_KINDS] = { 0 };
    for (int i = 0; i < G.resCount; i++) kinds[G.res[i].kind]++;
    int types[16] = { 0 };
    for (int i = 0; i < G.animalCount; i++) types[G.animals[i].type]++;
    printf("boxes %d/%d (solid %d, water %d, blobs %d)\n", G.boxCount, MAX_BOXES, solid, water, blobs);
    printf("resources %d/%d:", G.resCount, MAX_RES);
    for (int k = 0; k < RES_KINDS; k++) printf(" %d", kinds[k]);
    printf("\nanimals %d/%d:", G.animalCount, MAX_ANIMALS);
    for (int k = 0; k < AN_TYPES; k++) printf(" %d", types[k]);
    printf("\nvillagers %d workers %d torches %d ladders %d\n", G.villagerCount, G.workerCount, G.torchCount, G.ladderCount);
}

static void TimeUpdate(int frames)
{
    G.panel = PANEL_NONE;
    double t0 = Now();
    for (int f = 0; f < frames; f++) GameUpdate();
    double t1 = Now();
    printf("GameUpdate: %.3f ms/frame over %d frames (fighters %d)\n", (t1 - t0) * 1000.0 / frames, frames, FighterCount());
    double t2 = Now();
    for (int f = 0; f < 10; f++) {
        BeginDrawing();
        GameDraw();
        EndDrawing();
    }
    double t3 = Now();
    printf("GameDraw incl. software raster: %.1f ms/frame\n", (t3 - t2) * 1000.0 / 10);
}

static void Topo(const char *out)
{
    const int N = 1080;
    Image im = GenImageColor(N, N, BLACK);
    float scale = 2.0f * WORLD_HALF / (float)N;
    for (int py = 0; py < N; py++) {
        for (int px = 0; px < N; px++) {
            float x = -WORLD_HALF + ((float)px + 0.5f) * scale, z = -WORLD_HALF + ((float)py + 0.5f) * scale;
            float h = TerrainHeight(x, z);
            float f = Fertility(x, z);
            Color base = Mix((Color){ 196, 186, 130, 255 }, (Color){ 110, 150, 70, 255 }, f);
            float k = h / 30.0f;
            Color c = Mix(base, (Color){ 120, 80, 50, 255 }, k > 1 ? 1 : k);
            // contour lines every 2 m
            float frac = h / 2.0f - (float)(int)(h / 2.0f);
            if (h > 0.2f && frac < 0.08f) c = Mix(c, BLACK, 0.45f);
            ImageDrawPixel(&im, px, py, c);
        }
    }
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (!b->solid && !b->water) continue;
        int x0 = (int)((b->pos.x - b->size.x / 2 + WORLD_HALF) / scale), x1 = (int)((b->pos.x + b->size.x / 2 + WORLD_HALF) / scale);
        int z0 = (int)((b->pos.z - b->size.z / 2 + WORLD_HALF) / scale), z1 = (int)((b->pos.z + b->size.z / 2 + WORLD_HALF) / scale);
        Color c = b->water ? (Color){ 58, 110, 160, 255 } : (b->shape == SHAPE_CYL ? (Color){ 60, 90, 40, 255 } : (Color){ 90, 60, 40, 255 });
        ImageDrawRectangle(&im, x0, z0, x1 - x0 + 1, z1 - z0 + 1, c);
    }
    for (int rd = 0; rd < ROADS; rd++) {
        float len = RoadLength(rd);
        for (float s = 0; s < len; s += 1.0f) {
            Vector3 p = RoadPoint(rd, s, NULL);
            ImageDrawCircle(&im, (int)((p.x + WORLD_HALF) / scale), (int)((p.z + WORLD_HALF) / scale), 2, (Color){ 250, 240, 200, 255 });
        }
    }
    for (int i = 0; i < G.resCount; i++) {
        const Resource *r = &G.res[i];
        static const Color rc[] = { { 230, 120, 40, 255 }, { 200, 200, 60, 255 }, { 20, 110, 20, 255 }, { 150, 90, 60, 255 },
                                    { 40, 200, 160, 255 }, { 120, 160, 255, 255 }, { 220, 60, 220, 255 }, { 255, 255, 255, 255 } };
        ImageDrawCircle(&im, (int)((r->pos.x + WORLD_HALF) / scale), (int)((r->pos.z + WORLD_HALF) / scale), 2, rc[r->kind % 8]);
    }
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        static const Color ac[] = { { 255, 200, 0, 255 }, { 255, 140, 0, 255 }, { 120, 40, 0, 255 }, { 255, 255, 255, 255 },
                                    { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 0, 255, 255 } };
        ImageDrawCircle(&im, (int)((a->pos.x + WORLD_HALF) / scale), (int)((a->pos.z + WORLD_HALF) / scale), 3, ac[a->type % 8]);
    }
    ExportImage(im, out);
    UnloadImage(im);
}

static bool indoors;   // shotin: stand on the floor under a roof, not on the roof

static void Shot(const char *out, float x, float z, float yaw, float pitch, float dist, float hour, int frames, bool rain)
{
    Player *p = &G.player;
    // SHOT_FLOOR=<height> stands indoors on the floor just below that height (a ziggurat's upper rooms).
    float from = getenv("SHOT_FLOOR") ? (float)atof(getenv("SHOT_FLOOR")) : indoors ? 1.0f : 50.0f;
    p->pos = (Vector3){ x, GroundHeight(x, z, from), z };
    p->yaw = yaw;
    p->pitch = pitch;
    p->dist = dist;
    G.camDist = dist;
    G.hour = hour;
    G.panel = PANEL_NONE;
    // SHOT_FP=1: through the player's eyes; SHOT_EQUIP=<0-3>: what is in hand; SHOT_AIM=<0-1>: the bow drawn that far.
    if (getenv("SHOT_FP")) G.firstPerson = atoi(getenv("SHOT_FP")) != 0;
    if (getenv("SHOT_EQUIP")) G.equip = atoi(getenv("SHOT_EQUIP"));
    if (getenv("SHOT_AIM")) G.arrowCam = false;   // the loosed arrows must not carry the camera off
    // SHOT_ARROW=1: an arrow is loosed straight ahead on the first frame and the camera rides behind it.
    for (int f = 0; f < frames; f++) {
        if (f == 1 && getenv("SHOT_ARROW")) {
            Vector3 from = { p->pos.x, p->pos.y + 1.5f, p->pos.z };
            G.arrowCam = true;
            G.followArrow = ShootArrow(from, Vector3Add(from, (Vector3){ -sinf(p->yaw) * 60, 2, -cosf(p->yaw) * 60 }), 1.0f);
            G.followT = 0;
        }
        if (getenv("SHOT_AIM")) {
            p->aiming = true;
            p->draw = (float)atof(getenv("SHOT_AIM"));
        }
        GameUpdate();
        if (getenv("SHOT_AIM")) p->aiming = true;
        if (rain) G.raining = true;
        BeginDrawing();
        GameDraw();
        EndDrawing();
    }
    BeginDrawing();
    GameDraw();
    rlDrawRenderBatchActive();
    Image im = LoadImageFromScreen();
    ExportImage(im, out);
    UnloadImage(im);
    EndDrawing();
}


// Things meant to stand on the ground that hang above the drawn ground somewhere under them: each one's
// gap, place, size and shape, worst first, and how many in all. floaters [min gap, default 0.08 m]
static void Floaters(float minGap)
{
    int count = 0, shown = 0;
    float worst = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < G.boxCount; i++) {
            const Box *b = &G.boxes[i];
            if (b->water || b->glow) continue;
            float bottom = b->pos.y - b->size.y / 2, lo = 1e9f, hi = -1e9f;
            for (int u = 0; u <= 2; u++)
                for (int v = 0; v <= 2; v++) {
                    float x = b->pos.x + b->size.x * ((float)u / 2 - 0.5f) * 0.9f, z = b->pos.z + b->size.z * ((float)v / 2 - 0.5f) * 0.9f;
                    float g = GroundSurface(x, z);
                    lo = fminf(lo, g);
                    hi = fmaxf(hi, g);
                }
            // Standing on the ground: its bottom within half a metre of the highest ground under it.
            if (bottom > hi + 0.5f || bottom - lo < minGap || lo < WATER_LEVEL) continue;
            // Resting on something else (a floor, a plinth, a quay) is not floating.
            bool held = false;
            for (int k = 0; k < G.boxCount && !held; k++) {
                const Box *o = &G.boxes[k];
                float top = o->pos.y + o->size.y / 2;
                held = k != i && !o->glow && top > bottom - 0.12f && o->pos.y - o->size.y / 2 < bottom - 0.05f &&
                       fabsf(o->pos.x - b->pos.x) < (o->size.x + b->size.x) / 2 && fabsf(o->pos.z - b->pos.z) < (o->size.z + b->size.z) / 2;
            }
            if (held) continue;
            if (pass == 0) {
                count++;
                worst = fmaxf(worst, bottom - lo);
            } else if (bottom - lo > worst * 0.5f && shown < 40) {
                shown++;
                printf("gap %.2f at (%.1f, %.1f) size %.2f x %.2f x %.2f shape %d built %d solid %d color %d,%d,%d\n", bottom - lo, b->pos.x, b->pos.z,
                       b->size.x, b->size.y, b->size.z, b->shape, b->built, b->solid, b->color.r, b->color.g, b->color.b);
            }
        }
    }
    printf("%d floating (gap over %.2f m), the worst %.2f m\n", count, minGap, worst);
}

// Where any walker's way dips into a canal bed: every villager lane, street edge and patrol sampled
// every 0.5 m; prints each wet stretch. wetways
static void WetWays(void)
{
    int found = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (v->road >= 0 || v->from >= 0 || v->speed <= 0) continue;
        float len = XZDist(v->a, v->b);
        for (float s = 0; s <= len; s += 0.5f) {
            float x = v->a.x + (v->b.x - v->a.x) * s / len, z = v->a.z + (v->b.z - v->a.z) * s / len;
            if (TerrainHeight(x, z) < -0.15f || InWater(x, z)) {
                printf("villager %d kind %d lane (%.1f,%.1f)-(%.1f,%.1f): wet at (%.1f,%.1f) ground %.2f\n", i, v->kind, v->a.x, v->a.z, v->b.x, v->b.z, x, z, TerrainHeight(x, z));
                found++;
                break;
            }
        }
    }
    for (int n = 0; n < StreetNodeCount(); n++) {
        Vector3 a = StreetNode(n);
        if (TerrainHeight(a.x, a.z) < -0.15f) printf("street node %d wet\n", n);
    }
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (v->from < 0) continue;
        float len = XZDist(v->a, v->b);
        for (float s = 0; s <= len; s += 0.5f)
            for (float side = -2.2f; side <= 2.21f; side += 1.1f) {
                float dx = (v->b.x - v->a.x) / len, dz = (v->b.z - v->a.z) / len;
                float x = v->a.x + dx * s + dz * side, z = v->a.z + dz * s - dx * side;
                if (TerrainHeight(x, z) < -0.15f) { printf("street %d-%d wet at (%.1f,%.1f) side %.1f\n", v->from, v->to, x, z, side); found++; s = len + 1; break; }
            }
    }
    for (int i = 0; i < G.workerCount; i++) {
        const Worker *w = &G.workers[i];
        float len = XZDist(w->hut, w->spot);
        for (float s = 0; s <= len; s += 0.5f) {
            float x = w->hut.x + (w->spot.x - w->hut.x) * s / len, z = w->hut.z + (w->spot.z - w->hut.z) * s / len;
            if (w->job != JOB_BOAT && (TerrainHeight(x, z) < -0.15f || SolidAt(x, z, 0.2f))) {
                printf("worker %d job %d (%.1f,%.1f)-(%.1f,%.1f): %s at (%.1f,%.1f)\n", i, w->job, w->hut.x, w->hut.z, w->spot.x, w->spot.z,
                       SolidAt(x, z, 0.2f) ? "solid" : "wet", x, z);
                found++;
                break;
            }
        }
    }
    for (int c = 0; c < ChannelCount(); c++) {
        bool wet;
        Rectangle r = ChannelRect(c, &wet, NULL);
        printf("channel %d: x %.0f..%.0f z %.0f..%.0f wet %d\n", c, r.x, r.x + r.width, r.y, r.y + r.height, wet);
    }
    printf("%d wet ways\n", found);
}


// A text map of a region every 'step' metres: ~ water, = road, # solid, otherwise ground height (0-9 m, + higher). textmap x0 z0 x1 z1 step
static void TextMap(float x0, float z0, float x1, float z1, float step)
{
    for (float z = z0; z <= z1; z += step) {
        printf("%6.0f ", z);
        for (float x = x0; x <= x1; x += step) {
            float h = TerrainHeight(x, z);
            char c = InWater(x, z) ? '~' : SolidAt(x, z, 0.2f) ? '#' : RoadDistance(x, z) < 2.5f ? '=' : h > 9.5f ? '+' : h < 0 ? '_' : (char)('0' + (int)h);
            putchar(c);
        }
        putchar('\n');
    }
    printf("       x from %.0f to %.0f\n", x0, x1);
}

#include "tests.c"

// Top-down map of a region: solid boxes dark, water blue, villager lanes red (a) to yellow (b),
// travellers' roads skipped. citymap <out.png> x0 z0 x1 z1 pxPerMeter
static void CityMap(const char *out, float x0, float z0, float x1, float z1, float k)
{
    int w = (int)((x1 - x0) * k), h = (int)((z1 - z0) * k);
    Image im = GenImageColor(w, h, (Color){ 214, 196, 150, 255 });
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (!b->solid && !b->water) continue;
        if (b->pos.y - b->size.y / 2 > TerrainHeight(b->pos.x, b->pos.z) + 1.0f) continue;
        Color c = b->water ? (Color){ 70, 120, 150, 255 } : (b->size.y > 2.0f ? (Color){ 110, 84, 60, 255 } : (Color){ 160, 130, 96, 255 });
        ImageDrawRectangle(&im, (int)((b->pos.x - b->size.x / 2 - x0) * k), (int)((b->pos.z - b->size.z / 2 - z0) * k),
                           (int)fmaxf(1, b->size.x * k), (int)fmaxf(1, b->size.z * k), c);
    }
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (v->road >= 0) continue;
        int ax = (int)((v->a.x - x0) * k), az = (int)((v->a.z - z0) * k), bx = (int)((v->b.x - x0) * k), bz = (int)((v->b.z - z0) * k);
        ImageDrawLine(&im, ax, az, bx, bz, (Color){ 200, 30, 30, 255 });
        ImageDrawCircle(&im, ax, az, 3, (Color){ 200, 30, 30, 255 });
        ImageDrawCircle(&im, bx, bz, 3, (Color){ 240, 200, 0, 255 });
        ImageDrawText(&im, TextFormat("%d", i), ax + 4, az + 2, 10, BLACK);
    }
    for (int gx = (int)ceilf(x0 / 10) * 10; gx < x1; gx += 10)
        ImageDrawLine(&im, (int)((gx - x0) * k), 0, (int)((gx - x0) * k), h, (Color){ 0, 0, 0, gx % 50 ? 25 : 70 });
    for (int gz = (int)ceilf(z0 / 10) * 10; gz < z1; gz += 10)
        ImageDrawLine(&im, 0, (int)((gz - z0) * k), w, (int)((gz - z0) * k), (Color){ 0, 0, 0, gz % 50 ? 25 : 70 });
    ExportImage(im, out);
    UnloadImage(im);
}

// A candidate street network: every edge sampled every 0.5 m for walls and houses (0.6 m clearance),
// drawn over the city map. streets <out.png>
static const float SN[][2] = {
    { 0, -3 }, { 0, 9 }, { 0, 39 }, { 0, 72 }, { -37, 9 }, { -54, 9 }, { -86, 9 }, { 35, 9 }, { 65, 9 }, { 86, 9 },
    { -37, -35 }, { -37, -74 }, { -45, 39 }, { -76, 39 }, { 45, 39 }, { 76, 39 }, { -60, 26 }, { -84, 26 }, { -54.5f, -18 },
};
static const int SE[][2] = {
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 1, 4 }, { 4, 5 }, { 5, 6 }, { 1, 7 }, { 7, 8 }, { 8, 9 }, { 4, 10 }, { 10, 11 },
    { 2, 12 }, { 12, 13 }, { 2, 14 }, { 14, 15 }, { 16, 17 }, { 5, 18 }, { 16, 5 },
};
static void StreetCheck(const char *out)
{
    const float x0 = -110, z0 = -100, k = 5;
    int nn = (int)(sizeof(SN) / sizeof(SN[0])), ne = (int)(sizeof(SE) / sizeof(SE[0]));
    CityMap(out, -110, -100, 110, 90, 5);
    Image im = LoadImage(out);
    for (int e = 0; e < ne; e++) {
        float ax = SN[SE[e][0]][0], az = SN[SE[e][0]][1], bx = SN[SE[e][1]][0], bz = SN[SE[e][1]][1];
        float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
        int bad = 0;
        for (float s = 0; s <= len; s += 0.5f) {
            float x = ax + (bx - ax) * s / len, z = az + (bz - az) * s / len;
            if (SolidAt(x, z, 0.6f) || InWater(x, z)) {
                bad++;
                ImageDrawCircle(&im, (int)((x - x0) * k), (int)((z - z0) * k), 3, RED);
            }
        }
        printf("edge %2d: %2d-%2d  %5.1f m  %s\n", e, SE[e][0], SE[e][1], len, bad ? TextFormat("BLOCKED at %d samples", bad) : "clear");
        ImageDrawLine(&im, (int)((ax - x0) * k), (int)((az - z0) * k), (int)((bx - x0) * k), (int)((bz - z0) * k), bad ? RED : (Color){ 0, 120, 255, 255 });
    }
    for (int i = 0; i < nn; i++) {
        ImageDrawCircle(&im, (int)((SN[i][0] - x0) * k), (int)((SN[i][1] - z0) * k), 4, (Color){ 0, 90, 220, 255 });
        ImageDrawText(&im, TextFormat("%d", i), (int)((SN[i][0] - x0) * k) + 5, (int)((SN[i][1] - z0) * k) - 12, 10, (Color){ 0, 0, 160, 255 });
        printf("node %2d (%.0f, %.0f) inside walls by 2 m: %d\n", i, SN[i][0], SN[i][1], InsideWalls(SN[i][0], SN[i][1], 4.0f));
    }
    ExportImage(im, out);
    UnloadImage(im);
}


// Frames the first active animal of a type: the player stands 'dist' metres from it, the camera
// looks past the player's shoulder toward it.
static void ShotAnimal(const char *out, int type, float dist, float camDist, float pitch, int frames)
{
    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (!a->active || (int)a->type != type) continue;
        printf("animal %d type %d at %.1f %.1f state %d\n", i, type, a->pos.x, a->pos.z, a->state);
        float ang = 0.9f;
        float ox = sinf(ang) * dist, oz = cosf(ang) * dist;
        Shot(out, a->pos.x + ox, a->pos.z + oz, ang + 0.35f, pitch, camDist, 11.0f, frames, false);
        printf("after: state %d hp %d health %.1f at %.1f %.1f\n", a->state, a->hp, G.health, a->pos.x, a->pos.z);
        return;
    }
}

// One frame of a panel: ui <out.png> <panel> <lang 0/1> [trader]
static void ShotPanel(const char *out, int panel, int lang, int trader)
{
    SetLanguage(lang);
    G.hasMace = true;
    BagAdd(IT_HERB, 4);
    BagAdd(IT_SHEEP, 1);
    BagAdd(IT_WOLF, 2);
    BagAdd(IT_BREAD, 3);
    G.herbT = 12.0f;
    G.trader = trader;
    if (panel == PANEL_CONSOLE) {
        G.devTools = true;
        ConsoleRun("help");
        ConsoleRun("where");
        ConsoleRun("give dates 5");
    }
    G.msgT = 0;
    for (int f = 0; f < 3; f++) {
        G.panel = (Panel)panel;
        if (panel == PANEL_LOOK) FollowCamera(0.016f);
        BeginDrawing();
        GameDraw();
        EndDrawing();
    }
    BeginDrawing();
    GameDraw();
    rlDrawRenderBatchActive();
    Image im = LoadImageFromScreen();
    ExportImage(im, out);
    UnloadImage(im);
    EndDrawing();
}

// The war mace mid-swing, seen from the side.
static void ShotMace(const char *out, float swing)
{
    G.hasMace = true;
    G.equip = EQ_MACE;
    Player *p = &G.player;
    p->pos = (Vector3){ 60, 0, 120 };
    p->yaw = 1.2f;
    p->pitch = 0.25f;
    p->dist = 4.0f;
    G.camDist = 4.0f;
    G.msgT = 0;
    for (int f = 0; f < 3; f++) {
        GameUpdate();
        BeginDrawing();
        GameDraw();
        EndDrawing();
    }
    p->facing = p->yaw + PI - 1.2f;
    G.swingT = swing;
    BeginDrawing();
    GameDraw();
    rlDrawRenderBatchActive();
    Image im = LoadImageFromScreen();
    ExportImage(im, out);
    UnloadImage(im);
    EndDrawing();
}

// Lets the first caravan reach its stall, then looks at it.
static void ShotCaravan(const char *out)
{
    for (int k = 0; k < 600 && G.caravans[0].state != CV_TRADE; k++) UpdateCaravans(0.05f);
    Shot(out, -3, -15, 2.03f, 0.3f, 7.0f, 11.0f, 3, false);
}

// A line-up of people to look at: lineup <out.png> <variant> [hour]
//   0 the player's choices (hair, beard, dress, jewellery) on six players' colors, with armor on the last three
//   1 the townsfolk, priests, guards, merchants, workers and raiders
//   2 poses: walking, crouching, drawing a bow, a sword raised, struck down, the mace swept, a spear thrust
//   3 close up: three heads
static int lineupVariant;
static Vector3 lineupAt;

static void Lineup(void)
{
    Vector3 at = lineupAt;
    float t = (float)GetTime();
    for (int k = 0; k < 7; k++) {
        Vector3 p = { at.x - 4.5f + 1.5f * (float)k, at.y, at.z + 3.0f };
        LightFor(p);
        if (lineupVariant == 0) {
            static const unsigned char styles[7][4] = {
                { HAIR_SHORT, BEARD_FULL, DRESS_FLEECE, JEWEL_NONE }, { HAIR_LONG, BEARD_LONG, DRESS_SHAWL, JEWEL_GOLD },
                { HAIR_SHAVED, BEARD_NONE, DRESS_KILT, JEWEL_BEADS }, { HAIR_BUN, BEARD_SHORT, DRESS_ROBE, JEWEL_BANDS },
                { HAIR_SHORT, BEARD_SHORT, DRESS_KILT, JEWEL_NONE }, { HAIR_LONG, BEARD_FULL, DRESS_FLEECE, JEWEL_BEADS },
                { HAIR_BUN, BEARD_LONG, DRESS_ROBE, JEWEL_GOLD },
            };
            static const int armor[7] = { 0, 0, 0, 0, ARMOR_WOLF, ARMOR_LEATHER, ARMOR_BRONZE };
            Look l = PlayerLook(k % MAX_PLAYERS, PackStyle(styles[k][0], styles[k][1], styles[k][2], styles[k][3]), armor[k]);
            DrawHuman(p, PI, 0, l, false, false, 0);
        } else if (lineupVariant == 1) {
            static const int roles[7] = { LOOK_TOWN, LOOK_TOWN, LOOK_PRIEST, LOOK_GUARD, LOOK_MERCHANT, LOOK_WORKER, LOOK_BARBARIAN };
            static const unsigned seeds[7] = { 3, 8, 1, 4, 2, 5, 6 };
            Look l = PersonLook(roles[k], seeds[k], (Color){ 150, 90, 70, 255 });
            if (k == 1) l = PersonLook(LOOK_TOWN, 11, (Color){ 70, 100, 150, 255 });
            for (unsigned s = 0; k <= 1 && s < 40 && l.woman != (k == 1); s++) l = PersonLook(LOOK_TOWN, 20 + s, (Color){ 70, 100, 150, 255 });
            if (k == 2) for (unsigned s = 0; s < 40 && !l.woman; s++) l = PersonLook(LOOK_PRIEST, 40 + s, (Color){ 200, 180, 140, 255 });
            DrawHuman(p, PI, 0, l, false, false, 0);
        } else if (lineupVariant == 2) {
            Look l = PlayerLook(k % MAX_PLAYERS, PackStyle(HAIR_LONG, BEARD_FULL, k & 1 ? DRESS_KILT : DRESS_FLEECE, JEWEL_BANDS), k == 6 ? ARMOR_BRONZE : 0);
            float f = PI * 0.62f;
            switch (k) {
            case 0: DrawHuman(p, f, 1.2f, l, false, false, 0); break;
            case 1: DrawHuman(p, f, 0, l, true, false, 0); break;
            case 2: DrawHuman(p, f, 0, l, false, true, 0.9f); break;
            case 3: HoldWeapon(p, f, false, false, 0.2f, false); DrawWeaponPart(WEAPON_SWORD); DrawHuman(p, f, 0, l, false, false, 0); break;
            case 4: HoldWeapon(p, f, false, false, CONTACT_TIME, false); DrawWeaponPart(WEAPON_SWORD); DrawHuman(p, f, 0, l, false, false, 0); break;
            case 5: HoldWeapon(p, f, false, true, CONTACT_TIME - 0.05f, false); DrawWeaponPart(WEAPON_MACE); DrawHuman(p, f, 0, l, false, false, 0); break;
            default: {
                rlPushMatrix();
                rlTranslatef(p.x, p.y, p.z);
                rlRotatef(f * RAD2DEG, 0, 1, 0);
                GripHere(HAND_WEAPON, 0.3f, 1.35f, 0.85f - 0.2f);
                DrawShadedBox((Vector3){ 0.3f, 1.35f, 0.85f }, (Vector3){ 0.05f, 0.05f, 2.4f }, COL_WOOD);
                rlPopMatrix();
                DrawHuman(p, f, 0, PersonLook(LOOK_GUARD, 4, (Color){ 120, 100, 80, 255 }), false, false, 0);
            }
            }
        } else if (k >= 2 && k <= 4) {
            static const unsigned char styles[3] = { 0 };
            (void)styles;
            Look l = k == 2 ? PlayerLook(0, PackStyle(HAIR_BUN, BEARD_LONG, DRESS_SHAWL, JEWEL_GOLD), 0)
                   : k == 3 ? PersonLook(LOOK_TOWN, 21, (Color){ 70, 100, 150, 255 }) : PlayerLook(1, PackStyle(HAIR_SHAVED, BEARD_NONE, DRESS_FLEECE, JEWEL_BEADS), 0);
            for (unsigned s = 0; k == 3 && s < 40 && !l.woman; s++) l = PersonLook(LOOK_TOWN, 20 + s, (Color){ 70, 100, 150, 255 });
            DrawHuman((Vector3){ at.x - 0.45f * (float)(3 - k), at.y, at.z + 3.0f }, PI, 0, l, false, false, 0);
        }
    }
    (void)t;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    SetTraceLogLevel(LOG_WARNING);
    if (getenv("SUMER_ROYALE")) SetRoyale(true);
    InitWindow(1280, 720, "harness");
    rlSetClipPlanes(0.1, 1200.0);
    GameInit();
    SetPanel(PANEL_NONE);
    if (strcmp(argv[1], "stats") == 0) {
        Stats();
        TimeUpdate(argc > 2 ? atoi(argv[2]) : 300);
    } else if (strcmp(argv[1], "streets") == 0 && argc > 2) {
        StreetCheck(argv[2]);
    } else if (strcmp(argv[1], "citymap") == 0 && argc > 7) {
        CityMap(argv[2], (float)atof(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]), (float)atof(argv[7]));
    } else if (strcmp(argv[1], "topo") == 0 && argc > 2) {
        Topo(argv[2]);
    } else if (strcmp(argv[1], "heights") == 0 && argc > 6) {
        // heights x0 z0 x1 z1 step: a text grid of terrain heights
        float x0 = (float)atof(argv[2]), z0 = (float)atof(argv[3]), x1 = (float)atof(argv[4]), z1 = (float)atof(argv[5]), st = (float)atof(argv[6]);
        printf("%7s", "z\\x");
        for (float x = x0; x <= x1; x += st) printf("%6.0f", x);
        printf("\n");
        for (float z = z0; z <= z1; z += st) {
            printf("%7.0f", z);
            for (float x = x0; x <= x1; x += st) printf("%6.1f", TerrainHeight(x, z));
            printf("\n");
        }
    } else if (strcmp(argv[1], "spot") == 0 && argc > 3) {
        // Is (x, z) a place someone could stand? spot x z
        float x = (float)atof(argv[2]), z = (float)atof(argv[3]);
        printf("%.1f %.1f: ground %.2f solid %d water %d inside %d keepout %d\n", x, z, TerrainHeight(x, z), SolidAt(x, z, 0.3f), InWater(x, z), InsideWalls(x, z, 0.0f), InKeepOut(x, z, 0.0f));
    } else if (strcmp(argv[1], "swingshot") == 0 && argc > 4) {
        // A swing seen from the side, t seconds in, with a townsman to strike: swingshot <out.png> <t> <mace 0/1>
        float t = (float)atof(argv[3]);
        bool mace = atoi(argv[4]) != 0;
        G.hasMace = true;
        G.equip = mace ? EQ_MACE : EQ_MELEE;
        G.player.pos = (Vector3){ 38, GroundHeight(38, 20, 50.0f), 20 };
        G.player.yaw = 0;
        G.player.dist = 4.0f;
        G.camDist = 4.0f;
        G.hour = 10;
        for (int f = 0; f < 3; f++) {
            GameUpdate();
            BeginDrawing();
            GameDraw();
            EndDrawing();
        }
        G.player.facing = PI;
        G.swingT = (mace ? MACE_TIME : SWING_TIME) - t;
        G.strikePending = false;
        Vector3 at = G.player.pos;
        G.cam.position = (Vector3){ at.x + 3.2f, at.y + 1.7f, at.z - 0.8f };
        G.cam.target = (Vector3){ at.x, at.y + 1.3f, at.z - 0.8f };
        BeginDrawing();
        GameDraw();
        rlDrawRenderBatchActive();
        Image im = LoadImageFromScreen();
        ExportImage(im, argv[2]);
        UnloadImage(im);
        EndDrawing();
    } else if (strcmp(argv[1], "lineup") == 0 && argc > 3) {
        lineupVariant = atoi(argv[3]);
        G.player.pos = (Vector3){ 38, GroundHeight(38, 20, 50.0f), 20 };
        G.hour = argc > 4 ? (float)atof(argv[4]) : 10.0f;
        G.camDist = 4.0f;
        for (int f = 0; f < 3; f++) {
            GameUpdate();
            BeginDrawing();
            GameDraw();
            EndDrawing();
        }
        ExtraFigures = Lineup;
        Vector3 at = lineupAt = G.player.pos;
        bool close = lineupVariant == 3;
        G.cam.position = (Vector3){ at.x + (close ? 0.0f : 0.0f), at.y + (close ? 1.8f : 1.55f), at.z + 3.0f - (close ? 1.25f : 6.2f) };
        G.cam.target = (Vector3){ at.x, at.y + (close ? 1.74f : 1.0f), at.z + 3.0f };
        G.player.pos.z -= 30.0f;
        for (int f = 0; f < 2; f++) {
            BeginDrawing();
            GameDraw();
            rlDrawRenderBatchActive();
            if (f == 1) {
                Image im = LoadImageFromScreen();
                ExportImage(im, argv[2]);
                UnloadImage(im);
            }
            EndDrawing();
        }
    } else if (strcmp(argv[1], "royale") == 0 && argc > 6) {
        // A battle royale round alone, t seconds in, seen from (x, z): royale <out.png> <x> <z> <yaw> <t> [statue]
        SetRoyale(true);
        GameInit();
        G.role = NET_SOLO;
        RoyaleStart();
        RoyaleSkipTo((float)atof(argv[6]));
        float x = (float)atof(argv[3]), z = (float)atof(argv[4]);
        if (argc > 7) {
            Vector3 p = StatuePos(atoi(argv[7]));
            x = p.x + 4.0f;
            z = p.z + 4.0f;
        }
        Shot(argv[2], x, z, (float)atof(argv[5]), 0.25f, 7.0f, 11.0f, 4, false);
    } else if (strcmp(argv[1], "sfx") == 0 && argc > 2) {
        // Every sound effect as a .wav file: sfx <dir>
        SfxExport(argv[2]);
    } else if (strcmp(argv[1], "guards") == 0) {
        // Every guard's post (a) and patrol end (b), then the same after a PvP game takes the ziggurat's away.
        for (int pass = 0; pass < 2; pass++) {
            if (pass) RemoveZigguratGuards();
            printf("%s: %d people\n", pass ? "PvP" : "solo", G.villagerCount);
            for (int i = 0; i < G.villagerCount; i++) {
                const Villager *v = &G.villagers[i];
                if (v->kind == NPC_GUARD)
                    printf("  guard %2d  a %6.1f %5.1f %6.1f  b %6.1f %5.1f %6.1f%s\n", i, v->a.x, v->a.y, v->a.z, v->b.x, v->b.y, v->b.z, v->templePost ? "  temple post" : "");
            }
        }
    } else if (strcmp(argv[1], "wetways") == 0) {
        WetWays();
    } else if (strcmp(argv[1], "textmap") == 0 && argc > 6) {
        TextMap((float)atof(argv[2]), (float)atof(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]));
    } else if (strcmp(argv[1], "spots") == 0) {
        // Free patches inside the city (nothing solid, no keep-out zone, no water) of 2*half+1 metres square: spots <x0> <z0> <x1> <z1> <step> <half>
        float x0 = (float)atof(argv[2]), z0 = (float)atof(argv[3]), x1 = (float)atof(argv[4]), z1 = (float)atof(argv[5]), st = (float)atof(argv[6]), half = (float)atof(argv[7]);
        int skip = argc > 8 ? atoi(argv[8]) : 0;
        for (float cz = z0; cz <= z1; cz += st)
            for (float cx = x0; cx <= x1; cx += st) {
                bool ok = true;
                for (float dz = -half; dz <= half && ok; dz += 1)
                    for (float dx = -half; dx <= half && ok; dx += 1) {
                        float x = cx + dx, z = cz + dz;
                        ok = (skip & 1 || !SolidAt(x, z, 0.2f)) && !InWater(x, z) && (skip & 2 || !InKeepOut(x, z, 0.0f)) && (skip & 4 || InsideWalls(x, z, 2.0f));
                    }
                if (ok) printf("%.0f %.0f\n", cx, cz);
            }
    } else if (strcmp(argv[1], "pvpshot") == 0 && argc > 3) {
        // How another player looks in a fight, and the red lights of a blow on you: pvpshot <out.png> <variant 0-4>
        //   0 raising a sword (yellow)  1 striking down (red)  2 flinching, with a blow from ahead and left on you
        //   3 sweeping the mace (yellow)  4 holding the sword up to parry (green)
        int v = atoi(argv[3]);
        G.role = NET_SOLO;
        Player *pl = &G.player;
        pl->pos = (Vector3){ 38, GroundHeight(38, 20, 50.0f), 20 };
        pl->yaw = 0;
        pl->pitch = 0.2f;
        pl->dist = 4.0f;
        G.camDist = 4.0f;
        G.hour = 12;
        for (int f = 0; f < 3; f++) {
            GameUpdate();
            BeginDrawing();
            GameDraw();
            EndDrawing();
        }
        G.role = NET_HOST;
        G.pvp = true;
        G.self = G.actor = 0;
        G.peers[1] = (Peer){ .active = true, .net = 1, .health = HEALTH_MAX, .flags = PF_GROUND, .equip = EQ_MELEE, .weapon = 2 };
        snprintf(G.peers[1].name, NAME_LEN, "Bob");
        Peer *b = &G.peers[1];
        b->pos = b->shown = (Vector3){ 40.4f, GroundHeight(40.4f, 16.8f, 50.0f), 16.8f };
        b->facing = b->shownFacing = atan2f(38 - 40.4f, 20 - 16.8f);
        b->lastHealth = HEALTH_MAX;
        if (v == 0) { b->swingT = 0.34f; b->mark = MARK_WARN; }
        if (v == 1) { b->swingT = 0.12f; b->mark = MARK_HIT; }
        if (v == 2) { b->hitT = 0.3f; b->hitX = 0; b->hitZ = -1; b->mark = MARK_PARRIED; G.hitLight[0] = 0.8f; G.hitLight[2] = 0.9f; G.hurtT = 0.2f; }
        if (v == 3) { b->equip = EQ_MACE; b->swingT = 0.28f; b->mark = MARK_WARN; }
        if (v == 4) { b->flags |= PF_PARRY; b->mark = MARK_PARRIED; }
        FollowCamera(1.0f);
        BeginDrawing();
        GameDraw();
        rlDrawRenderBatchActive();
        Image im = LoadImageFromScreen();
        ExportImage(im, argv[2]);
        UnloadImage(im);
        EndDrawing();
    } else if (strcmp(argv[1], "archers") == 0) {
        TestArchers();
        printf("%d passed, %d failed\n", passes, fails);
    } else if (strcmp(argv[1], "pvp") == 0) {
        TestPvp();
        printf("%d passed, %d failed\n", passes, fails);
    } else if (strcmp(argv[1], "canals") == 0) {
        TestCanals();
    } else if (strcmp(argv[1], "aim") == 0) {
        TestAim();
    } else if (strcmp(argv[1], "test") == 0) {
        RunTests();
    } else if (strcmp(argv[1], "load") == 0) {
        // Loads slot 1 (an old save), runs a few seconds of play and saves it in the new format.
        SetSaveSlot(0);
        bool ok = LoadGame();
        printf("load %s: day %d %.1fh at %.0f,%.0f weapon %d equip %d armor %d mace %d wolf pelts %d waste %d\n", ok ? "ok" : "FAILED",
               G.day, G.hour, G.player.pos.x, G.player.pos.z, G.weapon, G.equip, G.armor, G.hasMace, BagCount(IT_WOLF), BagCount(IT_WASTE));
        int held = 0, regrowing = 0;
        for (int i = 0; i < G.animalCount; i++) held += !G.animals[i].active;
        for (int i = 0; i < G.resCount; i++) regrowing += G.res[i].regrow > 0;
        printf("animals held for spring %d, resources regrowing %d (old indexes are dropped)\n", held, regrowing);
        for (int f = 0; f < 120; f++) {
            GameUpdate();
            BeginDrawing();
            GameDraw();
            EndDrawing();
        }
        printf("played 120 frames: day %d panel %d; resave %d\n", G.day, G.panel, SaveGame());
        GameInit();
        bool again = LoadGame();
        printf("reload %d: day %d %.1fh at %.0f,%.0f weapon %d armor %d pelts %d\n", again, G.day, G.hour, G.player.pos.x, G.player.pos.z, G.weapon, G.armor, BagCount(IT_WOLF));
    } else if (strcmp(argv[1], "floaters") == 0) {
        Floaters(argc > 2 ? (float)atof(argv[2]) : 0.08f);
    } else if (strcmp(argv[1], "ui") == 0 && argc > 4) {
        ShotPanel(argv[2], atoi(argv[3]), atoi(argv[4]), argc > 5 ? atoi(argv[5]) : 0);
    } else if (strcmp(argv[1], "mace") == 0 && argc > 3) {
        ShotMace(argv[2], (float)atof(argv[3]));
    } else if (strcmp(argv[1], "caravan") == 0 && argc > 2) {
        ShotCaravan(argv[2]);
    } else if (strcmp(argv[1], "animal") == 0 && argc > 7) {
        ShotAnimal(argv[2], atoi(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]), atoi(argv[7]));
    } else if ((strcmp(argv[1], "shot") == 0 || strcmp(argv[1], "shotin") == 0) && argc > 8) {
        indoors = strcmp(argv[1], "shotin") == 0;
        Shot(argv[2], (float)atof(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]), (float)atof(argv[7]),
             (float)atof(argv[8]), argc > 9 ? atoi(argv[9]) : 3, argc > 10 && atoi(argv[10]) != 0);
    }
    CloseWindow();
    return 0;
}
