/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * TRS-80 Empire game attack screen source file.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 *
 * Includes.
 */

/* System includes. */
#include <math.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Local includes. */
#include "empire.h"
#include "cpu_strategy.h"
#include "ui.h"


/*------------------------------------------------------------------------------
 *
 * Prototypes.
 */

/*
 * Attack prototypes.
 */

static void Attack(Player *aPlayer, Player *aTargetPlayer);

static void GetSoldiersToAttack(Battle *aBattle);

static void RunBattle(Battle *aBattle);

static void DisplayBattleResults(Battle *aBattle);

static void Sack(Player *aTargetPlayer);

static void DrawAttackScreen(Player *aPlayer);

static void CPUAttack(Player *aPlayer, Player *aTargetPlayer);

static void DisplayCPUBattleResults(Battle *aBattle);

static void InitBattle(Battle *aBattle, Player *aPlayer);

static void SetBattleTarget(Battle *aBattle, Player *aTargetPlayer);

static void ApplyBattleResults(Battle *aBattle, Player *aPlayer,
                               Player *aTargetPlayer);


/*------------------------------------------------------------------------------
 *
 * External attack screen functions.
 */

/*
 * Manage attacking for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

void AttackScreen(Player *aPlayer)
{
    Player *targetPlayer;
    char    input[80];
    int     country;
    int     maxAttacks;

    /* Determine the maximum number of attacks per year. */
    maxAttacks = (aPlayer->nobleCount / 4) + 1;

    /* Attack other countries. */
    aPlayer->attackCount = 0;
    while (1)
    {
        /* Draw the attack screen. */
        DrawAttackScreen(aPlayer);

        /* Get country to attack. */
        CLEAR_MSG_AREA();
        printw("Attack which country (0 to skip)? ");
        getnstr(input, sizeof(input));
        country = ParseNum(input);

        /* Parse input. */
        if (country == 0)
        {
            break;
        }
        else if (country == 1)
        {
            targetPlayer = nullptr;
        }
        else if ((country >= 2) && (country <= (COUNTRY_COUNT + 1)))
        {
            targetPlayer = &(playerList[country - 2]);
            if (targetPlayer == aPlayer)
            {
                move(15, 0);
                ShowMessage("%s, that is your own country (#%d)!",
                            aPlayer->title, country);
                continue;
            }
        }

        /* Attack. */
        if (aPlayer->attackCount < maxAttacks)
        {
            Attack(aPlayer, targetPlayer);
        }
        else
        {
            CLEAR_MSG_AREA();
            ShowMessage("Due to a shortage of nobles, you are limited to only\n"
                        " %d attacks per year", maxAttacks);
        }
    }
}


/*
 * Manage attacking for the CPU player specified by aPlayer.
 * Delegates target selection to the current difficulty strategy.
 *
 *   aPlayer                CPU player.
 */

void CPUAttackScreen(Player *aPlayer)
{
    CPUStrategy *strategy = cpuStrategies[difficulty];
    Player *targetPlayer;
    int     attackChance;
    int     targetIndex;
    int     i;
    int     livingCount;
    int     livingIndices[COUNTRY_COUNT];

    /* Don't attack before the third year. */
    if (year < 3)
        return;

    /* Don't attack if no soldiers. */
    if (aPlayer->soldierCount <= 0)
        return;

    /* Decide whether to attack this year using the strategy's aggression. */
    attackChance = strategy->attackChanceBase
                   + (strategy->attackChancePerYear * year);
    if (attackChance > 95)
        attackChance = 95;
    if (RandRange(100) >= attackChance)
        return;

    /* Build a list of living players that are not this CPU player. */
    livingCount = 0;
    for (i = 0; i < COUNTRY_COUNT; i++)
    {
        if (!playerList[i].dead && (&playerList[i] != aPlayer))
            livingIndices[livingCount++] = i;
    }

    if (livingCount == 0 && barbarianLand == 0)
        return;

    /* Delegate target selection to the strategy. */
    targetIndex = strategy->selectTarget(aPlayer, livingIndices, livingCount);
    if (targetIndex == -1)
        return;
    targetPlayer = (targetIndex == -2) ? nullptr : &(playerList[targetIndex]);

    /* Attack. */
    aPlayer->attackCount = 0;
    CPUAttack(aPlayer, targetPlayer);
}


