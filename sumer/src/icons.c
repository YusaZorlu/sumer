// Pictures of the things you carry, for the baskets, the storeroom and barter. Each is a little model
// made of the same shaded parts as the world (a tray of dates, a sheaf of wheat, a jug of milk, a goose),
// drawn once into a sheet of pictures the first time a window needs one. They are drawn the classic
// way, lit by its fixed light from above, whatever the graphics setting, so they look alike everywhere.
#include "game.h"
#include "rlgl.h"
#include <math.h>

#define ICON_PX   128   // drawn this big, shown smaller: smooth edges without multisampling
#define ICON_COLS 8
#define ICON_ROWS ((IT_COUNT + ICON_COLS - 1) / ICON_COLS)

static RenderTexture2D sheet;
static bool tried, ready;

static Vector3 V(float x, float y, float z)
{
    return (Vector3){ x, y, z };
}

static Color Tone(Color c, float k)
{
    return (Color){ (unsigned char)fminf(255.0f, c.r * k), (unsigned char)fminf(255.0f, c.g * k), (unsigned char)fminf(255.0f, c.b * k), c.a };
}

static void Cube(float x, float y, float z, float sx, float sy, float sz, Color c)
{
    DrawShadedBox(V(x, y, z), V(sx, sy, sz), c);
}

static void Blob(float x, float y, float z, float rx, float ry, float rz, Color c)
{
    DrawShadedBlobFine(V(x, y, z), V(rx, ry, rz), c);
}

static void Cyl(float x, float y, float z, float r, float h, float top, Color c)
{
    DrawShadedCylinder(V(x, y, z), r, r, h, top, 14, c);
}

// A rod from a to b, 'r' thick (a cylinder laid along the line).
static void Rod(Vector3 a, Vector3 b, float r, Color c)
{
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 1e-4f) return;
    Vector3 up = { 0, 1, 0 }, axis = Vector3CrossProduct(up, Vector3Scale(d, 1.0f / len));
    float s = Vector3Length(axis), ang = atan2f(s, d.y / len);
    rlPushMatrix();
    rlTranslatef(a.x, a.y, a.z);
    if (s > 1e-4f) rlRotatef(ang * RAD2DEG, axis.x / s, axis.y / s, axis.z / s);
    DrawShadedCylinder(V(0, 0, 0), r, r, len, 1.0f, 8, c);
    rlPopMatrix();
}

static void Turn(float deg)
{
    rlRotatef(deg, 0, 1, 0);
}

// A beast lying on its side, as it is carried home: body, neck, head and folded legs.
static void Carcass(Color hide, Color belly, float len, float head, bool horns, bool tusks, bool ears)
{
    Blob(0, 0.2f, 0, len, 0.18f, 0.2f, hide);
    Blob(0, 0.12f, 0.05f, len * 0.8f, 0.1f, 0.16f, belly);
    Rod(V(len * 0.8f, 0.24f, 0), V(len * 1.15f, 0.3f, 0.04f), 0.08f, hide);
    Blob(len * 1.25f, 0.3f, 0.04f, head, 0.09f, 0.09f, hide);
    Blob(len * 1.25f + head * 0.8f, 0.27f, 0.04f, head * 0.45f, 0.06f, 0.07f, Tone(hide, 0.7f));
    for (int k = 0; k < 4; k++) {
        float x = (k < 2 ? 0.6f : -0.6f) * len + (k & 1 ? 0.06f : -0.06f);
        Rod(V(x, 0.1f, 0.12f), V(x + 0.16f, 0.06f, 0.3f), 0.03f, Tone(hide, 0.8f));
    }
    if (horns) {
        Rod(V(len * 1.2f, 0.36f, 0.0f), V(len * 1.0f, 0.62f, -0.06f), 0.02f, (Color){ 60, 50, 40, 255 });
        Rod(V(len * 1.24f, 0.36f, 0.08f), V(len * 1.06f, 0.6f, 0.12f), 0.02f, (Color){ 60, 50, 40, 255 });
    }
    if (ears) {
        Rod(V(len * 1.2f, 0.36f, 0.0f), V(len * 1.12f, 0.52f, -0.04f), 0.03f, hide);
        Rod(V(len * 1.22f, 0.36f, 0.08f), V(len * 1.16f, 0.52f, 0.12f), 0.03f, hide);
    }
    if (tusks) {
        Rod(V(len * 1.25f + head, 0.24f, 0.0f), V(len * 1.25f + head * 1.2f, 0.33f, -0.02f), 0.015f, (Color){ 236, 228, 210, 255 });
        Rod(V(len * 1.25f + head, 0.24f, 0.09f), V(len * 1.25f + head * 1.2f, 0.33f, 0.11f), 0.015f, (Color){ 236, 228, 210, 255 });
    }
}

