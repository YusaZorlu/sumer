#include "game.h"
#include <math.h>
#include <stdio.h>

#define T1  7.0f
#define T2  12.0f
#define T3  16.5f

static const int GIFTS[][2] = { { IT_BREAD, 2 }, { IT_ROAST, 2 }, { IT_GRILLED, 2 }, { IT_EGG, 4 }, { IT_MILK, 2 } };

static void Block(float x0, float y0, float z0, float x1, float y1, float z1, Color c)
{
    AddBox((x0 + x1) / 2, y0, (z0 + z1) / 2, x1 - x0, y1 - y0, z1 - z0, c, true);
}

static void Trim(float x0, float y0, float z0, float x1, float y1, float z1, Color c)
{
    AddBox((x0 + x1) / 2, y0, (z0 + z1) / 2, x1 - x0, y1 - y0, z1 - z0, c, false);
}

// Climbs toward -z from zStart.
static void StairZ(float x0, float x1, float zStart, float run, float y0, float rise, int steps, Color c)
{
    for (int k = 0; k < steps; k++) {
        float za = zStart - run * (float)k;
        Block(x0, y0, za - run, x1, y0 + rise * (float)(k + 1), za, c);
    }
}

// Climbs along dir (+1 or -1 on x) from xStart.
static void StairX(float xStart, float dir, float run, float z0, float z1, float rise, int steps, Color c)
{
    for (int k = 0; k < steps; k++) {
        float xa = xStart + dir * run * (float)k, xb = xa + dir * run;
        Block(fminf(xa, xb), 0, z0, fmaxf(xa, xb), rise * (float)(k + 1), z1, c);
    }
}

static void BuildTemenos(void)
{
    const float x0 = -28, x1 = 28, z0 = -66, z1 = -6, t = 1.6f, h = 5.0f;

    AddBox(0, 0, z0 + t / 2, x1 - x0, h, t, COL_HOUSE_DARK, true);
    AddBox(x0 + t / 2, 0, (z0 + z1) / 2, t, h, z1 - z0, COL_HOUSE_DARK, true);
    AddBox(x1 - t / 2, 0, (z0 + z1) / 2, t, h, z1 - z0, COL_HOUSE_DARK, true);
    AddBox((x0 - 5) / 2, 0, z1 - t / 2, -5 - x0, h, t, COL_HOUSE_DARK, true);
    AddBox((x1 + 5) / 2, 0, z1 - t / 2, x1 - 5, h, t, COL_HOUSE_DARK, true);
    AddBox(-5.8f, 0, z1 - t / 2, 1.6f, 6.5f, 2.4f, COL_MUD_PALE, true);
    AddBox(5.8f, 0, z1 - t / 2, 1.6f, 6.5f, 2.4f, COL_MUD_PALE, true);
    AddBox(0, 0, (z0 + z1) / 2, x1 - x0 - 2 * t, 0.04f, z1 - z0 - 2 * t, (Color){ 186, 150, 110, 255 }, false);
    KeepOut(x0 - 2, z0 - 2, x1 + 2, z1 + 2);
}

