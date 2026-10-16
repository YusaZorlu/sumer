#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PER_CAMP (MAX_RAIDERS / CAMPS)

static const float HOMES[PER_CAMP][2] = { { -4.5f, 2.5f }, { 3.5f, 3.0f }, { 5.0f, -3.5f }, { -2.5f, -5.0f } };
static const float CAMP_POS[CAMPS][2] = { { 34, 210 }, { 30, -240 } };

Vector3 CampCenter(int camp)
{
    float x = CAMP_POS[camp][0], z = CAMP_POS[camp][1];
    return (Vector3){ x, TerrainHeight(x, z), z };
}

static float Ground(float x, float z)
{
    return TerrainHeight(x, z);
}

static void Tent(float x, float z, float w, float d, Color c)
{
    float y = Ground(x, z);
    AddShape(x, y - 0.1f, z, w, 2.6f, d, c, true, SHAPE_CONE);
    AddShape(x, y + 2.3f, z, 0.12f, 0.7f, 0.12f, COL_WOOD, false, SHAPE_CYL);
    AddShape(x + 0.12f, y + 2.3f, z - 0.05f, 0.1f, 0.6f, 0.1f, COL_WOOD, false, SHAPE_CYL);
}

static void Restock(int camp)
{
    static const int pool[][3] = {
        { IT_BRONZE, 1, 3 }, { IT_TIN, 1, 2 }, { IT_COPPER, 3, 6 }, { IT_ARROWS, 6, 10 },
        { IT_WOLF, 1, 2 }, { IT_BREAD, 2, 4 }, { IT_COWHIDE, 1, 2 }, { IT_WOOD, 4, 8 },
    };
    for (int k = 0; k < 4; k++) {
        int pick = (int)(Hash2(G.day * 5 + k, 77 + camp) * 7.99f);
        if (k == 0) pick = Hash2(G.day, 3 + camp) < 0.5f ? 0 : 1;
        int n = pool[pick][1] + (int)(Hash2(G.day, k + 11 + camp) * (float)(pool[pick][2] - pool[pick][1] + 1) * 0.999f);
        if (n > ITEMS[pool[pick][0]].stack) n = ITEMS[pool[pick][0]].stack;
        G.stash[camp][k] = (Slot){ pool[pick][0], n };
    }
}

