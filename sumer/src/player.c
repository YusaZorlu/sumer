#include "game.h"
#include "rlgl.h"
#include <math.h>

#define WALK_SPEED    4.5f
#define RUN_SPEED     7.5f
#define CROUCH_SPEED  2.2f
#define HUNGRY_SPEED  3.4f
#define DRAW_TIME     0.85f
#define SWING_TIME    0.45f
#define SWING_COOL    1.0f
#define MACE_TIME     0.55f
#define MACE_COOL     1.4f
#define BLESS_BOOST   1.2f
#define PARRY_TIME    0.45f
#define PARRY_COOL    0.9f

static float Boost(void)
{
    return G.blessT > 0 ? BLESS_BOOST : 1.0f;
}

static void UpdateLook(Player *p, float dt, bool input)
{
    if (input && G.cursorLocked && PointerCaptured()) {
        Vector2 md = MouseLook();
        float sens = p->aiming ? 0.0018f : 0.003f;
        if (G.followArrow >= 0 && fabsf(md.x) + fabsf(md.y) > 6.0f) G.followArrow = -1;
        p->yaw -= md.x * sens;
        p->pitch += md.y * sens;
    }
    if (input) {
        if (IsKeyDown(KEY_LEFT)) p->yaw += 1.8f * dt;
        if (IsKeyDown(KEY_RIGHT)) p->yaw -= 1.8f * dt;
        if (IsKeyDown(KEY_UP)) p->pitch -= 1.2f * dt;
        if (IsKeyDown(KEY_DOWN)) p->pitch += 1.2f * dt;
        float wheel = GetMouseWheelMove();
        // Zooming in past the closest view enters first person; zooming out leaves it.
        if (G.firstPerson && wheel < 0) G.firstPerson = false;
        else if (!G.firstPerson && wheel > 0 && p->dist <= 3.0f) G.firstPerson = true;
        else if (!G.firstPerson) p->dist = Clamp(p->dist - wheel * 1.2f, 3.0f, 22.0f);
        if (KeyPressed(KEY_P)) G.firstPerson = !G.firstPerson;
    }
    p->pitch = Clamp(p->pitch, G.firstPerson ? -1.3f : -0.45f, 1.35f);
}

const char *EquipName(int eq)
{
    return eq == EQ_BOW ? L(BOW_NAMES[G.bow]) : eq == EQ_MELEE ? WeaponName(G.weapon) : eq == EQ_MACE ? MaceName() : L("Bare hands");
}

void SetEquip(int eq)
{
    if (G.equip == eq) return;
    if (eq == EQ_MACE && !G.hasMace) {
        Say("You have no war mace. The smith in the craft quarter makes one.");
        return;
    }
    G.equip = eq;
    G.player.aiming = false;
    G.player.draw = 0;
    G.swingT = 0;
    if (eq == EQ_NONE) Say("You put your weapons away.");
    else Say("%s in hand.", EquipName(eq));
}

static void HandleBow(Player *p, float dt, bool input)
{
    bool held = input && G.equip == EQ_BOW && G.cursorLocked && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    // The ignored click is over once the button is up and the frame that saw it press has passed: a
    // quick tap is already up in the frame that sees it, and must not strike then either.
    if (G.ignoreClick) {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !MousePressed(MOUSE_BUTTON_LEFT)) G.ignoreClick = false;
        held = false;
    }

    if (held) {
        if (!p->aiming && G.quiver == 0) {
            Say("Quiver empty. Press R to refill from your baskets, or restock at home.");
            G.ignoreClick = true;
            return;
        }
        p->aiming = true;
        p->draw = fminf(1.0f, p->draw + dt * Boost() / DRAW_TIME);
        return;
    }

    if (p->aiming && input && p->draw > 0.15f && G.quiver > 0) {
        Vector3 right = { cosf(p->yaw), 0, -sinf(p->yaw) };
        Vector3 fwd = { -sinf(p->yaw), 0, -cosf(p->yaw) };
        float chest = p->crouching ? 1.12f : 1.5f;
        Vector3 from = { p->pos.x, p->pos.y + chest, p->pos.z };
        from = Vector3Add(from, Vector3Add(Vector3Scale(right, -0.2f), Vector3Scale(fwd, 0.7f)));
        G.quiver--;
        int shot = ShootArrow(from, AimPoint(), p->draw);
        if (G.arrowCam && shot >= 0) {
            G.followArrow = shot;
            G.followT = 0;
        }
    }
    p->aiming = false;
    p->draw = 0;
}

