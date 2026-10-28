#include "game.h"
#include "rlgl.h"
#include <math.h>
#include <stddef.h>

#define CARAVAN_SPEED  2.7f
#define TRADE_SECONDS  150.0f
#define SPACING        2.6f

static const Offer DILMUN[] = {
    { IT_POT,    3, IT_NONE, 0, OUT_ITEM,   IT_TIN,     2, 0, 0 },
    { IT_FISH,   6, IT_NONE, 0, OUT_ITEM,   IT_TIN,     1, 0, 0 },
    { IT_WHEAT,  6, IT_NONE, 0, OUT_ITEM,   IT_COPPER,  4, 0, 0 },
    { IT_EGG,    6, IT_NONE, 0, OUT_ITEM,   IT_TIN,     1, 0, 0 },
    { IT_HERB,   6, IT_NONE, 0, OUT_ITEM,   IT_TIN,     1, 0, 0 },
    { IT_BRONZE, 2, IT_NONE, 0, OUT_BASKET, 0,          1, 0, 0 },
};

static const Offer ELAM[] = {
    { IT_POT,     2, IT_NONE, 0, OUT_ITEM, IT_WOOD,    8, 0, 0 },
    { IT_WOLF,    1, IT_NONE, 0, OUT_ITEM, IT_TIN,     2, 0, 0 },
    { IT_BREAD,   4, IT_NONE, 0, OUT_ITEM, IT_TIN,     1, 0, 0 },
    { IT_GRILLED, 4, IT_NONE, 0, OUT_ITEM, IT_WOOD,    6, 0, 0 },
    { IT_BRONZE,  1, IT_POT,  2, OUT_ITEM, IT_ARROWS, 20, 0, 0 },
};

static void SetPath(Caravan *c, float stallX)
{
    const Vector3 pts[6] = { { 0, 0, WORLD_LIMIT }, { 0, 0, 94 }, { 0, 0, 74 }, { 0, 0, -2 },
                             { stallX * 0.6f, 0, -9 }, { stallX, 0, -11 } };
    c->points = 6;
    c->length = 0;
    for (int i = 0; i < 6; i++) {
        c->path[i] = pts[i];
        if (i > 0) c->length += Vector3Distance(pts[i - 1], pts[i]);
    }
}

static Vector3 PathAt(const Caravan *c, float s, Vector3 *dir)
{
    s = Clamp(s, 0.0f, c->length);
    for (int i = 1; i < c->points; i++) {
        float seg = Vector3Distance(c->path[i - 1], c->path[i]);
        if (s <= seg || i == c->points - 1) {
            *dir = Vector3Normalize(Vector3Subtract(c->path[i], c->path[i - 1]));
            return Vector3Lerp(c->path[i - 1], c->path[i], seg > 0 ? Clamp(s / seg, 0.0f, 1.0f) : 0.0f);
        }
        s -= seg;
    }
    *dir = (Vector3){ 0, 0, -1 };
    return c->path[0];
}

void BuildCaravans(void)
{
    Trader *t = G.traders;

    t[TR_DILMUN] = (Trader){ "Dilmun merchant", "Tin and copper by ship from Dilmun and Magan. Pots sell well there.",
                             { -10, 0, -11 }, 0, { 70, 110, 140, 255 }, DILMUN, (int)(sizeof(DILMUN) / sizeof(Offer)), true, true, { .hp = 0 } };
    t[TR_ELAM] = (Trader){ "Elamite trader", "Timber from the eastern mountains. Wolf pelts fetch a fair price.",
                           { 10, 0, -11 }, 0, { 130, 70, 90, 255 }, ELAM, (int)(sizeof(ELAM) / sizeof(Offer)), true, true, { .hp = 0 } };

    Caravan *a = &G.caravans[0], *b = &G.caravans[1];
    SetPath(a, -10);
    SetPath(b, 10);
    a->trader = TR_DILMUN;
    b->trader = TR_ELAM;
    a->robe = t[TR_DILMUN].robe;
    b->robe = t[TR_ELAM].robe;
    a->cloth = (Color){ 180, 60, 50, 255 };
    b->cloth = (Color){ 200, 160, 70, 255 };
    a->state = CV_ARRIVE;
    a->s = a->length - 30.0f;
    b->state = CV_AWAY;
    b->timer = 80.0f;
}