static void BuildCamp(int camp)
{
    const float cx = CAMP_POS[camp][0], cz = CAMP_POS[camp][1];
    const Color hide = { 128, 98, 66, 255 }, dark = { 96, 74, 52, 255 };

    Tent(cx - 6.5f, cz - 1.0f, 3.0f, 3.6f, hide);
    Tent(cx + 6.0f, cz - 2.0f, 3.0f, 3.2f, dark);
    Tent(cx + 0.5f, cz + 7.0f, 3.6f, 3.0f, hide);

    float y = Ground(cx, cz);
    for (int k = 0; k < 7; k++) {
        float a = (float)k * 2.0f * PI / 7.0f;
        AddShape(cx + cosf(a) * 0.75f, y - 0.05f, cz + sinf(a) * 0.75f, 0.34f, 0.28f, 0.3f, COL_STONE, false, SHAPE_BLOB);
    }
    AddBox(cx, y, cz, 0.9f, 0.12f, 0.2f, COL_WOOD, false);
    AddBox(cx, y, cz, 0.2f, 0.12f, 0.9f, COL_WOOD, false);
    AddGlow(cx, y + 0.12f, cz, 0.55f, 0.35f, 0.55f, COL_FIRE);
    AddGlow(cx, y + 0.3f, cz, 0.3f, 0.4f, 0.3f, (Color){ 255, 214, 120, 255 });

    for (int k = 0; k < 4; k++) {
        float a = (float)k * PI / 2 + 0.4f;
        AddBox(cx + cosf(a) * 2.4f, Ground(cx + cosf(a) * 2.4f, cz + sinf(a) * 2.4f), cz + sinf(a) * 2.4f, 0.9f, 0.4f, 0.4f, COL_PALM_TRUNK, false);
    }

    float sx = cx + 2.4f, sz = cz - 7.0f, sy = Ground(sx, sz);
    G.stations[camp == 0 ? STN_STASH : STN_STASH2] = (Vector3){ sx, sy, sz + 1.1f };
    AddBox(sx, sy, sz, 1.4f, 0.7f, 0.9f, COL_WOOD, true);
    AddBox(sx, sy + 0.7f, sz, 1.5f, 0.12f, 1.0f, COL_PALM_TRUNK, false);
    AddBox(sx, sy + 0.25f, sz + 0.46f, 0.2f, 0.2f, 0.04f, COL_BRONZE, false);

    float rx = cx - 3.0f, rz = cz - 6.5f, ry = Ground(rx, rz);
    AddBox(rx - 1.0f, ry, rz, 0.1f, 1.8f, 0.1f, COL_WOOD, false);
    AddBox(rx + 1.0f, ry, rz, 0.1f, 1.8f, 0.1f, COL_WOOD, false);
    AddBox(rx, ry + 1.7f, rz, 2.2f, 0.08f, 0.08f, COL_WOOD, false);
    AddBox(rx - 0.4f, ry + 0.7f, rz, 0.7f, 1.0f, 0.04f, (Color){ 128, 122, 112, 255 }, false);
    AddBox(rx + 0.45f, ry + 0.8f, rz, 0.6f, 0.9f, 0.04f, (Color){ 130, 92, 60, 255 }, false);

    for (int k = 0; k < 10; k++) {
        float a = (float)k * 2.0f * PI / 10.0f + 0.3f;
        if (k == 2 || k == 7) continue;
        float px = cx + cosf(a) * 11.0f, pz = cz + sinf(a) * 11.0f;
        AddShape(px, Ground(px, pz) - 0.1f, pz, 0.24f, 1.5f + 0.3f * Hash2(k, 4), 0.24f, COL_PALM_TRUNK, true, SHAPE_CYL);
        AddShape(px, Ground(px, pz) + 1.35f + 0.3f * Hash2(k, 4), pz, 0.24f, 0.3f, 0.24f, COL_PALM_TRUNK, false, SHAPE_CONE);
    }
    KeepOut(cx - 12, cz - 12, cx + 12, cz + 12);

    for (int k = 0; k < PER_CAMP; k++) {
        Raider *r = &G.raiders[camp * PER_CAMP + k];
        r->camp = camp;
        r->home = (Vector3){ cx + HOMES[k][0], 0, cz + HOMES[k][1] };
        r->home.y = Ground(r->home.x, r->home.z);
        r->pos = r->home;
        r->face = RandF(-PI, PI);
        r->timer = RandF(1, 5);
        r->speed = 1.1f;
        r->respawnDay = 0;
        r->moving = false;
        r->vit = (Vitals){ .used = false };
    }
}

void BuildRaiders(void)
{
    for (int c = 0; c < CAMPS; c++) BuildCamp(c);
    G.stashDay = 0;
}

static void Idle(Raider *r, int i, float dt)
{
    bool night = G.night > 0.5f;
    float cx = CAMP_POS[r->camp][0], cz = CAMP_POS[r->camp][1];
    Vector3 seat = { cx + cosf((float)i * PI / 2 + 0.4f) * 2.4f, 0, cz + sinf((float)i * PI / 2 + 0.4f) * 2.4f };

    r->timer -= dt;
    if (night) {
        Vector3 to = Vector3Subtract(seat, r->pos);
        to.y = 0;
        float d = Vector3Length(to);
        r->moving = d > 0.6f;
        if (r->moving) {
            r->face = atan2f(to.x, to.z);
            r->pos.x += to.x / d * r->speed * dt;
            r->pos.z += to.z / d * r->speed * dt;
        } else {
            r->face = atan2f(cx - r->pos.x, cz - r->pos.z);
        }
    } else {
        if (r->timer <= 0) {
            r->timer = RandF(2.5f, 7.0f);
            r->moving = GetRandomValue(0, 2) > 0;
            r->face = XZDist(r->pos, r->home) > 6.0f ? atan2f(r->home.x - r->pos.x, r->home.z - r->pos.z) : RandF(-PI, PI);
        }
        if (r->moving) {
            float nx = r->pos.x + sinf(r->face) * r->speed * dt, nz = r->pos.z + cosf(r->face) * r->speed * dt;
            if (SolidAt(nx, nz, 0.35f) || BodyAt((Vector3){ nx, r->pos.y, nz }, 0.38f, OWNER_RAIDER + i)) {
                r->face += PI * 0.7f;
            } else {
                r->pos.x = nx;
                r->pos.z = nz;
            }
        }
    }
    r->pos.y = Ground(r->pos.x, r->pos.z);
}

