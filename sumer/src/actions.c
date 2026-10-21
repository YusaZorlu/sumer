// What a player can do with what is in front of them: the prompts, and the actions behind E and F.
// Taking from the shared world (a carcass, a tree's wood, the coop's eggs) comes in two halves. The
// world's half runs where the world runs: here when playing alone or hosting. The player's half runs
// on the player's own machine, so a guest asks the host first and finishes when the answer comes.
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define REACH 8.0f

typedef struct { ItemId item; int amount; float regrow; int tool; const char *verb, *wait; float food; } GatherDef;

// Picking dates is free; everything else costs food. Mining costs twice what woodcutting does.
static const GatherDef GATHER[RES_KINDS] = {
    [RES_PALM]  = { IT_DATES,  3, 24.0f, 0,         "Pick dates",      "Dates ripen again",      0.0f },
    [RES_REEDS] = { IT_REEDS,  4, 48.0f, 0,         "Cut reeds",       "Reeds regrowing",        1.0f },
    [RES_TREE]  = { IT_WOOD,   2, 16.0f, TOOL_AXE,  "Chop wood",       "Felled tree regrowing",  8.0f },
    [RES_CLAY]  = { IT_CLAY,   4, 48.0f, 0,         "Dig clay",        "Clay pit refilling",     2.0f },
    [RES_ORE]   = { IT_COPPER, 2, 10.0f, TOOL_PICK, "Mine copper ore", "Vein worked out",       16.0f },
    [RES_FISH]  = { IT_FISH,   2, 24.0f, TOOL_NET,  "Cast your net",   "Fish scattered",         1.5f },
    [RES_HERB]  = { IT_HERB,   2, 36.0f, 0,         "Gather herbs",    "Herbs growing back",     1.0f },
};

static const Vitals *TraderVitals(int i)
{
    const Trader *t = &G.traders[i];
    if (t->roaming)
        for (int c = 0; c < MAX_CARAVANS; c++)
            if (G.caravans[c].trader == i) return &G.caravans[c].vit[0];
    return &t->vit;
}

// No one trades with a player the guards are after.
bool TraderWilling(int i)
{
    const Vitals *v = TraderVitals(i);
    return !v->dead && !v->override && v->hostile <= 0 && G.peers[G.self].wanted <= 0;
}

static void Consider(Target *best, float *score, TargetKind kind, int index, Vector3 pos, float range)
{
    float s = XZDist(pos, G.player.pos) / range;
    if (s < *score && fabsf(pos.y - G.player.pos.y) < 2.5f) {
        *score = s;
        *best = (Target){ kind, index };
    }
}

