#include "game.h"
#include <math.h>
#include <stdlib.h>

bool InsideWalls(float x, float z, float margin)
{
    float dx = x / (CITY_A - margin), dz = (z - CITY_Z) / (CITY_B - margin);
    return dx * dx + dz * dz < 1.0f;
}

static void Bridge(float x, float z, bool alongX, float len)
{
    if (alongX) {
        AddBox(x, 0, z, len, 0.3f, 3.2f, COL_WOOD, true);
        AddBox(x, 0.3f, z - 1.5f, len - 2.0f, 0.6f, 0.12f, COL_WOOD, true);
        AddBox(x, 0.3f, z + 1.5f, len - 2.0f, 0.6f, 0.12f, COL_WOOD, true);
        KeepOut(x - len / 2 - 2, z - 3, x + len / 2 + 2, z + 3);
    } else {
        AddBox(x, 0, z, 3.2f, 0.3f, len, COL_WOOD, true);
        AddBox(x - 1.5f, 0.3f, z, 0.12f, 0.6f, len - 2.0f, COL_WOOD, true);
        AddBox(x + 1.5f, 0.3f, z, 0.12f, 0.6f, len - 2.0f, COL_WOOD, true);
        KeepOut(x - 3, z - len / 2 - 2, x + 3, z + len / 2 + 2);
    }
}

static void Boat(float x, float z, bool alongX)
{
    float l = 3.6f, w = 1.0f;
    if (alongX) {
        AddBox(x, -0.12f, z, l, 0.36f, w, COL_STRAW, false);
        AddBox(x - l / 2, 0.0f, z, 0.35f, 0.55f, w * 0.7f, COL_STRAW, false);
        AddBox(x + l / 2, 0.0f, z, 0.35f, 0.55f, w * 0.7f, COL_STRAW, false);
    } else {
        AddBox(x, -0.12f, z, w, 0.36f, l, COL_STRAW, false);
        AddBox(x, 0.0f, z - l / 2, w * 0.7f, 0.55f, 0.35f, COL_STRAW, false);
        AddBox(x, 0.0f, z + l / 2, w * 0.7f, 0.55f, 0.35f, COL_STRAW, false);
    }
}

// The river is broken only at the far north, where a shallow ford can be waded.
static void BuildRiver(void)
{
    AddWater(RIVER_X0, -WORLD_HALF, RIVER_X1, FORD_Z0);
    AddWater(RIVER_X0, FORD_Z1, RIVER_X1, WORLD_HALF);
    AddBox((RIVER_X0 + RIVER_X1) / 2, -0.12f, (FORD_Z0 + FORD_Z1) / 2, RIVER_X1 - RIVER_X0, 0.16f, FORD_Z1 - FORD_Z0,
           (Color){ 104, 150, 158, 255 }, false);
    for (int k = 0; k < 14; k++) {
        float x = RIVER_X0 + 1.5f + Hash2(k, 3) * (RIVER_X1 - RIVER_X0 - 3), z = FORD_Z0 + 1 + Hash2(k, 9) * (FORD_Z1 - FORD_Z0 - 2);
        AddShape(x, -0.15f, z, 0.9f + Hash2(k, 5), 0.4f, 0.8f + Hash2(k, 7), COL_STONE, false, SHAPE_BLOB);
    }
}

static void BuildWaterways(void)
{
    BuildRiver();
    AddWater(-128, 87, 124, 91);
    AddWater(120, -108, 124, 91);
    AddWater(-128, -108, 124, -104);
    AddWater(-128, -15, -74, -5);
    AddWater(-75, -26, -58, 2);

    Bridge(0, 89, false, 8);
    Bridge(122, 9, true, 8);
    Bridge(-37, -106, false, 8);
    Bridge(-114, -10, false, 14);

    AddBox(-66.5f, 0, -26.5f, 18.0f, 0.35f, 1.0f, COL_BRICK, false);
    AddBox(-66.5f, 0, 2.5f, 18.0f, 0.35f, 1.0f, COL_BRICK, false);
    AddBox(-57.5f, 0, -12.0f, 1.0f, 0.35f, 29.0f, COL_BRICK, false);
    Boat(-70, -20, true);
    Boat(-64, -16, true);
    Boat(-71, -3, true);
    Boat(-62, -2, false);
    KeepOut(-80, -29, -54, 5);
    KeepOut(-128, -17, -74, -3);
}

