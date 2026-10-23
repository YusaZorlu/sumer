#include "game.h"
#include "rlgl.h"
#include <math.h>

#define WALK_SPEED    4.5f
#define RUN_SPEED     7.5f
#define CROUCH_SPEED  2.2f
#define HUNGRY_SPEED  3.4f
#define DRAW_TIME     0.85f
#define SWING_COOL    1.0f
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
    // A finger dragged over the screen turns the view (by the UI's units it moved).
    Vector2 td = TouchLook();
    if (input && (td.x != 0 || td.y != 0)) {
        float sens = p->aiming ? 0.003f : 0.006f;
        if (G.followArrow >= 0 && fabsf(td.x) + fabsf(td.y) > 6.0f) G.followArrow = -1;
        p->yaw -= td.x * sens;
        p->pitch += td.y * sens;
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
        if (KeyPressed(KEY_P) || TouchPressed(TB_VIEW)) {
            G.firstPerson = !G.firstPerson;
            SaveGamePrefs();
        }
    }
    p->pitch = Clamp(p->pitch, G.firstPerson ? -1.3f : -0.45f, 1.35f);
}

const char *EquipName(int eq)
{
    return eq == EQ_BOW ? L(BOW_NAMES[UseBow()]) : eq == EQ_MELEE ? WeaponName(UseWeapon()) : eq == EQ_MACE ? MaceName() : L("Bare hands");
}

#define TORCH_TIME 1.0f

// From the evening on: by day the torch stays unlit at the belt.
bool TorchBurning(void)
{
    return G.torchHeld && TorchesLit();
}

// A torch is held in the other hand with bare hands, the sword or the mace; drawing the bow needs both hands,
// so it is dropped from the hand the moment the bow is drawn, and the draw starts at once. The other changes
// take a second: lighting it, putting it away, taking it up again after a shot or a change of weapon.
static void UpdateTorch(float dt, bool input)
{
    const Player *p = &G.player;
    if (input && KeyPressed(KEY_B)) {
        G.torchOn = !G.torchOn;
        Say(G.torchOn ? "You take up your torch (B). It burns from the evening on." : "You put your torch away (B).");
    }
    bool aimWanted = G.equip == EQ_BOW && input && ((G.cursorLocked && MouseHeld(MOUSE_BUTTON_LEFT)) || TouchDown(TB_ATTACK));
    bool bow = G.equip == EQ_BOW && (p->aiming || aimWanted);
    bool goal = G.torchOn && G.climb == 0 && !G.sleeping && !bow;
    if (bow) G.torchHeld = false;
    if (goal == G.torchHeld) {
        G.torchT = 0;
        return;
    }
    G.torchT += dt;
    if (G.torchT >= TORCH_TIME) {
        G.torchHeld = goal;
        G.torchT = 0;
    }
}

void SetEquip(int eq)
{
    if (G.equip == eq) return;
    if (eq != EQ_NONE && InAlehouse(G.player.pos)) {
        Say("No weapons in the alehouse.");
        return;
    }
    if (eq == EQ_MACE && !UseMace()) {
        Say("You have no war mace. The smith in the craft quarter makes one.");
        return;
    }
    G.equip = eq;
    // The hands are busy changing weapons: the torch comes back a second later.
    G.torchHeld = false;
    G.torchT = 0;
    G.player.aiming = false;
    G.player.draw = 0;
    G.swingT = 0;
    G.strikePending = false;
    if (eq == EQ_NONE) Say("You put your weapons away.");
    else Say("%s in hand.", EquipName(eq));
}

