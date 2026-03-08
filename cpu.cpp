/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * TRS-80 Empire game CPU strategy implementation.
 *
 * Implements the CPUStrategy abstract base class and five derived classes.
 * See cpu_strategy.h for the class hierarchy.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#include <ncurses.h>
#include <stdlib.h>

#include "empire.h"
#include "cpu_strategy.h"


/*------------------------------------------------------------------------------
 *
 * Strategy instances.
 *
 * One global instance per difficulty level, indexed by the difficulty setting.
 */

static VillageFool    villageFoolInstance;
static LandedPeasant  landedPeasantInstance;
static MinorNoble     minorNobleInstance;
static RoyalAdvisor   royalAdvisorInstance;
static Machiavelli    machiavelliInstance;

CPUStrategy *cpuStrategies[DIFFICULTY_COUNT] =
{
    &villageFoolInstance,
    &landedPeasantInstance,
    &minorNobleInstance,
    &royalAdvisorInstance,
    &machiavelliInstance,
};


/*==============================================================================
 *
 * CPUStrategy base class — shared helpers.
 *
 *============================================================================*/

int CPUStrategy::cpuBuy(Player *aPlayer, int *count, int desired, int cost)
{
    int bought = 0;
    while (bought < desired && aPlayer->treasury >= cost)
    {
        aPlayer->treasury -= cost;
        (*count)++;
        bought++;
    }
    return bought;
}


void CPUStrategy::cpuBuySoldiers(Player *aPlayer, int desired)
{
    int totalPeople = aPlayer->serfCount + aPlayer->merchantCount
                      + aPlayer->nobleCount;
    float equipRatio = 0.05 + 0.015 * aPlayer->foundryCount;
    int maxEquip = static_cast<int>(equipRatio * totalPeople) - aPlayer->soldierCount;
    int maxNobles = (20 * aPlayer->nobleCount) - aPlayer->soldierCount;
    if (maxEquip < 0) maxEquip = 0;
    if (maxNobles < 0) maxNobles = 0;

    int maxSoldiers = MIN(MIN(aPlayer->treasury / COST_SOLDIER,
                              aPlayer->serfCount),
                          MIN(maxEquip, maxNobles));
    if (desired > maxSoldiers)
        desired = maxSoldiers;
    if (desired <= 0)
        return;

    aPlayer->soldierCount += desired;
    aPlayer->serfCount -= desired;
    aPlayer->treasury -= desired * COST_SOLDIER;
}


void CPUStrategy::cpuInvest(Player *aPlayer, int wastePct)
{
    int costs[] = { COST_MARKETPLACE, COST_GRAIN_MILL, COST_FOUNDRY,
                    COST_SHIPYARD, COST_PALACE };
    int *counts[] = { &aPlayer->marketplaceCount, &aPlayer->grainMillCount,
                      &aPlayer->foundryCount, &aPlayer->shipyardCount,
                      &aPlayer->palaceCount };

    /* Waste a percentage of the budget on random purchases. */
    int wasteBudget = aPlayer->treasury * wastePct / 100;
    while (wasteBudget > 0 && aPlayer->treasury > 0)
    {
        int pick = RandRange(5) - 1;
        if (aPlayer->treasury >= costs[pick])
        {
            aPlayer->treasury -= costs[pick];
            (*counts[pick])++;
            wasteBudget -= costs[pick];
        }
        else
        {
            break;
        }
    }

    /* Optimal purchases with remaining budget. */
    cpuBuy(aPlayer, &aPlayer->foundryCount, RandRange(2) + 1, COST_FOUNDRY);
    cpuBuy(aPlayer, &aPlayer->palaceCount, RandRange(1) + 1, COST_PALACE);
    cpuBuy(aPlayer, &aPlayer->marketplaceCount, RandRange(2), COST_MARKETPLACE);
    cpuBuy(aPlayer, &aPlayer->grainMillCount, RandRange(1), COST_GRAIN_MILL);
    cpuBuy(aPlayer, &aPlayer->shipyardCount, RandRange(1), COST_SHIPYARD);

    /* Recruit soldiers up to capacity. */
    int totalPeople = aPlayer->serfCount + aPlayer->merchantCount
                      + aPlayer->nobleCount;
    float equipRatio = 0.05 + 0.015 * aPlayer->foundryCount;
    int soldierCap = static_cast<int>(equipRatio * totalPeople);
    int nobleCap = 20 * aPlayer->nobleCount;
    int desiredSoldiers = MIN(soldierCap, nobleCap) - aPlayer->soldierCount;
    if (desiredSoldiers > 0)
        cpuBuySoldiers(aPlayer, desiredSoldiers);
}


