#ifndef SUMER_GAME_H
#define SUMER_GAME_H

#include "raylib.h"
#include "raymath.h"
#include <stdbool.h>

#define WORLD_HALF      360.0f
#define WORLD_LIMIT     354.0f
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
#define CAMPS           2
#define RIVER_X0        (-150.0f)
#define RIVER_X1        (-128.0f)
#define FORD_Z0         (-338.0f)
#define FORD_Z1         (-326.0f)
#define SEASON_DAYS     9
#define WATER_DAYS      6.0f
#define HEN_LIFE_DAYS   108.0f
#define STARVE_DAYS     6.0f
#define FEED_MAX        30.0f

#define MAX_BOXES       9500
#define MAX_RES         320
#define MAX_ANIMALS     48
#define MAX_ARROWS      40
#define MAX_PLOTS       15
#define MAX_HENS        5
#define MAX_COWS        3
#define MAX_SHEEP       3
#define MAX_RAIDERS     8
#define MAX_LADDERS     8
#define MAX_FIGHTERS    96
#define MAX_VILLAGERS   32
#define MAX_WORKERS     16
#define MAX_CARAVANS    2
#define MAX_STARS       90
#define MAX_TORCHES     176
#define MAX_DYN_LIGHTS  16
#define MAX_PARTICLES   192
#define TRAIL_POINTS    16
#define SAVE_SLOTS      3
#define GROUND_TILES    120
#define TILE_SIZE       6.0f
#define BAG_START       6
#define BAG_MAX         12
#define STORE_SLOTS     24
#define QUIVER_START    10
#define QUIVER_BIG      20
#define FOOD_MAX        100.0f
#define HEALTH_MAX      100.0f

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
#define COL_PALM_LEAF   CLITERAL(Color){  62, 118,  52, 255 }
#define COL_DATES       CLITERAL(Color){ 150,  72,  30, 255 }
#define COL_REED        CLITERAL(Color){ 146, 160,  70, 255 }
#define COL_STRAW       CLITERAL(Color){ 190, 168, 104, 255 }
#define COL_GRASS       CLITERAL(Color){  92, 128,  54, 255 }
#define COL_SHRUB       CLITERAL(Color){ 118, 122,  70, 255 }
#define COL_TREE        CLITERAL(Color){  74, 108,  58, 255 }
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
#define COL_SOIL        CLITERAL(Color){  98,  74,  50, 255 }
#define COL_WHEAT_GREEN CLITERAL(Color){ 110, 150,  60, 255 }
#define COL_WHEAT_RIPE  CLITERAL(Color){ 222, 188,  92, 255 }
#define COL_FIRE        CLITERAL(Color){ 255, 150,  50, 255 }
#define COL_SKIN        CLITERAL(Color){ 170, 120,  84, 255 }
#define COL_HAIR        CLITERAL(Color){  40,  30,  24, 255 }

typedef enum {
    IT_NONE, IT_DATES, IT_REEDS, IT_SEEDS, IT_WHEAT, IT_FLOUR, IT_MILK, IT_DOUGH, IT_BREAD,
    IT_EGG, IT_ROAST, IT_ARROWS, IT_GAZELLE, IT_ONAGER, IT_BOAR, IT_GOOSE, IT_WOLF,
    IT_CLAY, IT_POT, IT_WOOD, IT_COPPER, IT_TIN, IT_BRONZE, IT_FISH, IT_GRILLED, IT_COWHIDE, IT_SHEEPSKIN,
    IT_BUCKET, IT_WATER, IT_WASTE, IT_HEN, IT_COUNT
} ItemId;

typedef struct { const char *name; int stack; int food; Color color; } ItemDef;
typedef struct { int item, count; } Slot;

enum { TOOL_AXE = 1, TOOL_PICK = 2, TOOL_ARROWHEADS = 4 };

typedef enum { OUT_ITEM, OUT_HEN, OUT_BASKET, OUT_BOW, OUT_QUIVER, OUT_TOOL, OUT_WEAPON, OUT_ARMOR } OutKind;
enum { EQ_NONE, EQ_BOW, EQ_MELEE };
enum { ARMOR_NONE, ARMOR_WOLF, ARMOR_LEATHER, ARMOR_BRONZE, ARMOR_KINDS };
enum { SHAPE_BOX, SHAPE_CYL, SHAPE_BLOB, SHAPE_CONE };
typedef struct { int in1, n1, in2, n2; OutKind kind; int out, nOut; int in3, n3; } Offer;