static void MoveHorizontal(Player *p, Vector3 move, float speed, float dt)
{
    Vector2 next = { p->pos.x + move.x * speed * dt, p->pos.z + move.z * speed * dt };

    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < G.boxCount; i++)
            if (IsBlocking(&G.boxes[i], p->pos.y)) next = PushOut(next, PLAYER_RADIUS, &G.boxes[i]);
    Vector2 shoved = PushOutBodies(next, PLAYER_RADIUS, OWNER_PLAYER + G.self);
    if (shoved.x != next.x || shoved.y != next.y) {
        next = shoved;
        for (int i = 0; i < G.boxCount; i++)
            if (IsBlocking(&G.boxes[i], p->pos.y)) next = PushOut(next, PLAYER_RADIUS, &G.boxes[i]);
    }
    p->pos.x = Clamp(next.x, -WORLD_LIMIT, WORLD_LIMIT);
    p->pos.z = Clamp(next.y, -WORLD_LIMIT, WORLD_LIMIT);
}

static void MoveVertical(Player *p, float dt)
{
    float ground = GroundHeight(p->pos.x, p->pos.z, p->pos.y);

    // Glue to stairs on the way down instead of hopping off each step.
    if (p->grounded && p->vy <= 0 && p->pos.y - ground < 0.6f) {
        p->pos.y = ground;
        return;
    }

    p->vy -= GRAVITY * dt;
    p->pos.y += p->vy * dt;

    float ceiling = CeilingHeight(p->pos.x, p->pos.z, p->pos.y - p->vy * dt);
    if (p->vy > 0 && p->pos.y + PLAYER_HEIGHT > ceiling) {
        p->pos.y = ceiling - PLAYER_HEIGHT;
        p->vy = 0;
    }

    p->grounded = false;
    if (p->pos.y <= ground) {
        p->pos.y = ground;
        p->vy = 0;
        p->grounded = true;
    }
}

void StartClimb(int ladder, bool up)
{
    Player *p = &G.player;
    if (ladder < 0 || ladder >= G.ladderCount) return;
    G.climb = up ? 1 : -1;
    G.climbLadder = ladder;
    G.climbT = 0;
    p->crouching = p->aiming = false;
    p->draw = 0;
    p->vy = 0;
}

// Base on the ground, up the ladder to the wall's edge, then a step onto the walk.
static void UpdateClimb(Player *p, float dt)
{
    const Ladder *l = &G.ladders[G.climbLadder];
    Vector3 foot = { l->base.x, l->base.y, l->base.z };
    Vector3 edge = { l->base.x - l->n.x * 0.45f, l->top, l->base.z - l->n.z * 0.45f };
    Vector3 walk = { l->wall.x, l->top, l->wall.z };

    G.climbT = fminf(1.0f, G.climbT + dt / 2.6f);
    float k = G.climb > 0 ? G.climbT : 1.0f - G.climbT;
    if (k < 0.85f) {
        float t = k / 0.85f;
        p->pos = (Vector3){ Lerp(foot.x, edge.x, fminf(t * 4.0f, 1.0f)), Lerp(foot.y, edge.y - 0.3f, t), Lerp(foot.z, edge.z, fminf(t * 4.0f, 1.0f)) };
    } else {
        float t = (k - 0.85f) / 0.15f;
        p->pos = (Vector3){ Lerp(edge.x, walk.x, t), Lerp(edge.y - 0.3f, walk.y, fminf(t * 2.0f, 1.0f)), Lerp(edge.z, walk.z, t) };
    }
    p->facing = atan2f(-l->n.x, -l->n.z);
    p->stride += dt * 7.0f;
    p->moving = true;
    p->grounded = false;
    if (G.climbT >= 1.0f) {
        p->pos = G.climb > 0 ? walk : foot;
        p->pos.y = GroundHeight(p->pos.x, p->pos.z, p->pos.y + 0.3f);
        p->grounded = true;
        p->stride = 0;
        G.climb = 0;
    }
}