static bool GateGap(float x, float z)
{
    if (z > CITY_Z && fabsf(x) < 5.5f) return true;
    if (z < CITY_Z && x > -42.5f && x < -31.5f) return true;
    if (x > 0 && z > 4.0f && z < 14.0f) return true;
    if (x < 0 && z > 21.0f && z < 31.0f) return true;
    if (x < 0 && z > -16.0f && z < -4.0f) return true;
    return false;
}

// Axis-snapped so the ladder lies flat on one face of the block.
static void AddLadder(int i, const float *px, const float *pz, int n)
{
    if (G.ladderCount >= MAX_LADDERS) return;
    float x = px[i], z = pz[i];
    float gx = x / (CITY_A * CITY_A), gz = (z - CITY_Z) / (CITY_B * CITY_B);
    Vector3 in = fabsf(gx) * CITY_A > fabsf(gz) * CITY_B ? (Vector3){ gx > 0 ? -1.0f : 1.0f, 0, 0 } : (Vector3){ 0, 0, gz > 0 ? -1.0f : 1.0f };
    Vector3 t = { -in.z, 0, in.x };
    int a = (i + n - 1) % n, b = (i + 1) % n;
    float da = (px[a] - x) * in.x + (pz[a] - z) * in.z, db = (px[b] - x) * in.x + (pz[b] - z) * in.z;
    int low = da < db ? a : b;
    float side = ((px[low] - x) * t.x + (pz[low] - z) * t.z) > 0 ? 1.0f : -1.0f;

    Vector3 c = { x + t.x * 0.6f * side, WALL_TOP, z + t.z * 0.6f * side };
    Vector3 face = { c.x + in.x * 1.7f, 0, c.z + in.z * 1.7f };
    float rx = fabsf(t.x) * 0.62f + 0.06f, rz = fabsf(t.z) * 0.62f + 0.06f;

    for (int k = -1; k <= 1; k += 2)
        AddBox(face.x + in.x * 0.12f + t.x * 0.28f * (float)k, 0, face.z + in.z * 0.12f + t.z * 0.28f * (float)k, 0.08f, WALL_TOP + 0.7f, 0.08f, COL_WOOD, false);
    for (float y = 0.35f; y < WALL_TOP + 0.4f; y += 0.42f)
        AddBox(face.x + in.x * 0.12f, y, face.z + in.z * 0.12f, rx, 0.05f, rz, COL_PALM_TRUNK, false);

    Ladder *l = &G.ladders[G.ladderCount++];
    l->n = in;
    l->wall = c;
    l->top = WALL_TOP;
    l->base = (Vector3){ face.x + in.x * 0.8f, 0, face.z + in.z * 0.8f };
    l->base.y = TerrainHeight(l->base.x, l->base.z);
    KeepOut(l->base.x - 1.6f, l->base.z - 1.6f, l->base.x + 1.6f, l->base.z + 1.6f);
}

static void BuildWalls(void)
{
    enum { N = 240 };
    static const int ladders[] = { 15, 45, 75, 111, 159, 200, 225 };
    float px[N], pz[N];
    bool gap[N];

    for (int i = 0; i < N; i++) {
        float a = 2.0f * PI * (float)i / (float)N;
        px[i] = CITY_A * cosf(a);
        pz[i] = CITY_Z + CITY_B * sinf(a);
        gap[i] = GateGap(px[i], pz[i]);
    }
    for (int i = 0; i < N; i++) {
        if (gap[i]) continue;
        bool tower = gap[(i + N - 1) % N] || gap[(i + 1) % N] || i % 12 == 0;
        if (tower) {
            AddBox(px[i], 0, pz[i], 5.2f, WALL_TOP + 0.5f, 5.2f, COL_HOUSE_DARK, true);
            AddBox(px[i], WALL_TOP + 0.4f, pz[i], 5.5f, 0.12f, 5.5f, COL_WALL, false);
            for (int k = 0; k < 4; k++)
                AddBox(px[i] + ((k & 1) ? 2.2f : -2.2f), WALL_TOP + 0.5f, pz[i] + ((k & 2) ? 2.2f : -2.2f), 0.9f, 0.9f, 0.9f, COL_HOUSE_DARK, true);
        } else {
            AddBox(px[i], 0, pz[i], 3.4f, WALL_TOP, 3.4f, COL_WALL, true);
            float gx = px[i] / (CITY_A * CITY_A), gz = (pz[i] - CITY_Z) / (CITY_B * CITY_B), gl = sqrtf(gx * gx + gz * gz);
            AddBox(px[i] + gx / gl * 1.45f, WALL_TOP, pz[i] + gz / gl * 1.45f, 0.7f, 0.8f, 0.7f, COL_WALL, true);
        }
    }
    G.ladderCount = 0;
    for (int k = 0; k < (int)(sizeof(ladders) / sizeof(ladders[0])); k++)
        if (!gap[ladders[k]]) AddLadder(ladders[k], px, pz, N);
}

