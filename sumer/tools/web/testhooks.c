// Test-only hooks for the browser build: read the game's state as JSON, and drive the player.
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <emscripten/emscripten.h>

void LobbyAgain(void);
void MusicGains(float *out);

// The music's gains, for the music tests: the made tunes (day, fight, night), then the places' tracks
// (city, village, open land, ziggurat; negative while that stream is paused).
EMSCRIPTEN_KEEPALIVE const char *TestMusic(void)
{
    static char buf[200];
    float g[7];
    MusicGains(g);
    snprintf(buf, sizeof(buf), "[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f]", g[0], g[1], g[2], g[3], g[4], g[5], g[6]);
    return buf;
}

static int Esc(char *dst, int size, const char *src)
{
    int n = 0;
    for (const char *c = src; *c && n < size - 2; c++) {
        if (*c == '"' || *c == '\\') dst[n++] = '\\';
        dst[n++] = ((unsigned char)*c >= 32) ? *c : ' ';
    }
    dst[n] = '\0';
    return n;
}

const char *ChatLast(void);
bool TouchButtonAt(int b, Vector2 *c, float *r);
bool BoardTapAt(int k, Vector2 *c);
bool BoardRulesShown(void);

EMSCRIPTEN_KEEPALIVE const char *TestInfo(void)
{
    static char buf[32768];
    char msg[300], failure[300], chat[300];
    Esc(msg, sizeof(msg), G.msgT > 0 ? G.msg : "");
    Esc(chat, sizeof(chat), ChatLast());
    Esc(failure, sizeof(failure), NetFailure());
    char note[300];
    Esc(note, sizeof(note), AccountNote());
    int n = snprintf(buf, sizeof(buf), "{\"xp\":%ld,\"account\":\"%s\",\"accState\":%d,\"note\":\"%s\",", AccountXp(), AccountName(), AccountState(), note);
    n += snprintf(buf + n, sizeof(buf) - (size_t)n,
        "\"lobby\":%d,\"role\":%d,\"self\":%d,\"players\":%d,\"day\":%d,\"hour\":%.3f,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,"
        "\"yaw\":%.3f,\"pitch\":%.3f,\"captured\":%d,\"lang\":%d,\"health\":%.1f,\"food\":%.1f,\"quiver\":%d,\"henCount\":%d,\"eggs\":%d,\"waitBed\":%d,\"sleeping\":%d,\"panel\":%d,\"swing\":%.2f,\"cool\":%.2f,\"equip\":%d,\"pvp\":%d,\"combat\":%d,\"mood\":%d,\"place\":%d,\"musicLevel\":%d,\"hitLight\":[%.2f,%.2f,%.2f,%.2f],\"flinch\":%.2f,\"villagers\":%d,\"gfx\":%d,\"ui\":%.3f,\"shake\":%.2f,"
        "\"target\":%d,\"targetIndex\":%d,\"promptE\":\"%s\",\"msg\":\"%s\",\"chat\":\"%s\",\"failure\":\"%s\",\"peers\":[",
        InLobby(), G.role, G.self, PlayerCount(), G.day, G.hour, G.player.pos.x, G.player.pos.y, G.player.pos.z,
        G.player.yaw, G.player.pitch, PointerCaptured(), Language(), G.health, G.food, G.quiver, G.henCount, G.coopEggs, G.waitBed, G.sleeping, G.panel, G.swingT, G.swingCool, G.equip, G.pvp, G.combat, MusicMood(), MusicPlace(), MusicLevel(), G.hitLight[0], G.hitLight[1], G.hitLight[2], G.hitLight[3], G.flinchT, G.villagerCount, GfxTier(), UiScale(), G.shakeT, G.target.kind, G.target.index,
        G.promptE, msg, chat, failure);
    for (int s = 0, first = 1; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active) continue;
        char name[40];
        Esc(name, sizeof(name), p->name);
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s{\"s\":%d,\"name\":\"%s\",\"x\":%.2f,\"z\":%.2f,\"flags\":%d,\"health\":%.0f,\"wanted\":%.0f,\"kills\":%d,\"deaths\":%d,\"mark\":%d}",
                      first ? "" : ",", s, name, p->pos.x, p->pos.z, p->flags, p->health, p->wanted, p->kills, p->deaths, p->mark);
        first = 0;
    }
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"screen\":[%d,%d],\"canvas\":[%d,%d],\"fpsLimit\":%d,\"aim\":%d,\"moving\":%d,\"stick\":[%.2f,%.2f],\"touch\":%d,\"fp\":%d,\"crouch\":%d,\"parry\":%.2f,\"buttons\":[", GetScreenWidth(), GetScreenHeight(),
                  EM_ASM_INT({ return Module.canvas.width | 0; }), EM_ASM_INT({ return Module.canvas.height | 0; }), FpsLimit(), G.player.aiming, G.player.moving, TouchStick().x, TouchStick().y, TouchOn(), G.firstPerson,
                  G.player.crouching, G.parryT + G.parryCool);
    for (int b = 0; b < TB_COUNT; b++) {
        Vector2 c;
        float r;
        bool shown = TouchButtonAt(b, &c, &r);
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%.1f,%.1f,%.1f]", b ? "," : "", shown ? c.x : -1, shown ? c.y : -1, shown ? r : 0);
    }
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"animals\":[");
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%d,%d,%d,%.2f,%.2f,%d,%d]", i ? "," : "", a->type, a->active, a->state, a->pos.x, a->pos.z, a->hp, a->target);
    }
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"bag\":[");
    for (int i = 0; i < G.bagSlots; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%d,%d]", i ? "," : "", G.bag[i].item, G.bag[i].count);
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"plots\":[");
    for (int i = 0; i < MAX_PLOTS; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s%d", i ? "," : "", G.plots[i].state);
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"guards\":[");
    for (int i = 0, first = 1; i < G.villagerCount; i++) {
        const Villager *v = &G.villagers[i];
        if (v->kind != NPC_GUARD) continue;
        Vector3 p = VillagerPos(v);
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%.1f,%.1f,%.1f,%d]", first ? "" : ",", p.x, p.y, p.z, v->vit.dead ? 1 : 0);
        first = 0;
    }
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"royale\":{\"on\":%d,\"phase\":%d,\"out\":%d,\"alive\":%d,\"winner\":%d,\"started\":%d,\"countdown\":%.1f,\"attack\":%.2f,\"storm\":%d,\"statues\":[",
                  Royale(), RoyalePhase(), OutOfRound(), RoyaleAlive(), RoyaleWinner(), RoyaleStarted(), RoyaleCountdown(), RoyaleAttack(), InStorm(G.player.pos));
    for (int i = 0; i < StatueCount(); i++) {
        Vector3 sp = StatuePos(i);
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%.1f,%.1f,%d]", i ? "," : "", sp.x, sp.z, StatueClaimed(i));
    }
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "]},\"party\":{\"on\":%d,\"phase\":%d,\"game\":%d,\"ready\":%d,\"vote\":%d,\"out\":%d,\"food\":%.0f,\"up\":%.2f,\"lava\":%d,\"seeker\":%d,\"hider\":%d,\"scores\":[",
                  Party(), PartyPhase(), PartyGame(), MatchReady(), PartyVote(), PartyOut(), G.food, G.player.pos.y - TerrainHeight(G.player.pos.x, G.player.pos.z), InLava(G.player.pos), PartySeeker(), PartyHider(G.self));
    for (int s = 0, first = 1; s < MAX_PLAYERS; s++)
        if (G.peers[s].active) {
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%d,%d,%d]", first ? "" : ",", s, PartyRound(s), PartyTotal(s));
            first = 0;
        }
    // The alehouse's boards: kind, seats, turn, throw, winner, moves; and each table's first seat.
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "]},\"board\":%d,\"boardView\":%d,\"boards\":[", BoardOf(G.self), BoardViewing());
    for (int b = 0; b < BOARDS; b++) {
        const Board *x = BoardAt(b);
        Vector3 s0 = BoardSeat(b, 0, NULL), s1 = BoardSeat(b, 1, NULL);
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s{\"kind\":%d,\"seat\":[%d,%d],\"turn\":%d,\"roll\":%d,\"winner\":%d,\"moves\":%d,\"at\":[[%.2f,%.2f,%.2f],[%.2f,%.2f,%.2f]]}",
                      b ? "," : "", x->kind, x->seat[0], x->seat[1], x->turn, x->roll, x->winner, x->moves, s0.x, s0.y, s0.z, s1.x, s1.y, s1.z);
    }
    // Where to tap on the board panel, in CSS pixels: Throw, Play a townsman, Get up, pieces 1-7, Rules ([-1,-1] when not shown).
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"boardTaps\":[");
    for (int k = 0; k < 4 + UR_PIECES; k++) {
        Vector2 c = { -1, -1 };
        BoardTapAt(k, &c);
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%.0f,%.0f]", k ? "," : "", c.x, c.y);
    }
    // The UI's pointer in CSS pixels (the game's own cursor while a panel keeps the mouse), the board's rules shown.
    Vector2 ptr = Vector2Scale(PointerPosition(), 1.0f / PixelRatio());
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"pointer\":[%.0f,%.0f],\"rules\":%d", ptr.x, ptr.y, BoardRulesShown());
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, ",\"pets\":[");
    for (int i = 0; i < PetCount(); i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%.2f,%.2f,%.2f,%d,%d]", i ? "," : "", PetPos(i).x, PetPos(i).y, PetPos(i).z, PetIsDog(i), PetFriend(i));
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"villager0\":[%.2f,%.2f]}", VillagerPos(&G.villagers[0]).x, VillagerPos(&G.villagers[0]).z);
    return buf;
}