void UpdatePlayer(float dt, bool input)
{
    Player *p = &G.player;

    UpdateLook(p, dt, input);
    if (G.climb) {
        UpdateClimb(p, dt);
        return;
    }
    HandleBow(p, dt, input);
    if (G.swingT > 0) G.swingT -= dt;
    if (G.swingCool > 0) G.swingCool -= dt;
    if (G.parryT > 0) G.parryT -= dt;
    if (G.parryCool > 0) G.parryCool -= dt;
    bool strike = G.cursorLocked && MousePressed(MOUSE_BUTTON_LEFT) && !G.ignoreClick;
    bool parry = (G.cursorLocked && MousePressed(MOUSE_BUTTON_RIGHT)) || KeyPressed(KEY_Q);
    bool armed = G.equip == EQ_MELEE || G.equip == EQ_MACE, mace = G.equip == EQ_MACE;
    if (input && armed && G.parryCool <= 0 && G.parryT <= 0 && G.swingT <= 0 && parry) {
        G.parryT = PARRY_TIME;
        G.parryCool = PARRY_COOL;
        p->facing = p->yaw + PI;
    }
    if (input && armed && G.swingCool <= 0 && G.parryT <= 0 && (strike || KeyPressed(KEY_V))) {
        G.swingT = mace ? MACE_TIME : SWING_TIME;
        G.swingCool = (mace ? MACE_COOL : SWING_COOL) / Boost();
        MeleeStrike();
    }

    Vector3 move = { 0 };
    if (input) {
        Vector3 fwd = { -sinf(p->yaw), 0, -cosf(p->yaw) };
        Vector3 right = { cosf(p->yaw), 0, -sinf(p->yaw) };
        if (IsKeyDown(KEY_W)) move = Vector3Add(move, fwd);
        if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, fwd);
        if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
        if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);

        if (KeyPressed(KEY_C)) p->crouching = !p->crouching;
        if (KeyPressed(KEY_SPACE)) {
            if (p->crouching) p->crouching = false;
            else if (p->grounded) {
                p->vy = JUMP_SPEED;
                p->grounded = false;
            }
        }
    }

    p->moving = Vector3Length(move) > 0.01f;
    if (p->moving || p->aiming) G.followArrow = -1;
    bool hungry = G.food <= 0.0f;
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    p->running = p->moving && input && shift && !p->crouching && !p->aiming && !hungry;

    float speed = hungry ? HUNGRY_SPEED : WALK_SPEED;
    if (p->crouching) speed = CROUCH_SPEED;
    else if (p->running) speed = RUN_SPEED * Boost();
    if (p->aiming) speed = fminf(speed, p->crouching ? 1.6f : 2.4f);

    if (p->moving) {
        move = Vector3Normalize(move);
        MoveHorizontal(p, move, speed, dt);
        if (p->grounded) p->stride += dt * speed * 2.0f;
    } else {
        p->stride = 0;
    }

    float target = p->facing;
    if (p->aiming || G.firstPerson || G.parryT > 0) target = p->yaw + PI;
    else if (p->moving) target = atan2f(move.x, move.z);
    float diff = atan2f(sinf(target - p->facing), cosf(target - p->facing));
    p->facing += diff * fminf(1.0f, dt * 12.0f);

    MoveVertical(p, dt);
}

// Rides behind the last arrow, holds briefly where it lands, then blends back.
static void ArrowCamera(float dt)
{
    float want = 0.0f;

    if (G.followArrow >= 0) {
        const Arrow *ar = &G.arrows[G.followArrow];
        if (!ar->flying) G.followT += dt;
        if (!ar->active || G.followT > 0.9f) {
            G.followArrow = -1;
        } else {
            want = 1.0f;
            G.followPos = ar->pos;
            if (ar->flying && Vector3Length(ar->vel) > 0.1f) G.followDir = Vector3Normalize(ar->vel);
        }
    }
    G.camBlend += (want - G.camBlend) * fminf(1.0f, dt * (want > 0 ? 6.0f : 3.0f));
    if (G.camBlend < 0.01f) return;

    Vector3 flat = Vector3Normalize((Vector3){ G.followDir.x, 0, G.followDir.z });
    Vector3 eye = Vector3Add(Vector3Subtract(G.followPos, Vector3Scale(flat, 4.0f)), (Vector3){ 0, 1.3f, 0 });
    Vector3 look = Vector3Add(G.followPos, Vector3Scale(G.followDir, 2.0f));
    G.cam.position = Vector3Lerp(G.cam.position, eye, G.camBlend);
    G.cam.target = Vector3Lerp(G.cam.target, look, G.camBlend);
}