static void HandleBow(Player *p, float dt, bool input)
{
    bool held = input && G.equip == EQ_BOW && G.cursorLocked && MouseHeld(MOUSE_BUTTON_LEFT);

    // The ignored click is over once the button is up and the frame that saw it press has passed: a
    // quick tap is already up in the frame that sees it, and must not strike then either.
    if (G.ignoreClick) {
        if (!MouseHeld(MOUSE_BUTTON_LEFT) && !MousePressed(MOUSE_BUTTON_LEFT)) G.ignoreClick = false;
        held = false;
    }
    if (input && G.equip == EQ_BOW && TouchDown(TB_ATTACK)) held = true;

    if (held) {
        if (!p->aiming && G.quiver == 0 && !G.combat) {
            Say("Quiver empty. Press R to refill from your baskets, or restock at home.");
            G.ignoreClick = true;
            return;
        }
        p->aiming = true;
        p->draw = fminf(1.0f, p->draw + dt * Boost() * RoyaleAttack() / DRAW_TIME);
        return;
    }

    if (p->aiming && input && p->draw > 0.15f && (G.quiver > 0 || G.combat)) {
        Vector3 right = { cosf(p->yaw), 0, -sinf(p->yaw) };
        Vector3 fwd = { -sinf(p->yaw), 0, -cosf(p->yaw) };
        float chest = p->crouching ? 1.12f : 1.5f;
        Vector3 from = { p->pos.x, p->pos.y + chest, p->pos.z };
        from = Vector3Add(from, Vector3Add(Vector3Scale(right, -0.2f), Vector3Scale(fwd, 0.7f)));
        if (!G.combat) G.quiver--;
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
    float reach = PLAYER_RADIUS + speed * dt + 1.0f;
    int n;
    const int *near = BoxesNear(p->pos.x - reach, p->pos.z - reach, p->pos.x + reach, p->pos.z + reach, &n);

    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < n; k++)
            if (!G.boxes[near[k]].water && IsBlocking(&G.boxes[near[k]], p->pos.y)) next = PushOut(next, PLAYER_RADIUS, &G.boxes[near[k]]);
    Vector2 shoved = PushOutBodies(next, PLAYER_RADIUS, OWNER_PLAYER + G.self);
    if (shoved.x != next.x || shoved.y != next.y) {
        next = shoved;
        for (int k = 0; k < n; k++)
            if (!G.boxes[near[k]].water && IsBlocking(&G.boxes[near[k]], p->pos.y)) next = PushOut(next, PLAYER_RADIUS, &G.boxes[near[k]]);
    }
    p->pos.x = Clamp(next.x, -WORLD_LIMIT, WORLD_LIMIT);
    p->pos.z = Clamp(next.y, -WORLD_LIMIT, WORLD_LIMIT);
}

