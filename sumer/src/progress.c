// Levels and experience: one account's progress over every way to play (alone, together, PvP and the
// battle royale). Level 1 needs 100 experience to reach level 2, level 2 another 200 to reach level 3, and
// so on up to level 100. It is only shown, never spent. A beast or a townsman brought down is worth 1, a
// fighter (guards, raiders, barbarians, bandits) 3, another player 10, and winning a battle royale round
// 10 for every player it began with. In a server's game the server counts it and keeps it; alone, the
// game counts it and sends it to the server (account.c).
#include "game.h"
#include <stdio.h>

// The experience a level begins at: 100 + 200 + ... for the levels below it.
long LevelStart(int level)
{
    if (level < 1) level = 1;
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    return 50L * level * (level - 1);
}

int LevelOf(long xp)
{
    int level = 1;
    while (level < MAX_LEVEL && xp >= LevelStart(level + 1)) level++;
    return level;
}

// "Level 12 (340/1200)": how far into the level, and what it takes; "Level 100" at the top.
const char *LevelText(long xp)
{
    int level = LevelOf(xp);
    if (level >= MAX_LEVEL) return TextFormat(L("Level %d"), level);
    return TextFormat(L("Level %d (%ld/%ld)"), level, xp - LevelStart(level), LevelStart(level + 1) - LevelStart(level));
}

// A person brought down: fighters are worth more.
int KillXp(int role)
{
    return role == ROLE_GUARD || role == ROLE_RAIDER || role == ROLE_BARBARIAN || role == ROLE_BANDIT ? XP_FIGHTER : XP_KILL;
}

// Experience (and kills and deaths at other players' hands) for the player in this slot.
void AwardXp(int slot, int xp, int kills, int deaths)
{
    if ((xp <= 0 && kills <= 0 && deaths <= 0) || slot < 0 || slot >= MAX_PLAYERS) return;
    if (G.role == NET_HOST) HostProgress(slot, xp, kills, deaths);
    else if (G.role == NET_SOLO && slot == G.self) AccountGain(xp, kills, deaths, true);
}

// Shown on this player's screen: "+3 XP" for a moment, and the news of a level reached.
void ShowGain(long before, long after)
{
    if (after <= before) return;
    G.xpGain = (int)(after - before) + (G.xpGainT > 0 ? G.xpGain : 0);
    G.xpGainT = 2.0f;
    int was = LevelOf(before), now = LevelOf(after);
    if (now > was) {
        Say("Level up! You are level %d.", now);
        G.msgT = 5.0f;
    }
}