// Over-the-shoulder while drawing the bow; pulled in front of walls.
void FollowCamera(float dt)
{
    const Player *p = &G.player;
    float aim = p->aiming ? 1.0f : 0.0f;
    G.camShoulder += (aim - G.camShoulder) * fminf(1.0f, dt * 8.0f);

    // Over the shoulder in tight rooms.
    float tight = Clamp((2.4f - G.camDist) / 1.2f, 0.0f, 1.0f);
    Vector3 right = { cosf(p->yaw), 0, -sinf(p->yaw) };
    float eye = p->crouching ? 1.05f : 1.5f;
    Vector3 target = { p->pos.x, p->pos.y + eye + 0.2f * tight, p->pos.z };
    target = Vector3Add(target, Vector3Scale(right, fmaxf(0.75f * G.camShoulder, 0.6f * tight)));

    float cp = cosf(p->pitch);
    Vector3 dir = { cp * sinf(p->yaw), sinf(p->pitch), cp * cosf(p->yaw) };
    if (G.firstPerson) {
        Vector3 eyePos = { p->pos.x - sinf(p->yaw) * 0.15f, p->pos.y + (p->crouching ? 1.2f : 1.68f), p->pos.z - cosf(p->yaw) * 0.15f };
        G.camDist = 0;
        G.camShoulder = 0;
        G.cam.position = eyePos;
        G.cam.target = Vector3Subtract(eyePos, dir);
        ArrowCamera(dt);
        G.cam.fovy = p->aiming ? 52.0f : 70.0f;
        return;
    }
    Ray ray = { target, dir };
    float dist = Lerp(p->dist, 3.0f, G.camShoulder);

    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (b->size.y < 1.0f || b->water) continue;
        float reach = dist + (b->size.x + b->size.z) * 0.5f;
        if (fabsf(b->pos.x - target.x) > reach || fabsf(b->pos.z - target.z) > reach) continue;
        BoundingBox bb = { Vector3Subtract(b->pos, Vector3Scale(b->size, 0.5f)),
                           Vector3Add(b->pos, Vector3Scale(b->size, 0.5f)) };
        RayCollision hit = GetRayCollisionBox(ray, bb);
        if (hit.hit && hit.distance > 0 && hit.distance < dist) dist = hit.distance;
    }
    dist = fminf(dist, RayTerrain(target, dir, dist + 0.5f) - 0.2f);
    dist = fmaxf(dist - 0.3f, 0.8f);

    if (dist < G.camDist) G.camDist = dist;
    else G.camDist += (dist - G.camDist) * fminf(1.0f, dt * 4.0f);

    G.cam.target = target;
    G.cam.position = Vector3Add(target, Vector3Scale(dir, G.camDist));
    ArrowCamera(dt);
    G.cam.position.y = fmaxf(G.cam.position.y, fmaxf(TerrainHeight(G.cam.position.x, G.cam.position.z), WATER_LEVEL) + 0.3f);
    G.cam.fovy = Lerp(55.0f, 42.0f, G.camShoulder);
}

static void WPart(float x, float y, float z, float w, float h, float d, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, d }, c);
}

// Raised during the first part of a swing, struck down at the end.
static void DrawWeapon(const Player *p)
{
    float k = Clamp(G.swingT / SWING_TIME, 0.0f, 1.0f);
    float angle = (G.swingT > 0) ? (k > 0.5f ? -80.0f : -80.0f + 140.0f * (1.0f - k / 0.5f)) : 25.0f;
    bool guard = G.parryT > 0;
    float shoulder = p->crouching ? 1.04f : 1.42f;

    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y + shoulder, p->pos.z);
    rlRotatef(p->facing * RAD2DEG, 0, 1, 0);
    if (guard) {
        rlTranslatef(0.3f, 0.12f, 0.38f);
        rlRotatef(-75.0f, 0, 1, 0);
        rlRotatef(-25.0f, 1, 0, 0);
    } else {
        rlTranslatef(0.32f, 0, 0.1f);
        rlRotatef(angle, 1, 0, 0);
    }
    switch (G.weapon) {
    case 0:
        WPart(0, 0, 0.35f, 0.06f, 0.06f, 0.7f, COL_WOOD);
        WPart(0, 0, 0.72f, 0.13f, 0.13f, 0.22f, COL_PALM_TRUNK);
        break;
    case 1:
        WPart(0, 0, 0.35f, 0.05f, 0.05f, 0.7f, COL_WOOD);
        WPart(0, 0, 0.75f, 0.16f, 0.16f, 0.18f, (Color){ 184, 115, 70, 255 });
        break;
    default:
        WPart(0, 0, 0.12f, 0.05f, 0.05f, 0.25f, COL_WOOD);
        WPart(0, 0, 0.5f, 0.03f, 0.08f, 0.6f, COL_BRONZE);
        WPart(0, -0.1f, 0.84f, 0.03f, 0.22f, 0.12f, COL_BRONZE);
        break;
    }
    rlPopMatrix();
}