/*------------------------------------------------------------------------------
 *
 * Attack helper functions.
 */

/*
 * Initialize a battle for the attacking player specified by aPlayer.
 *
 *   aBattle                Battle to initialize.
 *   aPlayer                Attacking player.
 */

static void InitBattle(Battle *aBattle, Player *aPlayer)
{
    *aBattle = {};
    aBattle->player = aPlayer;
    aBattle->soldierEfficiency = aPlayer->armyEfficiency;
    snprintf(aBattle->soldierLabel,
             sizeof(aBattle->soldierLabel),
             "%s %s OF %s",
             aPlayer->title,
             aPlayer->name,
             aPlayer->country->name);
}


/*
 * Set the target for the battle specified by aBattle.  If aTargetPlayer is
 * NULL, the target is barbarians.
 *
 *   aBattle                Battle.
 *   aTargetPlayer          Target player, or NULL for barbarians.
 */

static void SetBattleTarget(Battle *aBattle, Player *aTargetPlayer)
{
    if (aTargetPlayer != nullptr)
    {
        aBattle->targetPlayer = aTargetPlayer;
        aBattle->targetLand = aTargetPlayer->land;
        snprintf(aBattle->targetSoldierLabel,
                 sizeof(aBattle->targetSoldierLabel),
                 "%s %s OF %s",
                 aTargetPlayer->title,
                 aTargetPlayer->name,
                 aTargetPlayer->country->name);
        if (aTargetPlayer->soldierCount > 0)
        {
            aBattle->targetSoldierCount = aTargetPlayer->soldierCount;
            aBattle->targetSoldierEfficiency = aTargetPlayer->armyEfficiency;
        }
        else
        {
            aBattle->targetSerfs = true;
            aBattle->targetSoldierCount = aTargetPlayer->serfCount;
            aBattle->targetSoldierEfficiency = SERF_EFFICIENCY;
        }
    }
    else
    {
        aBattle->targetLand = barbarianLand;
        snprintf(aBattle->targetSoldierLabel,
                 sizeof(aBattle->targetSoldierLabel),
                 "Pagan Barbarians");
        aBattle->targetSoldierCount =
              RandRange(3 * RandRange(aBattle->soldierCount))
            + RandRange(RandRange(3 * aBattle->soldierCount / 2));
        aBattle->targetSoldierEfficiency = 9;
    }
}


/*
 * Apply the results of the battle specified by aBattle to the attacker and
 * target player holdings.
 *
 *   aBattle                Battle.
 *   aPlayer                Attacking player.
 *   aTargetPlayer          Target player, or NULL for barbarians.
 */

static void ApplyBattleResults(Battle *aBattle, Player *aPlayer,
                               Player *aTargetPlayer)
{
    aPlayer->attackCount++;
    aPlayer->soldierCount -= aBattle->soldiersToAttackCount
                             - aBattle->soldierCount;
    if (aTargetPlayer != nullptr)
    {
        if (aBattle->targetSerfs)
            aTargetPlayer->serfCount = aBattle->targetSoldierCount;
        else
            aTargetPlayer->soldierCount = aBattle->targetSoldierCount;
    }
    aPlayer->land += aBattle->landCaptured;
    if (aTargetPlayer != nullptr)
        aTargetPlayer->land -= aBattle->landCaptured;
    else
        barbarianLand -= aBattle->landCaptured;
    if ((aTargetPlayer != nullptr) && aBattle->targetOverrun)
    {
        aPlayer->serfCount += aTargetPlayer->serfCount;
        aTargetPlayer->dead = true;
    }
}


/*------------------------------------------------------------------------------
 *
 * Attack functions.
 */