static void MoveVertical(Player *p, float dt)
{
    float ground = GroundHeight(p->pos.x, p->pos.z, p->pos.y);
    // In deep water the body floats with its head out, treading water over the river bed.
    if (InWater(p->pos.x, p->pos.z)) ground = fmaxf(ground, WATER_LEVEL - 1.25f);

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

// Weapons are put away inside the alehouse and taken up again on the way out.
static void AlehouseRule(void)
{
    static int before = -1;
    bool in = InAlehouse(G.player.pos);
    if (in && before < 0) {
        before = G.equip;
        if (G.equip != EQ_NONE) {
            SetEquip(EQ_NONE);
            Say("You put your weapons away in the alehouse.");
        }
    } else if (!in && before >= 0) {
        int was = before;
        before = -1;
        if (was != EQ_NONE && G.equip == EQ_NONE) SetEquip(was);
    }
}

void UpdatePlayer(float dt, bool input)
{
    Player *p = &G.player;

    UpdateLook(p, dt, input);
    AlehouseRule();
    UpdateTorch(dt, input);
    if (G.climb) {
        UpdateClimb(p, dt);
        return;
    }
    HandleBow(p, dt, input);
    // A landed blow holds the weapon where it struck for a moment (G.hitStop).
    if (G.swingT > 0 && G.hitStop <= 0) G.swingT -= dt;
    if (G.swingCool > 0) G.swingCool -= dt;
    if (G.parryT > 0) G.parryT -= dt;
    if (G.parryCool > 0) G.parryCool -= dt;
    // The mouse fights only once the game has it (a tap on a touch screen is a click too).
    bool mouse = G.cursorLocked && PointerCaptured();
    bool strike = (mouse && MousePressed(MOUSE_BUTTON_LEFT) && !G.ignoreClick) || TouchPressed(TB_ATTACK);
    bool parry = (mouse && MousePressed(MOUSE_BUTTON_RIGHT)) || KeyPressed(KEY_Q) || TouchPressed(TB_PARRY);
    bool armed = G.equip == EQ_MELEE || G.equip == EQ_MACE, mace = G.equip == EQ_MACE;
    // The war mace is too heavy to parry with; only the melee weapon turns blows aside.
    if (input && mace && parry) Say("The war mace is too heavy to parry with. Parry with your melee weapon (2).");
    if (input && G.equip == EQ_MELEE && G.parryCool <= 0 && G.parryT <= 0 && G.swingT <= 0 && parry) {
        G.parryT = PARRY_TIME;
        G.parryCool = PARRY_COOL;
        p->facing = p->yaw + PI;
    }
    if (input && armed && G.swingCool <= 0 && G.parryT <= 0 && (strike || KeyPressed(KEY_V))) {
        G.swingT = mace ? MACE_TIME : SWING_TIME;
        G.swingCool = (mace ? MACE_COOL : SWING_COOL) / (Boost() * RoyaleAttack());
        G.strikePending = true;
        MeleeStrike(STRIKE_START);
    }
    // The blow lands when the weapon comes down on its target, not when the button is pressed.
    if (G.strikePending && (!armed || G.swingT <= 0 || SwingElapsed(G.swingT, mace) >= CONTACT_TIME)) {
        G.strikePending = false;
        if (armed) MeleeStrike(STRIKE_CONTACT);
    }

    Vector3 move = { 0 };
    if (input) {
        Vector3 fwd = { -sinf(p->yaw), 0, -cosf(p->yaw) };
        Vector3 right = { cosf(p->yaw), 0, -sinf(p->yaw) };
        if (IsKeyDown(KEY_W)) move = Vector3Add(move, fwd);
        if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, fwd);
        if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
        if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
        Vector2 st = TouchStick();
        move = Vector3Add(move, Vector3Add(Vector3Scale(fwd, st.y), Vector3Scale(right, st.x)));

        if (KeyPressed(KEY_C) || TouchPressed(TB_CROUCH)) p->crouching = !p->crouching;
        if (KeyPressed(KEY_SPACE) && !p->aiming) {
            if (p->crouching) p->crouching = false;
            else if (p->grounded) {
                p->vy = JUMP_SPEED;
                p->grounded = false;
            }
        }
    }

    // A drawn bow holds the archer where they stand.
    if (p->aiming) move = (Vector3){ 0 };
    p->moving = Vector3Length(move) > 0.01f;
    if (p->moving || p->aiming) G.followArrow = -1;
    bool hungry = G.food <= 0.0f;
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || TouchRun();
    p->running = p->moving && input && shift && !p->crouching && !p->aiming && !hungry;

    float speed = hungry ? HUNGRY_SPEED : WALK_SPEED;
    if (p->crouching) speed = CROUCH_SPEED;
    else if (p->running) speed = RUN_SPEED * Boost() * RoyaleRun();
    if (G.devSpeed > 0) speed *= G.devSpeed;

    // Water is waded through, slowly.
    if (p->pos.y < WATER_LEVEL + 0.2f) speed *= WadeSpeed(p->pos.x, p->pos.z);   // on a bridge or quay you walk
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
static void PlaceCamera(float dt)
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

    int n;
    Vector3 far = Vector3Add(target, Vector3Scale(dir, dist));
    const int *near = BoxesNear(target.x - 1, target.z - 1, far.x + 1, far.z + 1, &n);
    for (int k = 0; k < n; k++) {
        const Box *b = &G.boxes[near[k]];
        if (b->water || (b->size.y < 1.0f && b->pos.y < target.y + 0.5f)) continue;   // thin things count only as roofs overhead
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

// A blow on the player jolts the view for a moment.
// Choosing a look: the camera turns to face the player, who stands on the right of the screen beside the
// choices; the arrow keys walk the camera round them.
static void LookCamera(float dt)
{
    static float turn;
    const Player *p = &G.player;
    if (IsKeyDown(KEY_LEFT)) turn -= dt * 1.6f;
    if (IsKeyDown(KEY_RIGHT)) turn += dt * 1.6f;
    float a = p->facing + turn;
    Vector3 chest = { p->pos.x, p->pos.y + (p->crouching ? 0.75f : 1.15f), p->pos.z };
    G.cam.position = (Vector3){ chest.x + sinf(a) * 2.7f, chest.y + 0.35f, chest.z + cosf(a) * 2.7f };
    Vector3 d = Vector3Normalize(Vector3Subtract(chest, G.cam.position));
    Vector3 left = Vector3Normalize(Vector3CrossProduct((Vector3){ 0, 1, 0 }, d));
    G.cam.target = Vector3Add(chest, Vector3Scale(left, 0.75f));
    G.cam.fovy = 55.0f;
}

void FollowCamera(float dt)
{
    if (G.panel == PANEL_LOOK) {
        LookCamera(dt);
        return;
    }
    // Out of a battle royale round: the camera follows a player still in it.
    if (RoyaleOut()) {
        UpdateLook(&G.player, dt, G.panel == PANEL_NONE);
        if (RoyaleCamera()) return;
    }
    PlaceCamera(dt);
    // A landed blow kicks the view, more sharply for the mace.
    if (G.shakeT > 0) {
        float k = G.shakeT * G.shakeAmp / 0.2f, t = (float)GetTime();
        Vector3 kick = { sinf(t * 71.0f) * k, sinf(t * 89.0f + 1.0f) * k * 0.7f, sinf(t * 79.0f + 2.0f) * k };
        G.cam.position = Vector3Add(G.cam.position, kick);
        G.cam.target = Vector3Add(G.cam.target, Vector3Scale(kick, 0.5f));
    }
    if (G.flinchT > 0) {
        float k = G.flinchT / FLINCH_TIME, t = (float)GetTime();
        Vector3 jolt = { sinf(t * 47.0f) * 0.09f * k, sinf(t * 61.0f + 1.0f) * 0.07f * k, sinf(t * 53.0f + 2.0f) * 0.09f * k };
        G.cam.position = Vector3Add(G.cam.position, jolt);
        G.cam.target = Vector3Add(G.cam.target, jolt);
    }
}

static void WPart(float x, float y, float z, float w, float h, float d, Color c)
{
    DrawShadedBox((Vector3){ x, y, z }, (Vector3){ w, h, d }, c);
}

// Seconds since a swing began, from what is left of it; -1 when not swinging.
float SwingElapsed(float swingT, bool mace)
{
    return swingT > 0 ? (mace ? MACE_TIME : SWING_TIME) - swingT : -1.0f;
}

static float EaseOut(float u)
{
    return 1.0f - (1.0f - u) * (1.0f - u);
}

// The sword or club over a swing, by the time since it began (t < 0: at rest): raised quickly, cocked
// a moment, then struck down fast to meet the target at CONTACT_TIME, and followed through.
float WeaponAngle(float t)
{
    const float rest = 25.0f, top = -100.0f, cocked = -108.0f, hit = 20.0f;
    if (t < 0.0f || t >= SWING_TIME) return rest;
    if (t < 0.12f) return Lerp(rest, top, EaseOut(t / 0.12f));
    if (t < 0.22f) return Lerp(top, cocked, (t - 0.12f) / 0.10f);
    if (t < CONTACT_TIME) {
        float u = (t - 0.22f) / (CONTACT_TIME - 0.22f);
        return Lerp(cocked, hit, u * u);
    }
    float u = (t - CONTACT_TIME) / (SWING_TIME - CONTACT_TIME);
    return u < 0.35f ? Lerp(hit, 45.0f, u / 0.35f) : Lerp(45.0f, rest, (u - 0.35f) / 0.65f);
}

// The war mace's sweep, turned about the body (degrees, left is negative): drawn back to the right,
// then swept hard across the front, crossing straight ahead at CONTACT_TIME, and carried through.
float MaceYaw(float t)
{
    if (t < 0.18f) return Lerp(50.0f, 95.0f, EaseOut(Clamp(t / 0.18f, 0.0f, 1.0f)));
    if (t < 0.38f) return 95.0f - 165.0f * powf((t - 0.18f) / 0.20f, 1.2f);
    return Lerp(-70.0f, -82.0f, Clamp((t - 0.38f) / (MACE_TIME - 0.38f), 0.0f, 1.0f));
}

// A person's blow, from its windup (counting down to the moment it lands) and then its swing (counting
// down the follow-through): raised through the windup, struck down in its last moment so the weapon
// meets the target as the blow lands, and carried through back to 'rest'.
float BlowAngle(float windup, float swing, float rest)
{
    const float down = 0.12f, top = -120.0f, hit = 25.0f;
    if (windup > down) return Lerp(rest, top, EaseOut(Clamp((NPC_WINDUP - windup) / (NPC_WINDUP - down - 0.1f), 0.0f, 1.0f)));
    if (windup > 0.0f) {
        float u = 1.0f - windup / down;
        return Lerp(top, hit, u * u);
    }
    if (swing > 0.0f) {
        float k = Clamp(swing / NPC_SWING, 0.0f, 1.0f);
        return Lerp(rest, hit + 15.0f * k, k);
    }
    return rest;
}

// A spear's reach forward (where its shaft's middle is): drawn back through the windup, thrust out in
// its last moment to strike as the blow lands, then drawn back in.
float SpearThrust(float windup, float swing)
{
    const float back = 0.35f, out = 1.0f, thrust = 0.1f;
    if (windup > thrust) return back;
    if (windup > 0.0f) return Lerp(back, out, 1.0f - windup / thrust);
    return Lerp(0.4f, out, Clamp(swing / NPC_SWING, 0.0f, 1.0f));
}

// How far the body leans into a swing (negative: drawn back while the weapon is raised).
float SwingLean(float t, bool mace)
{
    if (t < 0.0f) return 0.0f;
    float d = (t - CONTACT_TIME) / (mace ? 0.12f : 0.09f);
    return expf(-d * d) - (t < CONTACT_TIME ? 0.35f * Clamp(t / 0.15f, 0.0f, 1.0f) : 0.0f);
}

// The sword or club arm over a swing (degrees below straight ahead; how far the hand is from the
// shoulder): raised up and back with the weapon, then brought down and out to meet the target at
// CONTACT_TIME, and back to the hip.
static void SwordArm(float t, float *arm, float *reach)
{
    const float rest = 62.0f, top = -122.0f, cocked = -130.0f, hit = 6.0f, follow = 40.0f;
    if (t < 0.0f || t >= SWING_TIME) {
        *arm = rest;
        *reach = 0.44f;
    } else if (t < 0.12f) {
        float u = EaseOut(t / 0.12f);
        *arm = Lerp(rest, top, u);
        *reach = Lerp(0.44f, 0.5f, u);
    } else if (t < 0.22f) {
        *arm = Lerp(top, cocked, (t - 0.12f) / 0.10f);
        *reach = 0.5f;
    } else if (t < CONTACT_TIME) {
        float u = (t - 0.22f) / (CONTACT_TIME - 0.22f);
        *arm = Lerp(cocked, hit, u * u);
        *reach = Lerp(0.5f, 0.56f, u);
    } else {
        float u = (t - CONTACT_TIME) / (SWING_TIME - CONTACT_TIME);
        *arm = u < 0.35f ? Lerp(hit, follow, u / 0.35f) : Lerp(follow, rest, (u - 0.35f) / 0.65f);
        *reach = Lerp(0.56f, 0.44f, u);
    }
}

// A weapon in the hand, seen from shoulder height (facing +z): the hand 'reach' out from the shoulder
// along 'arm' (degrees below straight ahead), the weapon pointing along 'blade' the same way.
Matrix ArmFrame(float arm, float reach, float blade)
{
    float a = arm * DEG2RAD;
    return MatrixMultiply(MatrixRotateX(blade * DEG2RAD), MatrixTranslate(0.215f, 0.025f - reach * sinf(a), reach * cosf(a)));
}

// The sword or club 't' seconds into a swing (t < 0: at rest).
Matrix SwordFrame(float t)
{
    float arm, reach;
    SwordArm(t, &arm, &reach);
    return ArmFrame(arm, reach, WeaponAngle(t));
}

// A tool or weapon held by someone else at 'blade' (see BlowAngle): the arm follows it up and down.
Matrix BlowFrame(float blade)
{
    float arm = blade >= 0.0f ? 12.0f + blade * 0.9f : blade * 1.1f;
    return ArmFrame(arm, blade >= 0.0f ? 0.5f : 0.47f, blade);
}

// Held up across the body to turn a blow aside.
static Matrix GuardFrame(void)
{
    return MatrixMultiply(MatrixMultiply(MatrixRotateX(-25.0f * DEG2RAD), MatrixRotateY(-75.0f * DEG2RAD)), MatrixTranslate(0.3f, 0.12f, 0.38f));
}

// Resting in the hand at the hip, its head up over the shoulder behind.
static Matrix MaceRestFrame(void)
{
    return ArmFrame(70.0f, 0.42f, -125.0f);
}

// Swept across the front in both hands, turned about the chest.
static Matrix MaceSweepFrame(float t)
{
    return MatrixMultiply(MatrixMultiply(MatrixRotateX(-6.0f * DEG2RAD), MatrixRotateY(MaceYaw(t) * DEG2RAD)), MatrixTranslate(0, -0.15f, 0.15f));
}

// Where a held weapon is: the hands take hold of it there (GripHere), so draw it before the body.
void HoldWeapon(Vector3 pos, float facing, bool crouching, bool mace, float t, bool guard)
{
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y + (crouching ? 1.04f : 1.42f), pos.z);
    rlRotatef(facing * RAD2DEG, 0, 1, 0);
    Matrix m = guard ? GuardFrame() : !mace ? SwordFrame(t) : t >= 0.0f ? MaceSweepFrame(t) : MaceRestFrame();
    rlMultMatrixf(MatrixToFloat(m));
    GripHere(HAND_WEAPON, 0, 0, mace && t >= 0.0f && !guard ? 0.32f : 0.0f);
    if (mace && (t >= 0.0f || guard)) GripHere(HAND_OTHER, 0, 0, guard ? 0.3f : 0.17f);
}

// The weapon itself, where HoldWeapon put it; this ends what HoldWeapon began.
void DrawWeaponPart(int kind)
{
    if (kind == WEAPON_MACE) {
        WPart(0, 0, 0.45f, 0.06f, 0.06f, 0.95f, COL_WOOD);
        DrawShadedBlob((Vector3){ 0, 0, 0.98f }, (Vector3){ 0.17f, 0.17f, 0.2f }, (Color){ 120, 112, 100, 255 });
        WPart(0, 0, 0.98f, 0.4f, 0.05f, 0.05f, COL_BRONZE);
        WPart(0, 0, 0.98f, 0.05f, 0.4f, 0.05f, COL_BRONZE);
    } else if (kind >= WEAPON_SWORD) {
        WPart(0, 0, 0.12f, 0.05f, 0.05f, 0.25f, COL_WOOD);
        WPart(0, 0, 0.5f, 0.03f, 0.08f, 0.6f, COL_BRONZE);
        WPart(0, -0.1f, 0.84f, 0.03f, 0.22f, 0.12f, COL_BRONZE);
    } else {
        WPart(0, 0, 0.35f, 0.06f - 0.01f * (float)kind, 0.06f - 0.01f * (float)kind, 0.7f, COL_WOOD);
        WPart(0, 0, 0.72f + 0.03f * (float)kind, 0.13f + 0.03f * (float)kind, 0.13f + 0.03f * (float)kind, 0.22f - 0.04f * (float)kind,
              kind ? (Color){ 184, 115, 70, 255 } : COL_PALM_TRUNK);
    }
    rlPopMatrix();
}


// ---- First person: the hands and what they hold, in front of the eye ----

// A forearm reaching from below the view to the hand at (x, y, z) in the eye's frame, and the hand.
static void ViewArm(float x, float y, float z, Color skin, Color sleeve)
{
    Vector3 hand = { x, y, z }, elbow = { x * 1.6f + (x > 0 ? 0.08f : -0.08f), y - 0.32f, z - 0.18f };
    Vector3 mid = Vector3Lerp(hand, elbow, 0.5f), d = Vector3Subtract(hand, elbow);
    rlPushMatrix();
    rlTranslatef(mid.x, mid.y, mid.z);
    rlRotatef(atan2f(d.x, d.z) * RAD2DEG, 0, 1, 0);
    rlRotatef(-atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z)) * RAD2DEG, 1, 0, 0);
    WPart(0, 0, 0, 0.065f, 0.06f, Vector3Length(d), sleeve);
    rlPopMatrix();
    DrawShadedBlob(hand, (Vector3){ 0.036f, 0.032f, 0.045f }, skin);
}

