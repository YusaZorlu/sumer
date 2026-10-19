// What the game normally provides, so src/music.c can be built and run without the rest of the game.
#include "../../src/music.c"
Game G;
const char *DataPath(const char *n) { return n; }
void SyncStorage(void) {}
Vector3 VillagerPos(const Villager *v) { (void)v; return (Vector3){ 0 }; }
bool CaravanMember(int c, int k, Vector3 *p, float *f) { (void)c; (void)k; (void)p; (void)f; return false; }
bool PlayerAwake(int s) { (void)s; return false; }
float XZDist(Vector3 a, Vector3 b) { return hypotf(a.x - b.x, a.z - b.z); }
