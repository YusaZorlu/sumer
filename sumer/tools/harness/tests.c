// Logic tests for the new features, driven with a fixed time step. Included by harness.c.
static int fails, passes;

static void Check(bool ok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("%s: ", ok ? "PASS" : "FAIL");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    if (ok) passes++;
    else fails++;
}

static Animal *First(int type)
{
    for (int i = 0; i < G.animalCount; i++)
        if (G.animals[i].active && (int)G.animals[i].type == type && G.animals[i].state != ST_DEAD) return &G.animals[i];
    return NULL;
}

static void PlaceAt(float x, float z, float yaw)
{
    G.player.pos = (Vector3){ x, TerrainHeight(x, z), z };
    G.player.yaw = yaw;
    G.player.crouching = false;
    G.player.moving = G.player.running = false;
    G.player.grounded = true;
    SyncSelf();
}

// The world sees the player through G.peers, synced once a frame.
static void Wild(float dt)
{
    SyncSelf();
    UpdateWildlife(dt);
}

static void Calm(Animal *a)
{
    a->state = ST_CALM;
    a->speed = 0;
    a->awareness = 0;
    a->timer = 5;
    a->bite = 0;
}

static void Fly(int steps)
{
    for (int k = 0; k < steps; k++) {
        SyncSelf();
        UpdateArrows(0.01f);
        UpdateWildlife(0.01f);
    }
}

static void TestHerds(void)
{
    int count[MAX_ANIMALS] = { 0 }, type[MAX_ANIMALS], west[MAX_ANIMALS], snakes = 0;
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        count[a->pack]++;
        type[a->pack] = a->type;
        west[a->pack] = a->pos.x < RIVER_X0;
        snakes += a->type == AN_SNAKE;
    }
    int cityPacks = 0, westPacks = 0;
    bool cityOk = true, westOk = true;
    for (int p = 0; p < MAX_ANIMALS; p++) {
        if (!count[p] || type[p] != AN_WOLF) continue;
        if (west[p]) { westPacks++; westOk = westOk && count[p] >= 5; }
        else { cityPacks++; cityOk = cityOk && count[p] == 4; }
    }
    Check(cityPacks == 5 && cityOk, "city-side wolf packs: %d packs of 4", cityPacks);
    Check(westPacks == 3 && westOk, "west-bank wolf packs: %d packs of 5 or 6", westPacks);
    Check(AnimalMaxHp(AN_WOLF) == 78, "wolf health %d (was 60)", AnimalMaxHp(AN_WOLF));
    Check(AnimalMaxHp(AN_BOAR) == 135, "boar health %d (3 x 45)", AnimalMaxHp(AN_BOAR));
    Check(snakes > 0 && snakes <= 6, "snakes: %d in the whole world", snakes);
    bool hidden = true;
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        if (a->type == AN_SNAKE) hidden = hidden && Fertility(a->pos.x, a->pos.z) >= 0.45f && !InsideWalls(a->pos.x, a->pos.z, -6.0f);
    }
    Check(hidden, "snakes lie in green grass outside the walls");
}

// Finds an animal of 'type' and a spot 'range' metres from it (trying four directions) with an open
// line of fire: firm open ground under the archer, nothing solid in the arrow's path (gravity included)
// or the animal's way back, and no other animal near the line. The aim point is 'aimY' up and 'side'
// metres to the side of the animal.
static bool FindShot(int type, float range, float aimY, float side, Animal **out, Vector3 *spot, Vector3 *aim)
{
    static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
    for (int i = 0; i < G.animalCount; i++) {
        Animal *a = &G.animals[i];
        if (!a->active || (int)a->type != type || a->state == ST_DEAD) continue;
        for (int d = 0; d < 8; d++) {
            float dx = dirs[d][0], dz = dirs[d][1], px = a->pos.x + dx * range, pz = a->pos.z + dz * range;
            if (SolidAt(px, pz, 1.0f) || InWater(px, pz) || InsideWalls(px, pz, -6.0f)) continue;
            Vector3 from = { px, TerrainHeight(px, pz) + 1.5f, pz };
            Vector3 to = { a->pos.x - dz * side, a->pos.y + aimY, a->pos.z + dx * side };
            float dist = Vector3Distance(from, to);
            bool open = true;
            for (float s = 0.0f; s < dist - 1.0f && open; s += 0.25f) {
                float t = s / BowSpeed();
                Vector3 p = Vector3Lerp(from, to, s / dist);
                p.y -= 2.5f * t * t;
                open = p.y > TerrainHeight(p.x, p.z) + 0.2f && !PointInSolid(p);
            }
            for (float s = 0.0f; s < range && open; s += 0.25f) {
                float x = a->pos.x + dx * s, z = a->pos.z + dz * s;
                open = !SolidAt(x, z, 0.5f) && !InWater(x, z);
            }
            for (int k = 0; k < G.animalCount && open; k++) {
                const Animal *o = &G.animals[k];
                if (o == a || !o->active || o->state == ST_DEAD) continue;
                Vector2 p = { o->pos.x, o->pos.z };
                Vector2 f = { from.x, from.z }, e = { to.x + dx * -4.0f, to.z + dz * -4.0f };
                Vector2 fe = Vector2Subtract(e, f);
                float u = Clamp(Vector2DotProduct(Vector2Subtract(p, f), fe) / Vector2DotProduct(fe, fe), 0.0f, 1.0f);
                open = Vector2Distance(p, Vector2Add(f, Vector2Scale(fe, u))) > 2.5f;
            }
            if (!open) continue;
            *out = a;
            *spot = (Vector3){ px, 0, pz };
            *aim = to;
            return true;
        }
    }
    return false;
}

static float FacingFrom(Vector3 spot, Vector3 target)
{
    return atan2f(spot.x - target.x, spot.z - target.z);
}

static int BoarsHunting(int pack)
{
    int n = 0;
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *m = &G.animals[i];
        n += m->active && m->type == AN_BOAR && m->pack == pack && m->state == ST_HUNT && m->target == G.self;
    }
    return n;
}

// Shoot one boar of a sounder: the sounder charges like a wolf pack, keeps at it, and gives up when
// you reach the city.
static void TestBoar(void)
{
    Animal *b;
    Vector3 spot, aim;
    if (!FindShot(AN_BOAR, 12.0f, 0.63f, 0.0f, &b, &spot, &aim)) { Check(false, "no boar with an open line of fire"); return; }
    Vector3 dir = Vector3Normalize((Vector3){ b->pos.x - spot.x, 0, b->pos.z - spot.z });
    int mates = 0;
    for (int i = 0; i < G.animalCount; i++) {
        Animal *m = &G.animals[i];
        if (!m->active || m->type != AN_BOAR || m->pack != b->pack || m->state == ST_DEAD) continue;
        Calm(m);
        if (m == b || mates >= 2) continue;
        float side = mates ? 3.0f : -3.0f;
        m->pos.x = b->pos.x + dir.x * 5.0f + dir.z * side;
        m->pos.z = b->pos.z + dir.z * 5.0f - dir.x * side;
        m->pos.y = TerrainHeight(m->pos.x, m->pos.z);
        mates++;
    }
    PlaceAt(spot.x, spot.z, FacingFrom(spot, b->pos));
    G.health = HEALTH_MAX;
    Vector3 from = { G.player.pos.x, G.player.pos.y + 1.5f, G.player.pos.z };
    ShootArrow(from, aim, 1.0f);
    Fly(60);
    Check(b->state == ST_HUNT, "a boar hit by an arrow charges (state %d, hp %d/135)", b->state, b->hp);
    int sounder = BoarsHunting(b->pack);
    Check(sounder >= 1 + mates && strstr(G.msg, "boars") != NULL, "its sounder charges with it: %d boars (\"%s\")", sounder, G.msg);
    float taken = 0, near = 99;
    for (int k = 0; k < 2000; k++) {
        G.health = HEALTH_MAX;
        Wild(0.01f);
        taken += HEALTH_MAX - G.health;
        if (k == 599) Check(taken > 0, "the charging boars gore you (%.0f damage in 6 s)", taken);
        if (k >= 1400) near = fminf(near, XZDist(b->pos, G.player.pos));
    }
    G.health = HEALTH_MAX;
    Check(BoarsHunting(b->pack) == sounder && near < 3.5f, "20 s on they are all still at you (%d boars, closest %.1f m, %.0f damage)", BoarsHunting(b->pack), near, taken);
    PlaceAt(0, 9, 0);
    Wild(0.01f);
    Check(BoarsHunting(b->pack) == 0, "safe in the city: the boars give up (%d still charging)", BoarsHunting(b->pack));
    G.health = HEALTH_MAX;
    for (int i = 0; i < G.animalCount; i++)
        if (G.animals[i].active && G.animals[i].type == AN_BOAR && G.animals[i].state != ST_DEAD) Calm(&G.animals[i]);
}

static void TestArrows(void)
{
    Animal *g;
    Vector3 spot, aim;
    bool found = FindShot(AN_GAZELLE, 24.0f, 1.0f, 2.0f, &g, &spot, &aim);
    Check(found, "a gazelle with an open line of fire");
    if (found) {
        Calm(g);
        PlaceAt(spot.x, spot.z, FacingFrom(spot, g->pos));
        Vector3 from = { G.player.pos.x, G.player.pos.y + 1.5f, G.player.pos.z };
        ShootArrow(from, aim, 1.0f);
        Fly(80);
        Check(g->state == ST_FLEE && g->hp == AnimalMaxHp(AN_GAZELLE), "an arrow whistling past a gazelle makes it bolt (state %d)", g->state);
    }
    Animal *w = First(AN_WOLF);
    if (w) {
        Calm(w);
        G.peers[0].aggro = 0;
        G.peers[0].alone = false;
        G.armor = ARMOR_NONE;
        PlaceAt(w->pos.x + 12, w->pos.z, PI / 2);
        Vector3 from = { G.player.pos.x, G.player.pos.y + 1.5f, G.player.pos.z };
        Vector3 to = { w->pos.x + 3.0f, TerrainHeight(w->pos.x + 3.0f, w->pos.z), w->pos.z };
        ShootArrow(from, to, 1.0f);
        Fly(100);
        Check(w->state == ST_HUNT && G.peers[0].aggro > 0, "an arrow landing near a wolf sets the pack on you (state %d, aggro %.0f)", w->state, G.peers[0].aggro);
        CalmBeasts(-1);
        G.peers[0].aggro = 0;
    }
}

static int MaceRound(int equip)
{
    PlaceAt(260, 470, 0);
    Animal *ws[3];
    int n = 0;
    for (int i = 0; i < G.animalCount && n < 3; i++)
        if (G.animals[i].type == AN_WOLF && G.animals[i].active) ws[n++] = &G.animals[i];
    for (int k = 0; k < n; k++) {
        float a = -0.6f + 0.6f * (float)k;
        ws[k]->pos = (Vector3){ 260 + sinf(a) * 1.8f, 0, 470 - cosf(a) * 1.8f };
        ws[k]->pos.y = TerrainHeight(ws[k]->pos.x, ws[k]->pos.z);
        ws[k]->hp = 60;
        ws[k]->state = ST_HUNT;
    }
    G.equip = equip;
    UpdateCombat(0.0001f);
    MeleeStrike(STRIKE_START);
    MeleeStrike(STRIKE_CONTACT);
    int hit = 0;
    for (int k = 0; k < n; k++) hit += ws[k]->hp < 60;
    for (int k = 0; k < n; k++) ws[k]->state = ST_FLEE;
    return hit;
}

// A swing lands when the weapon comes down on its target (CONTACT_TIME in), not when it starts, and the
// drawn weapon is down at that moment: raised before it, low and forward when it meets.
static void TestSwingContact(void)
{
    PlaceAt(260, 470, 0);
    Animal *w = NULL;
    for (int i = 0; i < G.animalCount && !w; i++)
        if (G.animals[i].type == AN_WOLF && G.animals[i].active) w = &G.animals[i];
    if (!w) {
        Check(false, "a wolf to swing at");
        return;
    }
    w->pos = (Vector3){ 260, TerrainHeight(260, 468.4f), 468.4f };
    w->hp = 60;
    w->state = ST_HUNT;
    G.equip = EQ_MELEE;
    G.swingCool = 0;
    G.hitStop = G.shakeT = 0;
    G.swingT = SWING_TIME;
    G.strikePending = true;
    MeleeStrike(STRIKE_START);
    Check(w->hp == 60, "a club swing does nothing yet as it starts (wolf hp %d)", w->hp);
    float t = 0, landed = -1;
    while (t < SWING_TIME + 0.2f && landed < 0) {
        UpdatePlayer(0.01f, false);
        t += 0.01f;
        w->pos = (Vector3){ 260, TerrainHeight(260, 468.4f), 468.4f };
        if (w->hp < 60) landed = t;
    }
    Check(fabsf(landed - CONTACT_TIME) < 0.025f, "it lands as the club comes down, %.2f s in (contact at %.2f)", landed, CONTACT_TIME);
    Check(G.hitStop > 0 || G.swingT > 0, "the swing holds a moment where it struck");
    Check(WeaponAngle(0.15f) < -80.0f && WeaponAngle(CONTACT_TIME) > 0.0f, "the club is raised before contact (%.0f) and down when it meets (%.0f)",
          WeaponAngle(0.15f), WeaponAngle(CONTACT_TIME));
    Check(fabsf(MaceYaw(CONTACT_TIME)) < 15.0f, "the war mace crosses straight ahead at contact (%.0f degrees)", MaceYaw(CONTACT_TIME));
    w->state = ST_FLEE;
    G.swingT = 0;
    G.hitStop = G.shakeT = 0;
    G.strikePending = false;
}