#define MAX_FIELDS 8
static int fieldFirst[MAX_FIELDS], fieldLast[MAX_FIELDS], fieldCount;

static void Field(float x0, float z0, float x1, float z1)
{
    AddBox((x0 + x1) / 2, 0, (z0 + z1) / 2, x1 - x0, 0.1f, z1 - z0, COL_SOIL, false);
    if (fieldCount < MAX_FIELDS) fieldFirst[fieldCount] = G.boxCount;
    for (float z = z0 + 0.7f; z < z1 - 0.4f; z += 1.6f)
        AddBox((x0 + x1) / 2, 0.1f, z, x1 - x0 - 0.8f, 0.6f, 0.6f, COL_WHEAT_GREEN, false);
    if (fieldCount < MAX_FIELDS) fieldLast[fieldCount++] = G.boxCount;
    KeepOut(x0, z0, x1, z1);
}

// Each field runs a little ahead of or behind its neighbour.
void UpdateFields(void)
{
    for (int f = 0; f < fieldCount; f++) {
        float growth;
        bool stubble, bare = !CropStage(0.06f * (float)(f % 3) - 0.06f, &growth, &stubble);
        float h = bare ? 0.03f : stubble ? 0.14f : 0.1f + 0.75f * growth;
        Color c = bare ? (Color){ 84, 64, 44, 255 } : stubble ? COL_STRAW
                : growth >= 1.0f ? COL_WHEAT_RIPE : Mix(COL_WHEAT_GREEN, COL_WHEAT_RIPE, growth * 0.4f);
        for (int i = fieldFirst[f]; i < fieldLast[f]; i++) {
            G.boxes[i].size.y = h;
            G.boxes[i].pos.y = 0.1f + h / 2;
            G.boxes[i].color = c;
        }
    }
}

void BuildCity(void)
{
    fieldCount = 0;
    BuildWaterways();
    BuildWalls();
    BuildTemple();

    KeepOut(-4.5f, -8, 4.5f, 82);
    KeepOut(-104, 6, 104, 12);
    KeepOut(-104, 23, -40, 29);
    KeepOut(-104, 37, 104, 41);
    KeepOut(-40, -94, -34, 6);

    AddBox(0, 0, 38, 7, 0.03f, 88, COL_ROAD, false);

    Field(-62, 96, -10, 136);
    Field(10, 96, 62, 136);
    Field(-84, -150, -44, -122);
    Field(-30, -150, 26, -122);
    Field(36, -150, 90, -122);
}

static void CourtyardHouse(float x, float z, float w, float d, float h, Color c)
{
    const float t = 0.6f;
    AddBox(x, 0, z - d / 2 + t / 2, w, h, t, c, true);
    AddBox(x, 0, z + d / 2 - t / 2, w, h, t, c, true);
    AddBox(x - w / 2 + t / 2, 0, z, t, h, d - 2 * t, c, true);
    AddBox(x + w / 2 - t / 2, 0, z, t, h, d - 2 * t, c, true);
    AddBox(x, 0, z, w - 2 * t, 0.03f, d - 2 * t, COL_BRICK, false);
}

