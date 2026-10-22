#ifndef SUMER_GAME_H
#define SUMER_GAME_H

#include "raylib.h"
#include "raymath.h"
#include <stdarg.h>
#include <stdbool.h>

#define WORLD_HALF      540.0f
#define WORLD_LIMIT     534.0f
#define HOUR_SECONDS    10.0f
#define STEP_HEIGHT     0.55f
#define PLAYER_RADIUS   0.4f
#define PLAYER_HEIGHT   1.9f
#define GRAVITY         18.0f
#define JUMP_SPEED      6.5f
#define ZIG_Z           (-40.0f)
#define CITY_Z          (-6.0f)
#define CITY_A          100.0f
#define CITY_B          84.0f
#define WALL_TOP        6.5f
#define CAMPS           4
#define EAST_CAMPS      2
#define BANDS           2
#define RIVER_X0        (-150.0f)
#define RIVER_X1        (-128.0f)
#define FORD_Z0         (-518.0f)
#define FORD_Z1         (-506.0f)
#define WATER_LEVEL     (-0.4f)
#define SEASON_DAYS     9
#define WATER_DAYS      6.0f
#define HEN_LIFE_DAYS   108.0f
#define STARVE_DAYS     6.0f
#define FEED_MAX        30.0f

#define MAX_BOXES       24000
#define MAX_RES         512
#define MAX_ANIMALS     112
#define MAX_ARROWS      40
#define MAX_PLOTS       15
#define MAX_HENS        5
#define MAX_COWS        3
#define MAX_SHEEP       3
#define MAX_FLOCK       8
#define MAX_RAIDERS     24   // 18 in the four camps, then the two roaming bands of three
#define MAX_LADDERS     8
#define MAX_FIGHTERS    160
#define MAX_VILLAGERS   80
#define MAX_WORKERS     16
#define MAX_CARAVANS    2
#define CARAVAN_GUARDS  2
#define MAX_PLAYERS     6
#define HOUSE_STN       6
#define MAX_STARS       90
#define MAX_TORCHES     176
#define MAX_DYN_LIGHTS  24
#define MAX_PARTICLES   192
#define TRAIL_POINTS    16
#define SAVE_SLOTS      3
#define GROUND_TILES    180
#define TILE_SIZE       6.0f
#define BAG_START       6
#define BAG_MAX         12
#define STORE_SLOTS     24
#define QUIVER_START    10
#define QUIVER_BIG      20
#define FOOD_MAX        100.0f
#define HEALTH_MAX      100.0f
#define HERB_BURST      10.0f
#define HERB_RATE       2.5f
#define HERB_SECONDS    20.0f
#define BOW_DRAW_TIME   1.1f

#define COL_MUD         CLITERAL(Color){ 172, 128,  86, 255 }
#define COL_MUD_LIGHT   CLITERAL(Color){ 186, 142,  98, 255 }
#define COL_MUD_PALE    CLITERAL(Color){ 196, 162, 120, 255 }
#define COL_MUD_DARK    CLITERAL(Color){ 146, 104,  68, 255 }
#define COL_HOUSE       CLITERAL(Color){ 208, 184, 140, 255 }
#define COL_HOUSE_DARK  CLITERAL(Color){ 186, 160, 118, 255 }
#define COL_WALL        CLITERAL(Color){ 198, 172, 126, 255 }
#define COL_WHITEWASH   CLITERAL(Color){ 228, 218, 196, 255 }
#define COL_LAPIS       CLITERAL(Color){  38,  84, 160, 255 }
#define COL_DOORWAY     CLITERAL(Color){  52,  36,  26, 255 }
#define COL_WATER       CLITERAL(Color){  58, 110, 128, 255 }
#define COL_ROAD        CLITERAL(Color){ 168, 148, 110, 255 }
#define COL_PALM_TRUNK  CLITERAL(Color){ 118,  88,  58, 255 }
// Alpha 254..251 asks the shaders for the grain in assets/detail3.png: bark, leaves, tilled earth, straw.
#define COL_BARK        CLITERAL(Color){ 118,  88,  58, 254 }
#define TAG_CROP(c)     ((Color){ (c).r, (c).g, (c).b, 251 })
#define COL_PALM_LEAF   CLITERAL(Color){  62, 118,  52, 253 }
#define COL_DATES       CLITERAL(Color){ 150,  72,  30, 255 }
#define COL_REED        CLITERAL(Color){ 146, 160,  70, 255 }
#define COL_STRAW       CLITERAL(Color){ 190, 168, 104, 255 }
#define COL_GRASS       CLITERAL(Color){  92, 128,  54, 255 }
#define COL_SHRUB       CLITERAL(Color){ 118, 122,  70, 255 }
#define COL_TREE        CLITERAL(Color){  74, 108,  58, 253 }
#define COL_WOOD        CLITERAL(Color){ 112,  80,  50, 255 }
#define COL_LINEN       CLITERAL(Color){ 232, 226, 210, 255 }
#define COL_BLANKET     CLITERAL(Color){ 150,  60,  44, 255 }
#define COL_CLAY        CLITERAL(Color){ 170,  96,  60, 255 }
#define COL_CLAY_DARK   CLITERAL(Color){ 110,  60,  40, 255 }
#define COL_STONE       CLITERAL(Color){ 128, 124, 116, 255 }
#define COL_ROCK        CLITERAL(Color){ 126, 118, 106, 255 }
#define COL_ORE         CLITERAL(Color){  64, 140, 118, 255 }
#define COL_BRONZE      CLITERAL(Color){ 190, 130,  60, 255 }
#define COL_POT         CLITERAL(Color){ 178, 104,  64, 255 }
#define COL_BRICK       CLITERAL(Color){ 160, 112,  76, 255 }
#define COL_SOIL        CLITERAL(Color){  98,  74,  50, 252 }
#define COL_WHEAT_GREEN CLITERAL(Color){ 110, 150,  60, 255 }
#define COL_WHEAT_RIPE  CLITERAL(Color){ 222, 188,  92, 255 }
#define COL_FIRE        CLITERAL(Color){ 255, 150,  50, 255 }
#define COL_SKIN        CLITERAL(Color){ 152, 100,  68, 255 }
#define COL_HAIR        CLITERAL(Color){  40,  30,  24, 255 }