// Every gate of the city has guards standing at it, and keeps them in a PvP game: only the guards on the
// ziggurat's terrace go.
// A chosen look survives packing for the network, and the townsfolk are not all alike.
// Water is waded, not crossed by boat: canals cost 60% of your speed, the river 85%; the ferry is gone.
static void TestWading(void)
{
    float canal = WadeSpeed(0, 89), river = WadeSpeed(-139, 0), land = WadeSpeed(10, 20);
    Check(fabsf(canal - 0.4f) < 0.01f && fabsf(river - 0.15f) < 0.01f && land == 1.0f,
          "wading: a canal leaves %.0f%% of your speed, the river %.0f%%, dry land %.0f%%", canal * 100, river * 100, land * 100);
    G.player.pos = G.stations[STN_FERRY_E];
    FindTarget(true);
    Check(!(G.target.kind == TGT_STATION && (G.target.index == STN_FERRY_E || G.target.index == STN_FERRY_W)), "there is no ferry to take at the old landing stage");
}

// A chosen look survives packing for the network, and the townsfolk are not all alike.
static void TestLooks(void)
{
    Look l = PlayerLook(1, PackStyle(HAIR_BUN, BEARD_LONG, DRESS_SHAWL, JEWEL_BEADS), ARMOR_BRONZE);
    Check(l.hairStyle == HAIR_BUN && l.beard == BEARD_LONG && l.dress == DRESS_SHAWL && l.jewel == JEWEL_BEADS && l.armor == ARMOR_BRONZE,
          "a chosen look comes back from its packed byte (hair %d beard %d dress %d jewel %d armor %d)", l.hairStyle, l.beard, l.dress, l.jewel, l.armor);
    int dresses[DRESS_KINDS] = { 0 }, women = 0, beards = 0;
    for (unsigned i = 0; i < 60; i++) {
        Look t = PersonLook(LOOK_TOWN, i, (Color){ 150, 90, 70, 255 });
        dresses[t.dress]++;
        women += t.woman;
        beards += t.beard != BEARD_NONE;
        if (t.woman && t.beard != BEARD_NONE) beards += 1000;
    }
    int kinds = 0;
    for (int k = 0; k < DRESS_KINDS; k++) kinds += dresses[k] > 0;
    Check(kinds == DRESS_KINDS && women >= 12 && women <= 40 && beards < 1000, "60 townsfolk wear all %d kinds of dress, %d are women, no woman has a beard", kinds, women);
    Look g = PersonLook(LOOK_GUARD, 3, (Color){ 120, 100, 80, 255 });
    Check(g.head == HEAD_HELMET && g.armor == ARMOR_BRONZE, "a guard wears a bronze helmet and the studded cloak");
}

static void TestGateGuards(void)
{
    static const float gates[5][2] = { { 0, 72 }, { -36.5f, -80 }, { 92, 9 }, { -86, 26 }, { -94, -10 } };
    static Villager saved[MAX_VILLAGERS];
    int count = G.villagerCount;
    memcpy(saved, G.villagers, sizeof(saved));
    for (int pass = 0; pass < 2; pass++) {
        if (pass) RemoveZigguratGuards();
        int guarded = 0, high = 0;
        for (int g = 0; g < 5; g++) {
            int n = 0;
            for (int i = 0; i < G.villagerCount; i++) {
                const Villager *v = &G.villagers[i];
                if (v->kind == NPC_GUARD && v->speed <= 0 && XZDist(v->a, (Vector3){ gates[g][0], 0, gates[g][1] }) < 10.0f) n++;
            }
            guarded += n >= 2;
        }
        for (int i = 0; i < G.villagerCount; i++) high += G.villagers[i].kind == NPC_GUARD && G.villagers[i].a.y >= 5.0f && !G.villagers[i].crouch;
        Check(guarded == 5, "%s: two guards stand at each of the city's 5 gates (%d gates guarded)", pass ? "PvP" : "alone", guarded);
        if (pass) Check(high == 0, "PvP: no guards left on the ziggurat's terrace (%d)", high);
    }
    G.villagerCount = count;
    memcpy(G.villagers, saved, sizeof(saved));
}

static void TestMace(void)
{
    G.hasMace = true;
    int mace = MaceRound(EQ_MACE), club = MaceRound(EQ_MELEE);
    G.hitStop = G.shakeT = 0;   // the blows' feel, which GameUpdate would let fade
    Check(mace == 3, "one war mace swing strikes all %d wolves in front", mace);
    Check(club == 1, "a club strikes only one (%d)", club);
    G.equip = EQ_BOW;
}

static void TestSnake(void)
{
    Animal *s = First(AN_SNAKE);
    if (!s) { Check(false, "no snake"); return; }
    Calm(s);
    s->lair = s->pos;
    PlaceAt(s->pos.x + 6, s->pos.z, PI / 2);
    G.health = HEALTH_MAX;
    for (int k = 0; k < 100; k++) Wild(0.02f);
    Check(s->state == ST_CALM && AnimalHidden(s), "a snake 6 m away stays hidden in its grass");
    PlaceAt(s->pos.x + 2.0f, s->pos.z, PI / 2);
    G.player.crouching = true;
    for (int k = 0; k < 100; k++) Wild(0.02f);
    Check(s->state == ST_CALM, "crouching, you can creep within 2 m unnoticed");
    G.player.crouching = false;
    Wild(0.02f);
    Check(s->state == ST_ALERT, "standing up 2 m away, it rears and hisses first (state %d)", s->state);
    for (int k = 0; k < 30; k++) Wild(0.02f);
    Check(G.health > HEALTH_MAX - 1.0f, "no bite during the hiss (health %.0f)", G.health);
    for (int k = 0; k < 100; k++) Wild(0.02f);
    Check(G.health < HEALTH_MAX, "then it strikes (health %.0f)", G.health);
    PlaceAt(s->pos.x + 20, s->pos.z, PI / 2);
    for (int k = 0; k < 300; k++) Wild(0.02f);
    Check(s->state == ST_CALM, "it settles back into the grass when you leave");
    G.health = HEALTH_MAX;
}

static void TestHerbs(void)
{
    G.health = 50;
    G.herbT = 0;
    BagAdd(IT_HERB, 3);
    int before = BagCount(IT_HERB);
    bool used = UseHerb();
    Check(used && G.health == 60 && G.herbT == HERB_SECONDS && BagCount(IT_HERB) == before - 1,
          "chewing an herb: +10 health now and %d s of fast healing", (int)G.herbT);
    G.health = HEALTH_MAX;
    Check(!UseHerb(), "an herb is not wasted when you are not hurt");
    BagRemove(IT_HERB, 99);
    int herbs = 0;
    for (int i = 0; i < G.resCount; i++) herbs += G.res[i].kind == RES_HERB;
    int west = 0;
    for (int i = 0; i < G.resCount; i++) west += G.res[i].kind == RES_HERB && G.res[i].pos.x < RIVER_X0;
    Check(herbs > 40 && west > herbs / 2, "%d herb plants, %d of them across the river", herbs, west);
    int buyers = 0;
    for (int t = 0; t < TR_COUNT; t++)
        for (int k = 0; k < G.traders[t].offerCount; k++)
            if (G.traders[t].offers[k].in1 == IT_HERB) buyers++;
    Check(buyers >= 4, "%d traders take herbs in barter", buyers);
}

static void TestPrompts(void)
{
    static const char *const expect[RES_KINDS] = {
        "[E] Pick dates", "[E] Cut reeds (-1 food)", "[E] Chop wood (hard work: -8 food)", "[E] Dig clay (hard work: -2 food)",
        "[E] Mine copper ore (hard work: -16 food)", "[E] Cast your net (-1.5 food)", "[E] Gather herbs (-1 food)",
    };
    for (int k = 0; k < RES_KINDS; k++) {
        int r = -1;
        for (int i = 0; i < G.resCount && r < 0; i++)
            if ((int)G.res[i].kind == k && !G.res[i].farm && G.res[i].regrow <= 0 && !WestBank(G.res[i].pos.x)) r = i;
        if (r < 0) { Check(false, "no resource of kind %d", k); continue; }
        PlaceAt(G.res[r].pos.x + 0.6f, G.res[r].pos.z, 0);
        G.food = 80;
        G.panel = PANEL_NONE;
        G.sleeping = false;
        GameUpdate();
        Check(strcmp(G.promptE, expect[k]) == 0, "prompt \"%s\"", G.promptE);
    }
}

static void TestTrade(void)
{
    bool others = true, herder = true, shepherd = false;
    for (int t = 0; t < TR_COUNT; t++) {
        const Trader *tr = &G.traders[t];
        for (int k = 0; k < tr->offerCount; k++) {
            const Offer *o = &tr->offers[k];
            bool fleece = o->kind == OUT_ITEM && (o->out == IT_SHEEPSKIN || o->out == IT_SHEEP);
            if (t == TR_SHEPHERD) shepherd = shepherd || fleece;
            else if (fleece) others = false;
            if (t == TR_HERDER && (o->kind != OUT_ITEM || (o->out != IT_MILK && o->out != IT_COWHIDE) || o->in1 == IT_WASTE)) herder = false;
        }
    }
    Check(shepherd && others, "only the herders' shepherd sells sheepskin and sheep");
    Check(herder, "the city's cattle herder deals only in milk and cowhide, no plant waste");
    bool mace = false;
    for (int k = 0; k < G.traders[TR_SMITH].offerCount; k++) mace = mace || G.traders[TR_SMITH].offers[k].kind == OUT_MACE;
    Check(mace, "the smith sells the war mace");
    Vector3 h = VillageCenter(VIL_HUNTERS), s = VillageCenter(VIL_HERDERS);
    Check(XZDist(G.traders[TR_HUNTER].pos, h) < 20 && XZDist(G.traders[TR_SHEPHERD].pos, s) < 20, "hunter and shepherd stand in their villages");
}

static void TestPeople(void)
{
    int travellers = 0;
    bool onRoad = true;
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (v->kind != NPC_TRAVELLER) continue;
        travellers++;
        Vector3 p = VillagerPos(v);
        onRoad = onRoad && RoadDistance(p.x, p.z) < 0.5f && !InsideWalls(p.x, p.z, 0.0f);
    }
    Check(travellers >= 3 && onRoad, "%d travellers, all on roads outside the walls", travellers);

    // Walk them for two minutes; whoever crosses the east canal does it on the bridge deck.
    for (int i = 0; i < G.villagerCount; i++)
        if (G.villagers[i].road == ROAD_EAST && G.villagers[i].dir < 0) G.villagers[i].t = 45.0f / RoadLength(ROAD_EAST);
    float t0[MAX_VILLAGERS];
    for (int i = 0; i < G.villagerCount; i++) t0[i] = G.villagers[i].t;
    float bridgeY = 99;
    bool stayed = true;
    for (int k = 0; k < 2400; k++) {
        CollectBodies();
        UpdateVillage(0.05f);
        for (int i = 0; i < G.villagerCount; i++) {
            const Villager *v = &G.villagers[i];
            if (v->kind != NPC_TRAVELLER) continue;
            Vector3 p = VillagerPos(v);
            if (fabsf(p.x - 122) < 1.5f && fabsf(p.z - 9) < 3) bridgeY = fminf(bridgeY, p.y);
            if (RoadDistance(p.x, p.z) > 0.5f) stayed = false;
        }
    }
    bool moved = true;
    for (int i = 0; i < G.villagerCount; i++)
        if (G.villagers[i].kind == NPC_TRAVELLER) moved = moved && fabsf(G.villagers[i].t - t0[i]) > 0.05f;
    Check(moved && stayed, "travellers keep walking their roads");
    Check(bridgeY < 99 && bridgeY > 0.25f, "a traveller crossing the canal bridge walks on its deck (lowest y %.2f)", bridgeY);

    UpdateCaravans(0.0f);
    for (int k = 0; k < 400; k++) UpdateCaravans(0.05f);
    UpdateCombat(0.0001f);
    int guards = 0, active = 0, raiders = 0, barbarians = 0;
    float raiderHp = 0, barbarianHp = 0;
    for (int c = 0; c < MAX_CARAVANS; c++) active += G.caravans[c].state != CV_AWAY;
    for (int i = 0; i < FighterCount(); i++) {
        Vector3 pos;
        float hp, maxHp;
        const char *name;
        bool hostile, dead;
        FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead);
        if (strcmp(name, "Caravan guard") == 0) guards++;
        if (strcmp(name, "Raider") == 0) { raiders++; raiderHp = maxHp; }
        if (strcmp(name, "Barbarian") == 0) { barbarians++; barbarianHp = maxHp; }
    }
    Check(active > 0 && guards == 2 * active, "%d caravans on the road, %d caravan guards", active, guards);
    Check(raiders == 8 && barbarians == 8, "%d raiders east of the river, %d barbarians (and their bowmen) west", raiders, barbarians);
    Check(raiderHp > 190 && barbarianHp == 2 * raiderHp, "raiders tougher (%.0f, was 190), barbarians twice that (%.0f)", raiderHp, barbarianHp);
}

