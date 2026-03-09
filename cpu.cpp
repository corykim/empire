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

#include "platform.h"
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

/*
 * Unified attack decision and target selection.
 *
 * Each candidate's weight combines three factors:
 *   1. Diplomacy: max(0.01, 1 - diplomacy), amplified by weakness for enemies
 *   2. Envy: max(0, targetPower/attackerPower - 1) * ENVY_SCALE
 *   3. Theory of mind: SimulateAttackOutcome predicts diplomatic fallout
 *      and retaliation risk, adjusting weight up or down
 *
 * Weights are blended toward uniform by errorPct for difficulty scaling.
 * The strongest weight scales base aggression to determine attack probability.
 * If the roll says attack, a weighted random pick selects the target.
 * Returns a playerList index, TARGET_BARBARIANS, or TARGET_NONE.
 */

int CPUStrategy::selectTargetByDiplomacy(Player *aPlayer, int *livingIndices,
                                       int livingCount, float requireOdds)
{
    float weights[COUNTRY_COUNT];
    float totalWeight = 0.0f;
    float maxWeight = 0.0f;
    int   eligibleCount = 0;
    float attackerPower = ComputePlayerPower(aPlayer);
    float err = errorPct / 100.0f;

    for (int i = 0; i < livingCount; i++)
    {
        int idx = livingIndices[i];
        Player *candidate = &playerList[idx];

        /* Skip if we don't meet the required soldier odds. */
        if (requireOdds > 0.0f &&
            aPlayer->soldierCount <
            static_cast<int>(candidate->soldierCount * requireOdds))
        {
            weights[i] = 0.0f;
            continue;
        }

        float w = DiplomacyAttackWeight(aPlayer->diplomacy[idx],
                                      candidate->soldierCount);

        /* Envy: powerful targets are attractive regardless of diplomacy.
         * Quadratic growth ensures envy overwhelms retaliation fear
         * at high power disparity — a 5x stronger target is irresistible. */
        float targetPower = ComputePlayerPower(candidate);
        float envy = (targetPower / attackerPower) - 1.0f;
        if (envy < 0.0f) envy = 0.0f;
        w += envy * envy * DIPLOMACY_ENVY_SCALE;

        /* Theory of mind: simulate diplomatic consequences of this attack. */
        float simScore = SimulateAttackOutcome(aPlayer, idx);
        w += simScore;
        if (w < 0.01f) w = 0.01f;

        /* Blend toward uniform (1.0) based on error rate.  Village Fool
         * (errorPct=50) picks nearly at random; Machiavelli (errorPct=5)
         * follows diplomacy weights closely. */
        w = w * (1.0f - err) + 1.0f * err;

        weights[i] = w;
        totalWeight += w;
        if (w > maxWeight) maxWeight = w;
        eligibleCount++;
    }

    /* Add barbarians as a weighted candidate based on troop strength.
     * Stronger armies are more likely to expand into barbarian lands. */
    float barbarianWeight = 0.0f;
    if (barbarianLand > 0)
    {
        float strength = 1.0f - MilitaryWeakness(aPlayer->soldierCount);
        barbarianWeight = strength;
        totalWeight += barbarianWeight;
        if (barbarianWeight > maxWeight) maxWeight = barbarianWeight;
    }

    /* Base aggression, used for the attack roll. */
    int aggression = attackChanceBase + (attackChancePerYear * year);

    if (totalWeight <= 0.0f)
        return TARGET_NONE;

    /* Attack probability = base aggression scaled by strongest weight.
     * maxWeight=1 (neutral) → normal aggression.
     * maxWeight=2 (enemy at -1) → double aggression.
     * maxWeight≈0 (all friends, weak army) → near-zero aggression. */
    int effectiveChance = static_cast<int>(aggression * maxWeight);
    if (effectiveChance > 95) effectiveChance = 95;

    GameLog("  Attack chance: base %d%%, maxWeight %.2f, effective %d%%\n",
            aggression, maxWeight, effectiveChance);

    if (RandRange(100) >= effectiveChance)
        return TARGET_NONE;

    /* Weighted random selection among all candidates including barbarians. */
    float roll = static_cast<float>(RandRange(10000)) / 10000.0f * totalWeight;
    float cumulative = 0.0f;
    for (int i = 0; i < livingCount; i++)
    {
        cumulative += weights[i];
        if (roll <= cumulative && weights[i] > 0.0f)
            return livingIndices[i];
    }

    /* Barbarian slot sits at the end of the weight pool. */
    if (barbarianWeight > 0.0f)
        return TARGET_BARBARIANS;

    return TARGET_NONE;
}


