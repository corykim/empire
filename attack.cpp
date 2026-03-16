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
#include "diplomacy.h"
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

static SackResult ComputeSack(Player *aTargetPlayer);
static void ApplySack(Player *aTargetPlayer, const SackResult &r);
static void DisplaySack(const SackResult &r, bool humanAttacker);

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

    /* Defensive turtling: if any player outmatches us 3:1+ and we have
     * a small army, skip attacks to rebuild.  Still allow barbarian raids. */
    float myPower = ComputePlayerPower(aPlayer);
    bool shouldTurtle = false;
    for (int j = 0; j < COUNTRY_COUNT; j++)
    {
        if (!playerList[j].dead && &playerList[j] != aPlayer)
        {
            if (ComputePlayerPower(&playerList[j]) > myPower * CPU_TURTLE_POWER_RATIO)
            {
                shouldTurtle = true;
                break;
            }
        }
    }
    if (shouldTurtle && aPlayer->soldierCount < 50)
    {
        GameLog("  Turtling: outmatched %.0f:1 with %d soldiers\n",
                CPU_TURTLE_POWER_RATIO, aPlayer->soldierCount);
        return;
    }

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
        OnPlayerDeath(aTargetPlayer - playerList);
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

    /* Attack force threshold: don't send too few troops against a player.
     * Factor in allied strength — if allies would back you up, a smaller
     * individual commitment is viable for coalition attacks. */
    if (aTargetPlayer != nullptr)
    {
        float avgSoldiers = ComputeAverageSoldierCount();

        /* Scale attack ratio continuously: full 25% when avg army is
         * 100+, drops smoothly toward 10% as armies shrink. */
        float scaledRatio = CPU_MIN_ATTACK_RATIO
                            * (0.4f + 0.6f * MIN(avgSoldiers / 100.0f, 1.0f));

        float targetPower = ComputePlayerPower(aTargetPlayer);
        int estimatedMilitary = static_cast<int>(targetPower * 0.5f);
        int minForce = static_cast<int>(estimatedMilitary * scaledRatio);
        if (minForce < CPU_MIN_ATTACK_FORCE)
            minForce = CPU_MIN_ATTACK_FORCE;

        /* Reduce minimum by allied strength: allies who hate the target
         * and like us are effectively fighting alongside us. */
        int ti = aTargetPlayer - playerList;
        int alliedTroops = ComputeAlliedStrength(aPlayer, ti);
        int effectiveForce = soldiersToSend + alliedTroops;

        if (effectiveForce < minForce)
        {
            GameLog("  Aborting attack: %d+%d allied = %d < minimum %d "
                    "(ratio=%.0f%% avg=%.0f)\n",
                    soldiersToSend, alliedTroops, effectiveForce, minForce,
                    scaledRatio * 100.0f, avgSoldiers);
            return false;
        }
    }

    /* Garrison floor: keep at least 25% of army as defense.
     * Exception: allow all-in against a nemesis (diplomacy -2.0, massive
     * power imbalance) when allied strength backs us up. */
    if (aTargetPlayer != nullptr)
    {
        int garrison = aPlayer->soldierCount / 4;
        int ti = aTargetPlayer - playerList;
        bool isNemesis = (aPlayer->diplomacy[ti] <= -DIPLOMACY_CLAMP + 0.1f);
        float targetPow = ComputePlayerPower(aTargetPlayer);
        float myPow = ComputePlayerPower(aPlayer);
        int alliedTroops = ComputeAlliedStrength(aPlayer, ti);
        bool alliedBacked = (alliedTroops > aTargetPlayer->soldierCount / 2);

        if (isNemesis && targetPow > myPow * 1.5f && alliedBacked)
        {
            /* Nemesis all-in: send everything, no garrison. */
            GameLog("  Nemesis all-in: diplomacy=%.1f power=%.0f vs %.0f "
                    "allies=%d\n",
                    aPlayer->diplomacy[ti], myPow, targetPow, alliedTroops);
        }
        else if (soldiersToSend > aPlayer->soldierCount - garrison)
        {
            soldiersToSend = aPlayer->soldierCount - garrison;
            if (soldiersToSend < CPU_MIN_ATTACK_FORCE)
            {
                GameLog("  Garrison floor: can only send %d after keeping %d\n",
                        soldiersToSend, garrison);
                return false;
            }
        }
    }

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