typedef struct { Vector3 pos, size; Color color; bool solid, water, glow; float torch; unsigned char shape; } Box;

typedef enum { RES_PALM, RES_REEDS, RES_TREE, RES_CLAY, RES_ORE, RES_FISH, RES_KINDS } ResKind;
typedef struct { Vector3 pos; ResKind kind; float regrow, spin; bool farm; } Resource;

typedef enum { AN_GAZELLE, AN_ONAGER, AN_BOAR, AN_GOOSE, AN_WOLF, AN_TYPES } AnimalType;
typedef enum { ST_CALM, ST_ALERT, ST_FLEE, ST_DEAD, ST_HUNT } AnimalState;

typedef struct {
    AnimalType type;
    AnimalState state;
    Vector3 pos;
    float heading, speed, timer, awareness, respawn, bite;
    int hp, arrowsIn, pack;
    bool active;
} Animal;

typedef struct {
    Vector3 pos, vel;
    float life, trailT, fade;
    bool flying, active, spent;
    Vector3 trail[TRAIL_POINTS];
    int trailCount;
} Arrow;

typedef struct { Vector3 pos, vel; float life, maxLife, size; Color color; bool ring; } Particle;

typedef enum { PLOT_EMPTY, PLOT_WHEAT, PLOT_SAPLING, PLOT_PALM, PLOT_DEAD } PlotState;
typedef struct { Vector3 pos; PlotState state; float growth, dry; } Plot;

typedef struct { Vector3 pos; float heading, timer, speed; } Wanderer;
typedef enum { ROLE_CIVILIAN, ROLE_WORKER, ROLE_CLERIC, ROLE_MERCHANT, ROLE_GUARD, ROLE_RAIDER, ROLE_COUNT } Role;

// Combat state shared by every person; while 'override' is set, 'at' replaces their scripted position.
typedef struct {
    float hp, hostile, cool, swing, deadT, windup, markT, foeT;
    Vector3 at;
    float face;
    int foe;
    bool dead, override, looted, used;
    unsigned char mark;
} Vitals;

enum { MARK_NONE, MARK_WARN, MARK_PARRIED, MARK_HIT };

typedef enum { NPC_CIVILIAN, NPC_GUARD, NPC_CLERIC } NpcKind;
typedef struct { Vector3 a, b; float t, dir, speed; Color robe; NpcKind kind; float face, side; bool moving, templePost; Vitals vit; } Villager;
typedef struct { Vector3 home, pos; float face, timer, speed, respawnDay; bool moving; int camp; Vitals vit; } Raider;
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
    float s, length, timer, blockedT, pushT;
    Color robe, cloth;
    Vitals vit[2];
} Caravan;

typedef enum { TR_BOWYER, TR_FARMER, TR_WEAVER, TR_HERDER, TR_SMITH, TR_DILMUN, TR_ELAM, TR_COUNT } TraderId;

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
    STN_WELL, STN_DITCH, STN_FERRY_E, STN_FERRY_W, STN_COUNT
} StationId;
typedef enum { PANEL_NONE, PANEL_BAG, PANEL_STORAGE, PANEL_TRADE, PANEL_PAUSE, PANEL_HELP, PANEL_COOP } Panel;
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

typedef struct { Color skin, cloth, top, hair; } Look;

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
    Raider raiders[MAX_RAIDERS];
    Slot stash[CAMPS][4];
    int stashDay;
    Ladder ladders[MAX_LADDERS];
    int ladderCount, climb, climbLadder, prayDay;
    float climbT;
    int equip, armor, armorOwned, dawnDay;
    float wanted, wolfAggro, drawFog, parryT, parryCool;
    int mapZoom;
    bool templeAccess, wasFocused, firstPerson;
    Villager villagers[MAX_VILLAGERS];
    int villagerCount;
    Worker workers[MAX_WORKERS];
    int workerCount;
    Caravan caravans[MAX_CARAVANS];
    Trader traders[TR_COUNT];
    Vector3 stations[STN_COUNT];
    Vector3 home, coop, pen;
    Vector3 stars[MAX_STARS];
    Vector3 torches[MAX_TORCHES];
    int torchCount;
    Vector3 dynLights[MAX_DYN_LIGHTS];
    int dynCount;
    float night, drawTorch;
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
    float swingT, swingCool, hitMarkT, impactT;
    Vector3 impact;
    int followArrow;
    float followT, camBlend;
    Vector3 followPos, followDir;
    bool arrowCam;
    float food, hungerNag;
    float health, hurtT, safeT, wolfWarn;
    bool alone;

    int day;
    float hour, daylight;
    Vector3 light;
    Color sky;
    float sleepT;
    bool sleeping, slept;

    char msg[256];
    float msgT;
    char promptE[192], promptF[192];
    bool okE, okF;
    Panel panel;
    int trader;
    Target target;
    bool cursorLocked, ignoreClick, quit;
} Game;