// Keeps to the right of its direction of travel, so arriving and leaving caravans pass each other.
static Vector3 LaneAt(const Caravan *c, float s, bool leaving, Vector3 *dir)
{
    Vector3 p = PathAt(c, s, dir);
    Vector3 d = leaving ? Vector3Negate(*dir) : *dir;
    float k = Clamp((c->length - s) / 5.0f, 0.0f, 1.0f) * Clamp(s / 5.0f, 0.0f, 1.0f);
    p.x += -d.z * 1.4f * k;
    p.z += d.x * 1.4f * k;
    return p;
}

// Ignores other caravans; anything else blocking for long is pushed past.
static bool Blocked(Caravan *c, int i, float ds, float dt)
{
    Vector3 dir;
    bool leaving = ds < 0;
    Vector3 cur = LaneAt(c, c->s, leaving, &dir), next = LaneAt(c, c->s + ds * 8.0f, leaving, &dir);

    if (c->pushT > 0) {
        c->pushT -= dt;
        return false;
    }
    if (!BodyAheadIgnoring(cur, next, 0.6f, CARAVAN_OWNER(i, 0), OWNER_CARAVAN, CARAVAN_OWNER(MAX_CARAVANS, 0))) {
        c->blockedT = 0;
        return false;
    }
    c->blockedT += dt;
    if (c->blockedT > 2.5f) {
        c->blockedT = 0;
        c->pushT = 2.0f;
    }
    return true;
}

// The caravan halts while any of its people is fighting.
static bool Halted(const Caravan *c)
{
    for (int k = 0; k <= CARAVAN_GUARDS; k++)
        if (c->vit[k].override) return true;
    return false;
}

void UpdateCaravans(float dt)
{
    if (Match()) return;   // no caravans (and their guards) in a battle royale or a party
    for (int i = 0; i < MAX_CARAVANS; i++) {
        Caravan *c = &G.caravans[i];
        Trader *t = &G.traders[c->trader];

        switch (c->state) {
        case CV_AWAY:
            c->timer -= dt;
            if (c->timer <= 0) {
                c->state = CV_ARRIVE;
                c->s = 0;
            }
            break;
        case CV_ARRIVE:
            if (Halted(c) || Blocked(c, i, CARAVAN_SPEED * dt, dt)) break;
            c->s += CARAVAN_SPEED * dt;
            if (c->s >= c->length) {
                c->s = c->length;
                c->state = CV_TRADE;
                c->timer = TRADE_SECONDS;
                Announce("The %s has set up in the temple courtyard.", t->name);
            }
            break;
        case CV_TRADE:
            c->timer -= dt;
            if (c->timer <= 0) c->state = CV_LEAVE;
            break;
        case CV_LEAVE:
            if (Halted(c) || Blocked(c, i, -CARAVAN_SPEED * dt, dt)) break;
            c->s -= CARAVAN_SPEED * dt;
            if (c->s <= 0) {
                c->state = CV_AWAY;
                c->timer = RandF(60, 100);
            }
            break;
        }
        t->away = c->state != CV_TRADE || c->vit[0].dead || c->vit[0].override;
    }
}

// Member 0 is the merchant, 1-2 donkeys, 3-4 the guards.
static Vector3 Member(const Caravan *c, int k, float *facing)
{
    Vector3 dir;
    Vector3 stall = c->path[c->points - 1];
    float side = stall.x > 0 ? 1.0f : -1.0f;
    Vector3 p;

    if (c->state == CV_TRADE) {
        static const float off[5][3] = { { 0, 0, 0 }, { 2.4f, 0, -1.2f }, { 2.4f, 0, 1.0f }, { -1.9f, 0, 1.6f }, { -2.2f, 0, -1.3f } };
        p = (Vector3){ stall.x + off[k][0] * side, 0, stall.z + off[k][2] };
        *facing = (k == 1 || k == 2) ? -side * PI / 2 : 0.0f;
    } else {
        bool leaving = c->state == CV_LEAVE;
        float s = leaving ? c->s + SPACING * (float)k : c->s - SPACING * (float)k;
        p = LaneAt(c, s, leaving, &dir);
        if (leaving) dir = Vector3Negate(dir);
        *facing = atan2f(dir.x, dir.z);
    }
    p.y = GroundHeight(p.x, p.z, 0.4f);
    return p;
}