/*
 * Compute one round of battle.  Pure logic — no UI.
 * Updates soldierCount, targetSoldierCount, landCaptured in the battle.
 * Returns true if the battle is over.
 */

static bool ComputeBattleRound(Battle *aBattle)
{
    int soldierKillCount = (aBattle->soldierCount / CASUALTY_DIV_BASE) + 1;
    if (aBattle->soldierCount > CASUALTY_THRESHOLD_MID)
        soldierKillCount = (aBattle->soldierCount / CASUALTY_DIV_MID) + 1;
    if (aBattle->soldierCount > CASUALTY_THRESHOLD_HIGH)
        soldierKillCount = (aBattle->soldierCount / CASUALTY_DIV_HIGH) + 1;

    if (RandRange(aBattle->soldierEfficiency)
        < RandRange(aBattle->targetSoldierEfficiency))
    {
        /* Attacker lost this round. */
        aBattle->soldierCount -= soldierKillCount;
        if (aBattle->soldierCount < 0)
            aBattle->soldierCount = 0;
    }
    else
    {
        /* Attacker won this round — capture land, kill defenders. */
        aBattle->landCaptured +=   RandRange(26 * soldierKillCount)
                                 - RandRange(soldierKillCount + 5);
        if (aBattle->landCaptured < 0)
            aBattle->landCaptured = 0;
        else if (aBattle->landCaptured > aBattle->targetLand)
            aBattle->landCaptured = aBattle->targetLand;
        aBattle->targetSoldierCount -= soldierKillCount;
        if (aBattle->targetSoldierCount < 0)
            aBattle->targetSoldierCount = 0;
    }

    /* Battle ends when one side is eliminated or all land captured. */
    return (aBattle->soldierCount == 0)
        || (aBattle->targetSoldierCount == 0)
        || (aBattle->landCaptured >= aBattle->targetLand);
}


/*
 * Display one round of battle on screen.
 */

static void DisplayBattleRound(Battle *aBattle)
{
    clear();
    getmaxyx(stdscr, winrows, wincols);
    move(1, 0);
    UISeparator();
    UIColor(UIC_HEADING);
    mvprintw(2, 41, "Soldiers Remaining:");
    UIColorOff();
    mvprintw(4, 13, "%s:", aBattle->soldierLabel);
    mvprintw(5, 13, "%s:", aBattle->targetSoldierLabel);
    mvprintw(4, 51, "%s", FmtNum(aBattle->soldierCount));
    mvprintw(5, 51, "%s", FmtNum(aBattle->targetSoldierCount));
    if (aBattle->targetSerfs && aBattle->targetPlayer)
    {
        UIColor(UIC_BAD);
        mvprintw(8, 0, "%s's serfs are forced to defend their country!",
                 aBattle->targetPlayer->country->name);
        UIColorOff();
    }
    refresh();

    /* Per-round delay scales with sqrt of the smaller force.
     * Press Enter to skip remaining delays and see results instantly. */
    if (!fastMode && !aBattle->skipDelay)
    {
        int smaller = MIN(aBattle->soldierCount, aBattle->targetSoldierCount);
        if (smaller < 1) smaller = 1;
        float delayScale = aBattle->targetSerfs ? 0.25f : 1.0f;
        int roundDelayMs = static_cast<int>(
            BATTLE_DELAY_MS * delayScale
            * sqrt(static_cast<double>(smaller)));

        /* Wait for the delay, but allow Enter to skip. */
        timeout(roundDelayMs);
        int ch = getch();
        timeout(-1);  /* Restore blocking mode. */
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
            aBattle->skipDelay = true;
    }
}