static void BuildTier1(void)
{
    const Color c = COL_MUD, dark = COL_MUD_DARK;

    Block(-20, 0, -30, -5.2f, T1, -22, c);
    Block(-3.8f, 0, -30, 3.8f, T1, -22, c);
    Block(5.2f, 0, -30, 20, T1, -22, c);
    Block(-5.2f, 2.6f, -30, -3.8f, T1, -22, c);
    Block(3.8f, 2.6f, -30, 5.2f, T1, -22, c);
    Block(-20, 0, -60, 20, T1, -50, c);
    Block(-10, 6, -50, 10, T1, -30, c);

    for (int s = -1; s <= 1; s += 2) {
        float f = (float)s;
        Block(fminf(20 * f, 18 * f), 0, -50, fmaxf(20 * f, 18 * f), T1, -30, c);
        Block(fminf(18 * f, 10 * f), 0, -33, fmaxf(18 * f, 10 * f), T1, -30, c);
        Block(fminf(18 * f, 10 * f), 0, -50, fmaxf(18 * f, 10 * f), T1, -47, c);
        Block(fminf(11 * f, 10 * f), 0, -47, fmaxf(11 * f, 10 * f), T1, -45.4f, c);
        Block(fminf(11 * f, 10 * f), 0, -44.0f, fmaxf(11 * f, 10 * f), T1, -33, c);
        Block(fminf(11 * f, 10 * f), 2.4f, -45.4f, fmaxf(11 * f, 10 * f), T1, -44.0f, c);
        Block(fminf(18 * f, 11 * f), 6, -47, fmaxf(18 * f, 11 * f), T1, -33, c);

        for (int k = 0; k < 4; k++) AddShape(14.5f * f + (k - 1.5f) * 1.3f * f, 0, -46.3f, 0.7f, 1.1f, 0.7f, COL_POT, true, SHAPE_BLOB);
        for (int k = 0; k < 3; k++) AddBox(17.3f * f, 0, -42.0f + 2.2f * (float)k, 0.8f, 0.9f, 1.6f, s < 0 ? COL_STRAW : COL_WOOD, true);
        if (s > 0) {
            // The east wing: the temple's treasury, where metal is weighed in and out on a balance.
            for (int k = 0; k < 3; k++) AddBox(13.5f + 1.0f * (float)k, 0.9f, -34.0f, 0.6f, 0.25f, 0.4f, COL_BRONZE, false);
            AddBox(14.5f, 0, -34.0f, 3.2f, 0.9f, 0.9f, COL_WOOD, true);
            Trim(16.2f, 0.9f, -34.1f, 16.28f, 1.6f, -34.02f, COL_WOOD);
            Trim(15.7f, 1.55f, -34.1f, 16.8f, 1.6f, -34.02f, COL_WOOD);
            Trim(15.6f, 1.15f, -34.2f, 15.85f, 1.18f, -33.95f, COL_BRONZE);
            Trim(16.65f, 1.2f, -34.2f, 16.9f, 1.23f, -33.95f, COL_BRONZE);
            AddBox(12.6f, 0, -38.0f, 0.9f, 0.7f, 0.9f, COL_WOOD, true);
            AddBox(12.6f, 0.7f, -38.0f, 0.95f, 0.08f, 0.95f, COL_BRONZE, false);
        } else {
            // The west wing: the granary. Heaps of barley on mats, measured out by the temple's stores.
            for (int k = 0; k < 2; k++) {
                float x = -14.2f - 1.8f * (float)k, z = -38.4f - 1.4f * (float)k;
                AddBox(x, 0, z, 1.9f, 0.03f, 1.6f, COL_STRAW, false);
                AddShape(x, 0, z, 1.5f, 0.7f, 1.3f, (Color){ 214, 190, 120, 255 }, false, SHAPE_BLOB);
            }
            AddShape(-12.6f, 0, -37.0f, 0.5f, 0.45f, 0.5f, COL_STRAW, false, SHAPE_CYL);
        }
    }

    AddBox(0, 0, ZIG_Z, 20, 0.06f, 20, COL_BRICK, false);
    for (int i = 0; i < 4; i++)
        AddShape((i & 1) ? 4.5f : -4.5f, 0, ZIG_Z + ((i & 2) ? 3.5f : -3.5f), 1.0f, 6.0f, 1.0f, COL_CONES, true, SHAPE_CYL);
    for (int i = 0; i < 2; i++) {
        float x = i ? 3.0f : -3.0f;
        AddShape(x, 0, ZIG_Z + 8.0f, 0.5f, 0.9f, 0.5f, COL_STONE, true, SHAPE_CYL);
        AddGlow(x, 0.9f, ZIG_Z + 8.0f, 0.36f, 0.28f, 0.36f, COL_FIRE);
    }

    for (int k = -4; k <= 4; k++) {
        float o = 4.0f * (float)k;
        Trim(o - 0.6f, 0, -60.4f, o + 0.6f, 6.8f, -60, dark);
        if (k > -4 && k < 4) {
            Trim(-20.4f, 0, -41 + o - 0.6f, -20, 6.8f, -41 + o + 0.6f, dark);
            Trim(20, 0, -41 + o - 0.6f, 20.4f, 6.8f, -41 + o + 0.6f, dark);
        }
    }
}