static void BuildPrompts(void)
{
    const int n = (int)sizeof(G.promptE);
    int i = G.target.index;

    G.promptE[0] = G.promptF[0] = '\0';
    G.okE = G.okF = false;

    switch (G.target.kind) {
    case TGT_CARCASS:
        G.okE = true;
        if (G.animals[i].type == AN_WOLF) Fmt(G.promptE, n, "[E] Skin the wolf for its pelt");
        else Fmt(G.promptE, n, "[E] Take the %s carcass (needs a free basket)", AnimalName(G.animals[i].type));
        break;
    case TGT_RES: {
        const Resource *r = &G.res[i];
        const GatherDef *g = &GATHER[r->kind];
        bool heavy = g->food >= 2.0f;
        G.okE = r->regrow <= 0 && (!heavy || G.food >= g->food);
        if (r->kind == RES_PALM && r->regrow > 0 && Season() == WINTER) Fmt(G.promptE, n, "No dates in winter");
        else if (r->regrow >= 24.0f) Fmt(G.promptE, n, "%s: about %.1f days", L(g->wait), r->regrow / 24.0f);
        else if (r->regrow > 0) Fmt(G.promptE, n, "%s: about %d hours", L(g->wait), (int)ceilf(r->regrow));
        else if (!G.okE) Fmt(G.promptE, n, "Too hungry for this work. Eat first (needs %d food)", (int)g->food);
        else if (heavy) Fmt(G.promptE, n, "[E] %s (hard work: -%d food)", L(g->verb), (int)g->food);
        else if (g->food > 0) Fmt(G.promptE, n, "[E] %s (-%g food)", L(g->verb), (double)g->food);
        else Fmt(G.promptE, n, "[E] %s", L(g->verb));
        break;
    }
    case TGT_STATION:
        if (StashCamp(i) >= 0) StashPrompt(StashCamp(i), G.promptE, n, &G.okE);
        else StationPrompt(i, G.promptE, n, &G.okE);
        if (i == STN_SMELTER) ForgePrompt(G.promptF, n, &G.okF);
        if (i == STN_BENCH) CloakPrompt(G.promptF, n, &G.okF);
        break;
    case TGT_PLOT:
        PlotPrompt(i, G.promptE, G.promptF, n, &G.okE, &G.okF);
        break;
    case TGT_COOP:
        CoopPrompt(G.promptE, G.promptF, n, &G.okE, &G.okF);
        break;
    case TGT_TRADER:
        G.okE = true;
        Fmt(G.promptE, n, "[E] Barter with the %s", L(G.traders[i].name));
        break;
    case TGT_BODY: {
        Vector3 pos;
        float hp, maxHp;
        const char *name = "";
        bool hostile, dead;
        FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead);
        G.okE = true;
        Fmt(G.promptE, n, "[E] Search the %s's body", L(name));
        break;
    }
    case TGT_OFFER:
        OfferPrompt(G.promptE, n, &G.okE);
        break;
    case TGT_LADDER:
        G.okE = true;
        Fmt(G.promptE, n, (i & 1) ? "[E] Climb down the ladder" : "[E] Climb the ladder to the wall walk");
        break;
    case TGT_WATER:
        G.okE = true;
        Fmt(G.promptE, n, "[E] Fill the bucket");
        break;
    default:
        break;
    }
}

void FindTarget(bool play)
{
    Target best = { TGT_NONE, -1 };
    float score = 1.0f;

    if (play) {
        for (int i = 0; i < G.animalCount; i++)
            if (G.animals[i].active && G.animals[i].state == ST_DEAD && CarcassItem(G.animals[i].type) != IT_NONE)
                Consider(&best, &score, TGT_CARCASS, i, G.animals[i].pos, 2.4f);
        for (int i = 0; i < G.resCount; i++)
            Consider(&best, &score, TGT_RES, i, G.res[i].pos, 2.4f);
        for (int i = 0; i < STN_COUNT; i++)
            if (i != STN_FERRY_E && i != STN_FERRY_W) Consider(&best, &score, TGT_STATION, i, G.stations[i], 1.9f);
        for (int i = 0; i < MAX_PLOTS; i++)
            if (G.plots[i].state != PLOT_PALM) Consider(&best, &score, TGT_PLOT, i, G.plots[i].pos, 2.1f);
        Consider(&best, &score, TGT_COOP, 0, G.coop, 4.2f);
        for (int i = 0; i < TR_COUNT; i++)
            if (!G.traders[i].away && TraderWilling(i)) Consider(&best, &score, TGT_TRADER, i, G.traders[i].pos, 3.2f);
        for (int i = 0; i < FighterCount(); i++) {
            Vector3 pos;
            float hp, maxHp;
            const char *name;
            bool hostile, dead;
            if (BodyLootable(i) && FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead))
                Consider(&best, &score, TGT_BODY, i, pos, 2.2f);
        }
        if (!G.templeAccess && G.peers[G.self].wanted <= 0) Consider(&best, &score, TGT_OFFER, 0, GatePos(), 2.4f);
        for (int i = 0; i < G.ladderCount; i++) {
            const Ladder *l = &G.ladders[i];
            Consider(&best, &score, TGT_LADDER, i * 2, l->base, 1.5f);
            Consider(&best, &score, TGT_LADDER, i * 2 + 1, (Vector3){ l->wall.x, l->top, l->wall.z }, 1.5f);
        }
        if (best.kind == TGT_NONE && BagCount(IT_BUCKET) > 0 && NearWater(G.player.pos)) best = (Target){ TGT_WATER, 0 };
    }
    G.target = best;
    BuildPrompts();
}

