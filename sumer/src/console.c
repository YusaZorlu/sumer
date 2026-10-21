// The dev console (Tab, then "Dev console"; or ` or F2): typed commands that move you about, change the
// hour and the weather, and set beasts and fighters where a test needs them. Commands that change the
// shared world are for the host (or a game alone); in a joined PvP game a guest may only look.
#include "game.h"
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINES 14
#define WIDE  120

static char lines[LINES][WIDE];
static int lineCount;
static char last[WIDE];

static void Out(const char *fmt, ...)
{
    va_list ap;
    if (lineCount == LINES) {
        memmove(lines[0], lines[1], sizeof(lines[0]) * (LINES - 1));
        lineCount--;
    }
    va_start(ap, fmt);
    vsnprintf(lines[lineCount++], WIDE, L(fmt), ap);
    va_end(ap);
}

int ConsoleLines(void)
{
    return lineCount;
}

const char *ConsoleLine(int i)
{
    return i >= 0 && i < lineCount ? lines[i] : "";
}

const char *ConsoleLast(void)
{
    return last;
}

static bool Host(void)
{
    if (!IsClient()) return true;
    Out("Only the host can change the world.");
    return false;
}

static bool Self(void)
{
    if (!IsClient() || !G.pvp) return true;
    Out("Not for a guest in a PvP game.");
    return false;
}

static void Teleport(float x, float z)
{
    Player *p = &G.player;
    p->pos = (Vector3){ x, GroundHeight(x, z, 50.0f), z };
    p->vy = 0;
    SyncSelf();
    Out("Moved to %.0f, %.0f.", x, z);
}

// Where a named place is; false if there is no such place.
static bool Place(const char *name, float *x, float *z)
{
    static const char *const villages[VILLAGES] = { "hunters", "herders", "fishers" };
    for (int v = 0; v < VILLAGES; v++)
        if (strcmp(name, villages[v]) == 0) {
            Vector3 c = VillageCenter(v);
            *x = c.x + 8;
            *z = c.z;
            return true;
        }
    if (strcmp(name, "city") == 0 || strcmp(name, "home") == 0) {
        *x = 3;
        *z = 4;
        return true;
    }
    if (strcmp(name, "ziggurat") == 0) {
        *x = 0;
        *z = -16;
        return true;
    }
    if (strncmp(name, "camp", 4) == 0 && name[4] >= '1' && name[4] < '1' + CAMPS) {
        Vector3 c = CampCenter(name[4] - '1');
        *x = c.x + 16;
        *z = c.z;
        return true;
    }
    if (strncmp(name, "band", 4) == 0 && name[4] >= '1' && name[4] < '1' + BANDS) {
        for (int i = 0; i < MAX_RAIDERS; i++)
            if (RaiderBandit(i) && G.raiders[i].camp == CAMPS + name[4] - '1') {
                *x = G.raiders[i].pos.x + 30;
                *z = G.raiders[i].pos.z;
                return true;
            }
    }
    if (strcmp(name, "wolf") == 0 || strcmp(name, "wolves") == 0) {
        float best = 1e9f;
        for (int i = 0; i < G.animalCount; i++) {
            const Animal *a = &G.animals[i];
            float d = XZDist(a->pos, G.player.pos);
            if (a->active && a->type == AN_WOLF && a->state != ST_DEAD && d < best) {
                best = d;
                *x = a->pos.x + 30;
                *z = a->pos.z;
            }
        }
        return best < 1e9f;
    }
    return false;
}

// The item a typed name starts, in English; IT_NONE if none.
static int ItemNamed(const char *name)
{
    for (int i = IT_NONE + 1; i < IT_COUNT; i++) {
        const char *a = ITEMS[i].name, *b = name;
        while (*b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) a++, b++;
        if (name[0] && !*b) return i;
    }
    return IT_NONE;
}

static const char *const BEASTS[AN_TYPES] = { "gazelle", "onager", "boar", "goose", "wolf", "snake" };