static void Bird(Color body, Color wing, Color beak, float size, bool comb, bool neck)
{
    Blob(0, 0.26f * size, 0, 0.3f * size, 0.22f * size, 0.22f * size, body);
    Blob(-0.04f * size, 0.3f * size, 0.12f * size, 0.22f * size, 0.13f * size, 0.08f * size, wing);
    Blob(-0.04f * size, 0.3f * size, -0.12f * size, 0.22f * size, 0.13f * size, 0.08f * size, wing);
    Blob(-0.3f * size, 0.34f * size, 0, 0.1f * size, 0.08f * size, 0.08f * size, wing);   // tail
    float hx = 0.26f * size, hy = neck ? 0.62f * size : 0.46f * size;
    if (neck) Rod(V(0.2f * size, 0.34f * size, 0), V(hx, hy, 0), 0.06f * size, body);
    Blob(hx, hy, 0, 0.1f * size, 0.1f * size, 0.09f * size, body);
    Rod(V(hx + 0.08f * size, hy - 0.01f * size, 0), V(hx + 0.2f * size, hy - 0.04f * size, 0), 0.03f * size, beak);
    Blob(hx + 0.05f * size, hy + 0.03f * size, 0.08f * size, 0.02f * size, 0.02f * size, 0.01f * size, (Color){ 20, 16, 14, 255 });
    if (comb) Cube(hx, hy + 0.11f * size, 0, 0.1f * size, 0.06f * size, 0.03f * size, (Color){ 200, 40, 30, 255 });
    Rod(V(0.02f * size, 0.08f * size, 0.06f * size), V(0.02f * size, -0.02f * size, 0.06f * size), 0.015f * size, beak);
    Rod(V(0.02f * size, 0.08f * size, -0.06f * size), V(0.02f * size, -0.02f * size, -0.06f * size), 0.015f * size, beak);
}

static void Jar(float x, float z, float h, float r, Color c)
{
    Cyl(x, 0, z, r * 0.7f, h * 0.18f, 1.4f, c);
    Cyl(x, h * 0.18f, z, r, h * 0.45f, 0.62f, Tone(c, 1.05f));
    Cyl(x, h * 0.63f, z, r * 0.62f, h * 0.2f, 0.75f, c);
    Cyl(x, h * 0.83f, z, r * 0.5f, h * 0.08f, 1.15f, Tone(c, 0.9f));
}

static void Bucket(Color inside)
{
    const Color wood = { 120, 92, 64, 255 }, band = { 70, 56, 44, 255 };
    Cyl(0, 0, 0, 0.3f, 0.5f, 1.15f, wood);
    Cyl(0, 0.08f, 0, 0.31f, 0.05f, 1.0f, band);
    Cyl(0, 0.38f, 0, 0.34f, 0.05f, 1.0f, band);
    Cyl(0, 0.5f, 0, 0.3f, 0.005f, 1.0f, inside);
    for (int k = 0; k < 8; k++) {   // the handle, a rope arched over it
        float a0 = PI * (float)k / 8.0f, a1 = PI * (float)(k + 1) / 8.0f;
        Rod(V(cosf(a0) * 0.34f, 0.52f + sinf(a0) * 0.32f, 0), V(cosf(a1) * 0.34f, 0.52f + sinf(a1) * 0.32f, 0), 0.02f, band);
    }
}