/*
 *   Attack the player specified by aTargetPlayer for the player specified by
 * aPlayer.  If aTargetPlayer is NULL, attack barbarians.
 *
 *   aPlayer                Player.
 *   aTargetPlayer          Player to attack.
 */

static void Attack(Player *aPlayer, Player *aTargetPlayer)
{
    Battle battle;

    /* Can't attack other players until the third year. */
    if ((aTargetPlayer != nullptr) && (year < 3))
    {
        CLEAR_MSG_AREA();
        ShowMessage("Due to international treaty, you cannot attack\n"
                    "other nations until the third year.");
        return;
    }

    /* If attacking the barbarians, make sure they have land. */
    if ((aTargetPlayer == nullptr) && (barbarianLand == 0))
    {
        CLEAR_MSG_AREA();
        ShowMessage("All barbarian lands have been seized\n");
        return;
    }

    /* Set up the battle. */
    InitBattle(&battle, aPlayer);
    GetSoldiersToAttack(&battle);
    SetBattleTarget(&battle, aTargetPlayer);

    /* Battle. */
    RunBattle(&battle);
    DisplayBattleResults(&battle);
    ApplyBattleResults(&battle, aPlayer, aTargetPlayer);
}


/*
 *   Attack the player specified by aTargetPlayer for the CPU player specified
 * by aPlayer.  If aTargetPlayer is NULL, attack barbarians.
 *
 *   aPlayer                CPU player.
 *   aTargetPlayer          Player to attack.
 */

static void CPUAttack(Player *aPlayer, Player *aTargetPlayer)
{
    Battle battle;
    int    soldiersToSend;

    /* If attacking the barbarians, make sure they have land. */
    if ((aTargetPlayer == nullptr) && (barbarianLand == 0))
        return;

    /* Set up the battle. */
    InitBattle(&battle, aPlayer);

    /* Delegate troop commitment to the strategy. */
    soldiersToSend = cpuStrategies[difficulty]->chooseSoldiersToSend(
                         aPlayer, aTargetPlayer);
    if (soldiersToSend > aPlayer->soldierCount)
        soldiersToSend = aPlayer->soldierCount;
    if (soldiersToSend <= 0)
        return;
    battle.soldiersToAttackCount = soldiersToSend;
    battle.soldierCount = soldiersToSend;

    /* Set up the target and fight. */
    SetBattleTarget(&battle, aTargetPlayer);
    RunBattle(&battle);
    DisplayCPUBattleResults(&battle);
    ApplyBattleResults(&battle, aPlayer, aTargetPlayer);
}


/*
 * Display results of the CPU battle specified by aBattle.
 *
 *   aBattle                Battle for which to display results.
 */

static void DisplayCPUBattleResults(Battle *aBattle)
{
    Player *player;
    Player *targetPlayer;
    int     landCaptured;

    /* Get battle information. */
    player = aBattle->player;
    targetPlayer = aBattle->targetPlayer;
    landCaptured = aBattle->landCaptured;

    /* Display battle results. */
    clear();
    mvprintw(0, 0, "----------------------------------------------------------------");
    mvprintw(1, 23, "Battle Over\n\n");
    if ((targetPlayer != nullptr) && aBattle->targetOverrun)
    {
        printw("The country of %s was overrun by %s %s!\n",
               targetPlayer->country->name,
               player->title,
               player->name);
    }
    else if ((targetPlayer == nullptr) && aBattle->targetOverrun)
    {
        printw("%s %s seized all barbarian lands!\n",
               player->title,
               player->name);
    }
    else if (aBattle->targetDefeated)
    {
        printw("The forces of %s %s were victorious.\n",
               player->title,
               player->name);
        printw(" %s acres were seized", FmtNum(landCaptured));
        if (targetPlayer != nullptr)
            printw(" from %s.\n", targetPlayer->country->name);
        else
            printw(" from the barbarians.\n");
    }
    else
    {
        printw("%s %s was defeated", player->title, player->name);
        if (targetPlayer != nullptr)
            printw(" by %s.\n", targetPlayer->country->name);
        else
            printw(" by the barbarians.\n");
        if (landCaptured > 2)
            landCaptured /= RandRange(3);
        else
            landCaptured = 0;
    }

    /* Check for sacking. */
    if (   (targetPlayer != nullptr)
        && !aBattle->targetOverrun
        && (landCaptured > (aBattle->targetLand / 3)))
    {
        Sack(targetPlayer);
    }

    /* If a human player was involved, wait for acknowledgement. */
    if ((targetPlayer != nullptr && targetPlayer->human) ||
        (player != nullptr && player->human))
    {
        printw("\n<Enter>? ");
        char input[80];
        getnstr(input, sizeof(input));
    }
    else
    {
        refresh();
        usleep(2 * DELAY_TIME);
    }

    /* Update battle information. */
    aBattle->landCaptured = landCaptured;
}