bool CaravanMemberScript(int ci, int k, Vector3 *pos, float *facing)
{
    const Caravan *c = &G.caravans[ci];
    if (c->state == CV_AWAY) return false;
    *pos = Member(c, k, facing);
    return true;
}

static const Vitals *MemberVitals(const Caravan *c, int k)
{
    return k == 0 ? &c->vit[0] : k >= 3 && k < 3 + CARAVAN_GUARDS ? &c->vit[k - 2] : NULL;
}

bool CaravanMember(int ci, int k, Vector3 *pos, float *facing)
{
    const Caravan *c = &G.caravans[ci];
    const Vitals *v = MemberVitals(c, k);
    if (c->state == CV_AWAY) return false;
    if (v && v->override) {
        *pos = v->at;
        *facing = v->face;
        return true;
    }
    *pos = Member(c, k, facing);
    return true;
}

bool CaravanNear(Vector3 p, float r)
{
    for (int i = 0; i < MAX_CARAVANS; i++) {
        Vector3 m;
        float f;
        for (int k = 0; k < 3 + CARAVAN_GUARDS; k++) {
            const Vitals *v = MemberVitals(&G.caravans[i], k);
            if (v && !v->dead && CaravanMember(i, k, &m, &f) && XZDist(m, p) < r) return true;
        }
    }
    return false;
}

static void Part(float x, float y, float z, float w, float h, float d, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, d }, c);
}