typedef enum {
    IT_NONE, IT_DATES, IT_REEDS, IT_SEEDS, IT_WHEAT, IT_FLOUR, IT_MILK, IT_DOUGH, IT_BREAD,
    IT_EGG, IT_ROAST, IT_ARROWS, IT_GAZELLE, IT_ONAGER, IT_BOAR, IT_GOOSE, IT_WOLF,
    IT_CLAY, IT_POT, IT_WOOD, IT_COPPER, IT_TIN, IT_BRONZE, IT_FISH, IT_GRILLED, IT_COWHIDE, IT_SHEEPSKIN,
    IT_BUCKET, IT_WATER, IT_WASTE, IT_HEN, IT_HERB, IT_SHEEP, IT_PARTRIDGE, IT_COUNT
} ItemId;

typedef struct { const char *name; int stack; int food; Color color; } ItemDef;
typedef struct { int item, count; } Slot;

enum { TOOL_AXE = 1, TOOL_PICK = 2, TOOL_ARROWHEADS = 4, TOOL_NET = 8 };

typedef enum { OUT_ITEM, OUT_HEN, OUT_BASKET, OUT_BOW, OUT_QUIVER, OUT_TOOL, OUT_WEAPON, OUT_ARMOR, OUT_MACE } OutKind;
enum { EQ_NONE, EQ_BOW, EQ_MELEE, EQ_MACE };
enum { ARMOR_NONE, ARMOR_WOLF, ARMOR_LEATHER, ARMOR_BRONZE, ARMOR_KINDS };
enum { SHAPE_BOX, SHAPE_CYL, SHAPE_BLOB, SHAPE_CONE };
typedef struct { int in1, n1, in2, n2; OutKind kind; int out, nOut; int in3, n3; } Offer;

// 'built' marks walls, houses and other works of people, drawn in finer detail up close than plants are.
typedef struct { Vector3 pos, size; Color color; bool solid, water, glow; float torch; unsigned char shape; bool built; } Box;

typedef enum { RES_PALM, RES_REEDS, RES_TREE, RES_CLAY, RES_ORE, RES_FISH, RES_HERB, RES_KINDS } ResKind;
typedef struct { Vector3 pos; ResKind kind; float regrow, spin; bool farm; } Resource;

typedef enum { AN_GAZELLE, AN_ONAGER, AN_BOAR, AN_GOOSE, AN_WOLF, AN_SNAKE, AN_PARTRIDGE, AN_TYPES } AnimalType;
// ST_HUNT: a wolf pack hunting, a wounded boar charging, a snake striking.
typedef enum { ST_CALM, ST_ALERT, ST_FLEE, ST_DEAD, ST_HUNT } AnimalState;

// 'target' is the player a hunting animal is after (meaningful only in ST_HUNT). In a joined game
// 'netPos' and 'netHeading' are where the host last said the animal was; pos eases toward them.
typedef struct {
    AnimalType type;
    AnimalState state;
    Vector3 pos, lair, netPos;
    float heading, speed, timer, awareness, respawn, bite, netHeading;
    int hp, arrowsIn, pack, target;
    bool active;
} Animal;

// 'owner' is the player who shot it, or -1 for someone else's archer (who strikes only players, as
// 'cause'); the damage was fixed when it left the bow.
typedef struct {
    Vector3 pos, vel;
    float life, trailT, fade;
    bool flying, active, spent;
    Vector3 trail[TRAIL_POINTS];
    int trailCount, owner, damage, cause;
} Arrow;

typedef struct { Vector3 pos, vel; float life, maxLife, size; Color color; bool ring; } Particle;

typedef enum { PLOT_EMPTY, PLOT_WHEAT, PLOT_SAPLING, PLOT_PALM, PLOT_DEAD } PlotState;
typedef struct { Vector3 pos; PlotState state; float growth, dry; } Plot;

typedef struct { Vector3 pos; float heading, timer, speed; } Wanderer;
typedef enum { ROLE_CIVILIAN, ROLE_WORKER, ROLE_CLERIC, ROLE_MERCHANT, ROLE_GUARD, ROLE_RAIDER, ROLE_BARBARIAN, ROLE_BANDIT, ROLE_COUNT } Role;

// Combat state shared by every person; while 'override' is set, 'at' replaces their scripted position.
// 'target' is the player they are hostile to (meaningful while 'hostile' runs); 'netAt' is where the
// host last put them in a joined game.
typedef struct {
    float hp, hostile, cool, swing, deadT, windup, markT, foeT;
    Vector3 at, netAt;
    float face;
    int foe, target;
    bool dead, override, looted, used;
    unsigned char mark;
    // A jump over a canal under way (the host's alone): 'jumpDur' runs while it lasts.
    float jumpT, jumpDur;
    Vector3 jumpFrom, jumpTo;
    // An archer drawing its bow ('windup' counts down to the shot).
    bool drawing;
    // Going round something big: the side it keeps to (+1 or -1), how long since it got any nearer.
    signed char detour;
    float detourT, bestLeft;
} Vitals;

enum { MARK_NONE, MARK_WARN, MARK_PARRIED, MARK_HIT };

// NPC_WARDEN: a village's guard, with a bow.
typedef enum { NPC_CIVILIAN, NPC_GUARD, NPC_CLERIC, NPC_VILLAGER, NPC_TRAVELLER, NPC_WARDEN } NpcKind;
// Walks the straight lane a-b, or the whole of road 'road' when it is 0 or more. A townsperson who
// walks the city's streets goes from crossing 'from' to crossing 'to' (a and b are their places),
// may stand a while ('wait') and turns back when someone has blocked the way too long ('stuck').
typedef struct {
    Vector3 a, b;
    float t, dir, speed;
    Color robe;
    NpcKind kind;
    float face, side;
    bool moving, templePost;
    Vitals vit;
    int road;
    float netT, netSide;
    int from, to;
    float wait, stuck;
    int bow;   // 0: no bow; 1-3: the bow they shoot with (hunting, composite, war)
    bool crouch;   // someone at work low down: grinding grain, mending a net, tending a fire
} Villager;
// 'camp' 0 to CAMPS-1 is the camp they live in; CAMPS + b is roaming band b.
typedef struct { Vector3 home, pos, netPos; float face, timer, speed, respawnDay; bool moving; int camp; Vitals vit; } Raider;
typedef struct { Vector3 wall, base, n; float top; } Ladder;

