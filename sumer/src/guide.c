// The first days, playing alone: one quiet line under the status box naming the next thing worth doing, for
// someone who has never seen the game. Each step is done when the world shows it (a bucket filled, a plot
// sown, a hen in the coop), so nothing has to be clicked away; after the last it is gone for good. Game
// settings turns it off or starts it over. How far it has come is kept with the other preferences.
#include "game.h"
#include <stddef.h>

static int step;   // the step waited for; -1: finished or turned off

static bool Sown(void)
{
    for (int i = 0; i < MAX_PLOTS; i++)
        if (G.plots[i].state == PLOT_WHEAT || G.plots[i].state == PLOT_SAPLING) return true;
    return TooLateToSow();   // past sowing time there is nothing to wait for
}

typedef struct { const char *text; bool (*done)(void); } GuideStep;

static bool AtJars(void) { return G.panel == PANEL_STORAGE; }
static bool HasBucket(void) { return (BagCount(IT_BUCKET) > 0 || BagCount(IT_WATER) > 0) && BagCount(IT_SEEDS) > 0; }
static bool HasWater(void) { return BagCount(IT_WATER) > 0; }
static bool HasHen(void) { return G.henCount > 0; }
static bool HasRoast(void) { return BagCount(IT_ROAST) > 0 || SlotsCount(G.store, STORE_SLOTS, IT_ROAST) > 0; }
static bool Slept(void) { return G.sleeping || G.day > 1; }

static const GuideStep STEPS[] = {
    { "Go home (the blue square on the map) and open the storage jars with E", AtJars },
    { "Take the bucket and the wheat seeds out of the jars", HasBucket },
    { "Fill the bucket at the well or the river (E)", HasWater },
    { "Sow a plot of your field (E)", Sown },
    { "Scatter seeds in the coop, then barter a hen from the farmer in the ziggurat", HasHen },
    { "Hunt outside the walls and roast the meat in your oven", HasRoast },
    { "When night falls, sleep in your bed", Slept },
};
#define GUIDE_STEPS ((int)(sizeof(STEPS) / sizeof(STEPS[0])))

int GuideStepNow(void)
{
    return step;
}

// From the preferences, or the settings: a step to wait for, or -1 for none.
void GuideSetStep(int s)
{
    step = s >= 0 && s < GUIDE_STEPS ? s : -1;
}

static bool Showing(void)
{
    return step >= 0 && G.role == NET_SOLO && !Royale() && !Party();
}

// The line to show, or NULL: only alone, and not over a window.
const char *GuideLine(void)
{
    return Showing() ? STEPS[step].text : NULL;
}

void GuideUpdate(void)
{
    if (!Showing() || !STEPS[step].done()) return;
    step++;
    if (step >= GUIDE_STEPS) {
        step = -1;
        Say("That is a day in Ur. H shows everything else.");
    }
    SaveGamePrefs();
}
