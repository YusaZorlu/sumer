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

EMSCRIPTEN_KEEPALIVE const char *TestInfo(void)
{
    static char buf[32768];
    char msg[300], failure[300], chat[300];
    Esc(msg, sizeof(msg), G.msgT > 0 ? G.msg : "");
    Esc(chat, sizeof(chat), ChatLast());
    Esc(failure, sizeof(failure), NetFailure());
    int n = snprintf(buf, sizeof(buf),
        "{\"lobby\":%d,\"role\":%d,\"self\":%d,\"players\":%d,\"day\":%d,\"hour\":%.3f,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,"
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
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "],\"royale\":{\"on\":%d,\"phase\":%d,\"out\":%d,\"alive\":%d,\"winner\":%d,\"attack\":%.2f,\"storm\":%d,\"statues\":[",
                  Royale(), RoyalePhase(), RoyaleOut(), RoyaleAlive(), RoyaleWinner(), RoyaleAttack(), InStorm(G.player.pos));
    for (int i = 0; i < StatueCount(); i++) {
        Vector3 sp = StatuePos(i);
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s[%.1f,%.1f,%d]", i ? "," : "", sp.x, sp.z, StatueClaimed(i));
    }
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "]},\"villager0\":[%.2f,%.2f]}", VillagerPos(&G.villagers[0]).x, VillagerPos(&G.villagers[0]).z);
    return buf;
}

// 1 teleport to (a, b); 2 press E; 3 press F; 4 look (yaw a, pitch b); 5 shoot a full-drawn arrow where
// the camera looks; 6 swing the weapon; 7 add b of item a to the baskets; 8 take an animal: put it
// down dead at the player's feet (a = index); 9 set the hour to a; 10 shoot at the ground point (a, b)
// at body height; 11 language a; 12 barter offer b of trader a; 13 hurt the player by a; 14 equip a;
// 15 open panel a (PANEL_* in game.h); 20 start a battle royale round (the host); 21 skip the round's clock to a s.
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
    default: break;
    }
}

// Runs a dev console command (see src/console.c) and returns what it said last.