typedef enum { JOB_WOOD, JOB_MINE, JOB_POTTER, JOB_KILN, JOB_SMITH, JOB_BOAT, JOB_DOCK, JOB_FARM } JobKind;

typedef struct {
    JobKind job;
    Vector3 hut, spot, pos;
    float facing, phase;
    bool visible, walking;
    Vitals vit;
} Worker;

typedef enum { CV_AWAY, CV_ARRIVE, CV_TRADE, CV_LEAVE } CaravanState;

typedef struct {
    CaravanState state;
    Vector3 path[6];
    int points, trader;
    float s, length, timer, blockedT, pushT, netS;
    Color robe, cloth;
    Vitals vit[1 + CARAVAN_GUARDS];
} Caravan;

typedef enum { TR_BOWYER, TR_FARMER, TR_WEAVER, TR_HERDER, TR_SMITH, TR_HUNTER, TR_SHEPHERD, TR_FISHER, TR_DILMUN, TR_ELAM, TR_COUNT } TraderId;

typedef struct {
    const char *name, *greeting;
    Vector3 pos;
    float facing;
    Color robe;
    const Offer *offers;
    int offerCount;
    bool roaming, away;
    Vitals vit;
} Trader;

typedef enum {
    STN_BED, STN_STORAGE, STN_QUERN, STN_TROUGH, STN_OVEN, STN_KILN, STN_SMELTER, STN_BENCH, STN_ALTAR, STN_STASH, STN_STASH2,
    STN_STASH3, STN_STASH4, STN_WELL, STN_DITCH, STN_FERRY_E, STN_FERRY_W, STN_COUNT
} StationId;
typedef enum { PANEL_NONE, PANEL_BAG, PANEL_STORAGE, PANEL_TRADE, PANEL_PAUSE, PANEL_HELP, PANEL_COOP, PANEL_LOOK, PANEL_CONSOLE, PANEL_SETTINGS } Panel;
typedef enum { TGT_NONE, TGT_CARCASS, TGT_RES, TGT_STATION, TGT_PLOT, TGT_COOP, TGT_TRADER, TGT_BODY, TGT_OFFER, TGT_LADDER, TGT_WATER } TargetKind;
enum { SPRING, SUMMER, FALL, WINTER };
typedef struct { TargetKind kind; int index; } Target;

typedef struct {
    Vector3 pos;
    float vy, facing, stride;
    float yaw, pitch, dist;
    float draw;
    bool grounded, crouching, aiming, moving, running;
} Player;

// How a person looks: colors, then hair, beard, dress, jewellery, headgear and armor (the enums below).
typedef struct {
    Color skin, cloth, trim, hair;
    unsigned char hairStyle, beard, dress, jewel, head, armor;
    bool woman;
} Look;
enum { HAIR_SHAVED, HAIR_SHORT, HAIR_LONG, HAIR_BUN, HAIR_KINDS };
enum { BEARD_NONE, BEARD_SHORT, BEARD_FULL, BEARD_LONG, BEARD_KINDS };
enum { DRESS_KILT, DRESS_FLEECE, DRESS_SHAWL, DRESS_ROBE, DRESS_KINDS };
enum { JEWEL_NONE, JEWEL_BEADS, JEWEL_GOLD, JEWEL_BANDS, JEWEL_KINDS };
enum { HEAD_NONE, HEAD_HELMET, HEAD_CAP, HEAD_STONECAP, HEAD_HORNS, HEAD_TALL };
enum { LOOK_TOWN, LOOK_VILLAGER, LOOK_TRAVELLER, LOOK_MERCHANT, LOOK_WORKER, LOOK_PRIEST, LOOK_GUARD, LOOK_WARDEN, LOOK_SOLDIER,
       LOOK_RAIDER, LOOK_BARBARIAN };
enum { HAND_WEAPON, HAND_OTHER };
enum { WEAPON_CLUB, WEAPON_COPPER, WEAPON_SWORD, WEAPON_MACE };

// Every player has a house of their own: bed, storage, quern, trough, workbench and oven.
typedef struct { Vector3 center, door; Vector3 stations[HOUSE_STN]; float bedFacing; } House;

// Playing alone, hosting a game others join, or playing in someone else's.
typedef enum { NET_SOLO, NET_HOST, NET_CLIENT } NetRole;
// PF_SLEEP: asleep or out cold (nothing attacks them); PF_BED: lying in bed waiting for the night to pass.
// A swing's length (also how the weapon is drawn), and how long a blow on another player takes to land
// from the start of the swing: the warning the one about to be hit gets.
#define SWING_TIME   0.45f
#define MACE_TIME    0.55f
#define PVP_WINDUP   0.3f
#define NPC_WINDUP   0.55f          // a person's blow lands this long after it begins (warning to contact)
#define NPC_SWING    0.35f          // and its follow-through takes this long
#define CONTACT_TIME PVP_WINDUP     // a swing's weapon meets its target this long after it starts, whatever it hits
#define HITSTOP_TIME 0.07f          // a landed blow holds the swing still this long (the mace a little longer)
#define FLINCH_TIME  0.45f
enum { PF_CROUCH = 1, PF_RUN = 2, PF_MOVE = 4, PF_GROUND = 8, PF_AIM = 16, PF_SLEEP = 32, PF_CLIMB = 64, PF_PARRY = 128, PF_BED = 256, PF_TORCH = 512 };
#define NAME_LEN 16
#define SAFE_RADIUS 22.0f

// What every machine knows of each player. The owner reports where it is and what it is doing; the
// host adds what only it can judge (alone outside the walls, wanted by the guards, wolves' anger).
typedef struct {
    bool active, alone;
    char name[NAME_LEN];
    Vector3 pos, shown;
    float facing, shownFacing, stride, draw, health, swingT, wanted, aggro, heard;
    int flags, equip, weapon, bow, armor, net;
    unsigned short style;   // how they chose to look (PackStyle, and STYLE_WOMAN)
    int kills, deaths;   // players brought down by this one, and times brought down by a player
    // The warning mark over this player's head while they swing at someone, then how it came out
    // (MARK_*, the host decides); and how long since this machine saw them hurt (a flinch).
    int mark;
    float markT, hitT, hitX, hitZ, lastHealth, lastSwing;
} Peer;