// Up to n of a kind of beast brought to stand calm 14 m ahead of you.
static void Spawn(const char *what, int n)
{
    int type = -1, moved = 0;
    for (int t = 0; t < AN_TYPES; t++)
        if (strncmp(BEASTS[t], what, strlen(what)) == 0 && what[0]) type = t;
    if (type < 0) {
        Out("Beasts: gazelle onager boar goose wolf snake");
        return;
    }
    float yaw = G.player.yaw;
    for (int i = 0; i < G.animalCount && moved < n; i++) {
        Animal *a = &G.animals[i];
        if ((int)a->type != type || !a->active || a->state == ST_DEAD) continue;
        float side = ((float)moved - (float)(n - 1) / 2) * 1.6f;
        float x = G.player.pos.x + sinf(yaw) * 14 + cosf(yaw) * side, z = G.player.pos.z + cosf(yaw) * 14 - sinf(yaw) * side;
        a->pos = (Vector3){ x, TerrainHeight(x, z), z };
        a->state = ST_CALM;
        a->speed = 0;
        a->awareness = 0;
        a->timer = 5;
        moved++;
    }
    Out("Brought %d %s.", moved, AnimalName(type));
}

static void Help(void)
{
    Out("where | tp <x> <z> | tp city ziggurat hunters herders fishers camp1-4 band1 band2 wolf");
    Out("heal | god | gear | arrows | give <item> [n] | speed <x>");
    Out("Host: hour <0-24> | rain on/off | spawn <beast> [n] | kill [m] | calm");
    Out("Up arrow: the last command again. Tab or Esc: close.");
}