// ---- Carcasses ----

static void TakeCarcass(int i)
{
    const Animal *a = &G.animals[i];
    if (SlotsSpace(G.bag, G.bagSlots, CarcassItem(a->type)) <= 0) {
        Say(a->type == AN_WOLF ? "No room in your baskets for the pelt." : "No free basket. A carcass needs a whole basket to itself.");
        return;
    }
    Act(RQ_CARCASS, &i, 1);
}

static bool ClaimCarcass(int i, Vector3 by, int *res, int *nres)
{
    if (i < 0 || i >= G.animalCount) return false;
    Animal *a = &G.animals[i];
    if (!a->active || a->state != ST_DEAD || CarcassItem(a->type) == IT_NONE || XZDist(a->pos, by) > REACH) return false;
    res[0] = (int)a->type;
    res[1] = a->arrowsIn;
    *nres = 2;
    a->active = false;
    a->respawn = -1.0f;
    return true;
}

static void GotCarcass(int type, int arrows)
{
    if (type < 0 || type >= AN_TYPES || BagAdd(CarcassItem(type), 1) == 0) {
        Say("No free basket. A carcass needs a whole basket to itself.");
        return;
    }
    const char *name = AnimalName(type);
    int back = arrows > 0 ? GiveArrows(arrows) : 0;
    Exert(1.0f);
    if (type == AN_WOLF && back) Say("Skinned the wolf for its pelt. Arrows recovered: %d.", back);
    else if (type == AN_WOLF) Say("Skinned the wolf for its pelt.");
    else if (back) Say("Took the %s carcass. Arrows recovered: %d.", name, back);
    else Say("Took the %s carcass.", name);
}

// ---- Gathering ----

static void Gather(int i)
{
    const Resource *r = &G.res[i];
    const GatherDef *g = &GATHER[r->kind];

    if (r->regrow > 0) return;
    if (g->food >= 2.0f && G.food < g->food) {
        Say("Too hungry for this work. Eat something first.");
        return;
    }
    if (SlotsSpace(G.bag, G.bagSlots, g->item) <= 0) {
        Say("No room in your baskets.");
        return;
    }
    Act(RQ_GATHER, &i, 1);
}

static bool ClaimResource(int i, Vector3 by)
{
    if (i < 0 || i >= G.resCount) return false;
    Resource *r = &G.res[i];
    if (r->regrow > 0 || XZDist(r->pos, by) > REACH) return false;
    r->regrow = GATHER[r->kind].regrow;
    return true;
}

static void Gathered(int i)
{
    if (i < 0 || i >= G.resCount) return;
    Resource *r = &G.res[i];
    const GatherDef *g = &GATHER[r->kind];
    int amount = g->amount * ((g->tool && (G.tools & g->tool)) ? 2 : 1);
    int got = BagAdd(g->item, amount);

    r->regrow = g->regrow;
    Exert(g->food);
    if (!got) Say("No room in your baskets.");
    else if (g->food >= 2.0f) Say("+%d %s. Hard work: -%d food.", got, ItemName(g->item), (int)g->food);
    else if (r->kind == RES_HERB) Say("+%d %s (-%g food). Chew herbs from your baskets (Tab) to heal faster.", got, ItemName(g->item), (double)g->food);
    else if (g->food > 0) Say("+%d %s (-%g food)", got, ItemName(g->item), (double)g->food);
    else Say("+%d %s", got, ItemName(g->item));
}