// Everything a player owns and where they stand: kept by the host for each name, saved with its game.
typedef struct {
    char name[NAME_LEN];
    int slot;
    Vector3 pos;
    float yaw, pitch, food, health, blessT;
    int bagSlots, quiver, quiverMax, bow, tools, weapon, equip, armor, armorOwned, prayDay;
    bool hasMace, templeAccess;
    Slot bag[BAG_MAX], store[STORE_SLOTS];
} PlayerRecord;
#define MAX_RECORDS 24

// Bytes for saves and network messages, little-endian. 'bad' is set on overflow or a short read.
typedef struct { unsigned char *d; int n, cap; bool bad; } Writer;
typedef struct { const unsigned char *d; int n, at; bool bad; } Reader;

enum { LINK_OFF, LINK_CONNECTING, LINK_OPEN, LINK_CLOSED };
// What the relay server says; S_DATA carries a message from another player.
enum { S_HELLO = 1, S_JOIN, S_LEAVE, S_FULL, S_NOHOST, S_TAKEN, S_HOSTLEFT, S_DATA = 16 };
typedef struct { int kind, from; const unsigned char *data; int len; } NetMessage;

// What a guest asks the host before it changes the shared world (shots and blows need no answer).
enum {
    RQ_SHOOT = 1, RQ_STRIKE, RQ_CARCASS, RQ_GATHER, RQ_STASH, RQ_PLOT, RQ_DITCH, RQ_COOPWATER, RQ_EGGS, RQ_FEED,
    RQ_SLAUGHTER, RQ_HEN, RQ_LOOT, RQ_COLLAPSE
};
#define REQ_ARGS 12
enum { WARN_WOLF, WARN_SNAKE, WARN_KINDS };
// What hurt a player last, so whoever is brought down hears by whom: BY_PLAYER + slot for a player.
enum { BY_NONE, BY_GUARD, BY_CARAVAN, BY_TOWNSFOLK, BY_VILLAGERS, BY_RAIDER, BY_BARBARIAN, BY_WOLF, BY_BOAR, BY_SNAKE, BY_BANDIT, BY_PLAYER };

typedef struct {
    Box boxes[MAX_BOXES];
    int boxCount;
    Resource res[MAX_RES];
    int resCount;
    Animal animals[MAX_ANIMALS];
    int animalCount;
    Arrow arrows[MAX_ARROWS];
    Plot plots[MAX_PLOTS];
    Wanderer hens[MAX_HENS];
    float henBorn[MAX_HENS], henMeal[MAX_HENS];
    int henCount, coopEggs;
    float coopFeed, layClock;
    float farmWater, coopWater, coopDry, rainStart, rainEnd, blessT;
    bool rainToday, rainDone, raining;
    int seasonSeen, slaughterHen, slaughterStep;
    Wanderer cows[MAX_COWS];
    Wanderer sheep[MAX_SHEEP];
    Wanderer flock[MAX_FLOCK];
    Raider raiders[MAX_RAIDERS];
    Slot stash[CAMPS][4];
    int stashDay;
    Ladder ladders[MAX_LADDERS];
    int ladderCount, climb, climbLadder, prayDay;
    float climbT;
    int equip, armor, armorOwned, dawnDay;
    float drawFog, parryT, parryCool;
    int mapZoom;
    bool templeAccess, wasFocused, firstPerson;
    Villager villagers[MAX_VILLAGERS];
    int villagerCount;
    Worker workers[MAX_WORKERS];
    int workerCount;
    Caravan caravans[MAX_CARAVANS];
    Trader traders[TR_COUNT];
    Vector3 stations[STN_COUNT];
    House houses[MAX_PLAYERS];
    Vector3 home, coop, pen;
    Vector3 stars[MAX_STARS];
    Vector3 torches[MAX_TORCHES];
    int torchCount;
    Vector3 dynLights[MAX_DYN_LIGHTS];
    int dynCount;
    float night, torchGlow, drawTorch;   // torchGlow: how bright torches burn, from the evening on
    float tileTorch[GROUND_TILES][GROUND_TILES];
    Particle particles[MAX_PARTICLES];
    bool confirmNew, requestNew;
    int requestSlot;
    float heights[GROUND_TILES + 1][GROUND_TILES + 1];
    Color groundTint[GROUND_TILES][GROUND_TILES];

    Player player;
    Camera3D cam;
    float camDist, camShoulder;

    Slot bag[BAG_MAX];
    int bagSlots;
    Slot store[STORE_SLOTS];
    int quiver, quiverMax, bow, tools, weapon;
    bool hasMace;
    float swingT, swingCool, hitMarkT, impactT, herbT;
    float hitStop, shakeT, shakeAmp;   // a landed blow: the swing held still, the view kicked
    bool strikePending;                // swinging, the weapon has not met anything yet
    bool meleeing;                     // a swing's contact is being played out (not an arrow)
    // Where the last blow on this player came from, as red lights at the screen's edges (front, back, left,
    // right) that fade, and the flinch of the body: how long it lasts and the way it was pushed.
    float hitLight[4], flinchT, flinchX, flinchZ;
    Vector3 impact;
    int followArrow;
    float followT, camBlend;
    Vector3 followPos, followDir;
    bool arrowCam;
    // The player's torch: wanted (B), in hand and burning, and how far through the second it takes to take
    // it up or put it away (it waits for that after a shot or a change of weapon too).
    bool torchOn, torchHeld;
    float torchT;
    float food, hungerNag;
    float health, hurtT, safeT;
    // How much of each thing in the baskets the player had when the day began (or took out of
    // storage since): the rest was gathered today and is lost if they are brought down.
    int safe[IT_COUNT], safeDay, hurtBy;

    int day;
    float hour, daylight;
    Vector3 light;
    Color sky;
    float sleepT;
    bool sleeping, slept;

    char msg[384];
    float msgT;
    char promptE[192], promptF[192];
    bool okE, okF;
    Panel panel;
    int trader;
    Target target;
    bool cursorLocked, ignoreClick, quit, wasCaptured;
    float escQuiet;   // a moment after the mouse was released by Esc, Esc does nothing
    bool devTools;    // the host (or a player alone) turned on dev tools: the dev console can be opened
    bool god;         // dev console: nothing hurts this player
    float devSpeed;   // dev console: walking speed times this (0 means 1)
    float autosaveT;

    NetRole role;
    int self, actor;
    Peer peers[MAX_PLAYERS];
    PlayerRecord records[MAX_RECORDS];
    int recordCount;
    bool waitBed;
    // The host's rules for a shared game: players can hurt each other ('pvp'), and in combat mode
    // everyone fights with the best of everything and the guards leave player fights alone.
    bool pvp, combat;
} Game;