void ConsoleRun(const char *text)
{
    char buf[WIDE], *arg[6] = { 0 };
    int n = 0;

    snprintf(buf, sizeof(buf), "%s", text);
    for (char *t = strtok(buf, " \t"); t && n < 6; t = strtok(NULL, " \t")) arg[n++] = t;
    if (!n) return;
    snprintf(last, sizeof(last), "%s", text);
    for (char *c = arg[0]; *c; c++) *c = (char)tolower((unsigned char)*c);
    Out("> %s", text);
    const char *cmd = arg[0];
    Player *p = &G.player;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        Help();
    } else if (strcmp(cmd, "clear") == 0) {
        lineCount = 0;
    } else if (strcmp(cmd, "where") == 0) {
        int v = -1;
        float dv = 1e9f;
        for (int k = 0; k < VILLAGES; k++)
            if (XZDist(p->pos, VillageCenter(k)) < dv) {
                dv = XZDist(p->pos, VillageCenter(k));
                v = k;
            }
        Out("At %.1f, %.1f (%.1f m up). Day %d, %02d:%02d. %s %.0f m away.", p->pos.x, p->pos.z, p->pos.y, G.day, (int)G.hour,
            (int)(fmodf(G.hour, 1.0f) * 60), VillageName(v), dv);
    } else if (strcmp(cmd, "tp") == 0 && n >= 3 && Self()) {
        Teleport((float)atof(arg[1]), (float)atof(arg[2]));
    } else if (strcmp(cmd, "tp") == 0 && n == 2 && Self()) {
        float x = 0, z = 0;
        if (Place(arg[1], &x, &z)) Teleport(x, z);
        else Out("No such place. Places: city ziggurat hunters herders fishers camp1-4 band1 band2 wolf");
    } else if (strcmp(cmd, "heal") == 0 && Self()) {
        G.health = HEALTH_MAX;
        G.food = FOOD_MAX;
        Out("Health and food full.");
    } else if (strcmp(cmd, "god") == 0 && Self()) {
        G.god = !G.god;
        Out(G.god ? "Nothing hurts you now (god again to stop)." : "You can be hurt again.");
    } else if (strcmp(cmd, "gear") == 0 && Self()) {
        G.bow = 2;
        G.weapon = 2;
        G.hasMace = true;
        G.armorOwned |= (1 << ARMOR_WOLF) | (1 << ARMOR_LEATHER) | (1 << ARMOR_BRONZE);
        G.armor = ARMOR_BRONZE;
        G.quiver = G.quiverMax;
        Out("The best bow, sword, the war mace and bronze armor.");
    } else if (strcmp(cmd, "arrows") == 0 && Self()) {
        G.quiver = G.quiverMax;
        Out("Quiver full.");
    } else if (strcmp(cmd, "give") == 0 && n >= 2 && Self()) {
        int count = n >= 3 ? atoi(arg[n - 1]) : 1;
        char name[WIDE] = "";
        for (int k = 1; k < (n >= 3 && atoi(arg[n - 1]) > 0 ? n - 1 : n); k++)
            snprintf(name + strlen(name), sizeof(name) - strlen(name), "%s%s", k > 1 ? " " : "", arg[k]);
        int item = ItemNamed(name);
        if (item == IT_NONE) Out("No such thing: %s", name);
        else Out("Put %d %s in your baskets.", BagAdd(item, count < 1 ? 1 : count), ItemName(item));
    } else if (strcmp(cmd, "speed") == 0 && n >= 2 && Self()) {
        G.devSpeed = Clamp((float)atof(arg[1]), 0.2f, 10.0f);
        Out("Walking and running %.1f times as fast.", G.devSpeed);
    } else if (strcmp(cmd, "hour") == 0 && n >= 2 && Host()) {
        G.hour = fmodf(fmaxf(0.0f, (float)atof(arg[1])), 24.0f);
        Out("It is %02d:%02d.", (int)G.hour, (int)(fmodf(G.hour, 1.0f) * 60));
    } else if (strcmp(cmd, "rain") == 0 && n >= 2 && Host()) {
        bool on = strcmp(arg[1], "on") == 0;
        G.rainToday = on;
        G.rainDone = false;
        G.rainStart = G.hour;
        G.rainEnd = fminf(24.0f, G.hour + 2.0f);
        Out(on ? "Rain for two hours." : "No more rain today.");
    } else if (strcmp(cmd, "spawn") == 0 && n >= 2 && Host()) {
        Spawn(arg[1], n >= 3 ? Clamp(atoi(arg[2]), 1, 8) : 1);
    } else if (strcmp(cmd, "kill") == 0 && Host()) {
        float r = n >= 2 ? (float)atof(arg[1]) : 20.0f;
        int people = 0, beasts = KillAnimalsNear(p->pos, r);
        G.actor = G.self;
        for (int i = 0; i < FighterCount(); i++) {
            Vector3 at;
            float hp, max;
            const char *name;
            bool hostile, dead;
            if (!FighterInfo(i, &at, &hp, &max, &name, &hostile, &dead) || dead || XZDist(at, p->pos) > r || !FighterIsFoe(i)) continue;
            DamagePerson(i, 1e6f);
            people++;
        }
        Out("Brought down %d foes and %d beasts within %.0f m.", people, beasts, r);
    } else if (strcmp(cmd, "calm") == 0 && Host()) {
        CalmBeasts(-1);
        ForgetFights(G.self);
        G.peers[G.self].aggro = 0;
        G.peers[G.self].wanted = 0;
        Out("Beasts and fighters let you be.");
    } else if (strcmp(cmd, "tp") == 0 || strcmp(cmd, "give") == 0 || strcmp(cmd, "speed") == 0 || strcmp(cmd, "hour") == 0 ||
               strcmp(cmd, "rain") == 0 || strcmp(cmd, "spawn") == 0) {
        if (n < 2) Out("That needs more: type help.");
    } else if (strcmp(cmd, "heal") && strcmp(cmd, "god") && strcmp(cmd, "gear") && strcmp(cmd, "arrows") && strcmp(cmd, "kill") &&
               strcmp(cmd, "calm")) {
        Out("Unknown command. Type help.");
    }
}