static void BuildStairs(void)
{
    const Color c = COL_MUD_DARK;

    StairZ(-2, 2, -11, 11.0f / 13.0f, 0, T1 / 13.0f, 13, c);
    StairX(-20, 1, 1.0f, -22, -19, T1 / 13.0f, 13, c);
    StairX(20, -1, 1.0f, -22, -19, T1 / 13.0f, 13, c);
    Trim(-2.25f, 0, -22, -2.0f, T1 + 0.5f, -11, COL_MUD);
    Trim(2.0f, 0, -22, 2.25f, T1 + 0.5f, -11, COL_MUD);
}

static void BuildGatehouse(void)
{
    const Color c = COL_MUD_PALE;

    Block(-6, T1, -25, -2.2f, T1 + 3, -22, c);
    Block(2.2f, T1, -25, 6, T1 + 3, -22, c);
    Block(-6, T1, -27, -0.9f, T1 + 3, -25, c);
    Block(0.9f, T1, -27, 6, T1 + 3, -25, c);
    Block(-6, T1 + 3, -27, 6, T1 + 3.5f, -22, c);
    Block(-6.6f, T1, -30, -6, T1 + 3, -26.5f, c);
    Block(6, T1, -30, 6.6f, T1 + 3, -26.5f, c);

    Trim(-6.05f, T1 + 2.2f, -22.1f, 6.05f, T1 + 2.6f, -21.95f, COL_LAPIS);
    for (int k = 0; k < 6; k++) {
        float x = -5.4f + 2.16f * (float)k;
        Trim(x - 0.35f, T1 + 3.5f, -22.6f, x + 0.35f, T1 + 4.1f, -22, c);
    }
}

static const Color HIDE = { 130, 94, 62, 255 }, TABLET = { 176, 138, 96, 255 };

// The west room of the middle terrace: where the temple guards sleep and eat off duty. Cots with
// blankets, a rack of spears and leather shields, a table with bread and beer.
static void BuildGuardRoom(void)
{
    for (int k = 0; k < 3; k++) {
        float z = -48.8f + 2.0f * (float)k;
        AddBox(-8.0f, T1, z, 1.6f, 0.45f, 0.9f, COL_LINEN, true);
        Trim(-8.3f, T1 + 0.45f, z - 0.46f, -7.2f, T1 + 0.5f, z + 0.46f, COL_BLANKET);
        AddShape(-8.6f, T1 + 0.45f, z, 0.32f, 0.14f, 0.5f, (Color){ 214, 206, 186, 255 }, false, SHAPE_BLOB);
    }
    // The rack against the west wall: two posts, a bar, three spears and two shields.
    Trim(-9.95f, T1, -37.4f, -9.8f, T1 + 1.9f, -37.25f, COL_WOOD);
    Trim(-9.95f, T1, -35.0f, -9.8f, T1 + 1.9f, -34.85f, COL_WOOD);
    Trim(-9.95f, T1 + 1.55f, -37.4f, -9.75f, T1 + 1.65f, -34.85f, COL_WOOD);
    for (int k = 0; k < 3; k++) {
        float z = -36.9f + 0.45f * (float)k;
        AddShape(-9.7f, T1, z, 0.06f, 2.3f, 0.06f, COL_WOOD, false, SHAPE_CYL);
        AddShape(-9.7f, T1 + 2.3f, z, 0.1f, 0.28f, 0.1f, COL_BRONZE, false, SHAPE_CONE);
    }
    for (int k = 0; k < 2; k++) {
        float z = -38.6f - 1.0f * (float)k;
        Trim(-9.98f, T1 + 0.2f, z - 0.35f, -9.9f, T1 + 1.1f, z + 0.35f, HIDE);
        Trim(-9.9f, T1 + 0.58f, z - 0.08f, -9.86f, T1 + 0.72f, z + 0.08f, COL_BRONZE);
    }
    // Supper: a low table, two stools, bread, a jug of beer and cups.
    AddBox(-8.0f, T1, -40.6f, 1.3f, 0.5f, 0.8f, COL_WOOD, true);
    AddBox(-9.1f, T1, -40.6f, 0.4f, 0.32f, 0.4f, COL_WOOD, false);
    AddBox(-7.0f, T1, -39.5f, 0.4f, 0.32f, 0.4f, COL_WOOD, false);
    AddShape(-8.2f, T1 + 0.5f, -40.7f, 0.34f, 0.14f, 0.3f, (Color){ 196, 150, 90, 255 }, false, SHAPE_BLOB);
    AddShape(-7.7f, T1 + 0.5f, -40.4f, 0.22f, 0.36f, 0.22f, COL_POT, false, SHAPE_BLOB);
    AddShape(-8.5f, T1 + 0.5f, -40.35f, 0.1f, 0.1f, 0.1f, COL_CLAY, false, SHAPE_CYL);
    AddShape(-7.6f, T1 + 0.5f, -40.85f, 0.1f, 0.1f, 0.1f, COL_CLAY, false, SHAPE_CYL);
    for (int k = 0; k < 2; k++) AddShape(-9.4f, T1, -43.6f + 0.7f * (float)k, 0.5f, 0.8f, 0.5f, COL_POT, true, SHAPE_BLOB);
}