void UpdateRaiders(float dt)
{
    if (G.hour >= 6.0f && G.stashDay != G.day) {
        G.stashDay = G.day;
        for (int c = 0; c < CAMPS; c++) Restock(c);
    }
    for (int i = 0; i < MAX_RAIDERS; i++) {
        Raider *r = &G.raiders[i];
        if (r->vit.dead) {
            if (r->respawnDay == 0) r->respawnDay = (float)G.day;
            bool far = XZDist(G.player.pos, r->home) > 70.0f;
            if ((float)G.day > r->respawnDay && G.hour >= 6.0f && far) {
                r->vit = (Vitals){ .used = false };
                r->pos = r->home;
                r->respawnDay = 0;
            }
            continue;
        }
        if (!r->vit.override) Idle(r, i, dt);
    }
}

void DrawRaiders(void)
{
    float time = (float)GetTime();

    for (int i = 0; i < MAX_RAIDERS; i++) {
        const Raider *r = &G.raiders[i];
        Vector3 p = r->vit.override ? r->vit.at : r->pos;
        float f = r->vit.override ? r->vit.face : r->face;
        if (XZDist(p, G.cam.position) > 220.0f) continue;

        Look look = { (Color){ 150, 102, 70, 255 }, (Color){ 96, 72, 48, 255 }, (i & 1) ? (Color){ 150, 102, 70, 255 } : (Color){ 112, 88, 60, 255 },
                      (Color){ 30, 24, 20, 255 } };
        LightFor(p);
        if (r->vit.dead) {
            DrawPersonBody(p, f, look);
            continue;
        }
        bool sitting = !r->vit.override && !r->moving && G.night > 0.5f;
        float stride = (r->vit.override || r->moving) ? time * (r->vit.override ? 8.0f : 4.0f) : 0.0f;
        DrawHuman(p, f, stride, look, sitting, false, 0);

        float k = Clamp(r->vit.swing / 0.35f, 0.0f, 1.0f);
        float angle = r->vit.swing > 0 ? -80.0f + 120.0f * (1.0f - k) : (r->vit.override ? -40.0f : 30.0f);
        if (r->vit.windup > 0) angle = -40.0f - 80.0f * (1.0f - r->vit.windup / 0.55f);
        rlPushMatrix();
        rlTranslatef(p.x, p.y + (sitting ? 1.04f : 1.42f), p.z);
        rlRotatef(f * RAD2DEG, 0, 1, 0);
        rlTranslatef(0.3f, 0, 0.1f);
        rlRotatef(angle, 1, 0, 0);
        DrawShadedBox((Vector3){ 0, 0, 0.42f }, (Vector3){ 0.05f, 0.05f, 0.85f }, COL_WOOD);
        DrawShadedBox((Vector3){ 0, 0.1f, 0.8f }, (Vector3){ 0.04f, 0.3f, 0.2f }, COL_STONE);
        rlPopMatrix();
        rlPushMatrix();
        rlTranslatef(p.x, p.y, p.z);
        rlRotatef(f * RAD2DEG, 0, 1, 0);
        DrawShadedBox((Vector3){ 0, (sitting ? 1.52f : 1.9f) + 0.06f, -0.02f }, (Vector3){ 0.34f, 0.1f, 0.34f }, (Color){ 128, 122, 112, 255 });
        rlPopMatrix();
    }
    G.drawTorch = G.drawFog = 0;
}

static int StashItems(int camp)
{
    int n = 0;
    for (int k = 0; k < 4; k++) n += G.stash[camp][k].count;
    return n;
}

void StashPrompt(int camp, char *buf, int size, bool *ok)
{
    int n = StashItems(camp);
    *ok = n > 0;
    if (*ok) Fmt(buf, size, "[E] Take the raiders' loot (%d things)", n);
    else Fmt(buf, size, "The raiders' chest is empty. They bring back more at dawn.");
}

void TakeStash(int camp)
{
    char log[256] = "";
    bool left = false;

    for (int k = 0; k < 4; k++) {
        Slot *s = &G.stash[camp][k];
        if (s->item == IT_NONE) continue;
        int got = s->item == IT_ARROWS ? GiveArrows(s->count) : BagAdd(s->item, s->count);
        if (got > 0) snprintf(log + strlen(log), sizeof(log) - strlen(log), "%s%d %s", log[0] ? ", " : "", got, ItemName(s->item));
        s->count -= got;
        if (s->count <= 0) *s = (Slot){ IT_NONE, 0 };
        else left = true;
    }
    if (!log[0]) Say("No room in your baskets.");
    else Say(left ? "Took %s. Your baskets are full." : "Took %s.", log);
}