/*
 * Run the battle: display loop calls pure-logic rounds.
 */

static void RunBattle(Battle *aBattle)
{
    aBattle->landCaptured = 0;

    while (true)
    {
        DisplayBattleRound(aBattle);
        if (ComputeBattleRound(aBattle))
            break;
    }

    /* Determine outcome. */
    if (aBattle->soldierCount > 0)
    {
        aBattle->targetDefeated = true;
        if (aBattle->targetSerfs
            || (aBattle->landCaptured >= aBattle->targetLand))
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
            printw("The remaining enemy soldiers were imprisoned.\n");
            printw("All enemy serfs have pledged oaths of fealty to you.\n");
            printw("All enemy merchants fled the country.\n");
            printw("Unfortunately, all enemy assets were sacked and\n");
            printw("destroyed by your army in a victory riot.\n");
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
            else
                printw("%s captured %s acres in defeat.\n",
                       player->country->name, FmtNum(landCaptured));
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
        {
            SackResult sack = ComputeSack(targetPlayer);
            ApplySack(targetPlayer, sack);
            DisplaySack(sack, humanAttacker);
        }
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

/*
 * Compute sacking damage — pure logic, no UI.
 */

static SackResult ComputeSack(Player *aTargetPlayer)
{
    SackResult r = {};
    if (aTargetPlayer->serfCount > 0)
        r.serfsLost = RandRange(aTargetPlayer->serfCount);
    if (aTargetPlayer->marketplaceCount > 0)
        r.marketplacesLost = RandRange(aTargetPlayer->marketplaceCount);
    if (aTargetPlayer->grain > 0)
        r.grainLost = RandRange(aTargetPlayer->grain);
    if (aTargetPlayer->grainMillCount > 0)
        r.grainMillsLost = RandRange(aTargetPlayer->grainMillCount);
    if (aTargetPlayer->foundryCount > 0)
        r.foundriesLost = RandRange(aTargetPlayer->foundryCount);
    if (aTargetPlayer->shipyardCount > 0)
        r.shipyardsLost = RandRange(aTargetPlayer->shipyardCount);
    if (aTargetPlayer->nobleCount > 2)
        r.noblesLost = RandRange(aTargetPlayer->nobleCount / 2);
    return r;
}


/*
 * Apply sacking damage to the target player.
 */

static void ApplySack(Player *aTargetPlayer, const SackResult &r)
{
    aTargetPlayer->serfCount -= r.serfsLost;
    aTargetPlayer->marketplaceCount -= r.marketplacesLost;
    aTargetPlayer->grain -= r.grainLost;
    aTargetPlayer->grainMillCount -= r.grainMillsLost;
    aTargetPlayer->foundryCount -= r.foundriesLost;
    aTargetPlayer->shipyardCount -= r.shipyardsLost;
    aTargetPlayer->nobleCount -= r.noblesLost;
}


/*
 * Display sacking results.
 */

static void DisplaySack(const SackResult &r, bool humanAttacker)
{
    if (r.serfsLost > 0)
        printw(" %s serfs were beaten and murdered%s\n",
               FmtNum(r.serfsLost),
               humanAttacker ? " by your troops!" : ".");
    if (r.marketplacesLost > 0)
        printw(" %s marketplaces were destroyed\n", FmtNum(r.marketplacesLost));
    if (r.grainLost > 0)
        printw(" %s bushels of grain were burned\n", FmtNum(r.grainLost));
    if (r.grainMillsLost > 0)
        printw(" %s grain mills were sabotaged\n", FmtNum(r.grainMillsLost));
    if (r.foundriesLost > 0)
        printw(" %s foundries were leveled\n", FmtNum(r.foundriesLost));
    if (r.shipyardsLost > 0)
        printw(" %s shipyards were over-run\n", FmtNum(r.shipyardsLost));
    if (r.noblesLost > 0)
        printw(" %s nobles were summarily executed\n", FmtNum(r.noblesLost));
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