// Townsfolk walk the streets from crossing to crossing, keep out of the walls, and get round (or
// turn back from) someone standing in the way, never walking through them.
static void TestStreets(void)
{
    static bool seen[MAX_VILLAGERS][32];
    int list[MAX_VILLAGERS], walkers = 0, nodes = StreetNodeCount();
    for (int i = 0; i < G.villagerCount; i++)
        if (G.villagers[i].from >= 0) list[walkers++] = i;
    memset(seen, 0, sizeof(seen));
    PlaceAt(150, 150, 0);
    bool clear = true, inside = true;
    Vector3 bad = { 0 }, last[MAX_VILLAGERS];
    float jump = 0;
    int right = 0, samples = 0;
    for (int n = 0; n < walkers; n++) last[n] = VillagerPos(&G.villagers[list[n]]);
    for (int k = 0; k < 6000; k++) {
        CollectBodies();
        UpdateVillage(0.05f);
        for (int n = 0; n < walkers; n++) {
            const Villager *v = &G.villagers[list[n]];
            Vector3 p = VillagerPos(v);
            jump = fmaxf(jump, XZDist(p, last[n]));
            last[n] = p;
            if (v->moving) {
                samples++;
                right += v->side < -0.3f;
            }
            seen[n][v->to] = true;
            if (SolidAt(p.x, p.z, 0.15f)) { clear = false; bad = p; }
            if (!InsideWalls(p.x, p.z, -2.0f)) { inside = false; bad = p; }
        }
    }
    int least = 99, reached = 0;
    for (int n = 0; n < walkers; n++) {
        int c = 0;
        for (int j = 0; j < nodes; j++) c += seen[n][j];
        least = c < least ? c : least;
    }
    for (int j = 0; j < nodes; j++) {
        bool any = false;
        for (int n = 0; n < walkers; n++) any = any || seen[n][j];
        reached += any;
    }
    Check(walkers >= 10 && least >= 3, "%d townsfolk walk the streets; in 5 minutes each reached %d or more crossings", walkers, least);
    char missed[64] = "";
    for (int j = 0; j < nodes; j++) {
        bool any = false;
        for (int n = 0; n < walkers; n++) any = any || seen[n][j];
        if (!any) snprintf(missed + strlen(missed), sizeof(missed) - strlen(missed), " %d", j);
    }
    Check(reached >= nodes - 2, "between them they reached %d of the %d crossings (missed:%s)", reached, nodes, missed[0] ? missed : " none");
    Check(clear && inside, "nobody walked into a wall or out of the city (%.1f,%.1f)", bad.x, bad.z);
    Check(jump < 0.15f, "no one jumps, not even turning a corner (longest step %.3f m in 0.05 s)", jump);
    Check(right > samples * 6 / 10, "walking, they keep to the right of the street %d%% of the time", samples ? right * 100 / samples : 0);

    // Stand in a townsman's way on the main street.
    Villager *v = &G.villagers[list[0]];
    SetStreet(v, 1, 2);
    v->t = 0.3f;
    v->side = -1.0f;
    v->wait = v->stuck = 0;
    Villager ahead = *v;
    ahead.t += 2.5f / 30.0f;
    Vector3 block = VillagerLanePos(&ahead);
    PlaceAt(block.x, block.z, 0);
    float closest = 99;
    bool past = false, back = false;
    for (int k = 0; k < 400 && !past && !back; k++) {
        CollectBodies();
        UpdateVillage(0.05f);
        Vector3 p = VillagerPos(v);
        closest = fminf(closest, XZDist(p, G.player.pos));
        if (v->from == 1 && v->to == 2) past = p.z > block.z + 1.0f;
        else back = true;
    }
    Check((past || back) && closest > 0.6f, "a townsman blocked by you %s (closest %.2f m)", past ? "steps round you" : back ? "turns back" : "is stuck", closest);
    PlaceAt(150, 150, 0);
}

// Points the third-person bow crosshair at 'at' (as the player would with the mouse) and lets fly
// with the given draw. True when the arrow strikes person 'who'.
static bool ShootPersonAt(int who, Vector3 at, float draw, float *miss)
{
    Player *p = &G.player;
    p->aiming = true;
    p->draw = draw;
    G.firstPerson = false;
    G.camShoulder = 1.0f;
    for (int k = 0; k < 30; k++) {
        FollowCamera(1.0f);
        G.camShoulder = 1.0f;
        Vector3 f = Vector3Normalize(Vector3Subtract(at, G.cam.position));
        p->yaw = atan2f(-f.x, -f.z);
        p->pitch = asinf(-f.y);
    }
    FollowCamera(1.0f);
    Vector3 right = { cosf(p->yaw), 0, -sinf(p->yaw) }, fwd = { -sinf(p->yaw), 0, -cosf(p->yaw) };
    Vector3 from = { p->pos.x, p->pos.y + 1.5f, p->pos.z };
    from = Vector3Add(from, Vector3Add(Vector3Scale(right, -0.2f), Vector3Scale(fwd, 0.7f)));
    float hp0, hp, maxHp;
    Vector3 pos;
    const char *name;
    bool hostile, dead;
    FighterInfo(who, &pos, &hp0, &maxHp, &name, &hostile, &dead);
    int a = ShootArrow(from, AimPoint(), draw);
    // Where the arrow passes the target: its closest approach to the aimed point.
    *miss = 99;
    for (int k = 0; k < 300 && a >= 0 && G.arrows[a].flying; k++) {
        Vector3 before = G.arrows[a].pos;
        Fly(1);
        Vector3 after = G.arrows[a].pos, ab = Vector3Subtract(after, before);
        float len2 = Vector3DotProduct(ab, ab);
        float u = len2 > 0 ? Clamp(Vector3DotProduct(Vector3Subtract(at, before), ab) / len2, 0.0f, 1.0f) : 0.0f;
        *miss = fminf(*miss, Vector3Distance(Vector3Add(before, Vector3Scale(ab, u)), at));
    }
    FighterInfo(who, &pos, &hp, &maxHp, &name, &hostile, &dead);
    p->aiming = false;
    p->draw = 0;
    return hp < hp0;
}

// A townsman stands in the open; the crosshair on his chest must be where the arrow goes, near or far,
// fully drawn or not.
static void TestAim(void)
{
    // Open ground east of the city with nothing in the way for 40 m to the north.
    float sx = 0, sz = 0;
    bool found = false;
    for (float x = 140; x < 400 && !found; x += 7)
        for (float z = -200; z < 200 && !found; z += 7) {
            bool open = true;
            for (float d = -2; d <= 42 && open; d += 1.0f)
                open = !SolidAt(x, z - d, 1.5f) && !InWater(x, z - d) && fabsf(TerrainHeight(x, z - d) - TerrainHeight(x, z)) < 1.5f &&
                       RoadDistance(x, z - d) > 6.0f;
            if (open) { sx = x; sz = z; found = true; }
        }
    if (!found) { Check(false, "no open ground for the aim test"); return; }
    CollectBodies();
    CollectFighters();
    int who = FighterByOwner(OWNER_VILLAGER + 0);
    Villager *v = &G.villagers[0];
    static const float dists[] = { 3, 6, 10, 20, 35 };
    static const float draws[] = { 1.0f, 0.5f };
    int hits = 0, shots = 0;
    char log[512] = "";
    for (int di = 0; di < 5; di++)
        for (int k = 0; k < 2; k++) {
            PlaceAt(sx, sz, 0);
            float tz = sz - dists[di];
            v->vit = (Vitals){ .hp = 120, .target = -1, .used = true, .override = true, .at = { sx, TerrainHeight(sx, tz), tz } };
            CollectFighters();
            who = FighterByOwner(OWNER_VILLAGER + 0);
            float miss;
            bool hit = ShootPersonAt(who, (Vector3){ sx, v->vit.at.y + 1.1f, tz }, draws[k], &miss);
            shots++;
            hits += hit;
            snprintf(log + strlen(log), sizeof(log) - strlen(log), " %.0fm/%.0f%%:%s(%.2f)", dists[di], draws[k] * 100, hit ? "hit" : "MISS", miss);
        }
    v->vit = (Vitals){ .used = false };
    CollectFighters();
    Check(hits == shots, "the crosshair on a person's chest hits them, near and far: %d of %d;%s", hits, shots, log);
}


static void RunCombat(float seconds)
{
    for (float t = 0; t < seconds; t += 0.05f) {
        SyncSelf();
        CollectBodies();
        UpdateCombat(0.05f);
    }
}

static int VillagerWhere(int kind, bool (*ok)(Vector3))
{
    for (int i = 0; i < G.villagerCount; i++)
        if ((int)G.villagers[i].kind == kind && !G.villagers[i].vit.dead && ok(VillagerPos(&G.villagers[i]))) return i;
    return -1;
}
static bool FarEast(Vector3 p) { return p.x > 160.0f; }
static bool AtHunters(Vector3 p) { return VillageAt(p) == VIL_HUNTERS; }
static bool Anywhere(Vector3 p) { (void)p; return true; }

static void Stand(int villager, float x, float z)
{
    Vitals *v = &G.villagers[villager].vit;
    CollectFighters();
    v->override = true;
    v->at = (Vector3){ x, GroundHeight(x, z, 2.0f), z };
}

// A guard who has to go round the ziggurat to reach a wanted player behind it gets there.
static void TestGuardRound(void)
{
    G.self = G.actor = 0;
    G.health = 1e6f;
    RespawnPeople();
    // The guard who walks the precinct's west side, stopped half way along it; the player behind the ziggurat's
    // north-east, within the guard's reach but out of a straight line.
    int g = -1;
    for (int i = 0; i < G.villagerCount && g < 0; i++)
        if (G.villagers[i].kind == NPC_GUARD && G.villagers[i].a.x < -23.0f && G.villagers[i].a.x > -25.0f) g = i;
    if (g < 0) {
        Check(false, "no guard walks the precinct's west side");
        return;
    }
    G.villagers[g].t = 0.5f;
    G.villagers[g].speed = 0;
    Stand(g, -24, -35);
    Vitals *v = &G.villagers[g].vit;
    PlaceAt(2, -62, 0);
    G.peers[0].wanted = 200;
    v->hostile = 200;
    v->target = 0;
    float closest = 1e9f;
    for (float t = 0; t < 40; t += 0.05f) {
        SyncSelf();
        CollectBodies();
        UpdateCombat(0.05f);
        closest = fminf(closest, XZDist(v->at, G.player.pos));
        if (getenv("TRACE") && fmodf(t, 2.0f) < 0.05f) printf("  t %.0f guard (%.1f,%.1f) override %d hostile %.0f target %d\n", t, v->at.x, v->at.z, v->override, v->hostile, v->target);
    }
    Check(closest < 4.0f, "a guard goes round the ziggurat to the wanted player behind it (closest %.1f m)", closest);
    G.peers[0].wanted = 0;
    G.health = HEALTH_MAX;
    RespawnPeople();
}

// Wolves run round a tree between them and their prey, keep a body apart in the pack, and a badly hurt
// wolf fights on while its pack is beside it and limps away only once it is alone.
static void HuntFor(float secs, const Animal *w, float *closest)
{
    for (float t = 0; t < secs; t += 0.02f) {
        G.peers[0].aggro = 90;
        Wild(0.02f);
        if (closest) *closest = fminf(*closest, XZDist(w->pos, G.player.pos));
    }
}

static void TestWolfWays(void)
{
    G.self = G.actor = 0;
    G.health = 1e6f;
    Animal *w = First(AN_WOLF);
    const Box *tree = NULL;
    for (int i = 0; i < G.boxCount && !tree; i++) {
        const Box *b = &G.boxes[i];
        if (b->solid && !b->built && !b->water && b->size.x < 1.2f && b->size.y > 2.0f && b->pos.x > RIVER_X1 + 30 &&
            !InsideWalls(b->pos.x, b->pos.z, -30.0f) && FreeSpot(b->pos.x - 5, b->pos.z, 0.6f) && FreeSpot(b->pos.x + 5, b->pos.z, 0.6f) &&
            !SolidAt(b->pos.x - 2.5f, b->pos.z, 0.6f) && !SolidAt(b->pos.x + 2.5f, b->pos.z, 0.6f))
            tree = b;
    }
    if (!w || !tree) {
        Check(false, "a wolf and a lone tree on the plain");
        return;
    }
    CalmBeasts(-1);
    for (int i = 0; i < G.animalCount; i++)   // the rest of the world's beasts out of the way
        if (&G.animals[i] != w) G.animals[i].pos.x += 3000;
    PlaceAt(tree->pos.x + 5, tree->pos.z, 0);
    w->pos = (Vector3){ tree->pos.x - 5, TerrainHeight(tree->pos.x - 5, tree->pos.z), tree->pos.z };
    w->hp = AnimalMaxHp(AN_WOLF);
    w->state = ST_HUNT;
    w->target = 0;
    w->timer = 0;
    float closest = 1e9f;
    HuntFor(4, w, &closest);
    Check(closest < 1.6f, "a wolf runs round a tree between it and its prey (closest %.1f m)", closest);

    // Its pack with it: four hunting from one side do not end up standing in one another.
    Animal *pack[4] = { w };
    int n = 1;
    for (int i = 0; i < G.animalCount && n < 4; i++)
        if (&G.animals[i] != w && G.animals[i].type == AN_WOLF && G.animals[i].pack == w->pack && G.animals[i].active) pack[n++] = &G.animals[i];
    PlaceAt(tree->pos.x + 14, tree->pos.z, 0);
    for (int k = 0; k < n; k++) {
        Vector3 p = { tree->pos.x + 4, 0, tree->pos.z - 1.5f + (float)k };
        p.y = TerrainHeight(p.x, p.z);
        pack[k]->pos = p;
        pack[k]->hp = AnimalMaxHp(AN_WOLF);
        pack[k]->state = ST_HUNT;
        pack[k]->target = 0;
        pack[k]->timer = 0;
    }
    HuntFor(3, w, NULL);
    float apart = 1e9f;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) apart = fminf(apart, XZDist(pack[i]->pos, pack[j]->pos));
    Check(n == 4 && apart > 0.8f, "a pack of %d at its prey keeps a body apart (nearest two %.2f m)", n, apart);

    w->hp = AnimalMaxHp(AN_WOLF) / 3;
    HuntFor(1, w, NULL);
    Check(w->state == ST_HUNT, "a badly hurt wolf fights on beside its pack (state %d)", w->state);
    for (int k = 1; k < n; k++) pack[k]->pos.x += 3000;
    HuntFor(0.1f, w, NULL);
    Check(w->state == ST_FLEE, "alone, it limps away (state %d)", w->state);

    for (int i = 0; i < G.animalCount; i++)
        if (G.animals[i].pos.x > 2000) G.animals[i].pos.x -= 3000;
    CalmBeasts(-1);
    G.peers[0].aggro = 0;
    G.health = HEALTH_MAX;
}

