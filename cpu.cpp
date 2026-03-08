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

/*
 * Return an integer near the optimal value, randomly deviated by the
 * strategy's error percentage.  The deviation range is ±(errorPct% of
 * maxVal), clamped to [0, maxVal].
 */

int CPUStrategy::deviate(int optimal, int maxVal)
{
    int range = maxVal * errorPct / 100;
    if (range <= 0)
        return optimal;
    int result = optimal + RandRange(2 * range + 1) - range - 1;
    if (result < 0) result = 0;
    if (result > maxVal) result = maxVal;
    return result;
}


/*
 * Default grain/land trading for all CPU strategies.
 * Behavior scales with errorPct: low errorPct = smart, high = inaction.
 *
 *   - Sells surplus grain at a competitive price
 *   - Buys grain when starving (can't feed people + army)
 *   - Sells land as emergency fundraising when broke
 *   - Dumbest CPU (errorPct=50) mostly does nothing
 */

void CPUStrategy::manageGrainTrade(Player *aPlayer)
{
    int totalNeed = aPlayer->peopleGrainNeed + aPlayer->armyGrainNeed;

    /*
     * Chance to act at all: smarter CPUs trade more often.
     * Village Fool (errorPct=50): 50% chance to skip entirely.
     * Machiavelli (errorPct=5): 5% chance to skip.
     */
    if (RandRange(100) <= errorPct)
        return;

    /*
     * BUY GRAIN if we can't feed our people and army.
     * Look for the cheapest seller on the market.
     */
    if (aPlayer->grain < totalNeed)
    {
        int deficit = totalNeed - aPlayer->grain;
        /* Find cheapest seller. */
        Player *cheapest = nullptr;
        for (int i = 0; i < COUNTRY_COUNT; i++)
        {
            Player *p = &playerList[i];
            if (p == aPlayer || p->dead || p->grainForSale <= 0)
                continue;
            if (cheapest == nullptr ||
                p->grainPrice < cheapest->grainPrice)
            {
                cheapest = p;
            }
        }
        if (cheapest != nullptr)
            TradeGrain(aPlayer, cheapest, deficit);
    }

    /*
     * SELL SURPLUS GRAIN if we have much more than we need.
     * Keep a reserve of ~2x total need (smarter CPUs keep tighter reserves).
     * Price: optimal ~2.5, deviated by errorPct.
     */
    int reserveMultiple = deviate(200, 400);  /* 200% = 2x need */
    int reserve = totalNeed * reserveMultiple / 100;
    int surplus = aPlayer->grain - reserve;
    if (surplus > 500)
    {
        /* Sell a fraction of the surplus. */
        int sellAmount = surplus * (100 - errorPct) / 100;
        if (sellAmount > 100)
        {
            /* Price: optimal ~2.5, deviated. */
            float price = 1.5 + static_cast<float>(deviate(10, 20)) / 10.0;
            ListGrainForSale(aPlayer, sellAmount, price);
        }
    }

    /*
     * SELL LAND if treasury is very low and we need money.
     * Only sell if we have plenty of land relative to population.
     * Smarter CPUs are more willing to sell land for investment capital.
     */
    int population = aPlayer->serfCount + aPlayer->merchantCount
                     + aPlayer->nobleCount + aPlayer->soldierCount;
    if (aPlayer->treasury < COST_MARKETPLACE && aPlayer->land > population * 3)
    {
        /* Sell enough land to afford a marketplace. */
        int acresNeeded = (COST_MARKETPLACE - aPlayer->treasury + 1) / 2;
        /* Add some extra for smarter CPUs (they plan ahead). */
        acresNeeded += acresNeeded * (100 - errorPct) / 100;
        int maxSell = aPlayer->land / 10;  /* Never sell more than 10% */
        if (acresNeeded > maxSell)
            acresNeeded = maxSell;
        SellLandToBarbarians(aPlayer, acresNeeded);
    }
}