static void Stalk(float x, float z, float lean, float h, Color stem, Color ear)
{
    Vector3 top = { x + lean, h, z + lean * 0.4f };
    Rod(V(x, 0.0f, z), top, 0.014f, stem);
    if (ear.a) Blob(top.x, top.y + 0.06f, top.z, 0.035f, 0.09f, 0.035f, ear);
}

static void ItemModel(int item)
{
    Color c = ITEMS[item].color;
    switch (item) {
    case IT_DATES:
        Cyl(0, 0, 0, 0.46f, 0.14f, 1.15f, (Color){ 176, 140, 86, 255 });   // a palm-leaf tray
        for (int k = 0; k < 14; k++) {
            float a = (float)k * 2.4f, r = 0.08f + 0.04f * (float)(k % 5);
            Blob(cosf(a) * r, 0.2f + 0.05f * (float)(k % 3), sinf(a) * r, 0.07f, 0.1f, 0.07f, Tone(c, 0.85f + 0.08f * (float)(k % 4)));
        }
        break;
    case IT_REEDS:
        for (int k = 0; k < 11; k++) {
            float x = ((float)(k % 4) - 1.5f) * 0.05f, z = ((float)(k / 4) - 1.0f) * 0.05f;
            Rod(V(x - 0.3f, 0.08f, z), V(x + 0.32f, 0.86f, z + 0.04f), 0.022f, Tone(c, 0.85f + 0.05f * (float)(k % 4)));
        }
        Blob(0.36f, 0.92f, 0.03f, 0.08f, 0.13f, 0.08f, (Color){ 196, 170, 120, 255 });
        Rod(V(-0.03f, 0.43f, -0.1f), V(0.03f, 0.48f, 0.12f), 0.07f, (Color){ 110, 84, 50, 255 });
        break;
    case IT_SEEDS:
        Blob(0, 0.26f, 0, 0.3f, 0.28f, 0.28f, (Color){ 196, 172, 128, 255 });   // an open sack
        Cyl(0, 0.44f, 0, 0.2f, 0.08f, 1.2f, (Color){ 182, 158, 116, 255 });
        Cyl(0, 0.5f, 0, 0.22f, 0.02f, 1.0f, c);
        for (int k = 0; k < 9; k++) Blob(0.28f + 0.05f * (float)(k % 3), 0.03f, 0.18f + 0.06f * (float)(k / 3), 0.025f, 0.02f, 0.02f, Tone(c, 0.9f));
        break;
    case IT_WHEAT:
        for (int k = 0; k < 12; k++) {
            float a = (float)k * 2.1f;
            Stalk(cosf(a) * 0.05f, sinf(a) * 0.05f, cosf(a) * 0.16f, 0.72f + 0.04f * (float)(k % 3), (Color){ 206, 176, 90, 255 }, c);
        }
        Cyl(0, 0.3f, 0, 0.08f, 0.06f, 1.0f, (Color){ 120, 90, 50, 255 });
        break;
    case IT_FLOUR:
        Blob(0, 0.3f, 0, 0.32f, 0.32f, 0.3f, (Color){ 206, 190, 156, 255 });
        Cyl(0, 0.52f, 0, 0.22f, 0.08f, 1.2f, (Color){ 196, 180, 146, 255 });
        Blob(0, 0.6f, 0, 0.24f, 0.07f, 0.24f, c);
        Rod(V(-0.2f, 0.52f, 0.18f), V(0.2f, 0.52f, 0.18f), 0.02f, (Color){ 120, 90, 60, 255 });
        break;
    case IT_MILK:
        Jar(0, 0, 0.82f, 0.3f, (Color){ 186, 112, 70, 255 });
        Cyl(0, 0.82f, 0, 0.15f, 0.005f, 1.0f, c);
        Rod(V(0.28f, 0.36f, 0), V(0.3f, 0.66f, 0), 0.035f, (Color){ 170, 100, 62, 255 });   // a handle
        break;
    case IT_DOUGH:
        Cube(0, 0.04f, 0, 0.8f, 0.08f, 0.6f, (Color){ 140, 104, 70, 255 });   // a board
        Blob(0, 0.2f, 0, 0.3f, 0.14f, 0.26f, c);
        break;
    case IT_BREAD:
        Blob(0, 0.1f, 0, 0.42f, 0.08f, 0.42f, Tone(c, 0.9f));
        Blob(0.06f, 0.24f, 0.04f, 0.36f, 0.09f, 0.36f, c);
        for (int k = 0; k < 5; k++) Blob(0.06f + cosf((float)k * 1.3f) * 0.18f, 0.32f, 0.04f + sinf((float)k * 1.3f) * 0.18f, 0.04f, 0.015f, 0.04f, Tone(c, 0.7f));
        break;
    case IT_EGG:
        Cyl(0, 0, 0, 0.42f, 0.12f, 1.2f, (Color){ 186, 160, 100, 255 });   // a straw nest
        Blob(-0.12f, 0.24f, 0.04f, 0.12f, 0.16f, 0.12f, c);
        Blob(0.14f, 0.22f, -0.06f, 0.12f, 0.15f, 0.12f, Tone(c, 0.96f));
        Blob(0.04f, 0.22f, 0.18f, 0.11f, 0.15f, 0.11f, Tone(c, 1.02f));
        break;
    case IT_ROAST:
        Turn(-20);
        Blob(-0.06f, 0.24f, 0, 0.32f, 0.2f, 0.24f, c);
        Blob(-0.1f, 0.34f, 0.02f, 0.2f, 0.08f, 0.16f, Tone(c, 1.25f));
        Rod(V(0.2f, 0.22f, 0), V(0.5f, 0.3f, 0), 0.05f, (Color){ 236, 226, 206, 255 });
        Blob(0.52f, 0.31f, 0.0f, 0.06f, 0.07f, 0.09f, (Color){ 236, 226, 206, 255 });
        break;
    case IT_ARROWS:
        for (int k = 0; k < 3; k++) {
            float z = ((float)k - 1.0f) * 0.14f, lift = (float)k * 0.06f;
            Vector3 a = { -0.48f, 0.12f + lift, z }, b = { 0.44f, 0.4f + lift, z * 0.6f };
            Rod(a, b, 0.016f, (Color){ 150, 120, 80, 255 });
            Rod(b, Vector3Add(b, V(0.12f, 0.04f, 0)), 0.035f, (Color){ 120, 120, 116, 255 });
            Cube(a.x + 0.08f, a.y + 0.03f, a.z, 0.16f, 0.07f, 0.01f, (Color){ 236, 228, 210, 255 });
        }
        break;
    case IT_GAZELLE: Carcass(c, (Color){ 236, 222, 196, 255 }, 0.36f, 0.12f, true, false, false); break;
    case IT_ONAGER:
        Carcass(c, (Color){ 220, 212, 196, 255 }, 0.4f, 0.15f, false, false, true);
        Rod(V(0.32f, 0.36f, -0.02f), V(0.52f, 0.4f, -0.02f), 0.03f, (Color){ 60, 52, 46, 255 });   // the mane
        break;
    case IT_BOAR: Carcass(c, Tone(c, 1.25f), 0.38f, 0.16f, false, true, true); break;
    case IT_GOOSE: Bird(c, Tone(c, 0.85f), (Color){ 230, 140, 50, 255 }, 1.1f, false, true); break;
    case IT_WOLF:   // a pelt spread out, its far side raised to be seen: legs, tail, the head with its ears
        rlTranslatef(0, 0.2f, 0);
        rlRotatef(38, 1, 0, 0);
        Blob(0, 0, 0, 0.36f, 0.05f, 0.27f, c);
        Blob(0, 0.03f, 0, 0.32f, 0.03f, 0.07f, Tone(c, 0.68f));
        for (int k = 0; k < 4; k++) Blob((k < 2 ? 0.24f : -0.24f), 0, (k & 1 ? 0.3f : -0.3f), 0.08f, 0.03f, 0.1f, Tone(c, 0.9f));
        Blob(0.42f, 0.03f, 0, 0.12f, 0.06f, 0.1f, c);
        Blob(0.54f, 0.02f, 0, 0.06f, 0.04f, 0.05f, Tone(c, 0.8f));
        Rod(V(0.4f, 0.06f, -0.06f), V(0.38f, 0.16f, -0.08f), 0.03f, Tone(c, 0.7f));
        Rod(V(0.4f, 0.06f, 0.06f), V(0.38f, 0.16f, 0.08f), 0.03f, Tone(c, 0.7f));
        Rod(V(-0.34f, 0.0f, 0), V(-0.5f, 0.0f, 0.06f), 0.05f, Tone(c, 0.85f));
        break;
    case IT_CLAY:
        Blob(0, 0.2f, 0, 0.36f, 0.22f, 0.3f, c);
        Blob(0.2f, 0.3f, 0.14f, 0.16f, 0.12f, 0.14f, Tone(c, 1.1f));
        Cube(-0.1f, 0.4f, -0.05f, 0.24f, 0.02f, 0.2f, Tone(c, 0.85f));
        break;
    case IT_POT:
        Jar(-0.16f, -0.08f, 0.8f, 0.26f, c);
        Jar(0.24f, 0.18f, 0.56f, 0.2f, Tone(c, 0.92f));
        break;
    case IT_WOOD:
        for (int k = 0; k < 3; k++) {
            float y = k == 2 ? 0.36f : 0.15f, z = k == 2 ? 0.0f : (k ? 0.17f : -0.17f);
            Rod(V(-0.45f, y, z), V(0.45f, y, z), 0.15f, Tone(c, 0.9f + 0.08f * (float)k));
            rlPushMatrix();
            rlTranslatef(0.46f, y, z);
            rlRotatef(-90, 0, 0, 1);
            Cyl(0, 0, 0, 0.13f, 0.005f, 1.0f, (Color){ 200, 160, 110, 255 });   // the cut end
            rlPopMatrix();
        }
        break;
    case IT_COPPER:
        Blob(-0.12f, 0.18f, 0, 0.26f, 0.18f, 0.22f, c);
        Blob(0.2f, 0.14f, 0.12f, 0.18f, 0.14f, 0.16f, Tone(c, 0.85f));
        Blob(0.06f, 0.3f, -0.08f, 0.1f, 0.08f, 0.1f, (Color){ 176, 110, 60, 255 });
        break;
    case IT_TIN:
        for (int k = 0; k < 3; k++) Cube(((float)k - 1.0f) * 0.2f, 0.1f + (k == 1 ? 0.14f : 0.0f), (k == 1 ? 0.0f : 0.1f), 0.18f, 0.12f, 0.5f, Tone(c, 0.9f + 0.06f * (float)k));
        break;
    case IT_BRONZE:
        for (int k = 0; k < 3; k++) {   // ingots stacked crosswise
            rlPushMatrix();
            rlTranslatef(0, 0.08f + 0.13f * (float)k, 0);
            Turn(k & 1 ? 90.0f : 0.0f);
            DrawBeveledBox(V(-0.14f, 0, 0), V(0.22f, 0.12f, 0.7f), Tone(c, 0.92f + 0.05f * (float)k), 0.03f);
            DrawBeveledBox(V(0.14f, 0, 0), V(0.22f, 0.12f, 0.7f), Tone(c, 0.96f + 0.05f * (float)k), 0.03f);
            rlPopMatrix();
        }
        break;
    case IT_FISH:
    case IT_GRILLED: {
        bool grilled = item == IT_GRILLED;
        Turn(25);
        Blob(0, 0.2f, 0, 0.44f, 0.16f, 0.1f, c);
        Blob(0, 0.14f, 0, 0.36f, 0.08f, 0.09f, grilled ? Tone(c, 1.15f) : (Color){ 220, 226, 226, 255 });
        rlPushMatrix();
        rlTranslatef(-0.46f, 0.2f, 0);
        rlRotatef(45, 0, 0, 1);
        Cube(0, 0, 0, 0.18f, 0.18f, 0.02f, Tone(c, 0.85f));   // the tail
        rlPopMatrix();
        Blob(0.32f, 0.24f, 0.09f, 0.03f, 0.03f, 0.01f, (Color){ 20, 20, 20, 255 });
        if (grilled)
            for (int k = 0; k < 4; k++) Cube(-0.2f + 0.13f * (float)k, 0.24f, 0.09f, 0.03f, 0.2f, 0.02f, (Color){ 70, 40, 20, 255 });
        break;
    }
    case IT_COWHIDE:
        rlTranslatef(0, 0.2f, 0);
        rlRotatef(38, 1, 0, 0);
        Blob(0, 0.06f, 0, 0.5f, 0.04f, 0.36f, c);
        Blob(0.16f, 0.09f, 0.08f, 0.16f, 0.02f, 0.12f, (Color){ 236, 228, 214, 255 });
        Blob(-0.2f, 0.09f, -0.12f, 0.12f, 0.02f, 0.1f, (Color){ 236, 228, 214, 255 });
        break;
    case IT_SHEEPSKIN:
        rlTranslatef(0, 0.2f, 0);
        rlRotatef(38, 1, 0, 0);
        Blob(0, 0.06f, 0, 0.48f, 0.05f, 0.34f, Tone(c, 0.92f));
        for (int k = 0; k < 9; k++) Blob(-0.3f + 0.15f * (float)(k % 5), 0.12f, -0.16f + 0.16f * (float)(k / 3 % 3), 0.11f, 0.06f, 0.1f, c);
        break;
    case IT_BUCKET: Bucket((Color){ 70, 54, 40, 255 }); break;
    case IT_WATER: Bucket(c); break;
    case IT_WASTE:
        Blob(0, 0.14f, 0, 0.4f, 0.14f, 0.34f, c);
        for (int k = 0; k < 8; k++) {
            float a = (float)k * 0.8f;
            Rod(V(cosf(a) * 0.1f, 0.2f, sinf(a) * 0.1f), V(cosf(a) * 0.42f, 0.12f + 0.06f * (float)(k % 2), sinf(a) * 0.4f), 0.015f, Tone(c, 1.15f));
        }
        break;
    case IT_HEN: Bird(c, (Color){ 176, 120, 70, 255 }, (Color){ 220, 170, 60, 255 }, 1.15f, true, false); break;
    case IT_HERB:
        for (int k = 0; k < 6; k++) {
            float a = (float)k * 1.05f, lean = 0.12f;
            Vector3 top = { cosf(a) * lean, 0.6f + 0.06f * (float)(k % 2), sinf(a) * lean };
            Rod(V(0, 0, 0), top, 0.012f, (Color){ 90, 120, 60, 255 });
            for (int j = 0; j < 3; j++) {
                float t = 0.45f + 0.25f * (float)j;
                Blob(top.x * t + cosf(a + 1.0f) * 0.05f, top.y * t, top.z * t + sinf(a + 1.0f) * 0.05f, 0.07f, 0.035f, 0.05f, Tone(c, 0.9f + 0.1f * (float)j));
            }
        }
        Cyl(0, 0.12f, 0, 0.06f, 0.05f, 1.0f, (Color){ 150, 110, 70, 255 });
        break;
    case IT_SHEEP:
        for (int k = 0; k < 7; k++) Blob(-0.2f + 0.1f * (float)(k % 5), 0.36f + 0.08f * (float)(k / 5), -0.06f + 0.12f * (float)(k % 2), 0.16f, 0.15f, 0.15f, Tone(c, 0.94f + 0.03f * (float)(k % 3)));
        Blob(0.38f, 0.48f, 0.0f, 0.1f, 0.09f, 0.08f, (Color){ 60, 50, 44, 255 });
        for (int k = 0; k < 4; k++) Rod(V((k < 2 ? 0.2f : -0.2f), 0.26f, (k & 1 ? 0.1f : -0.1f)), V((k < 2 ? 0.2f : -0.2f), 0.0f, (k & 1 ? 0.1f : -0.1f)), 0.03f, (Color){ 70, 60, 52, 255 });
        break;
    case IT_PARTRIDGE: Bird(c, (Color){ 120, 96, 72, 255 }, (Color){ 120, 110, 100, 255 }, 0.95f, false, false); break;
    default: Blob(0, 0.3f, 0, 0.3f, 0.3f, 0.3f, c); break;
    }
}