/*
 *   Get the number of player soldiers to attack in the battle specified by
 * aBattle.
 *
 *   aBattle                Battle for which to get attacking soldiers.
 */

static void GetSoldiersToAttack(Battle *aBattle)
{
    char    input[80];
    Player *player;
    int     soldiersToAttackCount;
    bool    soldiersToAttackCountValid;

    /* Get battle information. */
    player = aBattle->player;

    /* Get the number of soldiers with which to attack. */
    soldiersToAttackCountValid = false;
    while (!soldiersToAttackCountValid)
    {
        CLEAR_MSG_AREA();
        printw("How many of your %s soldiers to send? ",
               FmtNum(player->soldierCount));
        getnstr(input, sizeof(input));
        soldiersToAttackCount = ParseNum(input);
        if (soldiersToAttackCount > player->soldierCount)
        {
            CLEAR_MSG_AREA();
            ShowMessage("You only have %s soldiers!",
                        FmtNum(player->soldierCount));
        }
        else
        {
            soldiersToAttackCountValid = true;
        }
    }

    /* Update battle information. */
    aBattle->soldiersToAttackCount = soldiersToAttackCount;
    aBattle->soldierCount = soldiersToAttackCount;
}


/*
 * Run the battle specified by aBattle.
 *
 *   aBattle                Battle to run.
 */