// The east room: the temple's tablet house, where scribes keep the accounts of its fields, flocks and
// stores on clay. Shelves of tablets along the wall, a writing bench, baskets of tablets.
static void BuildTabletRoom(void)
{
    for (int u = 0; u < 2; u++) {
        float z0 = u ? -38.6f : -48.6f, z1 = z0 + 3.4f;
        Trim(9.45f, T1, z0, 9.95f, T1 + 2.1f, z0 + 0.1f, COL_WOOD);
        Trim(9.45f, T1, z1 - 0.1f, 9.95f, T1 + 2.1f, z1, COL_WOOD);
        for (int b = 0; b < 3; b++) {
            float y = T1 + 0.45f + 0.6f * (float)b;
            Trim(9.45f, y - 0.05f, z0, 9.95f, y, z1, COL_WOOD);
            for (int k = 0; k < 9; k++) {
                if (Hash2(u * 40 + b * 10 + k, 3) < 0.15f) continue;
                float z = z0 + 0.3f + 0.35f * (float)k;
                Trim(9.55f, y, z - 0.1f, 9.85f, y + 0.24f + 0.06f * Hash2(k, b), z + 0.1f, Mix(TABLET, COL_CLAY, Hash2(k + u, b + 7) * 0.5f));
            }
        }
    }
    AddBox(8.2f, T1, -45.6f, 0.8f, 0.42f, 1.4f, COL_WOOD, true);
    AddBox(7.25f, T1, -45.6f, 1.0f, 0.03f, 1.6f, COL_STRAW, false);
    Trim(8.0f, T1 + 0.42f, -46.0f, 8.3f, T1 + 0.46f, -45.6f, TABLET);
    Trim(8.05f, T1 + 0.42f, -45.3f, 8.35f, T1 + 0.46f, -44.95f, TABLET);
    Trim(7.9f, T1 + 0.46f, -45.5f, 8.2f, T1 + 0.48f, -45.48f, COL_REED);
    for (int k = 0; k < 2; k++) {
        float z = -40.0f - 1.1f * (float)k;
        AddShape(9.2f, T1, z, 0.7f, 0.5f, 0.7f, COL_STRAW, true, SHAPE_CYL);
        for (int t = 0; t < 3; t++) Trim(9.0f + 0.15f * (float)t, T1 + 0.5f, z - 0.15f, 9.12f + 0.15f * (float)t, T1 + 0.62f, z + 0.12f, TABLET);
    }
}