/*
 * Default tax management: find optimal sales/income rates, then deviate
 * by errorPct.  Customs is set independently (affects immigration).
 */

void CPUStrategy::manageTaxes(Player *aPlayer)
{
    /* Save current rates before optimization. */
    int prevSales = aPlayer->salesTax;
    int prevIncome = aPlayer->incomeTax;

    /* Find optimal sales and income tax rates. */
    OptimizeTaxRates(aPlayer);
    int optSales = aPlayer->salesTax;
    int optIncome = aPlayer->incomeTax;

    /* Blend optimal toward previous rates based on error.
     * errorPct=50 (Village Fool): 100% previous rates (never changes).
     * errorPct=5 (Machiavelli): 90% optimal, 10% inertia.
     * adaptPct = 100 - 2*errorPct, clamped to [0, 100]. */
    int adaptPct = 100 - 2 * errorPct;
    if (adaptPct < 0) adaptPct = 0;
    aPlayer->salesTax = (optSales * adaptPct + prevSales * (100 - adaptPct)) / 100;
    aPlayer->incomeTax = (optIncome * adaptPct + prevIncome * (100 - adaptPct)) / 100;
    aPlayer->customsTax = deviate(18, 50);

    GameLog("  Tax adaptation: %d%% toward optimal (sales %d→%d→%d, "
            "income %d→%d→%d)\n",
            adaptPct, prevSales, optSales, aPlayer->salesTax,
            prevIncome, optIncome, aPlayer->incomeTax);
}


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
     * Strategic purchases with remaining budget.
     *
     * Guns vs butter: compute aggregate diplomacy to determine how much
     * of the budget goes to military vs economy.  High aggregate diplomacy
     * (many friends) → more butter.  Low/negative aggregate (enemies) →
     * more guns.  gunsPct ranges from ~20% (all friends) to ~80% (all
     * enemies), with 50% at neutral.
     */

    int desired, bought;
    int pi = aPlayer - playerList;
    float aggregateDiplomacy = 0.0f;
    int livingCount = 0;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == pi || playerList[i].dead)
            continue;
        aggregateDiplomacy += aPlayer->diplomacy[i];
        livingCount++;
    }
    /* Normalize to [-1, 1] range, then map to guns percentage.
     * avgDiplomacy = -1 → gunsPct = 80%, avgDiplomacy = 0 → 50%,
     * avgDiplomacy = +1 → 20%. */
    float avgDiplomacy = (livingCount > 0)
                       ? aggregateDiplomacy / livingCount : 0.0f;
    int gunsPct = 50 - static_cast<int>(avgDiplomacy * 30.0f);
    if (gunsPct < 20) gunsPct = 20;
    if (gunsPct > 80) gunsPct = 80;

    int gunsBudget = aPlayer->treasury * gunsPct / 100;
    int troopDeficit = aPlayer->desiredTroops - aPlayer->soldierCount;

    GameLog("  Guns/butter: aggregate diplomacy=%.2f, guns=%d%% (%d), "
            "butter=%d%%, troop deficit=%d\n",
            aggregateDiplomacy, gunsPct, gunsBudget,
            100 - gunsPct, troopDeficit);

    /* === GUNS: military infrastructure === */
    if (troopDeficit > 0 && gunsBudget > 0)
    {
        /* Determine which constraint is limiting troop recruitment. */
        int totalPeople = aPlayer->serfCount + aPlayer->merchantCount
                          + aPlayer->nobleCount;
        int noblesCap = 20 * aPlayer->nobleCount;
        float equipRatio = 0.05f + 0.015f * aPlayer->foundryCount;
        int equipCap = static_cast<int>(equipRatio * totalPeople);

        /* Buy palaces if noble leadership is the bottleneck. */
        if (noblesCap <= equipCap)
        {
            desired = MIN(gunsBudget / COST_PALACE, RandRange(3));
            bought = PurchaseInvestment(aPlayer, &aPlayer->palaceCount,
                                        desired, COST_PALACE);
            if (bought > 0)
            {
                boughtAnyPalaces = true;
                gunsBudget -= bought * COST_PALACE;
                GameLog("  Bought %d palaces (-%d)  Treasury: %d\n",
                        bought, bought * COST_PALACE, aPlayer->treasury);
            }
        }

        /* Buy foundries if equip ratio is the bottleneck. */
        if (equipCap <= noblesCap)
        {
            desired = MIN(gunsBudget / COST_FOUNDRY, RandRange(3));
            bought = PurchaseInvestment(aPlayer, &aPlayer->foundryCount,
                                        desired, COST_FOUNDRY);
            if (bought > 0)
            {
                gunsBudget -= bought * COST_FOUNDRY;
                GameLog("  Bought %d foundries (-%d)  Treasury: %d\n",
                        bought, bought * COST_FOUNDRY, aPlayer->treasury);
            }
        }

    }

    /* === BUTTER: economic infrastructure === */

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

    /* Palaces — economic pass (attract nobles for general growth). */
    if (!boughtAnyPalaces)
    {
        desired = MIN(aPlayer->treasury / COST_PALACE, RandRange(2));
        bought = PurchaseInvestment(aPlayer, &aPlayer->palaceCount,
                                    desired, COST_PALACE);
        if (bought > 0)
        {
            boughtAnyPalaces = true;
            GameLog("  Bought %d palaces (-%d)  Treasury: %d\n",
                    bought, bought * COST_PALACE, aPlayer->treasury);
        }
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

    /* Foundries — economic pass. */
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

    /* Recruit soldiers — target the deficit, capped by capacity. */
    SoldierCap cap = ComputeSoldierCap(aPlayer);
    int recruitTarget = (troopDeficit > 0)
                        ? MIN(troopDeficit, cap.maxSoldiers)
                        : cap.maxSoldiers;
    if (recruitTarget > 0)
        PurchaseSoldiers(aPlayer, recruitTarget);
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
    /* 70% chance to attack a player (diplomacy-weighted), else barbarians. */
    if ((livingCount > 0) && (RandRange(10) > 3))
        return selectTargetByDiplomacy(aPlayer, livingIndices, livingCount, 0.0f);
    if (barbarianLand > 0)
        return TARGET_BARBARIANS;
    if (livingCount > 0)
        return selectTargetByDiplomacy(aPlayer, livingIndices, livingCount, 0.0f);
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

    /* Diplomacy-weighted selection, no soldier requirement. */
    return selectTargetByDiplomacy(aPlayer, livingIndices, livingCount, 0.0f);
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
    if ((barbarianLand > 2000) && (RandRange(10) < 5))
        return TARGET_BARBARIANS;

    /* Diplomacy-weighted selection among weaker players (1:1 odds). */
    return selectTargetByDiplomacy(aPlayer, livingIndices, livingCount, 1.0f);
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
    /* Diplomacy-weighted selection, requires 1.5:1 soldier advantage. */
    return selectTargetByDiplomacy(aPlayer, livingIndices, livingCount, 1.5f);
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
    /* Diplomacy-weighted selection with 2:1 odds requirement.
     * Diplomacy naturally targets enemies: players who attacked us have
     * diplomacy of -1, and wounded players (who just attacked someone)
     * tend to have low diplomacy with observers. */
    return selectTargetByDiplomacy(aPlayer, livingIndices, livingCount, 2.0f);
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