extern Game G;
extern const ItemDef ITEMS[IT_COUNT];
extern const char *const BOW_NAMES[3];
const char *ArmorName(int armor);
float ArmorReduction(int armor);
// The gear a player fights with: their own, or the best of everything in combat mode (items.c).
int UseBow(void);
int UseWeapon(void);
int UseArmor(void);
bool UseMace(void);
bool UseArrowheads(void);

float RandF(float lo, float hi);
float Hash2(int a, int b);
float XZDist(Vector3 a, Vector3 b);
Color Mix(Color a, Color b, float t);
Color Lit(Color c, float shade);
void DrawShadedBox(Vector3 c, Vector3 s, Color col);
void DrawGlowBox(Vector3 c, Vector3 s, Color col);
void DrawShadedCylinder(Vector3 base, float rx, float rz, float h, float topScale, int sides, Color col);
void DrawShadedBlob(Vector3 c, Vector3 r, Color col);
void DrawShadedBlobFar(Vector3 c, Vector3 r, Color col);
void DrawShadedBlobFine(Vector3 c, Vector3 r, Color col);
void DrawBeveledBox(Vector3 c, Vector3 s, Color col, float bevel);
Color Clothe(Color cloth, Color skin);
void VPaint(Color c, Vector3 n);
float DrawFlash(void);
void DrawHuman(Vector3 pos, float facing, float stride, Look look, bool crouch, bool bow, float draw);
extern void (*ExtraFigures)(void);
void GripHere(int hand, float x, float y, float z);
void GripAt(int hand, float x, float y, float z);
void HoldBow(Vector3 pos, float facing, bool crouch);
void HoldBowFor(Vector3 pos, float facing, bool crouch, bool torch);
void SlungBow(Vector3 pos, float facing);
void DrawBow(Vector3 grip, Vector3 nock, float pull);
Look PlayerLook(int slot, unsigned short style, int armor);
Look PersonLook(int role, unsigned seed, Color robe);
#define STYLE_WOMAN 256   // a woman's body (slighter, no beard), above the four two-bit choices
unsigned short PackStyle(int hair, int beard, int dress, int jewel);
unsigned short ChosenStyle(void);
bool StyleChosen(void);
void ChooseStyle(unsigned short style);
const char *HairName(int k);
const char *BeardName(int k);
const char *DressName(int k);
const char *JewelName(int k);

void AddTorch(float x, float y, float z, bool pole);
void AddFireLight(float x, float y, float z);
void BuildLights(void);
void UpdateLights(void);
bool TorchesLit(void);
float TorchLightAt(Vector3 p);
void LightFor(Vector3 p);
float FogAt(Vector3 p);
float BoxLight(int i);
float TileLight(int i, int j);
void DrawTorches(void);
void DrawHandTorch(Vector3 pos, float facing);

void SpawnBurst(Vector3 pos, Color c, int count, float speed);
void SpawnRing(Vector3 pos, Color c);
void UpdateEffects(float dt);
void DrawEffects(void);

enum { OWNER_PLAYER = 1, OWNER_VILLAGER = 1000, OWNER_WORKER = 2000, OWNER_TRADER = 3000, OWNER_CARAVAN = 4000, OWNER_RAIDER = 5000 };
// Each caravan member has its own id, so a raider fighting one guard does not chase the other.
#define CARAVAN_OWNER(c, k) (OWNER_CARAVAN + (c) * 8 + (k))
void CollectBodies(void);
Vector2 PushOutBodies(Vector2 p, float r, int self);
bool BodyAhead(Vector3 from, Vector3 to, float r, int self);
bool BodyAheadIgnoring(Vector3 from, Vector3 to, float r, int self, int ignoreLo, int ignoreHi);
bool BodyAt(Vector3 p, float r, int self);

bool SaveGame(void);
bool LoadGame(void);
void DeleteSave(void);
int SaveSlot(void);
void SetSaveSlot(int slot);
const char *SaveFileName(int slot);
bool SaveSummary(int slot, int *day, float *hour);
void RememberSlot(void);
int LastSlot(void);

float TerrainHeight(float x, float z);
float MeshHeight(float x, float z);
float GroundSurface(float x, float z);
int ChannelCount(void);
Rectangle ChannelRect(int i, bool *wet, Rectangle *surface);
void DrawWater(void);
float RayTerrain(Vector3 o, Vector3 dir, float maxT);
float Fertility(float x, float z);
bool WestBank(float x);
void BuildTerrain(void);
void DrawGround(void);
float GroundTorch(float x, float z);

// Main roads outside the walls; city streets are only drawn on the map.
enum { ROAD_SOUTH, ROAD_EAST, ROAD_HERDERS, ROAD_NORTH, ROAD_FERRY, ROAD_WEST_NORTH, ROAD_WEST_SOUTH, ROADS };
void BuildRoads(void);
void DrawRoads(void);
float RoadDistance(float x, float z);
float RoadLength(int road);
Vector3 RoadPoint(int road, float s, Vector3 *dir);
void DrawRoadMap(float mx, float mz, float scale);

#define HUNTERS_X       440.0f
#define HUNTERS_Z       20.0f
#define HERDERS_X       (-62.0f)
#define HERDERS_Z       425.0f
#define FISHERS_X       (-188.0f)
#define FISHERS_Z       (-58.0f)
enum { VIL_HUNTERS, VIL_HERDERS, VIL_FISHERS, VILLAGES };
void BuildVillages(void);
Vector3 VillageCenter(int v);
const char *VillageName(int v);
int VillageAt(Vector3 p);
Rectangle FoldArea(void);

bool InsideWalls(float x, float z, float margin);
bool InCityLands(float x, float z);
int BridgeCount(void);
void BridgeEnds(int i, Vector3 *a, Vector3 *b);
void BuildCity(void);
void BuildCityHouses(void);
void BuildTemple(void);
bool InTempleInterior(Vector3 p);
void OfferPrompt(char *buf, int size, bool *ok);
void MakeOffering(void);
Vector3 GatePos(void);

