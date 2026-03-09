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
#include "platform.h"
#include <stdlib.h>
#include <string.h>

/* Local includes. */
#include "empire.h"
#include "economy.h"
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

static void DisplayBattleResults(Battle *aBattle, bool humanAttacker);

static void Sack(Player *aTargetPlayer);

static void DrawAttackScreen(Player *aPlayer);

static bool CPUAttack(Player *aPlayer, Player *aTargetPlayer, int reserve);


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
    maxAttacks = (aPlayer->nobleCount / ATTACKS_PER_NOBLE_DIV) + 1;

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
                move(winrows - 1, 0);
                ShowMessage("%s, that is your own country (#%d)!",
                            aPlayer->title, country);
                continue;
            }
        }

        /* Attack. */
        if (aPlayer->attackCount < maxAttacks)
        {
            GameLog("  Target: %s\n",
                    targetPlayer ? targetPlayer->country->name : "Barbarians");
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
    CPUStrategy *strategy = cpuStrategies[aPlayer->strategyIndex];
    Player *targetPlayer;
    int     targetIndex;
    int     i;
    int     livingCount;
    int     livingIndices[COUNTRY_COUNT];
    int     maxAttacks;

    /* Log this CPU's diplomacy scores before attack decisions. */
    LogPlayerDiplomacy(aPlayer);

    /* Don't attack during the treaty period. */
    if (year <= treatyYears)
        return;

    /* Don't attack if no soldiers. */
    if (aPlayer->soldierCount <= 0)
        return;

    /* Maximum attacks per year, same rule as humans. */
    maxAttacks = (aPlayer->nobleCount / ATTACKS_PER_NOBLE_DIV) + 1;
    aPlayer->attackCount = 0;

    /* Compute the soldier reserve needed to withstand retaliation. */
    int reserve = ComputeRetaliationReserve(aPlayer);

    /* Attack loop — re-evaluate targets and reserves after each battle. */
    while (aPlayer->attackCount < maxAttacks)
    {
        /* Stop if remaining soldiers can't exceed the reserve. */
        if (aPlayer->soldierCount <= reserve)
        {
            GameLog("  Holding %d soldiers in reserve (need %d)\n",
                    aPlayer->soldierCount, reserve);
            break;
        }

        /* Rebuild the living player list (targets may have died). */
        livingCount = 0;
        for (i = 0; i < COUNTRY_COUNT; i++)
        {
            if (!playerList[i].dead && (&playerList[i] != aPlayer))
                livingIndices[livingCount++] = i;
        }

        if (livingCount == 0 && barbarianLand == 0)
            break;

        /* Delegate target selection to the strategy (includes attack roll). */
        targetIndex = strategy->selectTarget(aPlayer, livingIndices,
                                             livingCount);
        if (targetIndex == TARGET_NONE)
            break;
        targetPlayer = (targetIndex == TARGET_BARBARIANS)
                       ? nullptr : &(playerList[targetIndex]);

        GameLog("  Attack #%d target: %s\n", aPlayer->attackCount + 1,
                targetPlayer ? targetPlayer->country->name : "Barbarians");

        /* Attack, holding reserve soldiers back.
         * Break if attack couldn't proceed (e.g., not enough soldiers). */
        if (!CPUAttack(aPlayer, targetPlayer, reserve))
            break;

        /* Recompute reserve — diplomacy and soldier counts may have changed. */
        reserve = ComputeRetaliationReserve(aPlayer);
    }
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
             "%s %s of %s",
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
                 "%s %s of %s",
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
    if (aTargetPlayer != nullptr)
    {
        int ti = aTargetPlayer - playerList;
        aPlayer->attackedTargets |= (1 << ti);
        aPlayer->landTakenFrom[ti] += aBattle->landCaptured;
    }
    int soldiersLost = aBattle->soldiersToAttackCount - aBattle->soldierCount;
    aPlayer->soldierCount -= soldiersLost;
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
    GameLog("  Battle vs %s: sent %d, lost %d, captured %d acres%s\n",
            aTargetPlayer ? aTargetPlayer->country->name : "Barbarians",
            aBattle->soldiersToAttackCount, soldiersLost,
            aBattle->landCaptured,
            aBattle->targetOverrun ? " (OVERRUN)" :
            aBattle->targetDefeated ? " (victory)" : " (defeated)");
    GameLog("    Land: %d  Soldiers: %d\n",
            aPlayer->land, aPlayer->soldierCount);
    if ((aTargetPlayer != nullptr) && aBattle->targetOverrun)
    {
        aPlayer->serfCount += aTargetPlayer->serfCount;
        aTargetPlayer->dead = true;
        aTargetPlayer->grainForSale = 0;
        aTargetPlayer->grainPrice = 0.0;
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

    /* Can't attack other players during the treaty period. */
    if ((aTargetPlayer != nullptr) && (year <= treatyYears))
    {
        CLEAR_MSG_AREA();
        ShowMessage("Due to international treaty, you cannot attack\n"
                    "other nations until year %d.", treatyYears);
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
    DisplayBattleResults(&battle, true);
    ApplyBattleResults(&battle, aPlayer, aTargetPlayer);
}


/*
 *   Attack the player specified by aTargetPlayer for the CPU player specified
 * by aPlayer.  If aTargetPlayer is NULL, attack barbarians.
 *
 *   aPlayer                CPU player.
 *   aTargetPlayer          Player to attack.
 */

static bool CPUAttack(Player *aPlayer, Player *aTargetPlayer, int reserve)
{
    Battle battle;
    int    soldiersToSend;
    int    available;

    /* If attacking the barbarians, make sure they have land. */
    if ((aTargetPlayer == nullptr) && (barbarianLand == 0))
        return false;

    /* Set up the battle. */
    InitBattle(&battle, aPlayer);

    /* Delegate troop commitment to the strategy, capped by reserve.
     * Don't bother if available troops are below the minimum attack force. */
    available = aPlayer->soldierCount - reserve;
    if (available < CPU_MIN_ATTACK_FORCE)
        return false;
    soldiersToSend = cpuStrategies[aPlayer->strategyIndex]->chooseSoldiersToSend(
                         aPlayer, aTargetPlayer);
    if (soldiersToSend > available)
        soldiersToSend = available;
    if (soldiersToSend <= 0)
        return false;
    battle.soldiersToAttackCount = soldiersToSend;
    battle.soldierCount = soldiersToSend;

    /* Set up the target and fight. */
    SetBattleTarget(&battle, aTargetPlayer);
    RunBattle(&battle);
    DisplayBattleResults(&battle, false);
    ApplyBattleResults(&battle, aPlayer, aTargetPlayer);
    return true;
}


/*
 * Display results of the CPU battle specified by aBattle.
 *
 *   aBattle                Battle for which to display results.
 */



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
        getmaxyx(stdscr, winrows, wincols);
        move(1, 0);
        UISeparator();
        UIColor(UIC_HEADING);
        mvprintw(2, 41, "Soldiers Remaining:");
        UIColorOff();
        mvprintw(4, 13, "%s:", aBattle->soldierLabel);
        mvprintw(5, 13, "%s:", aBattle->targetSoldierLabel);
        mvprintw(4, 51, "%s", FmtNum(soldierCount));
        mvprintw(5, 51, "%s", FmtNum(targetSoldierCount));
        if (targetSerfs)
        {
            UIColor(UIC_BAD);
            mvprintw(8, 0, "%s's serfs are forced to defend their country!",
                     targetPlayer->country->name);
            UIColorOff();
        }
        refresh();

        /*
         * Per-round delay scales with the square root of the smaller force,
         * recalculated each round so the pace slows as forces dwindle.
         *
         *   delay = BATTLE_DELAY_MS * sqrt(smaller)
         */
        {
            int smaller = soldierCount < targetSoldierCount
                          ? soldierCount : targetSoldierCount;
            if (smaller < 1) smaller = 1;
            float delayScale = aBattle->targetSerfs ? 0.25f : 1.0f;
            roundDelayUs = static_cast<int>(BATTLE_DELAY_MS * 1000.0 * delayScale * sqrt(static_cast<double>(smaller)));
        }
        if (!fastMode)
            SleepUs(roundDelayUs);

        /*
         * Determine how many soldiers were killed in this round, who won the
         * round, and how much land was captured.  Kill count is based on
         * the attacker's force so battles against large serf armies don't
         * end in a single round.
         */
        soldierKillCount = (soldierCount / CASUALTY_DIV_BASE) + 1;
        if (soldierCount > CASUALTY_THRESHOLD_MID)
            soldierKillCount = (soldierCount / CASUALTY_DIV_MID) + 1;
        if (soldierCount > CASUALTY_THRESHOLD_HIGH)
            soldierKillCount = (soldierCount / CASUALTY_DIV_HIGH) + 1;
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

/*
 * Display results of the battle specified by aBattle.  Unified for both
 * human-initiated and CPU-initiated battles.
 *
 *   aBattle                Battle for which to display results.
 *   humanAttacker          True if the attacking player is human.
 */

static void DisplayBattleResults(Battle *aBattle, bool humanAttacker)
{
    char    input[80];
    Player *player = aBattle->player;
    Player *targetPlayer = aBattle->targetPlayer;
    int     landCaptured = aBattle->landCaptured;
    bool    humanDefender = (targetPlayer != nullptr && targetPlayer->human);

    UITitle("Battle Over");
    printw("\n");

    if ((targetPlayer != nullptr) && aBattle->targetOverrun)
    {
        if (humanDefender)
        {
            UIColor(UIC_BAD);
            printw("Your country has been overrun by %s %s of %s!\n",
                   player->title, player->name, player->country->name);
            printw("All your nobles were executed!\n");
            UIColorOff();
        }
        else if (humanAttacker)
        {
            UIColor(UIC_GOOD);
            printw("The country of %s was overrun!\n",
                   targetPlayer->country->name);
            printw("All enemy nobles were summarily executed!\n\n\n");
            UIColorOff();
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
        else
        {
            UIColor(UIC_GOOD);
            printw("The country of %s was overrun by %s %s!\n",
                   targetPlayer->country->name,
                   player->title, player->name);
            UIColorOff();
        }
    }
    else if ((targetPlayer == nullptr) && aBattle->targetOverrun)
    {
        UIColor(UIC_GOOD);
        printw("All barbarian lands have been seized\n");
        if (humanAttacker)
            printw("The remaining barbarians fled\n");
        else
            printw("%s %s seized all barbarian lands!\n",
                   player->title, player->name);
        UIColorOff();
    }
    else if (aBattle->targetDefeated)
    {
        /* Attacker won. */
        if (humanDefender)
        {
            UIColor(UIC_BAD);
            printw("%s %s of %s attacked your country!\n",
                   player->title, player->name, player->country->name);
            printw("The enemy sent %s soldiers. You lost the battle.\n",
                   FmtNum(aBattle->soldiersToAttackCount));
            printw("%s acres were seized from your lands.\n",
                   FmtNum(landCaptured));
            UIColorOff();
        }
        else
        {
            UIColor(UIC_GOOD);
            printw("The forces of %s %s were victorious.\n",
                   player->title, player->name);
            printw(" %s acres were seized", FmtNum(landCaptured));
            if (targetPlayer != nullptr)
                printw(" from %s.\n", targetPlayer->country->name);
            else
                printw(" from the barbarians.\n");
            UIColorOff();
        }
    }
    else
    {
        /* Attacker lost. */
        if (humanDefender)
        {
            UIColor(UIC_GOOD);
            printw("%s %s of %s attacked your country!\n",
                   player->title, player->name, player->country->name);
            printw("The enemy sent %s soldiers. You repelled the attack!\n",
                   FmtNum(aBattle->soldiersToAttackCount));
            UIColorOff();
        }
        else if (humanAttacker)
        {
            UIColor(UIC_BAD);
            printw("%s %s was defeated.\n", player->title, player->name);
            UIColorOff();
        }
        else
        {
            UIColor(UIC_BAD);
            printw("%s %s was defeated", player->title, player->name);
            if (targetPlayer != nullptr)
                printw(" by %s.\n", targetPlayer->country->name);
            else
                printw(" by the barbarians.\n");
            UIColorOff();
        }

        /* Partial land capture on defeat. */
        if (landCaptured > 2)
        {
            landCaptured /= RandRange(3);
            if (humanDefender)
                printw("Despite your victory, the enemy captured %s acres.\n",
                       FmtNum(landCaptured));
            else if (humanAttacker)
                printw("In your defeat you nevertheless "
                       "managed to capture %s acres.\n",
                       FmtNum(landCaptured));
        }
        else
        {
            landCaptured = 0;
            if (humanDefender)
                printw("No land was lost.\n");
            else if (humanAttacker)
                printw(" 0 acres were seized.\n");
        }
    }

    /* Check for sacking. */
    if (   (targetPlayer != nullptr)
        && !aBattle->targetOverrun
        && (landCaptured > (aBattle->targetLand / SACK_THRESHOLD_DIV)))
    {
        Sack(targetPlayer);
    }

    /* Wait for Enter if a human was involved or a country was destroyed. */
    bool needsPrompt = humanAttacker || humanDefender
                       || ((targetPlayer != nullptr) && aBattle->targetOverrun);
    if (needsPrompt)
    {
        printw("\n");
        UISeparator();
        flushinp();
        printw("<Enter>? ");
        getnstr(input, sizeof(input));
    }
    else
    {
        refresh();
        if (!fastMode)
            SleepUs(2 * DELAY_TIME);
    }

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
    UIColor(UIC_HEADING);
    printw("  #  Country          Land\n");
    UIColorOff();
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