// ---- Doing it, or asking the host to ----

// Alone or hosting, both halves run at once. A guest sends the request (one of each kind at a time)
// and finishes in ApplyReply.
void Act(int kind, const int *args, int n)
{
    int res[REQ_ARGS] = { 0 }, nres = 0;
    if (IsClient()) {
        if (!Asking(kind, -1)) Ask(kind, args, n);
        return;
    }
    bool ok = ServeRequest(G.self, kind, args, n, res, &nres);
    ApplyReply(kind, args, n, ok, res, nres);
}

// A player was brought down: whatever was after them lets them go.
void CalmFor(int slot)
{
    CalmBeasts(slot);
    CalmPeople(slot);
    ForgetFights(slot);
    if (slot >= 0 && slot < MAX_PLAYERS) {
        G.peers[slot].aggro = 0;
        G.peers[slot].wanted = 0;
    }
}

// The world's half, done for player 'slot' (G.actor is them, so messages reach them). Every index
// and amount is checked: a guest's request may be stale, or simply wrong.
bool ServeRequest(int slot, int kind, const int *args, int n, int *res, int *nres)
{
    Vector3 by = PlayerAt(slot);
    int a = n > 0 ? args[0] : 0, b = n > 1 ? args[1] : 0;

    *nres = 0;
    switch (kind) {
    case RQ_CARCASS:
        return ClaimCarcass(a, by, res, nres);
    case RQ_GATHER:
        return ClaimResource(a, by);
    case RQ_STASH:
        if (n < 5) return false;
        *nres = 9;
        return ClaimStash(a, args + 1, res, by);
    case RQ_PLOT:
        return n >= 2 && a >= 0 && a < MAX_PLOTS && XZDist(G.plots[a].pos, by) < REACH && PlotServe(a, b);
    case RQ_DITCH:
    case RQ_COOPWATER: {
        bool coop = kind == RQ_COOPWATER;
        if (XZDist(coop ? G.coop : G.stations[STN_DITCH], by) > REACH + 2.0f) return false;
        res[0] = PourServe(coop);
        *nres = 1;
        return res[0] >= 0;
    }
    case RQ_EGGS:
        if (XZDist(G.coop, by) > REACH + 2.0f) return false;
        res[0] = EggsServe(a);
        *nres = 1;
        return res[0] > 0;
    case RQ_FEED:
        if (XZDist(G.coop, by) > REACH + 2.0f) return false;
        res[0] = FeedServe(a != 0, b, &res[1]);
        *nres = 2;
        return res[0] > 0;
    case RQ_SLAUGHTER:
        return XZDist(G.coop, by) < REACH + 2.0f && SlaughterServe(a, b);
    case RQ_HEN:
        return AddHen();
    case RQ_LOOT: {
        int i = FighterByOwner(a);
        Vector3 pos;
        float hp, maxHp;
        const char *name;
        bool hostile, dead;
        return FighterInfo(i, &pos, &hp, &maxHp, &name, &hostile, &dead) && XZDist(pos, by) < REACH && ClaimBody(i);
    }
    case RQ_COLLAPSE:
        // Brought down by another player: a kill for them, a death for this one.
        if (n >= 1 && a >= BY_PLAYER && a < BY_PLAYER + MAX_PLAYERS && a - BY_PLAYER != slot && PlayerIn(a - BY_PLAYER) && PlayerIn(slot)) {
            G.peers[a - BY_PLAYER].kills++;
            G.peers[slot].deaths++;
            // Everyone hears of it but the one brought down, whose own message lists what they lost.
            for (int s = 0; s < MAX_PLAYERS; s++)
                if (s != slot && PlayerIn(s)) SayTo(s, "%s brought down %s.", G.peers[a - BY_PLAYER].name, G.peers[slot].name);
        }
        CalmFor(slot);
        return true;
    default:
        return false;
    }
}