// Seen from the eye: the bow in both hands (drawn back while aiming), the sword or mace in the weapon hand
// with its swing, the torch in the other. They bob a little with each step and sway as the body breathes,
// little enough not to swim in front of the eye.
static void ViewModel(const Player *p, Look look)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(G.cam.target, G.cam.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, (Vector3){ 0, 1, 0 })), up = Vector3CrossProduct(right, fwd);
    float t = (float)GetTime(), still = p->aiming ? 0.25f : 1.0f;
    float bobX = p->moving ? 0.012f * sinf(p->stride * 0.5f) : 0.0f, bobY = p->moving ? 0.01f * fabsf(cosf(p->stride * 0.5f)) : 0.0f;
    float swayX = 0.004f * sinf(t * 1.1f), swayY = 0.005f * sinf(t * 1.7f);
    Vector3 o = Vector3Add(G.cam.position, Vector3Add(Vector3Scale(right, (bobX + swayX) * still), Vector3Scale(up, (swayY - bobY) * still)));
    Matrix m = { right.x, up.x, fwd.x, o.x, right.y, up.y, fwd.y, o.y, right.z, up.z, fwd.z, o.z, 0, 0, 0, 1 };
    Color skin = look.skin, sleeve = (look.dress == DRESS_ROBE) ? look.cloth : look.skin;
    if (UseArmor() == ARMOR_LEATHER) sleeve = (Color){ 96, 62, 40, 255 };
    bool torch = TorchBurning();

    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(m));
    if (G.equip == EQ_BOW) {
        // The bow upright before the left hand; aiming, the right hand draws the string back toward the cheek.
        float pull = p->aiming ? p->draw : 0.0f;
        Vector3 grip = p->aiming ? (Vector3){ -0.1f, -0.1f, 0.62f } : torch ? (Vector3){ 0.26f, -0.36f, 0.6f } : (Vector3){ -0.2f, -0.34f, 0.62f };
        rlPushMatrix();
        rlTranslatef(grip.x, grip.y, grip.z);
        rlRotatef(p->aiming ? -8.0f : -38.0f, 0, 0, 1);
        float back = -0.07f - 0.36f * pull;
        DrawBow((Vector3){ 0, 0, 0 }, (Vector3){ 0, 0, back }, pull);
        rlPopMatrix();
        if (!torch) ViewArm(grip.x - 0.01f, grip.y - 0.02f, grip.z - 0.02f, skin, sleeve);
        Vector3 nock = p->aiming ? (Vector3){ grip.x + 0.03f, grip.y, grip.z + back } : (Vector3){ grip.x + 0.06f, grip.y + 0.1f, grip.z - 0.08f };
        if (!torch) ViewArm(nock.x + 0.04f, nock.y - 0.02f, nock.z, skin, sleeve);
        else ViewArm(grip.x + 0.01f, grip.y - 0.02f, grip.z - 0.02f, skin, sleeve);
    } else if (G.equip == EQ_MELEE || G.equip == EQ_MACE) {
        bool mace = G.equip == EQ_MACE;
        float s = SwingElapsed(G.swingT, mace);
        float pitch = mace ? 20.0f : WeaponAngle(s), yaw = mace && s >= 0.0f ? MaceYaw(s) * 0.6f : 0.0f;
        if (G.parryT > 0) pitch = -20.0f, yaw = -60.0f;
        Vector3 hand = { 0.24f, -0.32f, 0.56f };
        rlPushMatrix();
        rlTranslatef(hand.x, hand.y, hand.z);
        rlRotatef(yaw, 0, 1, 0);
        rlRotatef(pitch - 30.0f, 1, 0, 0);
        DrawWeaponPart(mace ? WEAPON_MACE : UseWeapon());
        ViewArm(hand.x, hand.y, hand.z, skin, sleeve);
    }
    if (torch) {
        Vector3 hand = { -0.28f, -0.34f, 0.64f };
        WPart(hand.x, hand.y + 0.1f, hand.z, 0.03f, 0.36f, 0.03f, COL_WOOD);
        WPart(hand.x, hand.y + 0.29f, hand.z, 0.05f, 0.05f, 0.05f, COL_CLAY_DARK);
        ViewArm(hand.x, hand.y, hand.z, skin, sleeve);
    }
    rlPopMatrix();
    if (torch) {
        Vector3 tip = { -0.28f, -0.02f, 0.64f };
        Vector3 w = Vector3Add(o, Vector3Add(Vector3Add(Vector3Scale(right, tip.x), Vector3Scale(up, tip.y)), Vector3Scale(fwd, tip.z)));
        GfxFlame(w, 0.3f);
    }
}