void AddBox(float x, float y0, float z, float w, float h, float d, Color c, bool solid);
void AddBoxOn(float x, float z, float w, float h, float d, Color c, bool solid);
void AddShape(float x, float y0, float z, float w, float h, float d, Color c, bool solid, int shape);
void AddWater(float x0, float z0, float x1, float z1);
void AddPond(float cx, float cz, float rx, float rz);
void AddGlow(float x, float y0, float z, float w, float h, float d, Color c);
void AddTree(float x, float z, float scale, bool poplar);
void KeepOut(float x0, float z0, float x1, float z1);
bool InKeepOut(float x, float z, float margin);
bool FreeSpot(float x, float z, float margin);
int AddResource(float x, float z, ResKind kind);
void BuildWorld(void);
bool IsBlocking(const Box *b, float y);
Vector2 PushOut(Vector2 p, float radius, const Box *b);
float GroundHeight(float x, float z, float y);
float CeilingHeight(float x, float z, float y);
bool InWater(float x, float z);
float WadeSpeed(float x, float z);
bool PointInSolid(Vector3 p);
bool SolidAt(float x, float z, float radius);
void UpdateSky(void);
void DrawSky(void);
void DrawTerrain(void);
void DrawResources(void);
void DrawResourcesOf(bool still, bool moving);
void DrawResource(int i);
void DrawBoxAt(int i, float d2);
Vector3 SunDirection(void);

int SlotsCount(const Slot *s, int n, int item);
int SlotsSpace(const Slot *s, int n, int item);
int SlotsAdd(Slot *s, int n, int item, int count);
int SlotsRemove(Slot *s, int n, int item, int count);
int MoveStack(Slot *src, Slot *dst, int dn);
int BagCount(int item);
int BagAdd(int item, int count);
int BagRemove(int item, int count);
int GiveArrows(int n);
void RefillQuiver(void);
void RefillQuiverFromStore(void);
bool EatSlot(int slot);
bool UseHerb(void);
bool OfferCheck(const Offer *o, const char **why);
bool OfferApply(const Offer *o);
void OfferPay(const Offer *o);
const char *OfferOutput(const Offer *o);
#if defined(__GNUC__)
#define PRINTF_LIKE(f, a) __attribute__((format(printf, f, a)))
#else
#define PRINTF_LIKE(f, a)
#endif
void Say(const char *fmt, ...) PRINTF_LIKE(1, 2);
void Fmt(char *buf, int size, const char *fmt, ...) PRINTF_LIKE(3, 4);

void UpdatePlayer(float dt, bool input);
void SetEquip(int eq);
void StartClimb(int ladder, bool up);
const char *EquipName(int eq);
void FollowCamera(float dt);
void DrawPlayer(void);
float SwingElapsed(float swingT, bool mace);
float WeaponAngle(float t);
Matrix ArmFrame(float arm, float reach, float blade);
Matrix SwordFrame(float t);
Matrix BlowFrame(float blade);
void HoldWeapon(Vector3 pos, float facing, bool crouching, bool mace, float t, bool guard);
void DrawWeaponPart(int kind);
float MaceYaw(float t);
float SwingLean(float t, bool mace);
float BlowAngle(float windup, float swing, float rest);
float SpearThrust(float windup, float swing);
void BeginLean(Vector3 at, Vector2 dir, float degrees);
void EndLean(void);
Look Flushed(Look look, float k);

void InitWildlife(void);
void UpdateWildlife(float dt);
void DrawWildlife(void);
int ShootArrow(Vector3 from, Vector3 to, float power);
void UpdateArrows(float dt);
void DrawArrows(void);
Vector3 AimPoint(void);
float BowSpeed(void);
const char *AnimalName(int type);
int CarcassItem(int type);
float AnimalTop(const Animal *a);
bool AnimalHidden(const Animal *a);
bool AnyWolfHunting(void);
bool AnyBeastAttacking(void);
void CalmBeasts(int slot);
int KillAnimalsNear(Vector3 p, float r);
void ShootFor(int slot, Vector3 from, Vector3 vel, int damage);
void LooseFor(Vector3 from, Vector3 to, float speed, int damage, int cause);
enum { STRIKE_START, STRIKE_CONTACT };
bool StrikeFor(Vector3 chest, Vector3 fwd, int equip, int weapon, int phase);
bool StrikeWouldMeet(Vector3 chest, Vector3 fwd, int equip, Vector3 *at);
void DrawArrow(const Arrow *ar);
void DrawArrowTrails(const Arrow *arrows);
int AnimalMaxHp(int type);
void MeleeStrike(int phase);
const char *WeaponName(int tier);
const char *MaceName(void);