// Two bands of bandits, two swordsmen and an archer each, stronger than raiders, roam far from the city
// and the villages, off the world's edge, and rest now and then, never on a road.
static void TestBandits(void)
{
    G.self = G.actor = 0;
    PlaceAt(3, 4, 0);
    CollectFighters();
    int bandits = 0, archers = 0;
    float raiderHp = 0, banditHp = 0;
    for (int i = 0; i < FighterCount(); i++) {
        Vector3 p;
        float hp, max;
        const char *name;
        bool hostile, dead;
        FighterInfo(i, &p, &hp, &max, &name, &hostile, &dead);
        if (strcmp(name, "Raider") == 0) raiderHp = max;
        if (strncmp(name, "Bandit", 6) == 0) {
            bandits++;
            archers += strcmp(name, "Bandit archer") == 0;
            banditHp = max;
        }
    }
    Check(bandits == 6 && archers == 2, "two bands of three bandits, an archer in each (%d bandits, %d archers)", bandits, archers);
    Check(banditHp > raiderHp && raiderHp > 0, "a bandit is tougher than a raider (%.0f against %.0f)", banditHp, raiderHp);

    float nearCity = 1e9f, nearVillage = 1e9f, edge = 0, moved[BANDS] = { 0 }, restRoad = 1e9f, spread = 0;
    int rests = 0;
    Vector3 last[BANDS];
    bool wasStill[BANDS] = { false };
    for (int b = 0; b < BANDS; b++) last[b] = G.raiders[MAX_RAIDERS - BANDS * 3 + b * 3].pos;
    if (getenv("TRACE")) printf("  bands start at (%.1f,%.1f) and (%.1f,%.1f)\n", last[0].x, last[0].z, last[1].x, last[1].z);
    for (float t = 0; t < 900; t += 0.1f) {
        CollectBodies();
        UpdateRaiders(0.1f);
        for (int b = 0; b < BANDS; b++) {
            const Raider *lead = &G.raiders[MAX_RAIDERS - BANDS * 3 + b * 3];
            Vector3 p = lead->pos;
            nearCity = fminf(nearCity, hypotf(p.x, p.z - CITY_Z));
            for (int v = 0; v < VILLAGES; v++) nearVillage = fminf(nearVillage, XZDist(p, VillageCenter(v)));
            edge = fmaxf(edge, fmaxf(fabsf(p.x), fabsf(p.z)));
            moved[b] += XZDist(p, last[b]);
            last[b] = p;
            if (!lead->moving && !wasStill[b]) {
                rests++;
                restRoad = fminf(restRoad, RoadDistance(p.x, p.z));
            }
            wasStill[b] = !lead->moving;
            if (lead->moving)
                for (int k = 1; k < 3; k++) {
                    const Raider *m = &G.raiders[MAX_RAIDERS - BANDS * 3 + b * 3 + k];
                    if (getenv("TRACE") && XZDist(p, m->pos) > 6 && fmodf(t, 0.5f) < 0.1f)
                        printf("  t %.0f band %d lead (%.1f,%.1f) member %d (%.1f,%.1f) water %d solid %d\n", t, b, p.x, p.z, k, m->pos.x, m->pos.z,
                               InWater(m->pos.x, m->pos.z), SolidAt(m->pos.x, m->pos.z, 0.6f));
                    spread = fmaxf(spread, XZDist(p, m->pos));
                }
        }
    }
    Check(moved[0] > 150 && moved[1] > 150, "both bands roam (%.0f m and %.0f m in 15 minutes)", moved[0], moved[1]);
    Check(nearCity > 200 && nearVillage > 100, "far from the city (%.0f m) and the villages (%.0f m)", nearCity, nearVillage);
    Check(edge < WORLD_LIMIT - 50, "away from the edge of the world (%.0f m from the middle at most)", edge);
    Check(rests > 0 && restRoad > 8, "they stop now and then (%d times), never on a road (%.0f m from one at least)", rests, restRoad);
    Check(spread < 12, "the band keeps together on the move (%.1f m apart at most)", spread);

    // Someone walking up to a band is set upon.
    const Raider *lead = &G.raiders[MAX_RAIDERS - BANDS * 3];
    PlaceAt(lead->pos.x + 8, lead->pos.z, 0);
    G.health = HEALTH_MAX;
    for (int k = 0; k < 40; k++) {
        CollectBodies();
        UpdateRaiders(0.05f);
        UpdateCombat(0.05f);
    }
    Check(G.raiders[MAX_RAIDERS - BANDS * 3].vit.hostile > 0 && G.raiders[MAX_RAIDERS - BANDS * 3].vit.target == 0,
          "the bandits go for a player who walks up to them");
    ForgetFights(0);
    RespawnPeople();
    G.health = HEALTH_MAX;
    PlaceAt(3, 4, 0);
}

// The dev console's commands do what they say.
static void TestConsole(void)
{
    G.self = G.actor = 0;
    G.devTools = false;
    G.hour = 12;
    ConsoleRun("hour 22");
    Check(fabsf(G.hour - 12) < 0.01f && strstr(ConsoleLine(ConsoleLines() - 1), "off") != NULL,
          "console: does nothing while dev tools are off (\"%s\")", ConsoleLine(ConsoleLines() - 1));
    G.devTools = true;
    ConsoleRun("tp 120 -40");
    Check(XZDist(G.player.pos, (Vector3){ 120, 0, -40 }) < 0.1f, "console: tp 120 -40 moves you there (%.1f, %.1f)", G.player.pos.x, G.player.pos.z);
    ConsoleRun("tp hunters");
    Check(XZDist(G.player.pos, VillageCenter(VIL_HUNTERS)) < 12, "console: tp hunters takes you to the hunters' village");
    ConsoleRun("tp band2");
    float nearest = 1e9f;
    for (int i = 0; i < MAX_RAIDERS; i++)
        if (RaiderBandit(i)) nearest = fminf(nearest, XZDist(G.raiders[i].pos, G.player.pos));
    Check(nearest < 40, "console: tp band2 puts you within sight of a bandit band (%.0f m)", nearest);
    ConsoleRun("tp 120 -40");

    G.health = 40;
    ConsoleRun("heal");
    Check(G.health == HEALTH_MAX && G.food == FOOD_MAX, "console: heal fills health and food");
    ConsoleRun("god");
    HurtPlayer(30, BY_WOLF);
    Check(G.health == HEALTH_MAX, "console: god, and a bite does nothing (%.0f)", G.health);
    ConsoleRun("god");
    HurtPlayer(10, BY_WOLF);
    Check(G.health < HEALTH_MAX, "console: god again, and it hurts again (%.0f)", G.health);
    G.health = HEALTH_MAX;

    int dates = BagCount(IT_DATES);
    ConsoleRun("give dates 5");
    Check(BagCount(IT_DATES) == dates + 5, "console: give dates 5 (%d -> %d)", dates, BagCount(IT_DATES));
    ConsoleRun("hour 22");
    Check(fabsf(G.hour - 22) < 0.01f, "console: hour 22 (%.2f)", G.hour);
    ConsoleRun("hour 12");

    G.player.yaw = 0;
    ConsoleRun("spawn wolf 3");
    int near = 0;
    for (int i = 0; i < G.animalCount; i++)
        near += G.animals[i].type == AN_WOLF && G.animals[i].active && G.animals[i].state != ST_DEAD && XZDist(G.animals[i].pos, G.player.pos) < 20;
    Check(near >= 3, "console: spawn wolf 3 brings three wolves near you (%d)", near);
    ConsoleRun("kill 25");
    near = 0;
    for (int i = 0; i < G.animalCount; i++)
        near += G.animals[i].type == AN_WOLF && G.animals[i].active && G.animals[i].state != ST_DEAD && XZDist(G.animals[i].pos, G.player.pos) < 20;
    Check(near == 0 && strstr(ConsoleLine(ConsoleLines() - 1), "beasts") != NULL, "console: kill 25 brings them down (\"%s\")",
          ConsoleLine(ConsoleLines() - 1));
    ConsoleRun("nonsense");
    Check(strstr(ConsoleLine(ConsoleLines() - 1), "Unknown") != NULL, "console: an unknown command says so");
    ConsoleRun("speed 1");
    G.devSpeed = 0;
    G.devTools = false;
    RespawnPeople();
    G.health = HEALTH_MAX;
    PlaceAt(3, 4, 0);
}

// The ziggurat is lived in: people at work in its rooms, and the guards off duty stay in a PvP game.
static void TestTempleLife(void)
{
    int inside = 0, resting = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (InTempleInterior(v->a)) inside++;
        resting += v->kind == NPC_GUARD && v->crouch && v->a.y > 5.0f;
    }
    Check(inside >= 12 && resting == 2, "%d people inside the ziggurat, %d guards off duty in their room", inside, resting);
    CollectFighters();
    Check(G.villagerCount < MAX_VILLAGERS && FighterCount() < MAX_FIGHTERS, "room to spare: %d of %d people, %d of %d fighters", G.villagerCount,
          MAX_VILLAGERS, FighterCount(), MAX_FIGHTERS);
    static Villager saved[MAX_VILLAGERS];
    int before = G.villagerCount;
    memcpy(saved, G.villagers, sizeof(saved));
    RemoveZigguratGuards();
    int kept = 0;
    for (int i = 0; i < G.villagerCount; i++) kept += G.villagers[i].kind == NPC_GUARD && G.villagers[i].crouch;
    Check(before - G.villagerCount == 3 && kept == 2, "PvP takes the three guards on duty (%d gone) and leaves the two at supper", before - G.villagerCount);
    memcpy(G.villagers, saved, sizeof(saved));
    G.villagerCount = before;
}

// The torch: it takes a second to take up, goes down while weapons change, burns only after dark, others see
// it and it lights the way; a woman's body has no beard.
// Bark, leaves, tilled earth and straw are tagged for the shaders' grain; the open land has its
// shrubs, dead wood and stones, none of them in the way, and the world still fits its boxes.
static void TestWilds(void)
{
    int tag[5] = { 0 }, wood = 0, solidWild = 0;
    for (int i = 0; i < G.boxCount; i++) {
        const Box *b = &G.boxes[i];
        if (b->water || b->glow || b->color.a < 251 || b->color.a > 254) continue;
        tag[255 - b->color.a]++;
        if (b->color.a == 254 && b->size.y < 0.7f && !InsideWalls(b->pos.x, b->pos.z, 0)) wood++;
        if (b->solid && b->color.a == 253 && b->shape == SHAPE_CONE) solidWild++;
    }
    Check(tag[1] > 100 && tag[2] > 300 && tag[3] > 20 && tag[4] > 40, "tagged bark %d, leaves %d, earth %d, straw %d", tag[1], tag[2], tag[3], tag[4]);
    Check(wood >= 60, "fallen wood out on the land: %d", wood);
    Check(solidWild == 0, "shrubs do not block the way (%d do)", solidWild);
    Check(G.boxCount < MAX_BOXES - 1500, "boxes %d of %d", G.boxCount, MAX_BOXES);
}

// The loops of places run on with no click where they start again, and every footstep has a sound.
static void TestSounds(void)
{
    SfxExport(GetDirectoryPath(GetApplicationDirectory()));
    const char *dir = GetDirectoryPath(GetApplicationDirectory());
    for (int k = 0; k < 2; k++) {
        Wave w = LoadWave(TextFormat("%s/%s.wav", dir, k ? "village" : "city"));
        const short *s = w.data;
        int n = (int)w.frameCount;
        double typical = 0;
        for (int i = 1; i < n; i++) typical += abs(s[i] - s[i - 1]);
        typical /= n > 1 ? n - 1 : 1;
        Check(n > 0 && (float)n / (float)w.sampleRate > 7.5f && abs(s[0] - s[n - 1]) < 3.0 * typical + 50,
              "%s loop: %.1f s, seam %d against a usual step of %.0f", k ? "village" : "city", n > 0 ? (float)n / (float)w.sampleRate : 0.0f,
              n > 0 ? abs(s[0] - s[n - 1]) : -1, typical);
        UnloadWave(w);
    }
    for (int id = SFX_STEP_GRASS; id <= SFX_STEP_WATER; id++) {
        static const char *names[] = { "step-grass", "step-earth", "step-hard", "step-water" };
        Wave w = LoadWave(TextFormat("%s/%s.wav", dir, names[id - SFX_STEP_GRASS]));
        Check(w.frameCount > 1000, "%s: %u samples", names[id - SFX_STEP_GRASS], w.frameCount);
        UnloadWave(w);
    }
}

