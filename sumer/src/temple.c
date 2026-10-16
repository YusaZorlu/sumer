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
            for (int k = 0; k < 3; k++) AddBox(13.5f + 1.0f * (float)k, 0.9f, -34.0f, 0.6f, 0.25f, 0.4f, COL_BRONZE, false);
            AddBox(14.5f, 0, -34.0f, 3.2f, 0.9f, 0.9f, COL_WOOD, true);
        }
    }

    AddBox(0, 0, ZIG_Z, 20, 0.06f, 20, COL_BRICK, false);
    for (int i = 0; i < 4; i++)
        AddShape((i & 1) ? 4.5f : -4.5f, 0, ZIG_Z + ((i & 2) ? 3.5f : -3.5f), 1.0f, 6.0f, 1.0f, COL_MUD_PALE, true, SHAPE_CYL);
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
        for (int k = 0; k < 3; k++)
            AddBox(8.0f * (float)s, T1, -48.8f + 2.0f * (float)k, 1.6f, 0.5f, 0.9f, COL_LINEN, true);
        for (int k = 0; k < 4; k++)
            AddShape(9.4f * (float)s, T1, -38.0f + 1.0f * (float)k, 0.6f, 1.0f, 0.6f, COL_POT, true, SHAPE_BLOB);
    }
    AddBox(0, T1, -48.9f, 4.0f, 1.0f, 1.2f, COL_WOOD, true);
    AddBox(0, T1 + 1.0f, -48.9f, 0.9f, 1.6f, 0.6f, COL_BRONZE, false);
    AddBox(0, T1 + 2.6f, -48.9f, 0.5f, 0.5f, 0.5f, (Color){ 220, 180, 90, 255 }, false);
    AddBox(0, T1, -42.0f, 3.0f, 0.04f, 12.0f, COL_BLANKET, false);

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