static void DrawDonkey(Vector3 p, float facing, Color cloth, bool moving)
{
    const Color coat = { 140, 132, 124, 255 }, dark = { 70, 64, 60, 255 };
    float bob = moving ? fabsf(sinf((float)GetTime() * 6.0f)) * 0.04f : 0.0f;

    rlPushMatrix();
    rlTranslatef(p.x, p.y + bob, p.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    for (int i = 0; i < 4; i++)
        DrawShadedCylinder((Vector3){ (i & 1) ? 0.15f : -0.15f, 0, (i & 2) ? 0.4f : -0.4f }, 0.055f, 0.055f, 0.62f, 0.7f, 5, coat);
    DrawShadedBlob((Vector3){ 0, 0.82f, 0 }, (Vector3){ 0.25f, 0.29f, 0.62f }, coat);
    Part(0, 1.05f, 0.6f, 0.2f, 0.45f, 0.22f, coat);
    DrawShadedBlob((Vector3){ 0, 1.2f, 0.84f }, (Vector3){ 0.11f, 0.12f, 0.23f }, coat);
    Part(-0.06f, 1.45f, 0.7f, 0.05f, 0.28f, 0.05f, dark);
    Part(0.06f, 1.45f, 0.7f, 0.05f, 0.28f, 0.05f, dark);
    Part(-0.33f, 0.9f, -0.05f, 0.22f, 0.42f, 0.6f, cloth);
    Part(0.33f, 0.9f, -0.05f, 0.22f, 0.42f, 0.6f, cloth);
    Part(0, 1.15f, -0.05f, 0.5f, 0.2f, 0.5f, COL_STRAW);
    rlPopMatrix();
}

static void DrawStall(const Caravan *c)
{
    Vector3 s = c->path[c->points - 1];
    float z = s.z + 1.0f;

    Part(s.x, 0.06f, z, 3.2f, 0.03f, 2.2f, c->cloth);
    for (int i = 0; i < 4; i++)
        Part(s.x + ((i & 1) ? 1.5f : -1.5f), 1.15f, z + ((i & 2) ? 1.0f : -1.0f), 0.08f, 2.3f, 0.08f, COL_WOOD);
    Part(s.x, 2.35f, z, 3.4f, 0.08f, 2.4f, Mix(c->cloth, COL_LINEN, 0.5f));
    if (c->trader == TR_DILMUN) {
        for (int k = 0; k < 4; k++) Part(s.x - 1.0f + 0.35f * (float)k, 0.12f, z + 0.6f, 0.25f, 0.08f, 0.5f, COL_STONE);
        for (int k = 0; k < 3; k++) Part(s.x + 0.4f + 0.35f * (float)k, 0.12f, z + 0.6f, 0.25f, 0.08f, 0.5f, COL_BRONZE);
    } else {
        for (int k = 0; k < 3; k++) Part(s.x, 0.18f + 0.2f * (float)k, z + 0.5f, 2.4f, 0.2f, 0.2f + 0.05f * (float)k, COL_PALM_TRUNK);
    }
}

static void DrawSpear(Vector3 g, float f, float swing, float windup)
{
    rlPushMatrix();
    rlTranslatef(g.x, g.y, g.z);
    rlRotatef(f * RAD2DEG, 0, 1, 0);
    if (swing > 0 || windup > 0) {
        float z = SpearThrust(windup, swing), y = windup > 0.1f ? 1.45f : 1.35f;
        GripHere(HAND_WEAPON, 0.3f, y, z - 0.2f);
        Part(0.3f, y, z, 0.05f, 0.05f, 2.4f, COL_WOOD);
        Part(0.3f, y, z + 1.3f, 0.08f, 0.08f, 0.26f, COL_BRONZE);
    } else {
        GripHere(HAND_WEAPON, 0.36f, 1.18f, 0.12f);
        Part(0.36f, 1.2f, 0.12f, 0.05f, 2.4f, 0.05f, COL_WOOD);
        Part(0.36f, 2.5f, 0.12f, 0.08f, 0.26f, 0.08f, COL_BRONZE);
    }
    rlPopMatrix();
}

void DrawCaravans(void)
{
    if (Match()) return;
    float time = (float)GetTime();

    for (int i = 0; i < MAX_CARAVANS; i++) {
        const Caravan *c = &G.caravans[i];
        if (c->state == CV_AWAY) continue;
        bool moving = c->state != CV_TRADE && !Halted(c);
        float f;
        Vector3 p;

        CaravanMember(i, 0, &p, &f);
        if (XZDist(p, G.cam.position) > 220.0f || !InView(p, 6.0f)) continue;
        LightFor(c->path[c->points - 1]);
        if (c->state == CV_TRADE) DrawStall(c);

        Look merchant = PersonLook(LOOK_MERCHANT, 700u + (unsigned)i, c->robe);
        LightFor(p);
        if (c->vit[0].dead) {
            DrawPersonBody(p, f, merchant);
        } else {
            float stride = c->vit[0].override ? time * 8.0f : moving ? c->s * 2.5f : 0.0f;
            if (TorchesLit()) GripAt(HAND_OTHER, -0.36f, 1.36f, 0.28f);
            DrawHuman(p, f, stride, merchant, false, false, 0);
            if (TorchesLit()) DrawHandTorch(p, f);
        }

        for (int k = 1; k <= 2; k++) {
            Vector3 d = Member(c, k, &f);
            LightFor(d);
            DrawDonkey(d, f, c->cloth, moving);
        }

        for (int k = 0; k < CARAVAN_GUARDS; k++) {
            const Vitals *v = &c->vit[1 + k];
            Vector3 g;
            CaravanMember(i, 3 + k, &g, &f);
            LightFor(g);
            Look guard = PersonLook(LOOK_SOLDIER, 720u + (unsigned)(i * 2 + k), k ? (Color){ 100, 86, 70, 255 } : (Color){ 120, 100, 80, 255 });
            if (v->dead) {
                DrawPersonBody(g, f, guard);
                continue;
            }
            float stride = v->override ? time * 8.0f : moving ? c->s * 2.5f + 1.0f + (float)k : 0.0f;
            DrawSpear(g, f, v->swing, v->windup);
            if (TorchesLit()) GripAt(HAND_OTHER, -0.36f, 1.36f, 0.28f);
            DrawHuman(g, f, stride, guard, false, false, 0);
            if (TorchesLit()) DrawHandTorch(g, f);
        }
    }
    G.drawTorch = G.drawFog = 0;
}
