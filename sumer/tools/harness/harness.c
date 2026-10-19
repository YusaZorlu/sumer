// Test harness: links the game without main.c. Usage: harness <mode> [args]
//   stats            counts boxes, resources, animals, fighters; times the update loop
//   topo <out.png>   top-down map of terrain height, water, solids, roads
//   shot <out.png> x z yaw pitch dist hour [frames]   screenshot from a viewpoint
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

static void Shot(const char *out, float x, float z, float yaw, float pitch, float dist, float hour, int frames)
{
    Player *p = &G.player;
    p->pos = (Vector3){ x, GroundHeight(x, z, 50.0f), z };
    p->yaw = yaw;
    p->pitch = pitch;
    p->dist = dist;
    G.camDist = dist;
    G.hour = hour;
    G.panel = PANEL_NONE;
    for (int f = 0; f < frames; f++) {
        GameUpdate();
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
        Shot(out, a->pos.x + ox, a->pos.z + oz, ang + 0.35f, pitch, camDist, 11.0f, frames);
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
    G.msgT = 0;
    for (int f = 0; f < 3; f++) {
        G.panel = (Panel)panel;
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
    Shot(out, -3, -15, 2.03f, 0.3f, 7.0f, 11.0f, 3);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    SetTraceLogLevel(LOG_WARNING);
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
    } else if (strcmp(argv[1], "wetways") == 0) {
        WetWays();
    } else if (strcmp(argv[1], "textmap") == 0 && argc > 6) {
        TextMap((float)atof(argv[2]), (float)atof(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]));
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
    } else if (strcmp(argv[1], "ui") == 0 && argc > 4) {
        ShotPanel(argv[2], atoi(argv[3]), atoi(argv[4]), argc > 5 ? atoi(argv[5]) : 0);
    } else if (strcmp(argv[1], "mace") == 0 && argc > 3) {
        ShotMace(argv[2], (float)atof(argv[3]));
    } else if (strcmp(argv[1], "caravan") == 0 && argc > 2) {
        ShotCaravan(argv[2]);
    } else if (strcmp(argv[1], "animal") == 0 && argc > 7) {
        ShotAnimal(argv[2], atoi(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]), atoi(argv[7]));
    } else if (strcmp(argv[1], "shot") == 0 && argc > 8) {
        Shot(argv[2], (float)atof(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]), (float)atof(argv[7]),
             (float)atof(argv[8]), argc > 9 ? atoi(argv[9]) : 3);
    }
    CloseWindow();
    return 0;
}