static void RunBattle(Battle *aBattle)
{
    Player *player;
    Player *targetPlayer;
    int     soldierCount;
    int     soldierEfficiency;
    int     targetSoldierCount;
    int     targetSoldierEfficiency;
    int     targetLand;
    int     soldierKillCount;
    int     landCaptured = 0;
    int     roundDelayUs;
    bool    targetSerfs = false;
    bool    targetOverrun = false;
    bool    battleDone;

    /* Get battle information. */
    player = aBattle->player;
    targetPlayer = aBattle->targetPlayer;
    soldierCount = aBattle->soldierCount;
    soldierEfficiency = aBattle->soldierEfficiency;
    targetSoldierCount = aBattle->targetSoldierCount;
    targetSoldierEfficiency = aBattle->targetSoldierEfficiency;
    targetLand = aBattle->targetLand;
    targetSerfs = aBattle->targetSerfs;

    /* Battle. */
    landCaptured = 0;
    battleDone = false;
    while (!battleDone)
    {
        /* Show soldiers remaining. */
        clear();
        mvprintw(1, 0, "----------------------------------------------------------------");
        mvprintw(2, 41, "Soldiers Remaining:");
        mvprintw(4, 13, "%s:", aBattle->soldierLabel);
        mvprintw(5, 13, "%s:", aBattle->targetSoldierLabel);
        mvprintw(4, 51, "%s", FmtNum(soldierCount));
        mvprintw(5, 51, "%s", FmtNum(targetSoldierCount));
        if (targetSerfs)
        {
            mvprintw(8, 0, "%s's serfs are forced to defend their country!",
                     targetPlayer->country->name);
        }
        refresh();

        /*
         * Per-round delay scales with the square root of the smaller force,
         * recalculated each round so the pace slows as forces dwindle.
         *
         *   delay = 37.5ms * sqrt(smaller)
         */
        {
            int smaller = soldierCount < targetSoldierCount
                          ? soldierCount : targetSoldierCount;
            if (smaller < 1) smaller = 1;
            roundDelayUs = static_cast<int>(37500.0 * sqrt(static_cast<double>(smaller)));
        }
        usleep(roundDelayUs);

        /*
         * Determine how many soldiers were killed in this round, who won the
         * round, and how much land was captured.  Kill count is based on
         * the attacker's force so battles against large serf armies don't
         * end in a single round.
         */
        soldierKillCount = (soldierCount / 15) + 1;
        if (soldierCount > 200)
            soldierKillCount = (soldierCount / 8) + 1;
        if (soldierCount > 1000)
            soldierKillCount = (soldierCount / 5) + 1;
        if (RandRange(soldierEfficiency) < RandRange(targetSoldierEfficiency))
        {
            /* Player lost. */
            soldierCount -= soldierKillCount;
            if (soldierCount < 0)
                soldierCount = 0;

            /* Battle is done if all target land has been captured. */
            if (landCaptured >= targetLand)
                battleDone = true;
        }
        else
        {
            /* Player won. */
            landCaptured +=   RandRange(26 * soldierKillCount)
                            - RandRange(soldierKillCount + 5);
            if (landCaptured < 0)
                landCaptured = 0;
            else if (landCaptured > targetLand)
                landCaptured = targetLand;
            targetSoldierCount -= soldierKillCount;
            if (targetSoldierCount < 0)
                targetSoldierCount = 0;
        }

        /* Keep battling until one army is defeated. */
        if ((soldierCount == 0) || (targetSoldierCount == 0))
            battleDone = true;
    }

    /* Update battle information. */
    aBattle->soldierCount = soldierCount;
    aBattle->targetSoldierCount = targetSoldierCount;
    aBattle->landCaptured = landCaptured;
    if (soldierCount > 0)
    {
        aBattle->targetDefeated = true;
        if (targetSerfs || (landCaptured >= targetLand))
            aBattle->targetOverrun = true;
    }
}


/*
 * Display results of the battle specified by aBattle.
 *
 *   aBattle                Battle for which to display results.
 */

static void DisplayBattleResults(Battle *aBattle)
{
    char    input[80];
    Player *player;
    Player *targetPlayer;
    int     soldierCount;
    int     targetLand;
    int     landCaptured;
    bool    targetSerfs;

    /* Get battle information. */
    player = aBattle->player;
    targetPlayer = aBattle->targetPlayer;
    soldierCount = aBattle->soldierCount;
    targetLand = aBattle->targetLand;
    targetSerfs = aBattle->targetSerfs;
    landCaptured = aBattle->landCaptured;

    /* Display battle results. */
    clear();
    mvprintw(0, 0, "----------------------------------------------------------------");
    mvprintw(1, 23, "Battle Over\n\n");
    if ((targetPlayer != nullptr) && aBattle->targetOverrun)
    {
        /* Target player overrun. */
        printw("The country of %s was overrun!\n", targetPlayer->country->name);
        printw("All enemy nobles were summarily executed!\n\n\n");
        printw("The remaining enemy soldiers "
               "were imprisoned. All enemy serfs\n");
        printw("have pledged oaths of fealty to "
               "you, and should now be consid-\n");
        printw("ered to be your people too. All "
               "enemy merchants fled the coun-\n");
        printw("try. Unfortunately, all enemy "
               "assets were sacked and destroyed\n");
        printw("by your revengeful army in a "
               "drunken riot following the victory\n");
        printw("celebration.\n");
    }
    else if ((targetPlayer == nullptr) && aBattle->targetOverrun)
    {
        printw("All barbarian lands have been seized\n");
        printw("The remaining barbarians fled\n");
    }
    else if (aBattle->targetDefeated)
    {
        /* Player won. */
        printw("The forces of %s %s were victorious.\n",
               player->title,
               player->name);
        printw(" %s acres were seized.\n", FmtNum(landCaptured));
    }
    else
    {
        /* Player lost. */
        printw("%s %s was defeated.\n", player->title, player->name);
        if (landCaptured > 2)
        {
            landCaptured /= RandRange(3);
            printw("In your defeat you nevertheless "
                   "managed to capture %s acres.\n",
                   FmtNum(landCaptured));
        }
        else
        {
            landCaptured = 0;
            printw(" 0 acres were seized.\n");
        }
    }

    /* Check for sacking. */
    if (   (targetPlayer != nullptr)
        && !aBattle->targetOverrun
        && (landCaptured > (targetLand / 3)))
    {
        Sack(targetPlayer);
    }

    /* Wait for player. */
    printw("\n----------------------------------------------------------------\n");
    printw("<Enter>? ");
    getnstr(input, sizeof(input));

    /* Update battle information. */
    aBattle->landCaptured = landCaptured;
}