// The player's half, once the world's half is done (or refused: someone else was quicker).
void ApplyReply(int kind, const int *args, int n, bool ok, const int *res, int nres)
{
    int a = n > 0 ? args[0] : 0, b = n > 1 ? args[1] : 0;

    switch (kind) {
    case RQ_CARCASS:
        if (!ok || nres < 2) {
            Say("Someone else took it first.");
            break;
        }
        if (a >= 0 && a < G.animalCount) G.animals[a].active = false;
        GotCarcass(res[0], res[1]);
        break;
    case RQ_GATHER:
        if (ok) Gathered(a);
        else Say("Someone else got there first.");
        break;
    case RQ_STASH:
        StashApply(ok && nres >= 9, res);
        break;
    case RQ_PLOT:
        // A guest sees the plot change at once; the host's next picture says the same.
        if (ok && IsClient()) PlotServe(a, b);
        PlotApply(b, ok);
        break;
    case RQ_DITCH:
    case RQ_COOPWATER:
        PourApply(kind == RQ_COOPWATER, ok, nres > 0 ? res[0] : 0);
        break;
    case RQ_EGGS:
        if (ok && nres > 0) EggsApply(res[0]);
        else Say("Someone else took the eggs.");
        break;
    case RQ_FEED:
        if (ok && nres > 1) FeedApply(a != 0, res[0], res[1]);
        break;
    case RQ_SLAUGHTER:
        SlaughterApply(ok);
        break;
    case RQ_HEN: {
        if (a < 0 || a >= TR_COUNT || b < 0 || b >= G.traders[a].offerCount) break;
        const Offer *o = &G.traders[a].offers[b];
        if (!ok) {
            Say("The coop is full (%d hens).", MAX_HENS);
            break;
        }
        OfferPay(o);
        Say("Bartered for %s.", OfferOutput(o));
        break;
    }
    case RQ_LOOT:
        if (ok) GiveLoot(FighterByOwner(a));
        else Say("Someone else searched the body first.");
        break;
    default:
        break;
    }
}

// A hen goes into the shared coop, so buying one is the host's business; the rest is between the
// player and the trader.
void Barter(int trader, int offer)
{
    const Offer *o = &G.traders[trader].offers[offer];
    const char *why;

    if (o->kind != OUT_HEN) {
        if (OfferApply(o)) Say("Bartered for %s.", OfferOutput(o));
        return;
    }
    if (!OfferCheck(o, &why)) {
        Say("%s.", why);
        return;
    }
    int args[2] = { trader, offer };
    Act(RQ_HEN, args, 2);
}

void Interact(bool alt)
{
    int i = G.target.index;

    switch (G.target.kind) {
    case TGT_CARCASS: if (!alt) TakeCarcass(i); break;
    case TGT_RES:     if (!alt) Gather(i); break;
    case TGT_STATION:
        if (!alt && StashCamp(i) >= 0) TakeStash(StashCamp(i));
        else if (!alt) UseStation(i);
        else if (i == STN_SMELTER) UseForge();
        else if (i == STN_BENCH) SewCloak();
        break;
    case TGT_BODY:
        if (!alt) {
            int owner = FighterOwner(i);
            Act(RQ_LOOT, &owner, 1);
        }
        break;
    case TGT_OFFER:   if (!alt) MakeOffering(); break;
    case TGT_LADDER:  if (!alt) StartClimb(i / 2, (i & 1) == 0); break;
    case TGT_PLOT:    UsePlot(i, alt); break;
    case TGT_COOP:
        if (!alt) {
            G.slaughterStep = 0;
            SetPanel(PANEL_COOP);
        }
        break;
    case TGT_WATER:   if (!alt) FillBucket(); break;
    case TGT_TRADER:
        if (!alt) {
            G.trader = i;
            SetPanel(PANEL_TRADE);
        }
        break;
    default:
        break;
    }
}
