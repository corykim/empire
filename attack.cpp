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
        printw("ATTACK WHICH COUNTRY (0 TO SKIP)? ");
        getnstr(input, sizeof(input));
        country = strtol(input, nullptr, 0);

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
                ShowMessage("%s, THAT IS YOUR OWN COUNTRY (#%d)!",
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
            ShowMessage("DUE TO A SHORTAGE OF NOBLES , YOU ARE LIMITED TO ONLY\n"
                        " %d ATTACKS PER YEAR", maxAttacks);
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
                 "PAGAN BARBARIANS");
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
        ShowMessage("DUE TO INTERNATIONAL TREATY, YOU CANNOT ATTACK OTHER\n"
                    "NATIONS UNTIL THE THIRD YEAR.");
        return;
    }

    /* If attacking the barbarians, make sure they have land. */
    if ((aTargetPlayer == nullptr) && (barbarianLand == 0))
    {
        CLEAR_MSG_AREA();
        ShowMessage("ALL BARBARIAN LANDS HAVE BEEN SEIZED\n");
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
    mvprintw(1, 23, "BATTLE OVER\n\n");
    if ((targetPlayer != nullptr) && aBattle->targetOverrun)
    {
        printw("THE COUNTRY OF %s WAS OVERUN BY %s %s!\n",
               targetPlayer->country->name,
               player->title,
               player->name);
    }
    else if ((targetPlayer == nullptr) && aBattle->targetOverrun)
    {
        printw("%s %s SEIZED ALL BARBARIAN LANDS!\n",
               player->title,
               player->name);
    }
    else if (aBattle->targetDefeated)
    {
        printw("THE FORCES OF %s %s WERE VICTORIOUS.\n",
               player->title,
               player->name);
        printw(" %d ACRES WERE SEIZED", landCaptured);
        if (targetPlayer != nullptr)
            printw(" FROM %s.\n", targetPlayer->country->name);
        else
            printw(" FROM THE BARBARIANS.\n");
    }
    else
    {
        printw("%s %s WAS DEFEATED", player->title, player->name);
        if (targetPlayer != nullptr)
            printw(" BY %s.\n", targetPlayer->country->name);
        else
            printw(" BY THE BARBARIANS.\n");
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
        printw("\n<ENTER>? ");
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
        printw("HOW MANY OF YOUR %d SOLDIERS TO SEND? ",
               player->soldierCount);
        getnstr(input, sizeof(input));
        soldiersToAttackCount = strtol(input, nullptr, 0);
        if (soldiersToAttackCount > player->soldierCount)
        {
            CLEAR_MSG_AREA();
            ShowMessage("YOU ONLY HAVE %d SOLDIERS!",
                        player->soldierCount);
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
        mvprintw(2, 41, "SOLDIERS REMAINING:");
        mvprintw(4, 13, "%s:", aBattle->soldierLabel);
        mvprintw(5, 13, "%s:", aBattle->targetSoldierLabel);
        mvprintw(4, 51, "%d", soldierCount);
        mvprintw(5, 51, "%d", targetSoldierCount);
        if (targetSerfs)
        {
            mvprintw(8, 0, "%s'S SERFS ARE FORCED TO DEFEND THEIR COUNTRY!",
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
    mvprintw(1, 23, "BATTLE OVER\n\n");
    if ((targetPlayer != nullptr) && aBattle->targetOverrun)
    {
        /* Target player overrun. */
        printw("THE COUNTRY OF %s WAS OVERUN!\n", targetPlayer->country->name);
        printw("ALL ENEMY NOBLES WERE SUMMARILY EXECUTED!\n\n\n");
        printw("THE REMAINING ENEMY SOLDIERS "
               "WERE IMPRISONED. ALL ENEMY SERFS\n");
        printw("HAVE PLEDGED OATHS OF FEALTY TO "
               "YOU, AND SHOULD NOW BE CONSID-\n");
        printw("ERED TO BE YOUR PEOPLE TOO. ALL "
               "ENEMY MERCHANTS FLED THE COUN-\n");
        printw("TRY. UNFORTUNATELY, ALL ENEMY "
               "ASSETS WERE SACKED AND DESTROYED\n");
        printw("BY YOUR REVENGEFUL ARMY IN A "
               "DRUNKEN RIOT FOLLOWING THE VICTORY\n");
        printw("CELEBRATION.\n");
    }
    else if ((targetPlayer == nullptr) && aBattle->targetOverrun)
    {
        printw("ALL BARBARIAN LANDS HAVE BEEN SEIZED\n");
        printw("THE REMAINING BARBARIANS FLED\n");
    }
    else if (aBattle->targetDefeated)
    {
        /* Player won. */
        printw("THE FORCES OF %s %s WERE VICTORIOUS.\n",
               player->title,
               player->name);
        printw(" %d ACRES WERE SEIZED.\n", landCaptured);
    }
    else
    {
        /* Player lost. */
        printw("%s %s WAS DEFEATED.\n", player->title, player->name);
        if (landCaptured > 2)
        {
            landCaptured /= RandRange(3);
            printw("IN YOUR DEFEAT YOU NEVERTHELESS "
                   "MANAGED TO CAPTURE %d ACRES.\n",
                   landCaptured);
        }
        else
        {
            landCaptured = 0;
            printw(" 0 ACRES WERE SEIZED.\n");
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
    printw("<ENTER>? ");
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
        printw(" %d ENEMY SERFS WERE BEATEN AND MURDERED BY YOUR TROOPS!\n",
               sackCount);
    }

    /* Sack marketplaces. */
    if (aTargetPlayer->marketplaceCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->marketplaceCount);
        aTargetPlayer->marketplaceCount -= sackCount;
        printw(" %d ENEMY MARKETPLACES WERE DESTROYED\n", sackCount);
    }

    /* Sack grain. */
    if (aTargetPlayer->grain > 0)
    {
        sackCount = RandRange(aTargetPlayer->grain);
        aTargetPlayer->grain -= sackCount;
        printw(" %d BUSHELS OF ENEMY GRAIN WERE BURNED\n", sackCount);
    }

    /* Sack grain mills. */
    if (aTargetPlayer->grainMillCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->grainMillCount);
        aTargetPlayer->grainMillCount -= sackCount;
        printw(" %d ENEMY GRAIN MILLS WERE SABOTAGED\n", sackCount);
    }

    /* Sack foundries. */
    if (aTargetPlayer->foundryCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->foundryCount);
        aTargetPlayer->foundryCount -= sackCount;
        printw(" %d ENEMY FOUNDRIES WERE LEVELED\n", sackCount);
    }

    /* Sack shipyards. */
    if (aTargetPlayer->shipyardCount > 0)
    {
        sackCount = RandRange(aTargetPlayer->shipyardCount);
        aTargetPlayer->shipyardCount -= sackCount;
        printw(" %d ENEMY SHIPYARDS WERE OVER-RUN\n", sackCount);
    }

    /* Sack nobles. */
    if (aTargetPlayer->nobleCount > 2)
    {
        sackCount = RandRange(aTargetPlayer->nobleCount / 2);
        aTargetPlayer->nobleCount -= sackCount;
        printw(" %d ENEMY NOBLES WERE SUMMARILY EXECUTED\n", sackCount);
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

    /* Reset screen. */
    clear();
    move(0, 0);

    /* Display land holdings. */
    printw("LAND HOLDINGS:\n\n");
    for (i = 0; i < COUNTRY_COUNT + 1; i++)
    {
        /* Get the country name and player land.  Don't display dead players. */
        if (i == 0)
        {
            countryName = "BARBARIANS";
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

        /* Display land holdings. */
        printw(" %d)  %-11s %d\n", i + 1, countryName, playerLand);
    }
}