void DrawPlayer(void)
{
    const Player *p = &G.player;
    Look look = PlayerLook(G.self, ChosenStyle(), UseArmor());

    if (RoyaleOut()) return;
    // Riding behind an arrow, the eye has left the head: the hands in view would ride along with it, so
    // they are left out, and once the camera is clear of the head the body is drawn where it shot from.
    Vector3 head = { p->pos.x, p->pos.y + 1.5f, p->pos.z };
    bool away = G.camBlend > 0.01f;
    if (G.camDist < 1.3f && G.panel != PANEL_LOOK && !(away && Vector3Distance(G.cam.position, head) > 1.2f)) {
        if (!away && G.firstPerson && G.climb == 0 && !G.waitBed && !G.sleeping) {
            LightFor(p->pos);
            ViewModel(p, look);
            G.drawTorch = G.drawFog = 0;
        }
        return;
    }
    LightFor(p->pos);
    if (G.waitBed) {
        DrawInBed(G.self, p->pos, p->facing, look);
        G.drawTorch = G.drawFog = 0;
        return;
    }
    // Struck, the body flinches away from the blow and flushes red; swinging, it leans into the stroke.
    float hit = Clamp(G.flinchT / FLINCH_TIME, 0.0f, 1.0f), swing = 0;
    if (G.equip == EQ_MELEE || G.equip == EQ_MACE) swing = SwingLean(SwingElapsed(G.swingT, G.equip == EQ_MACE), G.equip == EQ_MACE);
    Vector2 ahead = { sinf(p->facing), cosf(p->facing) };
    Vector2 lean = { G.flinchX * 24.0f * hit + ahead.x * 12.0f * swing, G.flinchZ * 24.0f * hit + ahead.y * 12.0f * swing };
    float degrees = Vector2Length(lean);
    if (degrees > 0.5f) BeginLean(p->pos, Vector2Scale(lean, 1.0f / degrees), degrees);
    if (G.climb == 0 && (G.equip == EQ_MELEE || G.equip == EQ_MACE)) {
        bool mace = G.equip == EQ_MACE;
        HoldWeapon(p->pos, p->facing, p->crouching, mace, SwingElapsed(G.swingT, mace), G.parryT > 0);
        DrawWeaponPart(mace ? WEAPON_MACE : UseWeapon());
    }
    bool torch = TorchBurning() && G.climb == 0;
    if (torch) GripAt(HAND_OTHER, -0.36f, 1.36f, 0.28f);
    if (G.climb == 0 && G.equip == EQ_BOW && !p->aiming) HoldBowFor(p->pos, p->facing, p->crouching, torch);
    DrawHuman(p->pos, p->facing, p->stride, hit > 0 ? Flushed(look, hit) : look, p->crouching, p->aiming, p->draw);
    if (degrees > 0.5f) EndLean();
    if (torch) DrawHandTorch(p->pos, p->facing);
    G.drawTorch = G.drawFog = 0;
}