// Where a player set down at (x, z) stands: on the ground, or on a roof that covers it.
static float LandingHeight(float x, float z)
{
    float y = GroundHeight(x, z, TerrainHeight(x, z) + 1.0f);
    return PointInSolid((Vector3){ x, y + 1.0f, z }) ? GroundHeight(x, z, 50.0f) : y;
}

// 1 teleport to (a, b); 2 press E; 3 press F; 4 look (yaw a, pitch b); 5 shoot a full-drawn arrow where
// the camera looks; 6 swing the weapon; 7 add b of item a to the baskets; 8 take an animal: put it
// down dead at the player's feet (a = index); 9 set the hour to a; 10 shoot at the ground point (a, b)
// at body height; 11 language a; 12 barter offer b of trader a; 13 hurt the player by a; 14 equip a;
// 15 open panel a (PANEL_* in game.h); 20 start a battle royale round (the host); 21 skip the round's clock to a s;
// 23 forget whose this browser's saves are, as if made before accounts; 25 to board a's seat b; 26 beside pet a.
EMSCRIPTEN_KEEPALIVE void TestCommand(int cmd, float a, float b)
{
    Player *p = &G.player;
    switch (cmd) {
    case 1:
        p->pos = (Vector3){ a, LandingHeight(a, b), b };
        p->vy = 0;
        break;
    case 2: Interact(false); break;
    case 3: Interact(true); break;
    case 4:
        p->yaw = a;
        p->pitch = b;
        FollowCamera(1.0f);
        break;
    case 5: {
        Vector3 fwd = { -sinf(p->yaw), 0, -cosf(p->yaw) };
        Vector3 from = { p->pos.x + fwd.x * 0.7f, p->pos.y + 1.5f, p->pos.z + fwd.z * 0.7f };
        G.quiver = G.quiver > 0 ? G.quiver - 1 : 0;
        ShootArrow(from, AimPoint(), 1.0f);
        break;
    }
    case 6: MeleeStrike(STRIKE_START); MeleeStrike(STRIKE_CONTACT); break;
    case 7: BagAdd((int)a, (int)b); break;
    case 9: G.hour = a; break;
    case 10: {
        Vector3 to = { a, TerrainHeight(a, b) + 0.8f, b };
        Vector3 from = { p->pos.x, p->pos.y + 1.5f, p->pos.z };
        Vector3 dir = Vector3Normalize(Vector3Subtract(to, from));
        from = Vector3Add(from, Vector3Scale(dir, 0.7f));
        G.quiver = G.quiver > 0 ? G.quiver - 1 : 0;
        ShootArrow(from, to, 1.0f);
        break;
    }
    case 11: SetLanguage((int)a); break;
    case 12: Barter((int)a, (int)b); break;
    case 13: HurtPlayer(a, BY_NONE); break;
    case 14: SetEquip((int)a); break;
    case 15: SetPanel((Panel)(int)a); break;
    case 16: SfxSetLevel((int)a); break;
    case 17: G.quit = true; break;
    case 19: SetLeftHanded(a != 0); break;
    case 18: LobbyAgain(); break;
    case 20: RoyaleStart(); break;
    case 21: RoyaleSkipTo(a); break;
    case 23: remove(DataPath("sumer.owner")); SyncStorage(); break;   // this browser's saves as if made before accounts
    case 27:   // the storage jars filled with one of each item from item a on (to look at their pictures)
        for (int i = 0; i < STORE_SLOTS; i++) G.store[i] = (Slot){ (int)a + i < IT_COUNT ? (int)a + i : IT_NONE, (int)a + i < IT_COUNT ? 1 : 0 };
        break;
    case 28:   // rain today from now for a hours (a = 0: no rain today), to look at the wet sky and ground
        G.rainToday = a > 0;
        G.rainDone = false;
        G.rainStart = G.hour;
        G.rainEnd = G.hour + a;
        break;
    case 25:   // at the alehouse's board a, on the stool of side b
        p->pos = BoardSeat((int)a, (int)b, NULL);
        p->vy = 0;
        break;
    case 26: {   // beside cat or dog a, b metres east of it
        Vector3 q = PetPos((int)a);
        p->pos = (Vector3){ q.x + (b > 0 ? b : 1.0f), q.y, q.z };
        p->vy = 0;
        break;
    }
    default: break;
    }
}

// Runs a dev console command (see src/console.c) and returns what it said last.