// The sheet: every item drawn into its own square, seen from the front right and above.
static void Build(void)
{
    sheet = LoadRenderTexture(ICON_COLS * ICON_PX, ICON_ROWS * ICON_PX);
    if (!sheet.id) return;
    Vector3 light = G.light;
    float torch = G.drawTorch, fog = G.drawFog;
    G.light = (Vector3){ 1.12f, 1.06f, 0.96f };
    G.drawTorch = 0;
    G.drawFog = 0;
    SetDrawFlash(0);
    GfxHold(true);
    BeginTextureMode(sheet);
    ClearBackground(BLANK);
    rlEnableDepthTest();
    for (int it = 1; it < IT_COUNT; it++) {
        int col = it % ICON_COLS, row = it / ICON_COLS;
        rlDrawRenderBatchActive();
        rlViewport(col * ICON_PX, row * ICON_PX, ICON_PX, ICON_PX);
        rlMatrixMode(RL_PROJECTION);
        rlLoadIdentity();
        rlOrtho(-0.57, 0.57, -0.57, 0.57, -10.0, 10.0);
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
        Matrix view = MatrixLookAt((Vector3){ 1.5f, 1.5f, 2.2f }, (Vector3){ 0.0f, 0.36f, 0.0f }, (Vector3){ 0, 1, 0 });
        rlMultMatrixf(MatrixToFloat(view));
        rlPushMatrix();
        ItemModel(it);
        rlPopMatrix();
        rlDrawRenderBatchActive();
    }
    rlDisableDepthTest();
    EndTextureMode();
    GfxHold(false);
    G.light = light;
    G.drawTorch = torch;
    G.drawFog = fog;
    GenTextureMipmaps(&sheet.texture);
    SetTextureFilter(sheet.texture, TEXTURE_FILTER_TRILINEAR);
    ready = true;
}

// Called before the UI is drawn (it draws into a texture of its own, which the UI's scaling would spoil).
void IconsPrepare(void)
{
    if (tried || !IsWindowReady()) return;
    tried = true;
    Build();
}

// Item 'item' in the square r; false if there is no picture (then the caller draws its colour).
bool DrawItemIcon(int item, Rectangle r, Color tint)
{
    if (!ready || item <= IT_NONE || item >= IT_COUNT) return false;
    int col = item % ICON_COLS, row = item / ICON_COLS;
    Rectangle src = { (float)(col * ICON_PX), (float)(row * ICON_PX), (float)ICON_PX, -(float)ICON_PX };
    DrawTexturePro(sheet.texture, src, (Rectangle){ r.x + 2, r.y + 3, r.width, r.height }, (Vector2){ 0, 0 }, 0, Fade(BLACK, 0.28f * tint.a / 255.0f));
    DrawTexturePro(sheet.texture, src, r, (Vector2){ 0, 0 }, 0, tint);
    return true;
}