extern Game G;
extern const ItemDef ITEMS[IT_COUNT];
extern const char *const BOW_NAMES[3];
const char *ArmorName(int armor);
float ArmorReduction(int armor);

float RandF(float lo, float hi);
float Hash2(int a, int b);
float XZDist(Vector3 a, Vector3 b);
Color Mix(Color a, Color b, float t);
Color Lit(Color c, float shade);
void DrawShadedBox(Vector3 c, Vector3 s, Color col);
void DrawGlowBox(Vector3 c, Vector3 s, Color col);
void DrawShadedCylinder(Vector3 base, float rx, float rz, float h, float topScale, int sides, Color col);
void DrawShadedBlob(Vector3 c, Vector3 r, Color col);
void DrawHuman(Vector3 pos, float facing, float stride, Look look, bool crouch, bool bow, float draw);

void AddTorch(float x, float y, float z, bool pole);
void BuildLights(void);
void UpdateLights(void);
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
float RayTerrain(Vector3 o, Vector3 dir, float maxT);
float Fertility(float x, float z);
void BuildTerrain(void);
void DrawGround(void);

bool InsideWalls(float x, float z, float margin);
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
bool PointInSolid(Vector3 p);
bool SolidAt(float x, float z, float radius);
void UpdateSky(void);
void DrawSky(void);
void DrawTerrain(void);
void DrawResources(void);

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
bool OfferCheck(const Offer *o, const char **why);
bool OfferApply(const Offer *o);
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
bool AnyWolfHunting(void);
void CalmWolves(void);
int AnimalMaxHp(int type);
void MeleeStrike(void);
const char *WeaponName(int tier);

void BuildVillage(void);
void UpdateVillage(float dt);
void AdvanceHours(float h);
void DrawVillage(void);
Vector3 VillagerPos(const Villager *v);
Vector3 VillagerLanePos(const Villager *v);
Vector3 TraderPos(const Trader *t);
void DrawPersonBody(Vector3 pos, float facing, Look look);
void UpdateCombat(float dt);
void DamagePerson(int i, float dmg);
int PersonOnSegment(Vector3 a, Vector3 b);
int PersonInFront(Vector3 chest, Vector3 fwd, float range, float *dist);
int FighterCount(void);
bool FighterInfo(int i, Vector3 *pos, float *hp, float *maxHp, const char **name, bool *hostile, bool *dead);
int FighterMark(int i);
bool BodyLootable(int i);
void LootBody(int i);
void RespawnPeople(void);
void BuildRaiders(void);
void UpdateRaiders(float dt);
void DrawRaiders(void);
void StashPrompt(int camp, char *buf, int size, bool *ok);
void TakeStash(int camp);
Vector3 CampCenter(int camp);
void StationPrompt(int s, char *buf, int size, bool *ok);
void UseStation(int s);
void PlotPrompt(int p, char *e, char *f, int size, bool *okE, bool *okF);
void UsePlot(int p, bool alt);
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
void FerryPrompt(int s, char *e, char *f, int size, bool *okE, bool *okF);
void TakeFerry(int s);
void ChangeFare(void);
void DrawFerries(void);
void BlessPrompt(char *buf, int size, bool *ok);
void SpringRespawn(void);
int CityFarmCount(void);
Vector3 CityFarmSpot(int i);
unsigned long long AnimalsAwaitingSpring(void);
void HoldAnimalsForSpring(unsigned long long mask);
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
void HurtPlayer(float dmg);

#endif