// Rests on the shoulder; a strike sweeps it flat across the front from one side to the other.
static void DrawMace(const Player *p)
{
    float k = Clamp(G.swingT / MACE_TIME, 0.0f, 1.0f);
    float shoulder = p->crouching ? 1.04f : 1.42f;

    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y + shoulder, p->pos.z);
    rlRotatef(p->facing * RAD2DEG, 0, 1, 0);
    if (G.parryT > 0) {
        rlTranslatef(0.3f, 0.12f, 0.38f);
        rlRotatef(-75.0f, 0, 1, 0);
        rlRotatef(-25.0f, 1, 0, 0);
    } else if (G.swingT > 0) {
        rlTranslatef(0, -0.15f, 0.15f);
        rlRotatef(Lerp(-80.0f, 80.0f, k), 0, 1, 0);
        rlRotatef(-6.0f, 1, 0, 0);
    } else {
        rlTranslatef(0.32f, 0, 0.1f);
        rlRotatef(-35.0f, 1, 0, 0);
    }
    WPart(0, 0, 0.45f, 0.06f, 0.06f, 0.95f, COL_WOOD);
    DrawShadedBlob((Vector3){ 0, 0, 0.98f }, (Vector3){ 0.17f, 0.17f, 0.2f }, (Color){ 120, 112, 100, 255 });
    WPart(0, 0, 0.98f, 0.4f, 0.05f, 0.05f, COL_BRONZE);
    WPart(0, 0, 0.98f, 0.05f, 0.4f, 0.05f, COL_BRONZE);
    rlPopMatrix();
}

static void DrawHeldBow(const Player *p)
{
    float shoulder = p->crouching ? 0.92f : 1.3f;
    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y + shoulder, p->pos.z);
    rlRotatef(p->facing * RAD2DEG, 0, 1, 0);
    rlTranslatef(-0.4f, 0, 0.12f);
    rlRotatef(-12.0f, 1, 0, 0);
    WPart(0, 0, 0, 0.05f, 0.9f, 0.06f, COL_WOOD);
    WPart(0, 0.5f, 0.05f, 0.04f, 0.16f, 0.05f, COL_WOOD);
    WPart(0, -0.5f, 0.05f, 0.04f, 0.16f, 0.05f, COL_WOOD);
    WPart(0, 0, -0.07f, 0.015f, 1.05f, 0.015f, COL_LINEN);
    rlPopMatrix();
}

static void DrawArmor(const Player *p)
{
    float drop = p->crouching ? 0.38f : 0.0f;
    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y - drop, p->pos.z);
    rlRotatef(p->facing * RAD2DEG, 0, 1, 0);
    if (G.armor == ARMOR_WOLF) {
        const Color fur = { 120, 114, 104, 255 }, dark = { 80, 76, 70, 255 };
        WPart(0, 1.2f, -0.19f, 0.56f, 0.9f, 0.08f, fur);
        WPart(0, 1.62f, -0.05f, 0.58f, 0.14f, 0.36f, fur);
        WPart(0, 1.98f, -0.04f, 0.36f, 0.14f, 0.36f, dark);
        WPart(-0.1f, 2.07f, 0.05f, 0.07f, 0.1f, 0.06f, dark);
        WPart(0.1f, 2.07f, 0.05f, 0.07f, 0.1f, 0.06f, dark);
    } else if (G.armor == ARMOR_LEATHER) {
        const Color hide = { 128, 88, 56, 255 }, strap = { 84, 56, 36, 255 };
        WPart(0, 1.36f, 0, 0.5f, 0.44f, 0.3f, hide);
        WPart(-0.29f, 1.52f, 0, 0.16f, 0.14f, 0.2f, hide);
        WPart(0.29f, 1.52f, 0, 0.16f, 0.14f, 0.2f, hide);
        WPart(0, 1.16f, 0, 0.52f, 0.06f, 0.32f, strap);
    }
    rlPopMatrix();
}

void DrawPlayer(void)
{
    const Player *p = &G.player;
    Color own = PlayerColor(G.self);
    Look look = { COL_SKIN, own, Mix(own, COL_LINEN, 0.2f), COL_HAIR };

    if (G.camDist < 1.3f) return;
    LightFor(p->pos);
    if (G.waitBed) {
        DrawInBed(G.self, p->pos, p->facing, look);
        G.drawTorch = G.drawFog = 0;
        return;
    }
    DrawHuman(p->pos, p->facing, p->stride, look, p->crouching, p->aiming, p->draw);
    DrawArmor(p);
    if (G.climb == 0 && G.equip == EQ_MELEE) DrawWeapon(p);
    if (G.climb == 0 && G.equip == EQ_MACE) DrawMace(p);
    if (G.climb == 0 && G.equip == EQ_BOW && !p->aiming) DrawHeldBow(p);
    G.drawTorch = G.drawFog = 0;
}
