// Test-only hooks for the game's server program (built by tools/web/build.sh with SUMER_TEST): the browser
// suites ask server.py --test (POST /test/host?room=ID) what the room's world is like, or have it do
// something. "info" answers with the world as JSON; "cmd N a b" runs command N:
// 9 set the hour to a; 20 start a battle royale round; 21 skip the round's clock to a s; 22 the next
// round in a s.
#include "game.h"
#include <stdio.h>
#include <string.h>

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

static void Info(char *buf, int size)
{
    int n = snprintf(buf, (size_t)size, "{\"day\":%d,\"hour\":%.3f,\"players\":%d,\"henCount\":%d,\"eggs\":%d,\"pvp\":%d,\"combat\":%d,\"villagers\":%d,\"peers\":[",
                     G.day, G.hour, PlayerCount(), G.henCount, G.coopEggs, G.pvp, G.combat, G.villagerCount);
    for (int s = 0, first = 1; s < MAX_PLAYERS; s++) {
        const Peer *p = &G.peers[s];
        if (!p->active) continue;
        char name[40];
        Esc(name, sizeof(name), p->name);
        n += snprintf(buf + n, (size_t)(size - n), "%s{\"s\":%d,\"name\":\"%s\",\"x\":%.2f,\"z\":%.2f,\"flags\":%d,\"health\":%.0f,\"wanted\":%.0f,\"kills\":%d,\"deaths\":%d,\"mark\":%d}",
                      first ? "" : ",", s, name, p->pos.x, p->pos.z, p->flags, p->health, p->wanted, p->kills, p->deaths, p->mark);
        first = 0;
    }
    n += snprintf(buf + n, (size_t)(size - n), "],\"animals\":[");
    for (int i = 0; i < G.animalCount; i++) {
        const Animal *a = &G.animals[i];
        n += snprintf(buf + n, (size_t)(size - n), "%s[%d,%d,%d,%.2f,%.2f,%d,%d]", i ? "," : "", a->type, a->active, a->state, a->pos.x, a->pos.z, a->hp, a->target);
    }
    n += snprintf(buf + n, (size_t)(size - n), "],\"plots\":[");
    for (int i = 0; i < MAX_PLOTS; i++) n += snprintf(buf + n, (size_t)(size - n), "%s%d", i ? "," : "", G.plots[i].state);
    int guards = 0;
    for (int i = 0; i < G.villagerCount; i++) guards += G.villagers[i].kind == NPC_GUARD;
    n += snprintf(buf + n, (size_t)(size - n), "],\"guards\":%d,\"royale\":{\"on\":%d,\"phase\":%d,\"alive\":%d,\"winner\":%d,\"countdown\":%.1f,\"statues\":[",
                  guards, Royale(), RoyalePhase(), RoyaleAlive(), RoyaleWinner(), RoyaleCountdown());
    for (int i = 0; i < StatueCount(); i++) {
        Vector3 sp = StatuePos(i);
        n += snprintf(buf + n, (size_t)(size - n), "%s[%.1f,%.1f,%d]", i ? "," : "", sp.x, sp.z, StatueClaimed(i));
    }
    snprintf(buf + n, (size_t)(size - n), "]},\"villager0\":[%.2f,%.2f]}", VillagerPos(&G.villagers[0]).x, VillagerPos(&G.villagers[0]).z);
}

void HostTest(const char *in, char *out, int size)
{
    int c;
    float a = 0, b = 0;
    out[0] = '\0';
    if (strncmp(in, "info", 4) == 0) {
        Info(out, size);
        return;
    }
    if (sscanf(in, "cmd %d %f %f", &c, &a, &b) < 1) {
        snprintf(out, (size_t)size, "{\"error\":\"what?\"}");
        return;
    }
    switch (c) {
    case 9: G.hour = a; break;
    case 20: RoyaleStart(); break;
    case 21: RoyaleSkipTo(a); break;
    case 22: RoyaleCountdownTo(a); break;
    default: break;
    }
    snprintf(out, (size_t)size, "{\"done\":%d}", c);
}