/*==============================================================================
 *
 * VillageFool
 *
 * Acts randomly in all things.  50% investment waste.
 *
 *============================================================================*/

int VillageFool::selectTarget(Player *aPlayer, int *livingIndices,
                              int livingCount)
{
    if ((livingCount > 0) && (RandRange(10) > 3))
        return livingIndices[RandRange(livingCount) - 1];
    if (barbarianLand > 0)
        return TARGET_BARBARIANS;
    if (livingCount > 0)
        return livingIndices[RandRange(livingCount) - 1];
    return TARGET_NONE;
}

int VillageFool::chooseSoldiersToSend(Player *aPlayer, Player *aTarget)
{
    return RandRange(aPlayer->soldierCount);
}

void VillageFool::manageInvestments(Player *aPlayer)
{
    cpuInvest(aPlayer, 50);
}

void VillageFool::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = RandRange(50);
    aPlayer->salesTax = RandRange(20);
    aPlayer->incomeTax = RandRange(35);
}


/*==============================================================================
 *
 * LandedPeasant
 *
 * Picks the weaker of two random targets.  35% investment waste.
 *
 *============================================================================*/

int LandedPeasant::selectTarget(Player *aPlayer, int *livingIndices,
                                int livingCount)
{
    if ((barbarianLand > 0) && (RandRange(10) < 4))
        return TARGET_BARBARIANS;
    if (livingCount <= 0)
        return TARGET_NONE;

    int a = livingIndices[RandRange(livingCount) - 1];
    if (livingCount > 1)
    {
        int b = livingIndices[RandRange(livingCount) - 1];
        if (playerList[b].soldierCount < playerList[a].soldierCount)
            a = b;
    }
    return a;
}

int LandedPeasant::chooseSoldiersToSend(Player *aPlayer, Player *aTarget)
{
    return (aPlayer->soldierCount * 3 / 10)
           + RandRange(aPlayer->soldierCount * 3 / 10);
}

void LandedPeasant::manageInvestments(Player *aPlayer)
{
    cpuInvest(aPlayer, 35);
}

void LandedPeasant::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = 15 + RandRange(20);
    aPlayer->salesTax = 5 + RandRange(10);
    aPlayer->incomeTax = 15 + RandRange(15);
}


/*==============================================================================
 *
 * MinorNoble
 *
 * Only attacks weaker players.  20% investment waste.
 *
 *============================================================================*/

int MinorNoble::selectTarget(Player *aPlayer, int *livingIndices,
                             int livingCount)
{
    int bestIndex = TARGET_NONE;
    int bestScore = 0;

    if ((barbarianLand > 2000) && (RandRange(10) < 5))
        return TARGET_BARBARIANS;

    for (int i = 0; i < livingCount; i++)
    {
        Player *candidate = &(playerList[livingIndices[i]]);
        if (candidate->soldierCount < aPlayer->soldierCount)
        {
            int score = aPlayer->soldierCount - candidate->soldierCount;
            if (score > bestScore)
            {
                bestScore = score;
                bestIndex = livingIndices[i];
            }
        }
    }

    if (bestIndex != TARGET_NONE)
        return bestIndex;
    if (barbarianLand > 0)
        return TARGET_BARBARIANS;
    return TARGET_NONE;
}

int MinorNoble::chooseSoldiersToSend(Player *aPlayer, Player *aTarget)
{
    return (aPlayer->soldierCount * 4 / 10)
           + RandRange(aPlayer->soldierCount * 3 / 10);
}

void MinorNoble::manageInvestments(Player *aPlayer)
{
    cpuInvest(aPlayer, 20);
}

void MinorNoble::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = 20 + RandRange(10);
    aPlayer->salesTax = 5 + RandRange(8);
    aPlayer->incomeTax = 20 + RandRange(10);
}


/*==============================================================================
 *
 * RoyalAdvisor
 *
 * Targets weakest with most land, needs 1.5:1 odds.  10% investment waste.
 *
 *============================================================================*/