// Coveys of partridges live in the woods; walk up to one and it bursts up and away, and it is game.
static void TestPartridges(void)
{
    int n = 0;
    Animal *bird = NULL;
    for (int i = 0; i < G.animalCount; i++)
        if (G.animals[i].active && G.animals[i].type == AN_PARTRIDGE) {
            n++;
            if (!bird) bird = &G.animals[i];
        }
    Check(n == 12 && CarcassItem(AN_PARTRIDGE) == IT_PARTRIDGE, "%d partridges in the woods, game to carry home", n);
    if (!bird) return;
    Calm(bird);
    PlaceAt(bird->pos.x + 5, bird->pos.z, 0);
    G.player.moving = G.player.running = true;
    float top = 0;
    for (int k = 0; k < 60; k++) {
        Wild(0.05f);
        top = fmaxf(top, bird->pos.y - TerrainHeight(bird->pos.x, bird->pos.z));
    }
    Check(top > 1.0f, "a partridge flushed from 5 m flies up (%.1f m) (state %d)", top, bird->state);
    G.player.moving = G.player.running = false;
    Calm(bird);
}

static void TestTorch(void)
{
    G.self = G.actor = 0;
    PlaceAt(3, 4, 0);
    SetEquip(EQ_NONE);
    G.night = G.torchGlow = 1.0f;
    G.torchOn = true;
    G.torchHeld = false;
    G.torchT = 0;
    UpdatePlayer(0.5f, false);
    bool early = G.torchHeld;
    UpdatePlayer(0.6f, false);
    Check(!early && G.torchHeld, "torch: in hand after a second, not before (%d then %d)", early, G.torchHeld);
    SyncSelf();
    Check((G.peers[0].flags & PF_TORCH) != 0, "torch: other players are told it burns");
    float daylight = G.daylight;
    G.daylight = 0;
    UpdateLights();
    G.daylight = daylight;
    Check(G.dynCount >= 1, "torch: it lights the way (%d lights)", G.dynCount);
    SetEquip(EQ_MELEE);
    bool down = !G.torchHeld;
    UpdatePlayer(1.05f, false);
    Check(down && G.torchHeld, "torch: down while the sword is drawn, back a second later");
    G.torchOn = false;
    UpdatePlayer(0.5f, false);
    bool still = G.torchHeld;
    UpdatePlayer(0.6f, false);
    Check(still && !G.torchHeld, "torch: put away takes a second too");
    G.night = G.torchGlow = 0;
    G.torchOn = true;
    G.torchHeld = true;
    Check(!TorchBurning(), "torch: by day it does not burn");
    // Lit from five in the evening, brighter as the dusk deepens.
    float hour = G.hour;
    daylight = G.daylight;
    G.daylight = 0.9f;
    G.hour = 16.5f;
    UpdateLights();
    bool afternoon = TorchBurning();
    G.hour = 17.2f;
    UpdateLights();
    float evening = G.torchGlow;
    G.hour = 19.5f;
    UpdateLights();
    Check(!afternoon && TorchBurning() && evening >= 0.15f && G.torchGlow > evening + 0.3f,
          "torch: lit from the evening (%.2f at 17:12), brighter at dusk (%.2f at 19:30)", evening, G.torchGlow);
    G.hour = hour;
    G.daylight = daylight;
    UpdateLights();
    Look w = PlayerLook(1, PackStyle(HAIR_BUN, BEARD_LONG, DRESS_ROBE, JEWEL_GOLD) | STYLE_WOMAN, 0);
    Look m = PlayerLook(1, PackStyle(HAIR_BUN, BEARD_LONG, DRESS_ROBE, JEWEL_GOLD), 0);
    Check(w.woman && w.beard == BEARD_NONE && !m.woman && m.beard == BEARD_LONG, "body: a woman's body (no beard) and a man's from the same choices");
    // The bow is drawn at once: the torch leaves the hand that moment and comes back a second after.
    G.night = G.torchGlow = 1.0f;
    SetEquip(EQ_BOW);
    G.torchHeld = true;
    G.player.aiming = true;
    UpdatePlayer(0.02f, false);
    bool dropped = !G.torchHeld;
    UpdatePlayer(0.5f, false);
    early = G.torchHeld;
    UpdatePlayer(0.6f, false);
    Check(dropped && !early && G.torchHeld, "torch: dropped the moment the bow is drawn, back a second after (%d %d %d)", dropped, early, G.torchHeld);
    G.night = G.torchGlow = 0;
    G.torchHeld = false;
}

// The law: the city's guards keep the peace inside the canals only; a village stands together for two
// minutes; caravan and city guards do not chase far from their posts.
static void TestLaw(void)
{
    G.self = G.actor = 0;
    G.health = HEALTH_MAX;
    RespawnPeople();

    // In the city, by the temple gate's guards.
    int t = VillagerWhere(NPC_CIVILIAN, Anywhere);
    Stand(t, 3, 2);
    PlaceAt(3, 4, 0);
    CollectFighters();
    DamagePerson(FighterByOwner(OWNER_VILLAGER + t), 5);
    Check(G.peers[0].wanted > 0, "striking a townsman by the temple gate: wanted by the guards (\"%s\")", G.msg);
    RunCombat(3);
    int chasing = 0;
    for (int i = 0; i < G.villagerCount; i++)
        chasing += G.villagers[i].kind == NPC_GUARD && G.villagers[i].vit.hostile > 0 && G.villagers[i].vit.target == 0 && G.villagers[i].vit.override;
    Check(chasing >= 2, "%d guards come for you", chasing);

    // The guards will not follow past the canals.
    PlaceAt(0, 97, 0);
    G.peers[0].wanted = 100;
    G.health = HEALTH_MAX;
    RunCombat(25);
    bool kept = true;
    for (int i = 0; i < G.villagerCount; i++)
        if (G.villagers[i].kind == NPC_GUARD && G.villagers[i].vit.override && !InCityLands(G.villagers[i].vit.at.x, G.villagers[i].vit.at.z)) kept = false;
    Check(kept && G.health == HEALTH_MAX, "wanted but past the south canal: the guards stay in the city's lands");
    RespawnPeople();

    // Out on the east road no one tells the city.
    int tr = VillagerWhere(NPC_TRAVELLER, FarEast);
    if (tr < 0) { Check(false, "no traveller out east"); return; }
    Vector3 tp = VillagerPos(&G.villagers[tr]);
    Stand(tr, tp.x, tp.z);
    PlaceAt(tp.x + 1.5f, tp.z, 0);
    CollectFighters();
    DamagePerson(FighterByOwner(OWNER_VILLAGER + tr), 5);
    RunCombat(1);
    int guards = 0;
    for (int i = 0; i < G.villagerCount; i++) guards += G.villagers[i].kind == NPC_GUARD && G.villagers[i].vit.hostile > 0;
    Check(G.peers[0].wanted <= 0 && guards == 0, "striking a traveller out east: not wanted, no guard stirs (\"%s\")", G.msg);
    RespawnPeople();

    // A village stands together, for two minutes.
    int vv = VillagerWhere(NPC_VILLAGER, AtHunters);
    if (vv < 0) { Check(false, "no villager in the hunters' village"); return; }
    Vector3 vp = VillagerPos(&G.villagers[vv]);
    PlaceAt(vp.x + 1.2f, vp.z, 0);
    CollectFighters();
    DamagePerson(FighterByOwner(OWNER_VILLAGER + vv), 5);
    int roused = 0, folk = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        if (G.villagers[i].kind != NPC_VILLAGER || VillageAt(VillagerLanePos(&G.villagers[i])) != VIL_HUNTERS) continue;
        folk++;
        roused += G.villagers[i].vit.hostile > 100 && G.villagers[i].vit.target == 0;
    }
    Check(roused == folk && folk >= 3 && G.peers[0].wanted <= 0, "the whole hunters' village turns on you: %d of %d, not wanted in the city (\"%s\")", roused, folk, G.msg);
    float taken = 0;
    for (int k = 0; k < 20; k++) {
        G.health = HEALTH_MAX;
        RunCombat(0.5f);
        taken += HEALTH_MAX - G.health;
    }
    Check(taken > 0, "the villagers fight you (%.0f damage in 10 s)", taken);
    PlaceAt(vp.x + 200, vp.z, 0);
    RunCombat(115);
    int still = 0;
    for (int i = 0; i < G.villagerCount; i++) still += G.villagers[i].kind == NPC_VILLAGER && G.villagers[i].vit.hostile > 0;
    Check(still == 0, "two minutes on, the village calms down (%d still angry)", still);
    G.health = HEALTH_MAX;
    RespawnPeople();

    // Caravan guards keep by their caravan.
    UpdateCaravans(0.0f);
    for (int k = 0; k < 400; k++) UpdateCaravans(0.05f);
    int c = -1;
    for (int k = 0; k < MAX_CARAVANS && c < 0; k++)
        if (G.caravans[k].state != CV_AWAY) c = k;
    if (c < 0) { Check(false, "no caravan on the road"); return; }
    Vector3 home;
    float face;
    CaravanMemberScript(c, 3, &home, &face);
    CollectFighters();
    Vitals *gv = &G.caravans[c].vit[1];
    gv->hostile = 60;
    gv->target = 0;
    PlaceAt(home.x + 5, home.z, 0);
    RunCombat(2);
    Check(gv->override && XZDist(gv->at, home) > 1.0f, "a caravan guard goes for you");
    PlaceAt(home.x + 45, home.z, 0);
    RunCombat(12);
    Check(XZDist(gv->at, home) < 15.0f || !gv->override, "you run off: it stays by its caravan (%.1f m from its place)", XZDist(gv->at, home));
    RespawnPeople();
    G.health = HEALTH_MAX;
}

// Brought down: the day's gains not put away at home are lost, what the player woke with is kept,
// and the message says who did it.
static void TestDeathLosses(void)
{
    G.role = NET_SOLO;
    G.self = G.actor = 0;
    for (int i = 0; i < BAG_MAX; i++) G.bag[i] = (Slot){ IT_NONE, 0 };
    BagAdd(IT_DATES, 3);
    G.safeDay = 0;
    TrackGoods();
    BagAdd(IT_DATES, 2);
    BagAdd(IT_COPPER, 2);
    TrackGoods();
    for (int i = 0; i < BAG_MAX; i++)
        if (G.bag[i].item == IT_COPPER) MoveStack(&G.bag[i], G.store, STORE_SLOTS);
    TrackGoods();
    BagAdd(IT_COPPER, 1);
    int storedCopper = SlotsCount(G.store, STORE_SLOTS, IT_COPPER), wood = 0;
    for (int i = 0; i < STORE_SLOTS; i++)
        if (G.store[i].item == IT_WOOD) {
            int moved = MoveStack(&G.store[i], G.bag, G.bagSlots);
            G.safe[IT_WOOD] += moved;
            wood += moved;
        }
    TrackGoods();
    G.sleeping = false;
    G.health = 5;
    HurtPlayer(50, BY_VILLAGERS);
    Check(BagCount(IT_DATES) == 3 && BagCount(IT_COPPER) == 0 && BagCount(IT_WOOD) == wood && wood > 0 &&
          SlotsCount(G.store, STORE_SLOTS, IT_COPPER) == storedCopper,
          "brought down: today's 2 dates and 1 copper lost, the morning's 3 dates, stored copper and wood from storage kept (\"%s\")", G.msg);
    Check(strstr(G.msg, "villagers") != NULL && strstr(G.msg, "guards") == NULL, "the villagers did it, not the guards");
    G.sleeping = false;
    G.health = HEALTH_MAX;
}


// A townsman across water from home walks back: over a canal with a jump, round the harbor basin.
static bool WalkHome(int t, Vector3 home, Vector3 start, Vector3 watcher, float seconds, bool *jumped, float *left)
{
    Villager *v = &G.villagers[t];
    v->from = v->to = -1;
    v->speed = 0;
    v->a = v->b = home;
    v->side = 0;
    CollectFighters();
    v->vit.override = true;
    v->vit.hostile = 0;
    v->vit.at = (Vector3){ start.x, GroundHeight(start.x, start.z, 2.0f), start.z };
    PlaceAt(watcher.x, watcher.z, 0);
    *jumped = false;
    for (float s = 0; s < seconds && v->vit.override; s += 0.05f) {
        SyncSelf();
        CollectBodies();
        UpdateCombat(0.05f);
        *jumped = *jumped || v->vit.jumpDur > 0;
        if (getenv("TRACE") && fmodf(s, 2.0f) < 0.05f) printf("  t %.0f at (%.1f,%.1f)\n", s, v->vit.at.x, v->vit.at.z);
    }
    *left = v->vit.override ? XZDist(v->vit.at, home) : 0.0f;
    return !v->vit.override;
}

static void TestCanals(void)
{
    RespawnPeople();
    int t = VillagerWhere(NPC_CIVILIAN, Anywhere);
    Villager keep = G.villagers[t];
    bool jumped;
    float left;
    bool home = WalkHome(t, (Vector3){ 6, 0, 80 }, (Vector3){ 8, 0, 96 }, (Vector3){ 12, 0, 99 }, 15, &jumped, &left);
    Check(home && jumped, "a townsman south of the south canal jumps it on his way home (%s, %.1f m left)", jumped ? "jumped" : "no jump", left);
    home = WalkHome(t, (Vector3){ -54.5f, 0, -10 }, (Vector3){ -80, 0, -30 }, (Vector3){ -82, 0, -32 }, 60, &jumped, &left);
    Check(home, "one west of the harbor basin walks round it to the quay (%.1f m left)", left);
    G.villagers[t] = keep;
    RespawnPeople();
}