/*
 * Sack the player specified by aTargetPlayer.
 *
 *   aTargetPlayer          Player to sack.
 */

static void Sack(Player *aTargetPlayer)
{
    int  sackCount;

    /* Sack serfs. */
    if (aTargetPlayer->serfCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->serfCount);
        aTargetPlayer->serfCount -= sackCount;
        printw(" %s enemy serfs were beaten and murdered by your troops!\n",
               FmtNum(sackCount));
    }

    /* Sack marketplaces. */
    if (aTargetPlayer->marketplaceCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->marketplaceCount);
        aTargetPlayer->marketplaceCount -= sackCount;
        printw(" %s enemy marketplaces were destroyed\n", FmtNum(sackCount));
    }

    /* Sack grain. */
    if (aTargetPlayer->grain > 0)
    {
        sackCount = RandRange(aTargetPlayer->grain);
        aTargetPlayer->grain -= sackCount;
        printw(" %s bushels of enemy grain were burned\n", FmtNum(sackCount));
    }

    /* Sack grain mills. */
    if (aTargetPlayer->grainMillCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->grainMillCount);
        aTargetPlayer->grainMillCount -= sackCount;
        printw(" %s enemy grain mills were sabotaged\n", FmtNum(sackCount));
    }

    /* Sack foundries. */
    if (aTargetPlayer->foundryCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->foundryCount);
        aTargetPlayer->foundryCount -= sackCount;
        printw(" %s enemy foundries were leveled\n", FmtNum(sackCount));
    }

    /* Sack shipyards. */
    if (aTargetPlayer->shipyardCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->shipyardCount);
        aTargetPlayer->shipyardCount -= sackCount;
        printw(" %s enemy shipyards were over-run\n", FmtNum(sackCount));
    }

    /* Sack nobles. */
    if (aTargetPlayer->nobleCount > 2)
    {
        sackCount = RandRange(aTargetPlayer->nobleCount / 2);
        aTargetPlayer->nobleCount -= sackCount;
        printw(" %s enemy nobles were summarily executed\n", FmtNum(sackCount));
    }
}

/*
 * Draw the attack screen for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void DrawAttackScreen(Player *aPlayer)
{
    Player     *player;
    Country    *country;
    const char *countryName;
    int         playerLand;
    int         i;

    char rulerLine[160];
    snprintf(rulerLine, sizeof(rulerLine), "%s %s of %s",
             aPlayer->title, aPlayer->name, aPlayer->country->name);
    UITitle("Attack", rulerLine);

    /* Display land holdings. */
    printw("  #  Country          Land\n");
    for (i = 0; i < COUNTRY_COUNT + 1; i++)
    {
        /* Get the country name and player land.  Don't display dead players. */
        if (i == 0)
        {
            countryName = "Barbarians";
            playerLand = barbarianLand;
        }
        else
        {
            player = &(playerList[i - 1]);
            if (player->dead)
                continue;
            country = player->country;
            countryName = country->name;
            playerLand = player->land;
        }

        printw("  %d) %-14s %6s\n", i + 1, countryName, FmtNum(playerLand));
    }
    UISeparator();
}