void CPUStrategy::cpuInvest(Player *aPlayer)
{
    int costs[] = { COST_MARKETPLACE, COST_GRAIN_MILL, COST_FOUNDRY,
                    COST_SHIPYARD, COST_PALACE };
    int *counts[] = { &aPlayer->marketplaceCount, &aPlayer->grainMillCount,
                      &aPlayer->foundryCount, &aPlayer->shipyardCount,
                      &aPlayer->palaceCount };

    /* Track whether any marketplaces or palaces are bought this phase. */
    bool boughtAnyMarketplaces = false;
    bool boughtAnyPalaces = false;

    /* Waste a percentage of the budget on random purchases. */
    int wasteBudget = aPlayer->treasury * errorPct / 100;
    while (wasteBudget > 0 && aPlayer->treasury > 0)
    {
        int pick = RandRange(5) - 1;
        if (aPlayer->treasury >= costs[pick])
        {
            aPlayer->treasury -= costs[pick];
            (*counts[pick])++;
            wasteBudget -= costs[pick];
            if (pick == 0) boughtAnyMarketplaces = true;
            if (pick == 4) boughtAnyPalaces = true;
        }
        else
        {
            break;
        }
    }

    /*
     * Strategic purchases with remaining budget using shared functions.
     * Priority: marketplaces (cheap, revenue + merchants) → palaces
     * (nobles for soldier cap) → grain mills → foundries → shipyards.
     */

    int desired, bought;

    /* Marketplaces — cheap revenue generators. */
    desired = MIN(aPlayer->treasury / COST_MARKETPLACE, RandRange(3));
    bought = PurchaseInvestment(aPlayer, &aPlayer->marketplaceCount,
                                desired, COST_MARKETPLACE);
    if (bought > 0)
    {
        boughtAnyMarketplaces = true;
        GameLog("  Bought %d marketplaces (-%d)  Treasury: %d\n",
                bought, bought * COST_MARKETPLACE, aPlayer->treasury);
    }

    /* Palaces — raise the soldier leadership cap. */
    desired = MIN(aPlayer->treasury / COST_PALACE, RandRange(2));
    bought = PurchaseInvestment(aPlayer, &aPlayer->palaceCount,
                                desired, COST_PALACE);
    if (bought > 0)
    {
        boughtAnyPalaces = true;
        GameLog("  Bought %d palaces (-%d)  Treasury: %d\n",
                bought, bought * COST_PALACE, aPlayer->treasury);
    }

    /*
     * Apply merchant/noble bonuses ONCE per phase using shared functions
     * (matching human behavior: one roll per purchase action, not per item).
     */
    if (boughtAnyMarketplaces)
        ApplyMarketplaceBonus(aPlayer);
    if (boughtAnyPalaces)
        ApplyPalaceBonus(aPlayer);

    /* Grain mills. */
    desired = MIN(aPlayer->treasury / COST_GRAIN_MILL, RandRange(2));
    bought = PurchaseInvestment(aPlayer, &aPlayer->grainMillCount,
                                desired, COST_GRAIN_MILL);
    if (bought > 0)
        GameLog("  Bought %d grain mills (-%d)  Treasury: %d\n",
                bought, bought * COST_GRAIN_MILL, aPlayer->treasury);

    /* Foundries — enable equipping more soldiers. */
    desired = MIN(aPlayer->treasury / COST_FOUNDRY, RandRange(2));
    bought = PurchaseInvestment(aPlayer, &aPlayer->foundryCount,
                                desired, COST_FOUNDRY);
    if (bought > 0)
        GameLog("  Bought %d foundries (-%d)  Treasury: %d\n",
                bought, bought * COST_FOUNDRY, aPlayer->treasury);

    /* Shipyards. */
    desired = MIN(aPlayer->treasury / COST_SHIPYARD, RandRange(1));
    bought = PurchaseInvestment(aPlayer, &aPlayer->shipyardCount,
                                desired, COST_SHIPYARD);
    if (bought > 0)
        GameLog("  Bought %d shipyards (-%d)  Treasury: %d\n",
                bought, bought * COST_SHIPYARD, aPlayer->treasury);

    /* Recruit soldiers up to capacity using shared function. */
    SoldierCap cap = ComputeSoldierCap(aPlayer);
    if (cap.maxSoldiers > 0)
        PurchaseSoldiers(aPlayer, cap.maxSoldiers);
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
    cpuInvest(aPlayer);
}

void VillageFool::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = deviate(18, 50);
    aPlayer->salesTax = deviate(8, 20);
    aPlayer->incomeTax = deviate(28, 35);
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
    cpuInvest(aPlayer);
}

void LandedPeasant::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = deviate(18, 50);
    aPlayer->salesTax = deviate(8, 20);
    aPlayer->incomeTax = deviate(28, 35);
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
    cpuInvest(aPlayer);
}

void MinorNoble::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = deviate(18, 50);
    aPlayer->salesTax = deviate(8, 20);
    aPlayer->incomeTax = deviate(28, 35);
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
    cpuInvest(aPlayer);
}

void RoyalAdvisor::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = deviate(18, 50);
    aPlayer->salesTax = deviate(8, 20);
    aPlayer->incomeTax = deviate(28, 35);
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
    cpuInvest(aPlayer);
}

void Machiavelli::manageTaxes(Player *aPlayer)
{
    aPlayer->customsTax = deviate(18, 50);
    aPlayer->salesTax = deviate(8, 20);
    aPlayer->incomeTax = deviate(28, 35);
}


/*
 * Machiavelli grain trade: does everything the base class does,
 * plus exploits arbitrage — buys cheap grain and relists it higher.
 */

void Machiavelli::manageGrainTrade(Player *aPlayer)
{
    /* Run the standard trading logic first. */
    CPUStrategy::manageGrainTrade(aPlayer);

    /*
     * ARBITRAGE: scan the market for grain priced below 2.0 and buy it
     * to relist at a profit.  Only if we have treasury to spare.
     */
    if (aPlayer->treasury > 3 * COST_MARKETPLACE)
    {
        for (int i = 0; i < COUNTRY_COUNT; i++)
        {
            Player *seller = &playerList[i];
            if (seller == aPlayer || seller->dead || seller->grainForSale <= 0)
                continue;

            /* Buy grain priced below 2.0 and relist at 3.0+. */
            if (seller->grainPrice < 2.0)
            {
                /* Spend up to 20% of treasury on arbitrage. */
                int budget = aPlayer->treasury / 5;
                int canBuy = static_cast<int>(
                    (static_cast<float>(budget) * 0.9) / seller->grainPrice);
                if (canBuy > seller->grainForSale)
                    canBuy = seller->grainForSale;
                if (canBuy > 0)
                {
                    GameLog("  Arbitrage: buying cheap grain from %s at %.2f\n",
                            seller->country->name, seller->grainPrice);
                    int bought = TradeGrain(aPlayer, seller, canBuy);
                    if (bought > 0)
                    {
                        /* Relist at a 50-100% markup. */
                        float relistPrice = seller->grainPrice * 2.0;
                        if (relistPrice < 3.0) relistPrice = 3.0;
                        ListGrainForSale(aPlayer, bought, relistPrice);
                    }
                }
            }
        }
    }
}