// Fills every free cell inside the walls; a cell that touches a street tries a smaller, shifted house first.
void BuildCityHouses(void)
{
    const float cell = 7.0f;
    const Color tones[4] = { COL_HOUSE, COL_HOUSE_DARK, COL_WALL, COL_MUD_PALE };
    static const float tries[5][3] = { { 0, 0, 1.0f }, { 0, 2.0f, 0.8f }, { 0, -2.0f, 0.8f }, { 2.0f, 0, 0.8f }, { -2.0f, 0, 0.8f } };
    int ix = 0;

    for (float x = -CITY_A; x <= CITY_A; x += cell, ix++) {
        int iz = 0;
        for (float z = CITY_Z - CITY_B; z <= CITY_Z + CITY_B; z += cell, iz++) {
            if (Hash2(ix + 101, iz + 57) < 0.06f) continue;

            float bw = 4.4f + 2.0f * Hash2(ix * 7 + 1, iz * 13 + 2);
            float bd = 4.4f + 2.0f * Hash2(ix + 31, iz + 17);
            float jx = (Hash2(ix * 3 + 5, iz * 5 + 9) - 0.5f) * fmaxf(0.0f, cell - bw - 0.8f);
            float jz = (Hash2(iz * 11 + 3, ix * 2 + 7) - 0.5f) * fmaxf(0.0f, cell - bd - 0.8f);
            float cx = 0, cz = 0, w = 0, d = 0;
            bool ok = false;

            for (int t = 0; t < 5 && !ok; t++) {
                w = bw * tries[t][2];
                d = bd * tries[t][2];
                cx = x + jx + tries[t][0];
                cz = z + jz + tries[t][1];
                float half = fmaxf(w, d) / 2;
                ok = InsideWalls(cx, cz, 3.2f + half) && !InKeepOut(cx, cz, half + 0.5f) && !InWater(cx, cz);
            }
            if (!ok) continue;

            float h = 2.8f + 1.8f * Hash2(ix + 7, iz + 3);
            Color c = tones[(int)(Hash2(ix + 19, iz + 23) * 3.99f)];
            bool court = w > 5.6f && d > 5.6f && Hash2(ix + 41, iz + 43) < 0.3f;
            if (court) CourtyardHouse(cx, cz, w, d, h, c);
            else AddBox(cx, 0, cz, w, h, d, c, true);
            Color trim = Mix(c, COL_MUD_DARK, 0.35f), base = Mix(c, COL_MUD_DARK, 0.5f);
            if (court) {
                for (int k = -1; k <= 1; k += 2) {
                    AddBox(cx, h - 0.06f, cz + (float)k * (d / 2 - 0.3f), w + 0.16f, 0.2f, 0.76f, trim, false);
                    AddBox(cx + (float)k * (w / 2 - 0.3f), h - 0.06f, cz, 0.76f, 0.2f, d - 0.6f, trim, false);
                }
            } else {
                AddBox(cx, h - 0.06f, cz, w + 0.16f, 0.2f, d + 0.16f, trim, false);
                AddBox(cx, 0, cz, w + 0.1f, 0.35f, d + 0.1f, base, false);
            }

            if (Hash2(ix + 13, iz + 29) < 0.4f) {
                float ox = (Hash2(ix, iz + 61) < 0.5f ? -1.0f : 1.0f) * w / 4;
                AddBox(cx + ox, h, cz - d / 4, w / 2, 2.0f, d / 2, tones[(int)(Hash2(ix + 3, iz + 71) * 3.99f)], false);
            }
            int side = (int)(Hash2(ix + 77, iz + 5) * 3.99f);
            float dx = (side == 0) ? -w / 2 : (side == 1) ? w / 2 : 0.0f;
            float dz = (side == 2) ? -d / 2 : (side == 3) ? d / 2 : 0.0f;
            AddBox(cx + dx, 0, cz + dz, dx != 0 ? 0.12f : 1.0f, 2.0f, dz != 0 ? 0.12f : 1.0f, COL_DOORWAY, false);
            AddBox(cx + dx, 2.0f, cz + dz, dx != 0 ? 0.18f : 1.3f, 0.16f, dz != 0 ? 0.18f : 1.3f, COL_WOOD, false);
            for (int k = -1; k <= 1; k++) {
                float along = 0.9f * (float)k;
                AddBox(cx + dx * 1.04f + (dz != 0 ? along : 0), h - 0.42f, cz + dz * 1.04f + (dx != 0 ? along : 0), 0.13f, 0.13f, 0.13f, COL_PALM_TRUNK, false);
            }
            if (!court && h > 3.4f) {
                int wside = (side + 1 + (int)(Hash2(ix + 9, iz + 31) * 2.99f)) % 4;
                float wx = (wside == 0) ? -w / 2 : (wside == 1) ? w / 2 : 0.0f;
                float wz = (wside == 2) ? -d / 2 : (wside == 3) ? d / 2 : 0.0f;
                AddBox(cx + wx, h - 1.3f, cz + wz, wx != 0 ? 0.1f : 0.55f, 0.4f, wz != 0 ? 0.1f : 0.55f, COL_DOORWAY, false);
            }
            if (Hash2(ix + 5, iz + 91) < 0.16f) {
                float nx = (dx > 0) - (dx < 0), nz = (dz > 0) - (dz < 0);
                AddTorch(cx + dx + nx * 0.2f + nz * 0.9f, 2.3f, cz + dz + nz * 0.2f + nx * 0.9f, false);
            }
        }
    }
}