// The guest (slot 1) swings at the host where both stand; returns the host's health lost.
// A blow on another player takes PVP_WINDUP to land; this lets it (and anything else going on) play out.
static void Settle(float seconds)
{
    for (float t = 0; t < seconds; t += 0.05f) {
        SyncSelf();
        UpdateCombat(0.05f);
    }
}

// Both halves of a swing at once: its start (a blow at a player is set going) and its contact.
static bool WholeSwing(Vector3 chest, Vector3 fwd, int equip, int weapon)
{
    StrikeFor(chest, fwd, equip, weapon, STRIKE_START);
    return StrikeFor(chest, fwd, equip, weapon, STRIKE_CONTACT);
}

// The guest swings at the host (who is placed at hx, hz); the blow is on its way, not yet landed.
static void GuestSwing(float gx, float gz, float hx, float hz, int weapon)
{
    G.peers[1].pos = (Vector3){ gx, TerrainHeight(gx, gz), gz };
    G.peers[1].flags = (G.peers[1].flags & PF_PARRY) | PF_GROUND;
    PlaceAt(hx, hz, 0);
    SyncSelf();
    G.sleeping = false;
    G.health = HEALTH_MAX;
    Vector3 fwd = Vector3Normalize((Vector3){ hx - gx, 0, hz - gz }), chest = { gx, G.peers[1].pos.y + 1.0f, gz };
    G.actor = 1;
    WholeSwing(chest, fwd, EQ_MELEE, weapon);
    G.actor = 0;
}

static float GuestBlow(float gx, float gz, float hx, float hz, int weapon)
{
    G.peers[1].flags = PF_GROUND;
    GuestSwing(gx, gz, hx, hz, weapon);
    Settle(PVP_WINDUP + 0.3f);
    return HEALTH_MAX - G.health;
}

// Players fighting players, as the host sees it: the host's rules, the guards, parries, arrows.
static void TestPvp(void)
{
    G.role = NET_HOST;
    G.self = G.actor = 0;
    G.peers[1] = (Peer){ .active = true, .net = 1, .health = HEALTH_MAX, .flags = PF_GROUND, .equip = EQ_MELEE };
    snprintf(G.peers[1].name, NAME_LEN, "Guest");
    RespawnPeople();
    ForgetFights(0);
    ForgetFights(1);
    G.armor = ARMOR_NONE;
    G.combat = false;

    G.pvp = false;
    Check(GuestBlow(3, 6, 3, 4.5f, 2) == 0, "PvP off: the guest's blow does the host no harm");

    G.pvp = true;
    float lost = GuestBlow(3, 6, 3, 4.5f, 2);
    Check(lost > 0 && G.hurtBy == BY_PLAYER + 1, "PvP on: the guest's sword hurts the host (%.0f)", lost);
    Check(G.peers[1].wanted > 0 && G.peers[0].wanted <= 0, "the guest started it by the temple gate: the guest is wanted, the host is not");
    RunCombat(3);
    int after = 0;
    for (int i = 0; i < G.villagerCount; i++)
        after += G.villagers[i].kind == NPC_GUARD && G.villagers[i].vit.hostile > 0 && G.villagers[i].vit.target == 1;
    Check(after >= 1, "%d guards go after the guest", after);

    // The host hits back: self-defence.
    G.actor = 0;
    Vector3 hc = { 3, TerrainHeight(3, 4.5f) + 1.0f, 4.5f };
    WholeSwing(hc, (Vector3){ 0, 0, 1 }, EQ_MELEE, 0);
    Settle(PVP_WINDUP + 0.3f);
    Check(G.peers[0].wanted <= 0, "the host hits back: no crime");

    // A parry turns the blow aside.
    G.equip = EQ_MELEE;
    G.parryT = 1.0f;
    G.player.facing = 0.0f;   // facing +z, toward the guest
    float parried = GuestBlow(3, 6, 3, 4.5f, 2);
    G.player.facing = 0.0f;
    G.parryT = 0;
    Check(parried == 0, "a raised sword turns the guest's blow aside");

    // An arrow from the guest.
    G.health = HEALTH_MAX;
    PlaceAt(3, 4.5f, 0);
    SyncSelf();
    G.peers[1].pos = (Vector3){ 3, TerrainHeight(3, 16), 16 };
    Vector3 from = { 3, G.peers[1].pos.y + 1.5f, 15.3f }, to = { 3, G.player.pos.y + 1.1f, 4.5f };
    ShootFor(1, from, Vector3Scale(Vector3Normalize(Vector3Subtract(to, from)), 50.0f), 30);
    for (int k = 0; k < 100; k++) { SyncSelf(); UpdateArrows(0.01f); }
    Check(G.health < HEALTH_MAX, "the guest's arrow strikes the host (health %.0f)", G.health);

    // Brought down by a player: the message says by whom.
    RespawnPeople();
    GuestBlow(200, 1.5f, 200, 0, 2);
    G.health = 3;
    G.sleeping = false;
    G.actor = 1;
    WholeSwing((Vector3){ 200, TerrainHeight(200, 1.5f) + 1.0f, 1.5f }, (Vector3){ 0, 0, -1 }, EQ_MELEE, 2);
    G.actor = 0;
    Settle(PVP_WINDUP + 0.3f);
    Check(strstr(G.msg, "Guest beat you down") != NULL, "brought down by the guest: \"%s\"", G.msg);
    Check(G.peers[1].kills == 1 && G.peers[0].deaths == 1 && G.peers[0].kills == 0, "a kill for the guest, a death for the host: %s / %s",
          KillDeath(1), KillDeath(0));

    // The war mace cannot parry, but hits twice as hard as before (60).
    RespawnPeople();
    G.sleeping = false;
    G.equip = EQ_MACE;
    G.parryT = 1.0f;
    G.player.facing = 0.0f;
    float through = GuestBlow(200, 1.5f, 200, 0, 2);
    G.parryT = 0;
    G.equip = EQ_MELEE;
    Check(through > 0, "a raised war mace does not turn a blow aside (%.0f damage)", through);
    G.health = HEALTH_MAX;
    G.armor = ARMOR_NONE;
    G.actor = 1;
    G.peers[1].pos = (Vector3){ 200, TerrainHeight(200, 1.5f), 1.5f };
    PlaceAt(200, 0, 0);
    SyncSelf();
    G.sleeping = false;
    WholeSwing((Vector3){ 200, TerrainHeight(200, 1.5f) + 1.0f, 1.5f }, (Vector3){ 0, 0, -1 }, EQ_MACE, 0);
    G.actor = 0;
    Settle(PVP_WINDUP + 0.3f);
    Check(fabsf((HEALTH_MAX - G.health) - 30.0f) < 0.5f, "the war mace's blow on a player: 60, halved between players (%.0f)", HEALTH_MAX - G.health);
    G.health = HEALTH_MAX;
    G.sleeping = false;
    RespawnPeople();
    ForgetFights(0);
    ForgetFights(1);

    // Out past the canals nobody calls the guards.
    GuestBlow(200, 1.5f, 200, 0, 1);
    Check(G.peers[1].wanted <= 0, "a fight far out east: no one is wanted");

    // Combat mode: the best of everything, unlimited arrows, and the guards keep out of it.
    ForgetFights(0);
    ForgetFights(1);
    G.combat = true;
    int bow = G.bow;
    lost = GuestBlow(3, 6, 3, 4.5f, 2);
    PlayerRecord r;
    CaptureRecord(&r);
    Check(lost > 0 && G.peers[1].wanted <= 0, "combat mode: the blow lands and no one is wanted (%.0f)", lost);
    Check(UseBow() == 2 && UseWeapon() == 2 && UseArmor() == ARMOR_BRONZE && UseMace() && r.bow == bow,
          "combat mode: best bow, sword, bronze armor and the war mace, while the record keeps the player's own bow");
    G.quiver = 3;
    G.equip = EQ_BOW;
    G.player.aiming = true;
    G.player.draw = 1.0f;
    G.cursorLocked = true;
    UpdatePlayer(0.01f, true);
    Check(G.quiver == 3, "combat mode: a shot takes no arrow from the quiver (%d left)", G.quiver);
    G.combat = false;
    G.pvp = true;
    G.peers[1] = (Peer){ 0 };
    G.health = HEALTH_MAX;
    G.role = NET_SOLO;
    RespawnPeople();
}


// Runs combat and the arrows for 'seconds', keeping the player alive; returns the damage taken and,
// in *moved, whether 'watch' moved while its bow was drawn.
static float UnderFire(float seconds, Vitals *watch, bool *movedDrawing, int *by)
{
    float taken = 0;
    bool wasDrawing = false;
    Vector3 last = watch ? watch->at : (Vector3){ 0 };
    if (movedDrawing) *movedDrawing = false;
    *by = BY_NONE;
    for (float t = 0; t < seconds; t += 0.02f) {
        G.health = HEALTH_MAX;
        G.sleeping = false;
        SyncSelf();
        CollectBodies();
        UpdateCombat(0.02f);
        UpdateArrows(0.02f);
        if (G.health < HEALTH_MAX) { taken += HEALTH_MAX - G.health; *by = G.hurtBy; }
        if (watch) {
            if (watch->drawing && wasDrawing && XZDist(watch->at, last) > 0.001f && movedDrawing) *movedDrawing = true;
            last = watch->at;
            wasDrawing = watch->drawing;
        }
    }
    G.health = HEALTH_MAX;
    return taken;
}

static void TestArchers(void)
{
    G.role = NET_SOLO;
    G.self = G.actor = 0;
    RespawnPeople();
    for (int i = 0; i < MAX_ARROWS; i++) G.arrows[i].active = false;
    int by;
    bool moved;

    // The fishers' village guard, when the village turns on you, shoots from where it stands.
    int w = -1;
    for (int i = 0; i < G.villagerCount && w < 0; i++)
        if (G.villagers[i].kind == NPC_WARDEN && VillageAt(G.villagers[i].a) == VIL_FISHERS) w = i;
    int folk = VillagerWhere(NPC_VILLAGER, Anywhere);
    for (int i = 0; i < G.villagerCount; i++)
        if (G.villagers[i].kind == NPC_VILLAGER && VillageAt(G.villagers[i].a) == VIL_FISHERS) folk = i;
    if (w < 0) { Check(false, "no village guard at the fishers' village"); return; }
    Vector3 wp = G.villagers[w].a, fp = VillagerPos(&G.villagers[folk]);
    PlaceAt(fp.x + 1.2f, fp.z, 0);
    CollectFighters();
    DamagePerson(FighterByOwner(OWNER_VILLAGER + folk), 5);
    PlaceAt(wp.x - 16, wp.z + 4, 0);
    float taken = UnderFire(12, &G.villagers[w].vit, &moved, &by);
    Check(taken > 0 && by == BY_VILLAGERS && !moved, "the fishers' village guard shoots you from 16 m (%.0f damage in 12 s), standing still while it draws", taken);
    RespawnPeople();

    // The south gate's guards: after you, but you are over the canal, out of their reach.
    PlaceAt(0, 97, 0);
    for (int i = 0; i < G.villagerCount; i++) {
        Villager *v = &G.villagers[i];
        if (v->kind == NPC_GUARD && v->bow > 0 && v->a.z > 60) {
            CollectFighters();
            v->vit.hostile = 45;
            v->vit.target = 0;
        }
    }
    G.peers[0].wanted = 100;
    taken = UnderFire(12, NULL, NULL, &by);
    bool kept = true;
    for (int i = 0; i < G.villagerCount; i++)
        if (G.villagers[i].kind == NPC_GUARD && G.villagers[i].vit.override && !InCityLands(G.villagers[i].vit.at.x, G.villagers[i].vit.at.z)) kept = false;
    Check(taken > 0 && by == BY_GUARD && kept, "over the south canal, the gate guards shoot at you from the city's side (%.0f damage in 12 s)", taken);
    G.peers[0].wanted = 0;
    RespawnPeople();

    // A barbarian camp's bowman.
    int bowman = -1;
    for (int i = 0; i < MAX_RAIDERS && bowman < 0; i++)
        if (RaiderBowman(i) && !RaiderBandit(i)) bowman = i;
    int bowmen = 0;
    for (int i = 0; i < MAX_RAIDERS; i++) bowmen += RaiderBowman(i) && !RaiderBandit(i);
    Raider *b = &G.raiders[bowman];
    Vector3 c = CampCenter(b->camp);
    PlaceAt(c.x + 22, c.z, 0);
    taken = UnderFire(10, &b->vit, &moved, &by);
    Check(bowmen == 2 && taken > 0 && by == BY_BARBARIAN, "%d barbarian bowmen; one shoots you from 22 m (%.0f damage in 10 s)", bowmen, taken);
    for (int i = 0; i < MAX_RAIDERS; i++) G.raiders[i].vit.hostile = 0;
    RespawnPeople();
    for (int i = 0; i < MAX_ARROWS; i++) G.arrows[i].active = false;
    PlaceAt(11, 24, 0);
}