static void BuildTier2(void)
{
    const Color c = COL_MUD_LIGHT;

    Block(-13, T1, -34, -4.7f, T2, -30, c);
    Block(-3.3f, T1, -34, -1.5f, T2, -30, c);
    Block(1.5f, T1, -34, 3.3f, T2, -30, c);
    Block(4.7f, T1, -34, 13, T2, -30, c);
    Block(-4.7f, T1 + 2.4f, -34, -3.3f, T2, -30, c);
    Block(3.3f, T1 + 2.4f, -34, 4.7f, T2, -30, c);
    Block(-13, T1, -50, -10, T2, -34, c);
    Block(10, T1, -50, 13, T2, -34, c);
    Block(-13, T1, -54, 13, T2, -50, c);
    Block(-10, T2 - 1, -50, 10, T2, -34, c);

    for (int s = -1; s <= 1; s += 2) {
        float x0 = 5.7f * (float)s, x1 = 6.3f * (float)s;
        float lo = fminf(x0, x1), hi = fmaxf(x0, x1);
        Block(lo, T1, -50, hi, T2 - 1, -41.8f, c);
        Block(lo, T1, -40.2f, hi, T2 - 1, -34, c);
        Block(lo, T1 + 2.4f, -41.8f, hi, T2 - 1, -40.2f, c);
        AddShape(3.0f * (float)s, T1, -38.5f, 0.8f, 4.0f, 0.8f, COL_WHITEWASH, true, SHAPE_CYL);
        AddShape(3.0f * (float)s, T1, -45.5f, 0.8f, 4.0f, 0.8f, COL_WHITEWASH, true, SHAPE_CYL);
        AddShape(4.3f * (float)s, T1, -48.6f, 0.5f, 0.9f, 0.5f, COL_STONE, true, SHAPE_CYL);
        AddGlow(4.3f * (float)s, T1 + 0.9f, -48.6f, 0.36f, 0.26f, 0.36f, COL_FIRE);
    }
    BuildGuardRoom();
    BuildTabletRoom();
    AddBox(0, T1, -48.9f, 4.0f, 1.0f, 1.2f, COL_WOOD, true);
    AddBox(0, T1 + 1.0f, -48.9f, 0.9f, 1.6f, 0.6f, COL_BRONZE, false);
    AddBox(0, T1 + 2.6f, -48.9f, 0.5f, 0.5f, 0.5f, (Color){ 220, 180, 90, 255 }, false);
    AddBox(0, T1, -42.0f, 3.0f, 0.04f, 12.0f, COL_BLANKET, false);
    // Offerings on the god's table: bread, dates and a jar of oil.
    AddShape(-1.3f, T1 + 1.0f, -48.7f, 0.36f, 0.14f, 0.3f, (Color){ 196, 150, 90, 255 }, false, SHAPE_BLOB);
    AddShape(1.2f, T1 + 1.0f, -48.8f, 0.4f, 0.16f, 0.34f, COL_DATES, false, SHAPE_BLOB);
    AddShape(1.6f, T1 + 1.0f, -48.5f, 0.22f, 0.4f, 0.22f, COL_POT, false, SHAPE_BLOB);

    StairZ(-1.5f, 1.5f, -28.2f, 0.5f, T1, 0.5f, 10, COL_MUD_DARK);
    Block(-1.5f, T1, -34, 1.5f, T2, -33.2f, COL_MUD_DARK);
}