void BuildVillage(void);
Color PlayerColor(int slot);
const char *PlayerColorName(int slot);
void SetHome(int slot);
Vector3 CitySpawn(int slot);
void RemoveZigguratGuards(void);
int AddNpc(NpcKind kind, Vector3 a, Vector3 b, float speed, Color robe, float face);
void UpdateVillage(float dt);
void AdvanceHours(float h);
void DrawVillage(void);
Vector3 VillagerPos(const Villager *v);
Vector3 VillagerLanePos(const Villager *v);
int StreetNodeCount(void);
Vector3 StreetNode(int n);
void SetStreet(Villager *v, int from, int to);
Vector3 TraderPos(const Trader *t);
void DrawPersonBody(Vector3 pos, float facing, Look look);
void UpdateCombat(float dt);
void CollectFighters(void);
int FighterByOwner(int owner);
int FighterOwner(int i);
void CalmPeople(int slot);
bool ClaimBody(int i);
void GiveLoot(int i);
void DamagePerson(int i, float dmg);
int PersonOnSegment(Vector3 a, Vector3 b);
int PersonInFront(Vector3 chest, Vector3 fwd, float range, float *dist);
int PeopleInArc(Vector3 chest, Vector3 fwd, float range, float minDot, int *out, int max);
bool FighterIsFoe(int i);
int FighterCount(void);
bool FighterInfo(int i, Vector3 *pos, float *hp, float *maxHp, const char **name, bool *hostile, bool *dead);
void ConsoleRun(const char *text);
float LandingHeight(float x, float z);
void SaveGamePrefs(void);
void LoadGamePrefs(void);
int ConsoleLines(void);
const char *ConsoleLine(int i);
const char *ConsoleLast(void);
int FighterMark(int i);
bool BodyLootable(int i);
void HitPlayer(int s, float dmg, Vector3 from, bool melee);
int PlayerOnSegment(Vector3 a, Vector3 b, int skip);
int PlayersInArc(Vector3 chest, Vector3 fwd, float range, float minDot, int skip, int *out, int max);
void ForgetFights(int slot);
void RespawnPeople(void);
void BuildRaiders(void);
void UpdateRaiders(float dt);
void DrawRaiders(void);
bool RaiderBowman(int i);
bool RaiderBandit(int i);
void StashPrompt(int camp, char *buf, int size, bool *ok);
void TakeStash(int camp);
bool ClaimStash(int camp, const int *want, int *res, Vector3 by);
void StashApply(bool ok, const int *res);
int StashCamp(int station);
Vector3 CampCenter(int camp);
void StationPrompt(int s, char *buf, int size, bool *ok);
void UseStation(int s);
void PlotPrompt(int p, char *e, char *f, int size, bool *okE, bool *okF);
void UsePlot(int p, bool alt);
enum { PLOT_SOW, PLOT_PIT, PLOT_REAP, PLOT_CLEAR };
bool PlotServe(int p, int action);
void PlotApply(int action, bool ok);
int PourServe(bool coop);
void PourApply(bool coop, bool ok, int after);
int FeedServe(bool wheat, int n, int *feed);
void FeedApply(bool wheat, int n, int feed);
int EggsServe(int most);
void EggsApply(int n);
bool SlaughterServe(int i, int key);
void SlaughterApply(bool ok);
void CoopPrompt(char *e, char *f, int size, bool *okE, bool *okF);
void UseCoop(bool alt);
void CollectEggs(void);
void FeedSeeds(void);
void FeedWheat(void);
void PourCoop(void);
bool SlaughterHen(int i);
bool AddHen(void);
void BuildFarm(void);
void DrawFarm(void);
void FarmHours(float h);
void NewDay(void);
bool RainBefore(float hour);
bool CropStage(float lead, float *growth, bool *stubble);
void UpdateFields(void);
bool FieldBox(int i);
void DrawFields(void);
void UpdateWeather(void);
void DrawRain(void);
int Season(void);
int SeasonDay(void);
const char *SeasonName(int s);
float NowDays(void);
float DaysToWinter(void);
void DitchPrompt(char *buf, int size, bool *ok);
void PourDitch(void);
bool NearWater(Vector3 p);
void FillBucket(void);
void Exert(float food);
void BlessPrompt(char *buf, int size, bool *ok);
void SpringRespawn(void);
int CityFarmCount(void);
Vector3 CityFarmSpot(int i);
void AnimalsAwaitingSpring(unsigned char *mask, int bytes);
void HoldAnimalsForSpring(const unsigned char *mask, int bytes);
void ForgePrompt(char *buf, int size, bool *ok);
void UseForge(void);
void CloakPrompt(char *buf, int size, bool *ok);
void SewCloak(void);

void BuildJobs(void);
void UpdateWorkers(void);
void DrawWorkers(void);
bool PeopleNear(Vector3 p, float r);

void BuildCaravans(void);
void UpdateCaravans(float dt);
void DrawCaravans(void);
bool CaravanNear(Vector3 p, float r);
bool CaravanMember(int c, int k, Vector3 *pos, float *facing);
bool CaravanMemberScript(int c, int k, Vector3 *pos, float *facing);
Vector3 WorkerScriptPos(const Worker *w);

enum { LANG_EN, LANG_TR };
typedef struct { const char *en, *tr; } Phrase;
extern const Phrase TR_PHRASES[];
extern const int TR_PHRASE_COUNT;
const char *L(const char *en);
bool HasTranslation(const char *en);
int Language(void);
void SetLanguage(int lang);
void SaveLanguage(void);
void LoadLanguage(void);
const char *ItemName(int item);
Font GameFont(void);

void SetPanel(Panel p);
void DrawUI(void);

void GameInit(void);
void GameUpdate(void);
void GameDraw(void);
void StartSleep(void);
void HurtPlayer(float dmg, int by);
void HurtPlayerFrom(float dmg, int by, const Vector3 *src);
void WakeAfterNight(float hours, bool rained, bool dawn);
void TrackGoods(void);
void PassHours(float h);

// Browser storage, pointer capture and the page's address (web.c); plain files and no-ops natively.
const char *DataPath(const char *name);
void MountStorage(void);
bool StorageReady(void);
void SyncStorage(void);
void WantPointer(bool captured);
bool PointerCaptured(void);
void WatchClicks(void);
void PageFrame(void);
bool MousePressed(int button);
bool KeyPressed(int key);
Vector2 MouseLook(void);
const char *PageParam(const char *key);
void ServerAddress(char *buf, int size, const char *query);
bool OnWeb(void);
bool OnPhone(void);
Vector2 PointerPosition(void);
float PixelRatio(void);

// The connection to the relay server (net.c).
bool NetOpen(bool host, const char *room);
void NetShut(void);
int NetLink(void);
bool NetSend(int to, const void *data, int len);
bool NetPoll(NetMessage *m);

// Every player as the world sees them (players.c).
void SyncSelf(void);
bool PlayerIn(int s);
bool PlayerAwake(int s);
Vector3 PlayerAt(int s);
int NearestPlayer(Vector3 p, float *dist);
float NearestAnyone(Vector3 p);
float PlayerStance(int s);
void UpdatePlayerSafety(float dt);
bool Warn(int s, int kind, float quiet);
void HurtSlot(int s, float dmg, int by);
void HurtSlotFrom(int s, float dmg, int by, const Vector3 *src);
void HitMark(void);
void ShowImpact(Vector3 at);
void SayTo(int s, const char *fmt, ...) PRINTF_LIKE(2, 3);
void Announce(const char *fmt, ...) PRINTF_LIKE(1, 2);
void SayNear(Vector3 at, float r, const char *fmt, ...) PRINTF_LIKE(3, 4);
void UpdatePeers(float dt);
void DrawPeers(void);
void DrawInBed(int s, Vector3 at, float facing, Look look);
int PlayerCount(void);
int PlayersInBed(void);