// The fishers' village across the river: its people, its fisher's trades, and the fine net.
static void TestFishers(void)
{
    G.role = NET_SOLO;
    G.self = G.actor = 0;
    Check(VillageAt(G.traders[TR_FISHER].pos) == VIL_FISHERS && G.traders[TR_FISHER].offerCount >= 5 &&
          XZDist(G.traders[TR_FISHER].pos, G.stations[STN_FERRY_W]) < 60.0f,
          "the fishers' village stands across the river from the city, its fisher with %d offers", G.traders[TR_FISHER].offerCount);
    int people = 0, wardens = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        if (VillageAt(G.villagers[i].a) != VIL_FISHERS) continue;
        people += G.villagers[i].kind == NPC_VILLAGER;
        wardens += G.villagers[i].kind == NPC_WARDEN && G.villagers[i].bow > 0;
    }
    int allWardens = 0;
    for (int i = 0; i < G.villagerCount; i++) allWardens += G.villagers[i].kind == NPC_WARDEN && G.villagers[i].bow > 0;
    Check(people >= 2 && wardens == 1 && allWardens == 3, "%d fisherfolk and a village guard with a bow (one in each of the %d villages)", people, allWardens);

    // A cast of the net: 2 fish, 4 with the fine net.
    int r = -1;
    for (int i = 0; i < G.resCount && r < 0; i++)
        if (G.res[i].kind == RES_FISH && VillageAt(G.res[i].pos) == VIL_FISHERS) r = i;
    if (r < 0) { Check(false, "no fishing spot by the fishers' village"); return; }
    for (int i = 0; i < BAG_MAX; i++) G.bag[i] = (Slot){ IT_NONE, 0 };
    G.food = FOOD_MAX;
    G.tools &= ~TOOL_NET;
    PlaceAt(G.res[r].pos.x - 3, G.res[r].pos.z, 0);
    G.res[r].regrow = 0;
    Act(RQ_GATHER, &r, 1);
    int plain = BagCount(IT_FISH);
    BagAdd(IT_REEDS, 8);
    BagAdd(IT_COPPER, 2);
    Barter(TR_FISHER, 0);
    bool net = (G.tools & TOOL_NET) != 0;
    G.res[r].regrow = 0;
    Act(RQ_GATHER, &r, 1);
    Check(plain == 2 && net && BagCount(IT_FISH) == 2 + 4, "a cast brings 2 fish; with the fisher's fine net (8 reeds, 2 copper ore) 4 (%d in all)", BagCount(IT_FISH));
    G.tools &= ~TOOL_NET;
    for (int i = 0; i < BAG_MAX; i++) G.bag[i] = (Slot){ IT_NONE, 0 };
    PlaceAt(11, 24, 0);
}


static void TestSave(void)
{
    SetSaveSlot(2);
    G.hasMace = true;
    G.equip = EQ_MACE;
    int far = G.animalCount - 1;
    G.animals[far].active = false;
    G.animals[far].respawn = -1.0f;
    G.day = 7;
    Check(SaveGame(), "saved to %s", SaveFileName(2));
    GameInit();
    Check(LoadGame(), "loaded it back");
    Check(G.hasMace && G.equip == EQ_MACE, "the war mace is kept, and in hand");
    Check(!G.animals[far].active && G.animals[far].respawn < 0, "animal #%d (past the old 64-animal limit) waits for spring", far);
    Check(G.day == 7, "day %d", G.day);
}

bool SnapshotRoundTrip(int *size);

// A host with a guest in slot 1. There is no network here: what the host sends the guest goes nowhere.
static void TestHostWorld(void)
{
    G.role = NET_HOST;
    G.self = G.actor = 0;
    G.peers[1] = (Peer){ .active = true, .net = 1, .health = HEALTH_MAX, .flags = PF_GROUND };
    snprintf(G.peers[1].name, NAME_LEN, "Guest");
    PlaceAt(11, 24, 0);

    // Wolves go for the guest alone out on the plain, not for the host at home.
    Animal *w = NULL;
    for (int i = 0; i < G.animalCount && !w; i++)
        if (G.animals[i].active && G.animals[i].type == AN_WOLF && G.animals[i].pos.x > RIVER_X1 + 20 &&
            !PeopleNear((Vector3){ G.animals[i].pos.x + 10, 0, G.animals[i].pos.z }, SAFE_RADIUS)) w = &G.animals[i];
    if (!w) { Check(false, "no wolf east of the river away from people"); return; }
    Calm(w);
    G.peers[1].pos = (Vector3){ w->pos.x + 10, TerrainHeight(w->pos.x + 10, w->pos.z), w->pos.z };
    G.peers[1].aggro = 0;
    G.health = HEALTH_MAX;
    for (int k = 0; k < 50; k++) {
        CollectBodies();
        UpdateCombat(0.02f);
        UpdatePlayerSafety(0.02f);
        Wild(0.02f);
    }
    Check(G.peers[1].alone && !G.peers[0].alone, "the guest is alone outside the walls, the host at home is not");
    Check(w->state == ST_HUNT && w->target == 1, "the pack hunts the guest (state %d, target %d)", w->state, w->target);
    Check(G.health == HEALTH_MAX, "the host is left alone");
    CalmBeasts(1);
    Check(w->state == ST_FLEE, "a guest brought down: the pack gives up (state %d)", w->state);

    // Snakes and boars watch whichever player is nearest.
    Animal *sn = First(AN_SNAKE);
    if (sn) {
        Calm(sn);
        sn->lair = sn->pos;
        G.peers[1].pos = (Vector3){ sn->pos.x + 2.0f, sn->pos.y, sn->pos.z };
        G.peers[1].flags = PF_GROUND;
        Wild(0.02f);
        Check(sn->state == ST_ALERT && sn->target == 1, "a snake rears at the guest beside it (state %d)", sn->state);
        G.peers[1].pos.x += 30;
        for (int k = 0; k < 300; k++) Wild(0.02f);
    }

    // Requests: the host's half refuses what is out of reach or already gone.
    int r = -1;
    for (int i = 0; i < G.resCount && r < 0; i++)
        if (G.res[i].kind == RES_REEDS && G.res[i].regrow <= 0) r = i;
    int res[REQ_ARGS], nres;
    G.peers[1].pos = (Vector3){ G.res[r].pos.x + 30, 0, G.res[r].pos.z };
    Check(!ServeRequest(1, RQ_GATHER, &r, 1, res, &nres), "cutting reeds from 30 m away is refused");
    G.peers[1].pos = (Vector3){ G.res[r].pos.x + 1, 0, G.res[r].pos.z };
    Check(ServeRequest(1, RQ_GATHER, &r, 1, res, &nres) && G.res[r].regrow > 0, "next to them the guest may cut the reeds");
    Check(!ServeRequest(1, RQ_GATHER, &r, 1, res, &nres), "the same reeds cannot be cut twice");
    int bad = 99999;
    Check(!ServeRequest(1, RQ_GATHER, &bad, 1, res, &nres) && !ServeRequest(1, RQ_CARCASS, &bad, 1, res, &nres),
          "nonsense indexes are refused");

    // A carcass goes to whoever asks first, with the arrows in it.
    Animal *g = First(AN_GAZELLE);
    int gi = (int)(g - G.animals);
    g->state = ST_DEAD;
    g->timer = 100;
    g->arrowsIn = 2;
    G.peers[1].pos = g->pos;
    Check(ServeRequest(1, RQ_CARCASS, &gi, 1, res, &nres) && nres == 2 && res[0] == AN_GAZELLE && res[1] == 2 && !g->active,
          "the guest takes the gazelle carcass and its 2 arrows");
    Check(!ServeRequest(0, RQ_CARCASS, &gi, 1, res, &nres), "the host cannot take it a second time");

    // The farm is shared: a plot sown by the guest is no longer free for the host.
    int plot[2] = { 0, PLOT_SOW };
    G.plots[0].state = PLOT_EMPTY;
    G.peers[1].pos = G.plots[0].pos;
    G.day = 1;
    G.hour = 8;
    bool sown = ServeRequest(1, RQ_PLOT, plot, 2, res, &nres);
    Check(sown && G.plots[0].state == PLOT_WHEAT, "the guest sows plot 1");
    Check(!PlotServe(0, PLOT_SOW), "the host finds it already sown");

    // Raiders go for the guest who walks into their camp.
    G.peers[1].pos = CampCenter(0);
    G.peers[1].pos.x += 6;
    for (int k = 0; k < 20; k++) {
        CollectBodies();
        UpdateCombat(0.05f);
    }
    int after = 0;
    for (int i = 0; i < MAX_RAIDERS; i++) after += G.raiders[i].vit.hostile > 0 && G.raiders[i].vit.target == 1;
    Check(after >= 2, "%d raiders turn on the guest in their camp", after);
    CalmFor(1);
    int still = 0;
    for (int i = 0; i < MAX_RAIDERS; i++) still += G.raiders[i].vit.hostile > 0;
    Check(still == 0, "a guest brought down: the raiders let them go (%d still angry)", still);

    // The night passes only once both lie in bed.
    G.peers[1].pos = G.houses[1].stations[0];
    G.peers[1].flags = PF_GROUND | PF_BED | PF_SLEEP;
    G.hour = 21.0f;
    G.day = 4;
    PlaceAt(G.stations[STN_BED].x - 1, G.stations[STN_BED].z, 0);
    G.panel = PANEL_NONE;
    G.sleeping = false;
    GameUpdate();
    Check(G.day == 4 && G.hour > 20.9f, "the guest alone in bed: the night does not pass yet");
    G.waitBed = true;
    GameUpdate();
    Check(G.day == 5 && fabsf(G.hour - 6.0f) < 0.1f && !G.waitBed, "both in bed: dawn of day %d (%.1f h)", G.day, G.hour);

    int size = 0;
    bool same = SnapshotRoundTrip(&size);
    Check(same && size > 1000, "a picture of the world reads back byte for byte (%d bytes)", size);

    // The host keeps every player's things in its save.
    PlayerRecord rec = { 0 };
    snprintf(rec.name, NAME_LEN, "Guest");
    rec.slot = 1;
    rec.pos = G.houses[1].door;
    rec.bagSlots = BAG_START + 2;
    rec.quiver = 7;
    rec.quiverMax = QUIVER_START;
    rec.food = 55;
    rec.health = 90;
    rec.bag[0] = (Slot){ IT_HERB, 3 };
    KeepRecord(&rec);
    Check(SaveNetGame(), "the host saves the game");
    G.recordCount = 0;
    int day = G.day;
    GameInit();
    bool loaded = LoadNetGame();
    Check(loaded && G.day == day, "loads it back: day %d", G.day);
    const PlayerRecord *back = FindRecord("guest");
    Check(back && back->slot == 1 && back->bagSlots == BAG_START + 2 && back->bag[0].item == IT_HERB && back->bag[0].count == 3,
          "the guest's baskets are kept under their name");
    remove("sumer-mp.sav");
    G.role = NET_SOLO;
}

// The music follows the day, the fight and the night, from facts every player's game holds.
static int MoodAfter(float seconds)
{
    for (float t = 0; t < seconds; t += 0.1f) MusicUpdate(0.1f, true);
    return MusicMood();
}

static void MoodIs(float seconds, int want, const char *what)
{
    int got = MoodAfter(seconds);
    Check(got == want, "music: %s (mood %d, wanted %d)", what, got, want);
}

static void TestMusic(void)
{
    static float quiet[32 * 2];
    G.role = NET_SOLO;
    G.self = G.actor = 0;
    RespawnPeople();
    MusicRender(0, quiet, 32);   // gets the tunes ready without a sound device
    G.sleeping = G.waitBed = false;
    G.night = 0;
    G.health = 100;
    G.hurtT = 0;
    G.pvp = G.combat = false;
    PlaceAt(0, -200, 0);
    MoodIs(15, 0, "the morning tune in a quiet day");

    G.night = 0.7f;
    MoodIs(1, 2, "the night tune at night");
    G.night = 0.45f;
    MoodIs(1, 2, "dusk does not flip the tune back and forth");
    G.night = 0.3f;
    MoodIs(1, 0, "day again once it is light");

    Animal *w = First(AN_WOLF);
    if (!w) { Check(false, "no wolf to test the music with"); return; }
    w->pos = (Vector3){ G.player.pos.x + 20, G.player.pos.y, G.player.pos.z };
    w->state = ST_HUNT;
    w->target = G.self;
    MoodIs(0.2, 1, "a wolf pack hunting you brings the fight");
    w->state = ST_CALM;
    MoodIs(6, 1, "the fight tune lasts a while after it ends");
    MoodIs(8, 0, "then the morning tune returns");
    w->state = ST_HUNT;
    w->pos.x += 200;
    MoodIs(0.2, 0, "a wolf 200 m away is no threat");
    w->state = ST_CALM;

    HurtPlayer(5, BY_GUARD);
    MoodIs(0.2, 1, "a blow on you brings the fight");
    G.sleeping = true;
    MoodIs(0.2, 0, "being brought down ends it");
    G.sleeping = false;
    MoodAfter(15);

    int g = VillagerWhere(NPC_GUARD, Anywhere);
    Vector3 gp = VillagerPos(&G.villagers[g]);
    PlaceAt(gp.x + 8, gp.z, 0);
    G.villagers[g].vit.hostile = 5;
    G.villagers[g].vit.target = G.self;
    MoodIs(0.2, 1, "a guard after you brings the fight");
    G.villagers[g].vit.hostile = 0;
    MoodAfter(15);
    G.villagers[g].vit.hostile = 5;
    G.villagers[g].vit.target = 1;
    MoodIs(0.2, 0, "a guard after someone else is no threat to you");
    G.villagers[g].vit.hostile = 0;

    PlaceAt(0, -200, 0);
    G.peers[1].active = true;
    G.peers[1].flags = 0;
    G.peers[1].pos = (Vector3){ 30, 0, -200 };
    G.pvp = true;
    MoodIs(0.2, 0, "another player 30 m away, no fight, no combat mode");
    G.peers[1].pos.x = 15;
    G.peers[1].swingT = 0.3f;
    MoodIs(0.2, 1, "a player swinging a weapon nearby brings the fight");
    G.peers[1].swingT = 0;
    MoodAfter(15);
    G.combat = true;
    G.peers[1].pos.x = 50;
    MoodIs(0.2, 1, "in combat mode another player within 60 m brings the fight");
    G.peers[1].active = false;
    G.pvp = G.combat = false;
    MoodAfter(15);

    // Level choices are remembered between 0 and 3.
    MusicSetLevel(2);
    Check(MusicLevel() == 2, "music: volume level is kept (%d)", MusicLevel());
    MusicSetLevel(7);
    Check(MusicLevel() == 3, "music: volume level is clamped (%d)", MusicLevel());
    MusicSetLevel(2);
}

