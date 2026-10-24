// Prints the world as the game builds it (survival, or with "royale" the battle royale's): every box,
// resource and person's lane. tools/worldcheck.sh builds this natively and for the browser and compares.
#include "game.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    SetTraceLogLevel(LOG_WARNING);
    SetRoyale(argc > 1 && strcmp(argv[1], "royale") == 0);
    GameInit();
    printf("sign %d boxes %d res %d animals %d villagers %d workers %d\n", WorldSignature(), G.boxCount, G.resCount, G.animalCount, G.villagerCount, G.workerCount);
    for (int i = 0; i < G.boxCount; i++)
        printf("B %d %.6f %.6f %.6f %.6f %.6f %.6f\n", i, G.boxes[i].pos.x, G.boxes[i].pos.y, G.boxes[i].pos.z, G.boxes[i].size.x, G.boxes[i].size.y, G.boxes[i].size.z);
    for (int i = 0; i < G.resCount; i++) printf("R %d %d %.6f %.6f\n", i, G.res[i].kind, G.res[i].pos.x, G.res[i].pos.z);
    for (int i = 0; i < G.villagerCount; i++)
        printf("V %d %d %.6f %.6f %.6f %.6f\n", i, G.villagers[i].kind, G.villagers[i].a.x, G.villagers[i].a.z, G.villagers[i].b.x, G.villagers[i].b.z);
    return 0;
}