int RoyalAdvisor::selectTarget(Player *aPlayer, int *livingIndices,
                               int livingCount)
{
    int bestIndex = TARGET_NONE;
    int bestScore = 0;

    for (int i = 0; i < livingCount; i++)
    {
        Player *candidate = &(playerList[livingIndices[i]]);
        if (aPlayer->soldierCount < (candidate->soldierCount * 3 / 2))
            continue;
        int score = candidate->land;
        if (candidate->soldierCount > 0)
            score = score / (candidate->soldierCount + 1);
        else
            score = score * 3;
        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = livingIndices[i];
        }
    }

    if (bestIndex != TARGET_NONE)
        return bestIndex;
    if (barbarianLand > 0)
        return TARGET_BARBARIANS;
    return TARGET_NONE;
}

int RoyalAdvisor::chooseSoldiersToSend(Player *aPlayer, Player *aTarget)
{
    int count = (aPlayer->soldierCount * 5 / 10)
                + RandRange(aPlayer->soldierCount * 25 / 100);
    if (count > (aPlayer->soldierCount * 3 / 4))
        count = aPlayer->soldierCount * 3 / 4;
    return count;
}

void RoyalAdvisor::manageInvestments(Player *aPlayer)
{
    cpuInvest(aPlayer, 10);
}

void RoyalAdvisor::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = 25 + RandRange(10);
    aPlayer->salesTax = 8 + RandRange(5);
    aPlayer->incomeTax = 25 + RandRange(8);
}


/*==============================================================================
 *
 * Machiavelli
 *
 * Hunts the human leader, exploits wounded players.  0% investment waste.
 *
 *============================================================================*/

int Machiavelli::selectTarget(Player *aPlayer, int *livingIndices,
                              int livingCount)
{
    int humanLeaderIndex = TARGET_NONE;
    int humanLeaderStrength = 0;
    int worstIndex = TARGET_NONE;
    int worstScore = 999999;
    int bestWoundedIndex = TARGET_NONE;
    int bestWoundedScore = 0;

    for (int i = 0; i < livingCount; i++)
    {
        Player *candidate = &(playerList[livingIndices[i]]);

        if (candidate->human)
        {
            int strength = candidate->soldierCount
                           + (candidate->land / 100)
                           + (candidate->nobleCount * 50);
            if (strength > humanLeaderStrength)
            {
                humanLeaderStrength = strength;
                humanLeaderIndex = livingIndices[i];
            }
        }

        if (candidate->soldierCount < worstScore)
        {
            worstScore = candidate->soldierCount;
            worstIndex = livingIndices[i];
        }

        if ((candidate->attackCount > 0) &&
            (aPlayer->soldierCount > candidate->soldierCount))
        {
            int score = aPlayer->soldierCount
                        - candidate->soldierCount
                        + candidate->land;
            if (score > bestWoundedScore)
            {
                bestWoundedScore = score;
                bestWoundedIndex = livingIndices[i];
            }
        }
    }

    if ((humanLeaderIndex != TARGET_NONE) &&
        (aPlayer->soldierCount >=
         playerList[humanLeaderIndex].soldierCount * 2))
        return humanLeaderIndex;

    if (bestWoundedIndex != TARGET_NONE)
        return bestWoundedIndex;

    if ((worstIndex != TARGET_NONE) &&
        (aPlayer->soldierCount >= playerList[worstIndex].soldierCount * 2))
        return worstIndex;

    if (barbarianLand > 0)
        return TARGET_BARBARIANS;
    return TARGET_NONE;
}

int Machiavelli::chooseSoldiersToSend(Player *aPlayer, Player *aTarget)
{
    int count;

    if (aTarget != nullptr)
    {
        int targetStrength = aTarget->soldierCount;
        if (targetStrength <= 0)
            targetStrength = aTarget->serfCount / 3;
        count = (targetStrength * 12 / 10)
                + RandRange(targetStrength / 5);
        if (count > (aPlayer->soldierCount * 3 / 4))
            count = aPlayer->soldierCount * 3 / 4;
    }
    else
    {
        count = aPlayer->soldierCount / 2;
    }
    return count;
}

void Machiavelli::manageInvestments(Player *aPlayer)
{
    cpuInvest(aPlayer, 0);
}

void Machiavelli::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = 35 + RandRange(15);
    aPlayer->salesTax = 12 + RandRange(8);
    aPlayer->incomeTax = 28 + RandRange(7);
}