// Multiplayer messages (mp.c).
int WorldSignature(void);
bool Online(void);
bool IsClient(void);
void NetEffect(Vector3 pos, Color c, int count, float speed, bool ring);
const Arrow *NetArrows(void);
void DrawNetArrows(void);
void LocalSayV(const char *fmt, va_list ap);
void NetSayV(int slot, const char *fmt, va_list ap);
void NetHurt(int slot, float dmg, int by, const Vector3 *src);
void NetStunned(int slot, float secs);
void NetHitMark(int slot, bool arrow);
void NetBlowLanded(int slot, Vector3 at, bool heavy);
void NetImpact(int slot, Vector3 at);
void NetParried(int slot);
void NetSkip(float hours, bool rained, bool dawn);
bool Asking(int kind, int first);
void Ask(int kind, const int *args, int n);
void NetShoot(Vector3 from, Vector3 vel, int damage);
void NetStrike(Vector3 chest, Vector3 fwd, int equip, int weapon, int phase);
void NetSendRecord(void);
void NetSaveSoon(void);
float NetSilence(void);
bool NetBegin(bool host, const char *name, const char *room);
int NetLobby(void);
const char *NetFailure(void);
const char *NetRoom(void);
void NetLeave(void);
void NetFrame(float dt);
void NetRules(bool pvp, bool combat, bool dev);
const char *RulesText(void);
const char *KillDeath(int s);

// Save files and the records the host keeps for each player (save.c).
void W8(Writer *w, unsigned v);
void W16(Writer *w, unsigned v);
void W32(Writer *w, unsigned v);
void WF(Writer *w, float f);
void WStr(Writer *w, const char *s);
unsigned R8(Reader *r);
unsigned R16(Reader *r);
unsigned R32(Reader *r);
float RF(Reader *r);
void RStr(Reader *r, char *out, int max);
void WSlots(Writer *w, const Slot *s, int n);
void RSlots(Reader *r, Slot *s, int n);
bool WriteFile(const char *name, const Writer *w);
int ReadFile(const char *name, unsigned char *buf, int size);
void WriteRecord(Writer *w, const PlayerRecord *r);
bool ReadRecord(Reader *rd, PlayerRecord *r);
void CaptureRecord(PlayerRecord *r);
void ApplyRecord(const PlayerRecord *r);
bool SameName(const char *a, const char *b);
PlayerRecord *FindRecord(const char *name);
void KeepRecord(const PlayerRecord *r);
bool SaveNetGame(void);
bool LoadNetGame(void);
bool NetSaveSummary(int *day, float *hour);
void DeleteNetGame(void);

// Things a player does to the shared world: the host does them, a guest asks first (actions.c).
void Act(int kind, const int *args, int n);
bool ServeRequest(int slot, int kind, const int *args, int n, int *res, int *nres);
void ApplyReply(int kind, const int *args, int n, bool ok, const int *res, int nres);
void FindTarget(bool play);
void Interact(bool alt);
void Barter(int trader, int offer);
bool TraderWilling(int i);
void SaveNow(void);
void CalmFor(int slot);

// The title screen in the browser: name, play alone, host or join (lobby.c).
void OpenLobby(const char *why);
bool InLobby(void);
void LobbyFrame(void);
// The UI's own units (ui.c): a small screen gets the UI shrunk as if it were 960 x 540.
float UiScale(void);
float UiWidth(void);
float UiHeight(void);
Vector2 UiMouse(void);
void UiBegin(void);
void UiEnd(void);
void LeaveToLobby(const char *why);
const char *PlayerName(void);

// The music, made by a small synthesizer: the morning tune, the fight and the night (music.c).
void MusicInit(void);
void MusicUpdate(float dt, bool inGame);
void MusicClose(void);
int MusicLevel(void);              // 0 off, 1 low, 2 medium, 3 high
void MusicSetLevel(int level);
const char *MusicLevelName(void);
const char *MusicTitle(int mood);
int MusicMood(void);               // 0 morning, 1 the fight, 2 the night
void MusicRender(int mood, float *out, int frames);
float MusicPartBeats(int mood, int part);

// How a blow feels (effects.c): the swing held still a moment, the view kicked, sparks and blood, the one
// struck flashing; and the sounds (sfx.c).
void BlowLanded(Vector3 at, bool heavy);
void BlowTurnedAside(Vector3 at);
void BlowTaken(void);
Vector3 ChestAhead(void);
void AddFlash(Vector3 at);
float FlashAt(Vector3 p);
void UpdateHitFeel(float dt);
void SetDrawFlash(float k);
void DrawSwingTrails(void);
enum { SFX_SWING, SFX_SWING_HEAVY, SFX_HIT, SFX_HIT_HEAVY, SFX_CLANG, SFX_HURT, SFX_ARROW,
       SFX_STEP_GRASS, SFX_STEP_EARTH, SFX_STEP_HARD, SFX_STEP_WATER, SFX_COUNT };
void SfxPlay(int id, float volume);
void SfxPlayAt(int id, Vector3 at, float volume, float reach);
int SfxLevel(void);
void SfxSetLevel(int level);
void SfxLoadSetting(void);
const char *SfxLevelName(void);
void SfxAmbience(float dt, bool inGame);
void SfxExport(const char *dir);

// The shader renderer and its setting (gfx.c). While GfxOn() the draw code hands out plain colors
// (Lit() leaves them as they are), a material and normals, and the shaders do the light. The vertex
// stream below is how every piece of the world is drawn, whichever way it goes.
enum { GFX_AUTO, GFX_HIGH, GFX_LOW, GFX_CLASSIC, GFX_CHOICES };
enum { MAT_THING, MAT_GROUND, MAT_WATER, MAT_GLOW, MAT_FIGURE, MAT_BUILT, MAT_UNLIT };
void GfxLoadSetting(void);
int GfxChoice(void);               // what the player picked, GFX_AUTO included
int GfxTier(void);                 // what runs: GFX_HIGH, GFX_LOW or GFX_CLASSIC
void GfxSetChoice(int choice);
const char *GfxChoiceName(int choice);
const char *GfxLabel(void);
bool GfxOn(void);
void GfxFlame(Vector3 p, float size);
bool TorchBurning(void);
bool DrawingAll(void);             // drawing the whole map once for the shaders' meshes: no culling
void GfxWorldChanged(void);
bool GfxDrawWorld(void);           // the 3D world through the shaders; false when the classic way must draw it
void SetMaterial(int material);
int CurrentMaterial(void);
void VBegin(int mode);
void VEnd(void);
void VColor(Color c);
void VNormal(float x, float y, float z);
void VVertex(float x, float y, float z);

#endif