// The warning mark before a blow between players, and how it comes out: yellow, then green or red.
static void TestPlayerMarks(void)
{
    G.role = NET_HOST;
    G.self = G.actor = 0;
    G.peers[1] = (Peer){ .active = true, .net = 1, .health = HEALTH_MAX, .flags = PF_GROUND, .equip = EQ_MELEE };
    snprintf(G.peers[1].name, NAME_LEN, "Guest");
    RespawnPeople();
    ForgetFights(0);
    ForgetFights(1);
    G.armor = ARMOR_NONE;
    G.combat = false;
    G.pvp = true;
    G.equip = EQ_MELEE;
    G.parryT = 0;
    G.player.facing = 0.0f;

    // Out east, past the canals: no guards. The guest swings; for a moment the host sees yellow over them.
    GuestSwing(200, 1.5f, 200, 0, 2);
    Check(G.peers[1].mark == MARK_WARN && G.health == HEALTH_MAX, "a swing at a player shows yellow first and has not landed (mark %d, health %.0f)", G.peers[1].mark, G.health);
    Settle(PVP_WINDUP - 0.12f);
    Check(G.health == HEALTH_MAX && G.peers[1].mark == MARK_WARN, "still yellow a moment before it lands (health %.0f)", G.health);
    Settle(0.3f);
    Check(G.health < HEALTH_MAX && G.peers[1].mark == MARK_HIT, "then it lands and the mark turns red (health %.0f, mark %d)", G.health, G.peers[1].mark);
    float plain = HEALTH_MAX - G.health;
    Settle(1.3f);
    Check(G.peers[1].mark == MARK_NONE, "the mark fades (%d)", G.peers[1].mark);

    // Moving out of reach in the moment: the blow finds no one.
    GuestSwing(200, 1.5f, 200, 0, 2);
    PlaceAt(200, -8, 0);
    SyncSelf();
    Settle(PVP_WINDUP + 0.3f);
    Check(G.health == HEALTH_MAX && G.peers[1].mark == MARK_NONE, "a blow on someone who stepped away finds no one (health %.0f, mark %d)", G.health, G.peers[1].mark);

    // A raised weapon in time turns it aside: green over the swinger.
    G.parryT = 1.0f;
    float through = GuestBlow(200, 1.5f, 200, 0, 2);
    Check(through == 0 && G.peers[1].mark == MARK_PARRIED, "a parry in the moment: no harm, green over the swinger (%.0f, mark %d)", through, G.peers[1].mark);
    G.parryT = 0;

    // And a parry too late is no parry.
    G.parryT = 0;
    GuestSwing(200, 1.5f, 200, 0, 2);
    Settle(PVP_WINDUP - 0.1f);
    G.parryT = 1.0f;
    Settle(0.4f);
    Check(G.health == HEALTH_MAX && G.peers[1].mark == MARK_PARRIED, "a parry raised just before it lands still counts (health %.0f, mark %d)", G.health, G.peers[1].mark);
    G.parryT = 0;

    // The host swings at a guest who parries: the host is stunned a while and sees green.
    ForgetFights(0);
    ForgetFights(1);
    G.peers[1].pos = (Vector3){ 200, TerrainHeight(200, 1.5f), 1.5f };
    G.peers[1].flags = PF_GROUND | PF_PARRY;
    G.peers[1].facing = PI;   // facing -z, toward the host
    G.peers[1].equip = EQ_MELEE;
    PlaceAt(200, 0, 0);
    SyncSelf();
    G.sleeping = false;
    G.health = HEALTH_MAX;
    G.swingCool = 0;
    G.actor = 0;
    WholeSwing((Vector3){ 200, G.player.pos.y + 1.0f, 0 }, (Vector3){ 0, 0, 1 }, EQ_MELEE, 2);
    Settle(PVP_WINDUP + 0.3f);
    Check(G.peers[0].mark == MARK_PARRIED && G.swingCool >= 0.85f, "the host's blow is parried: green and stunned (mark %d, cooldown %.2f)", G.peers[0].mark, G.swingCool);

    // Left open: the guest's quick answer, before the green fades, hits harder than a plain blow.
    float answer = GuestBlow(200, 1.5f, 200, 0, 2);
    (void)answer;
    G.peers[0].mark = MARK_PARRIED;
    G.peers[0].markT = 0.6f;
    G.peers[1].flags = PF_GROUND;
    GuestSwing(200, 1.5f, 200, 0, 2);
    G.peers[0].mark = MARK_NONE;
    Settle(PVP_WINDUP + 0.3f);
    float riposte = HEALTH_MAX - G.health;
    Check(fabsf(riposte - plain * 1.25f) < 0.6f, "a quick answer to a parried blow hits a quarter harder (%.1f against %.1f)", riposte, plain);

    G.peers[1] = (Peer){ 0 };
    G.peers[0].mark = MARK_NONE;
    G.health = HEALTH_MAX;
    G.role = NET_SOLO;
    G.pvp = true;
    G.swingCool = 0;
    RespawnPeople();
}

// Red lights at the edges of the screen show which way a blow came from.
static void TestHitLights(void)
{
    G.role = NET_SOLO;
    G.self = G.actor = 0;
    G.sleeping = false;
    G.player.yaw = 0;
    PlaceAt(0, -200, 0);
    // Looking along yaw 0 means looking toward -z: that is ahead; +x is on the right.
    struct { const char *name; Vector3 src; int light; } cases[] = {
        { "ahead", { 0, 0, -12 }, 0 }, { "behind", { 0, 0, 12 }, 1 }, { "to the left", { -12, 0, 0 }, 2 }, { "to the right", { 12, 0, 0 }, 3 },
    };
    for (int c = 0; c < 4; c++) {
        for (int k = 0; k < 4; k++) G.hitLight[k] = 0;
        G.health = HEALTH_MAX;
        Vector3 src = { G.player.pos.x + cases[c].src.x, 0, G.player.pos.z + cases[c].src.z };
        HurtPlayerFrom(10, BY_PLAYER + 1, &src);
        int strongest = 0;
        for (int k = 1; k < 4; k++) if (G.hitLight[k] > G.hitLight[strongest]) strongest = k;
        Check(strongest == cases[c].light && G.hitLight[cases[c].light] > 0.5f, "a blow from %s lights the %s edge (%.2f %.2f %.2f %.2f)", cases[c].name,
              (const char *[]){ "top", "bottom", "left", "right" }[cases[c].light], G.hitLight[0], G.hitLight[1], G.hitLight[2], G.hitLight[3]);
    }
    // Turning around turns the lights with you: a blow from -z, with yaw PI (looking toward +z), is behind.
    for (int k = 0; k < 4; k++) G.hitLight[k] = 0;
    G.player.yaw = PI;
    Vector3 north = { G.player.pos.x, 0, G.player.pos.z - 12 };
    HurtPlayerFrom(10, BY_PLAYER + 1, &north);
    Check(G.hitLight[1] > 0.5f && G.hitLight[0] < 0.1f, "turned around, the same blow is behind you (%.2f front, %.2f back)", G.hitLight[0], G.hitLight[1]);
    G.player.yaw = 0;
    for (int k = 0; k < 4; k++) G.hitLight[k] = 0;
    HurtPlayer(5, BY_NONE);
    Check(G.hitLight[0] + G.hitLight[1] + G.hitLight[2] + G.hitLight[3] == 0 && G.flinchT > 0, "a hurt from nowhere in particular lights no edge, but the body still flinches");
    G.health = HEALTH_MAX;
    G.flinchT = 0;
}

// Six players, six houses; in PvP the start is anywhere in the city and the ziggurat is unguarded.
static void TestSixPlayers(void)
{
    Check(MAX_PLAYERS == 6, "six players");
    int sound = 0;
    bool distinct = true;
    for (int h = 0; h < MAX_PLAYERS; h++) {
        const House *hs = &G.houses[h];
        sound += InsideWalls(hs->center.x, hs->center.z, 4.0f) && !SolidAt(hs->door.x, hs->door.z, 0.4f);
        for (int o = 0; o < h; o++)
            distinct &= XZDist(hs->center, G.houses[o].center) > 9.0f && (PlayerColor(h).r != PlayerColor(o).r || PlayerColor(h).g != PlayerColor(o).g || PlayerColor(h).b != PlayerColor(o).b);
    }
    Check(sound == MAX_PLAYERS && distinct, "all six houses stand inside the walls with a clear doorstep, apart, each in its own color (%d of 6)", sound);
    const House *six = &G.houses[5];
    Check(XZDist(six->stations[STN_BED], six->center) < 6.0f && XZDist(six->stations[STN_STORAGE], six->center) < 6.0f, "the sixth house has its bed and storeroom");

    // Spawns: always inside the walls on open ground, spread out, and away from other players.
    G.role = NET_HOST;
    G.peers[1] = (Peer){ .active = true, .net = 1, .health = HEALTH_MAX, .flags = PF_GROUND };
    G.peers[1].pos = (Vector3){ 0, 0, 20 };
    int good = 0;
    float nearest = 1e9f, minX = 1e9f, maxX = -1e9f;
    for (int k = 0; k < 40; k++) {
        Vector3 at = CitySpawn(0);
        good += InsideWalls(at.x, at.z, 8.0f) && !SolidAt(at.x, at.z, 1.0f) && !InWater(at.x, at.z);
        nearest = fminf(nearest, XZDist(at, G.peers[1].pos));
        minX = fminf(minX, at.x);
        maxX = fmaxf(maxX, at.x);
    }
    Check(good == 40, "every spawn is inside the walls on open ground (%d of 40)", good);
    Check(nearest > 25.0f, "spawns keep away from other players (nearest %.0f m)", nearest);
    Check(maxX - minX > 60.0f, "spawns are spread over the city (x from %.0f to %.0f)", minX, maxX);
    G.peers[1] = (Peer){ 0 };
    G.role = NET_SOLO;

    // The guards on the ziggurat's stairs and terrace go in PvP; the others stay.
    Villager saved[MAX_VILLAGERS];
    int savedCount = G.villagerCount, up = 0, guards = 0;
    memcpy(saved, G.villagers, sizeof(saved));
    for (int i = 0; i < G.villagerCount; i++) {
        up += G.villagers[i].kind == NPC_GUARD && G.villagers[i].a.y >= 5.0f && !G.villagers[i].crouch;
        guards += G.villagers[i].kind == NPC_GUARD;
    }
    RemoveZigguratGuards();
    int upAfter = 0, guardsAfter = 0;
    for (int i = 0; i < G.villagerCount; i++) {
        upAfter += G.villagers[i].kind == NPC_GUARD && G.villagers[i].a.y >= 5.0f && !G.villagers[i].crouch;
        guardsAfter += G.villagers[i].kind == NPC_GUARD;
    }
    Check(up >= 3 && upAfter == 0 && guardsAfter == guards - up && G.villagerCount == savedCount - up,
          "%d guards of the ziggurat are gone, the other %d stay (%d people left of %d)", up, guardsAfter, G.villagerCount, savedCount);
    bool open = true;
    for (int i = 0; i < G.villagerCount; i++) open &= !G.villagers[i].templePost;
    Check(open, "no one stands at the temple gate to ask for an offering");
    memcpy(G.villagers, saved, sizeof(saved));
    G.villagerCount = savedCount;

    // Brought down in a PvP game: they come to somewhere in the city, not at home.
    G.pvp = true;
    G.role = NET_SOLO;
    G.sleeping = false;
    PlaceAt(0, -200, 0);
    Vector3 bed = G.stations[STN_BED];
    G.health = 1;
    HurtPlayer(50, BY_WOLF);
    Check(XZDist(G.player.pos, bed) > 4.0f && InsideWalls(G.player.pos.x, G.player.pos.z, 8.0f) && strstr(G.msg, "somewhere else in the city") != NULL,
          "brought down in PvP: comes to elsewhere in the city (%.0f, %.0f): \"%s\"", G.player.pos.x, G.player.pos.z, G.msg);
    G.sleeping = false;
    G.sleepT = 0;
    G.health = HEALTH_MAX;
    G.pvp = true;
    RespawnPeople();
}

static void RunTests(void)
{
    TestMusic();
    TestHerds();
    TestBoar();
    TestArrows();
    TestMace();
    TestSwingContact();
    TestGateGuards();
    TestLooks();
    TestWading();
    TestSnake();
    TestHerbs();
    TestTrade();
    TestPrompts();
    TestPeople();
    TestStreets();
    TestAim();
    TestLaw();
    TestGuardRound();
    TestWolfWays();
    TestBandits();
    TestConsole();
    TestTempleLife();
    TestWilds();
    TestSounds();
    TestPartridges();
    TestTorch();
    TestDeathLosses();
    TestCanals();
    TestArchers();
    TestFishers();
    TestSave();
    TestHostWorld();
    TestPvp();
    TestPlayerMarks();
    TestHitLights();
    TestSixPlayers();
    printf("%d passed, %d failed\n", passes, fails);
}