static void BuildTier3(void)
{
    const Color c = COL_MUD_PALE;
    const float y = T3;

    StairZ(-1.5f, 1.5f, -34, 0.55f, T2, 0.5f, 9, COL_MUD_DARK);
    Block(-8, T2, -38.95f, -1.5f, T3, -34, c);
    Block(1.5f, T2, -38.95f, 8, T3, -34, c);
    Block(-8, T2, -50, 8, T3, -38.95f, c);
    Trim(-8.05f, T3 - 0.5f, -50.05f, 8.05f, T3 - 0.2f, -49.95f, COL_LAPIS);
    Trim(-8.05f, T3 - 0.5f, -34.05f, -1.5f, T3 - 0.2f, -33.95f, COL_LAPIS);
    Trim(1.5f, T3 - 0.5f, -34.05f, 8.05f, T3 - 0.2f, -33.95f, COL_LAPIS);

    Block(-4, y, -48, 4, y + 3.4f, -47.4f, COL_WHITEWASH);
    Block(-4, y, -47.4f, -3.4f, y + 3.4f, -42.6f, COL_WHITEWASH);
    Block(3.4f, y, -47.4f, 4, y + 3.4f, -42.6f, COL_WHITEWASH);
    Block(-4, y, -42.6f, -0.8f, y + 3.4f, -42, COL_WHITEWASH);
    Block(0.8f, y, -42.6f, 4, y + 3.4f, -42, COL_WHITEWASH);
    Block(-0.8f, y + 2.4f, -42.6f, 0.8f, y + 3.4f, -42, COL_WHITEWASH);
    Block(-4.3f, y + 3.4f, -48.3f, 4.3f, y + 3.9f, -41.7f, COL_LAPIS);
    Trim(-0.8f, y, -42.05f, 0.8f, y + 2.4f, -41.98f, COL_DOORWAY);

    Block(-0.9f, y, -47.3f, 0.9f, y + 1.0f, -46.4f, COL_LAPIS);
    Trim(-0.7f, y + 1.0f, -47.1f, 0.7f, y + 1.06f, -46.6f, (Color){ 220, 180, 90, 255 });
    Trim(-0.25f, y + 1.06f, -47.0f, 0.25f, y + 1.7f, -46.7f, (Color){ 220, 180, 90, 255 });
    G.stations[STN_ALTAR] = (Vector3){ 0, y, -45.5f };
}

void BuildTemple(void)
{
    BuildTemenos();
    BuildTier1();
    BuildStairs();
    BuildGatehouse();
    BuildTier2();
    BuildTier3();
}

Vector3 GatePos(void)
{
    return (Vector3){ 0, T1, -23.6f };
}

bool InTempleInterior(Vector3 p)
{
    if (p.y < 6.0f) {
        if (fabsf(p.x) < 18.0f && p.z > -50.0f && p.z < -30.0f) return true;
        return fabsf(fabsf(p.x) - 4.5f) < 0.7f && p.z > -30.0f && p.z < -22.0f;
    }
    if (p.y >= T1 - 0.2f && p.y < T2 - 1.0f) return fabsf(p.x) < 10.0f && p.z > -50.0f && p.z < -34.0f;
    return p.y >= T3 - 0.2f && p.y < T3 + 3.4f && fabsf(p.x) < 3.4f && p.z > -47.4f && p.z < -42.6f;
}

static bool GuardsAtGate(void)
{
    for (int i = 0; i < G.villagerCount; i++)
        if (G.villagers[i].templePost && !G.villagers[i].vit.dead) return true;
    return false;
}

static int Gift(void)
{
    for (int i = 0; i < (int)(sizeof(GIFTS) / sizeof(GIFTS[0])); i++)
        if (BagCount(GIFTS[i][0]) >= GIFTS[i][1]) return i;
    return -1;
}

void OfferPrompt(char *buf, int size, bool *ok)
{
    int g = Gift();

    *ok = false;
    buf[0] = '\0';
    if (!GuardsAtGate()) return;
    *ok = g >= 0;
    if (*ok) Fmt(buf, size, "[E] Offer %d %s so the guards let you up", GIFTS[g][1], ItemName(GIFTS[g][0]));
    else Fmt(buf, size, "Guards: \"Only those who bring an offering of food go up.\" (bread, meat, fish, eggs or milk)");
}

void MakeOffering(void)
{
    int g = Gift();

    if (g < 0 || !GuardsAtGate()) return;
    BagRemove(GIFTS[g][0], GIFTS[g][1]);
    G.templeAccess = true;
    Say("The guards take your %s and step aside. The upper temple is open to you until dawn.", ItemName(GIFTS[g][0]));
}
