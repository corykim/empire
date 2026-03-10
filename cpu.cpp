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


/* CPU behavior constants (used only in this file). */
constexpr int   CPU_LAND_SURPLUS_MULT    = 8;      /* Only sell land if land > pop * this */
constexpr int   CPU_ACRES_PER_GRAIN_MILL = 2000;   /* Farmland acres per grain mill needed */
constexpr float CPU_GRAIN_SCARCITY_MAX   = 2.0f;   /* Max grain price multiplier when scarce */


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
    int   effError = ComputeErrorPct(aPlayer->cpuDifficulty);
    float err = effError / 100.0f;

    for (int i = 0; i < livingCount; i++)
    {
        int idx = livingIndices[i];
        Player *candidate = &playerList[idx];

        /* Diplomacy weight with military caution. */
        float diplomacyW = DiplomacyAttackWeight(aPlayer->diplomacy[idx],
                                                 candidate->soldierCount);

        /* Military caution: consider alliance strength, not just own troops.
         * If our side (us + allies) outnumbers the target, reduce caution. */
        int alliedTroops = ComputeAlliedStrength(aPlayer, idx);
        int totalFriendly = aPlayer->soldierCount + alliedTroops;
        if (requireOdds > 0.0f && candidate->soldierCount > 0)
        {
            float oddsRatio = static_cast<float>(totalFriendly)
                              / static_cast<float>(candidate->soldierCount);
            if (oddsRatio < requireOdds)
                diplomacyW *= oddsRatio / requireOdds;
        }

        /* Envy: powerful targets are attractive. */
        float targetPower = ComputePlayerPower(candidate);
        float envyRatio = (targetPower / attackerPower) - 1.0f;
        if (envyRatio < 0.0f) envyRatio = 0.0f;
        float envyW = envyRatio * envyRatio * envyRatio * DIPLOMACY_ENVY_SCALE;

        /* Vulnerability: wounded or defenseless targets are high priority. */
        float vulnW = ComputeVulnerability(idx);

        float w = diplomacyW + envyW + vulnW;

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

    /* Add barbarians as a weighted candidate.  Prioritize in early years
     * when barbarian land is free growth with no diplomatic consequences. */
    float barbarianWeight = 0.0f;
    if (barbarianLand > 0)
    {
        float strength = 1.0f - MilitaryWeakness(aPlayer->soldierCount);
        /* Boost barbarian priority in early years (years 1-5). */
        float earlyBoost = (year <= 5) ? 2.0f : 1.0f;
        barbarianWeight = strength * earlyBoost;
        totalWeight += barbarianWeight;
        if (barbarianWeight > maxWeight) maxWeight = barbarianWeight;
    }

    /* Base aggression from continuous difficulty. */
    int aggression = ComputeAttackChanceBase(aPlayer->cpuDifficulty)
                     + (attackChancePerYear * year);

    if (totalWeight <= 0.0f)
        return TARGET_NONE;

    /* Attack probability = base aggression scaled by strongest weight.
     * maxWeight=1 (neutral) → normal aggression.
     * maxWeight=2 (enemy at -1) → double aggression.
     * maxWeight≈0 (all friends, weak army) → near-zero aggression. */
    int effectiveChance = static_cast<int>(aggression * maxWeight);
    if (effectiveChance > MAX_ATTACK_CHANCE) effectiveChance = MAX_ATTACK_CHANCE;

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

    /* Blend optimal toward previous rates based on difficulty.
     * Low difficulty (err=50): 100% previous rates (never changes).
     * High difficulty (err=5): 90% optimal, 10% inertia. */
    int effError = ComputeErrorPct(aPlayer->cpuDifficulty);
    int adaptPct = 100 - 2 * effError;
    if (adaptPct < 0) adaptPct = 0;
    aPlayer->salesTax = (optSales * adaptPct + prevSales * (100 - adaptPct)) / 100;
    aPlayer->incomeTax = (optIncome * adaptPct + prevIncome * (100 - adaptPct)) / 100;
    aPlayer->customsTax = deviate(18, 50, effError);

    GameLog("  Tax adaptation: %d%% toward optimal (sales %d→%d→%d, "
            "income %d→%d→%d)\n",
            adaptPct, prevSales, optSales, aPlayer->salesTax,
            prevIncome, optIncome, aPlayer->incomeTax);
}


int CPUStrategy::deviate(int optimal, int maxVal)
{
    return deviate(optimal, maxVal, errorPct);
}

/*
 * Compute a target grain price based on:
 *   - Current market supply vs total demand (scarcity)
 *   - 3-year rolling inventory trend (rising → lower prices, falling → higher)
 *   - Worst-case demand estimate (what if harvest is bad?)
 *   - Random noise (even L5 isn't perfect)
 */

float CPUStrategy::ComputeGrainTargetPrice(Player *aPlayer, int effError)
{
    /* Current market supply and total demand across all players. */
    int totalMarketGrain = 0;
    int totalDemand = 0;
    int totalWorstDemand = 0;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (!playerList[i].dead)
        {
            totalMarketGrain += playerList[i].grainForSale;
            int need = playerList[i].peopleGrainNeed + playerList[i].armyGrainNeed;
            totalDemand += need;
            /* Worst case: need minus worst harvest. */
            int worstHarv = ComputeWorstCaseHarvest(&playerList[i]);
            int shortfall = need - worstHarv;
            if (shortfall > 0) totalWorstDemand += shortfall;
        }
    }

    /* Scarcity: supply vs worst-case demand. */
    float scarcity = (totalWorstDemand > 0)
        ? static_cast<float>(totalMarketGrain) / static_cast<float>(totalWorstDemand)
        : 2.0f;
    /* scarcity < 1 → not enough grain on market to cover a bad year. */
    float scarcityMult = (scarcity < 1.0f)
        ? CPU_GRAIN_SCARCITY_MAX - (CPU_GRAIN_SCARCITY_MAX - 1.0f) * scarcity
        : 1.0f / scarcity;
    if (scarcityMult < 0.5f) scarcityMult = 0.5f;
    if (scarcityMult > CPU_GRAIN_SCARCITY_MAX) scarcityMult = CPU_GRAIN_SCARCITY_MAX;

    /* Trend: 3-year rolling average vs current inventory. */
    float trendMult = 1.0f;
    int avgHistory = (marketGrainHistory[0] + marketGrainHistory[1]
                      + marketGrainHistory[2]) / 3;
    if (avgHistory > 0 && totalMarketGrain > 0)
    {
        float trend = static_cast<float>(totalMarketGrain)
                      / static_cast<float>(avgHistory);
        /* trend < 1: inventory shrinking → prices should rise.
         * trend > 1: inventory growing → prices should fall. */
        trendMult = 1.0f / trend;
        if (trendMult < 0.5f) trendMult = 0.5f;
        if (trendMult > 2.0f) trendMult = 2.0f;
    }

    /* Harvest quality: compare actual total harvest vs total demand.
     * Good harvest (surplus) → lower prices; bad harvest → higher prices.
     * Uses aggregate across all living players, not just this one. */
    float harvestMult = 1.0f;
    {
        int totalHarvest = 0;
        for (int i = 0; i < COUNTRY_COUNT; i++)
        {
            if (!playerList[i].dead)
                totalHarvest += playerList[i].grainHarvest;
        }
        if (totalDemand > 0)
        {
            float harvestRatio = static_cast<float>(totalHarvest)
                                 / static_cast<float>(totalDemand);
            /* ratio 2.0+ (bumper crop) → mult 0.5 (half price).
             * ratio 1.0 (break even) → mult 1.0.
             * ratio 0.5 (famine) → mult 1.5. */
            harvestMult = 1.0f / harvestRatio;
            if (harvestMult < 0.5f) harvestMult = 0.5f;
            if (harvestMult > 2.0f) harvestMult = 2.0f;
        }
    }

    float fundamentalPrice = GRAIN_PRICE_BASE * scarcityMult * trendMult * harvestMult;

    /* Blend with current market average for convergence.  CPUs anchor
     * 50% to fundamentals and 50% to what others are charging. */
    float marketAvg = 0.0f;
    int marketSellers = 0;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (!playerList[i].dead && playerList[i].grainForSale > 0)
        {
            marketAvg += playerList[i].grainPrice;
            marketSellers++;
        }
    }
    float price;
    if (marketSellers > 0)
        price = (fundamentalPrice + marketAvg / marketSellers) / 2.0f;
    else
        price = fundamentalPrice;

    /* Random noise: ±5-15% based on difficulty (L5 ≈ ±5%, L1 ≈ ±15%). */
    int noisePct = 5 + effError / 5;
    float noise = 1.0f + static_cast<float>(RandRange(2 * noisePct + 1)
                                            - noisePct - 1) / 100.0f;
    price *= noise;

    if (price < GRAIN_PRICE_BASE * 0.25f) price = GRAIN_PRICE_BASE * 0.25f;
    if (price > GRAIN_PRICE_MAX) price = GRAIN_PRICE_MAX;

    GameLog("  Target price: %.4f (scarcity=%.2f trend=%.2f harvest=%.2f noise=%.0f%%)\n",
            price, scarcityMult, trendMult, harvestMult, (noise - 1.0f) * 100.0f);
    return price;
}


int CPUStrategy::deviate(int optimal, int maxVal, int err)
{
    int range = maxVal * err / 100;
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
    int overfeedNeed = aPlayer->peopleGrainNeed * CPU_OVERFEED_PCT / 100
                       + aPlayer->armyGrainNeed;
    int effError = ComputeErrorPct(aPlayer->cpuDifficulty);

    /*
     * Chance to act at all: smarter CPUs trade more often.
     * Low difficulty: ~50% chance to skip. High difficulty: ~5% chance.
     */
    if (RandRange(100) <= effError)
        return;

    /*
     * WITHDRAW grain from our own market listing when needed.
     * First priority: starvation. Second: immigration threshold.
     */
    if (aPlayer->grainForSale > 0)
    {
        int target;
        if (aPlayer->grain < totalNeed)
        {
            /* Starving — withdraw enough to feed at 100%. */
            target = totalNeed;
        }
        else
        {
            /* Not starving — withdraw enough to overfeed for immigration. */
            target = overfeedNeed;
        }

        if (aPlayer->grain < target)
        {
            int deficit = target - aPlayer->grain;
            /* Withdraw extra to offset the 15% spoilage. */
            int withdrawAmount = MIN(
                static_cast<int>(deficit / (1.0f - GRAIN_WITHDRAW_FEE)) + 1,
                aPlayer->grainForSale);
            WithdrawGrain(aPlayer, withdrawAmount);
        }
    }

    /*
     * BUY GRAIN if we still can't feed our people and army.
     */
    if (aPlayer->grain < totalNeed)
    {
        int deficit = totalNeed - aPlayer->grain;
        Player *cheapest = nullptr;
        for (int i = 0; i < COUNTRY_COUNT; i++)
        {
            Player *p = &playerList[i];
            if (p == aPlayer || p->dead || p->grainForSale <= 0)
                continue;
            if (cheapest == nullptr ||
                p->grainPrice < cheapest->grainPrice)
                cheapest = p;
        }
        if (cheapest != nullptr)
            TradeGrain(aPlayer, cheapest, deficit);
    }

    /*
     * SELL SURPLUS GRAIN — only what exceeds the safe reserve plus
     * the immigration overfeed budget.  Keep enough to both survive
     * a bad year AND overfeed for immigration this year.
     */
    int reserve = ComputeSafeGrainReserve(aPlayer);
    reserve = reserve * deviate(110, 160, effError) / 100;
    /* Reserve enough to overfeed for immigration.  When immigration
     * is critical (2+ years without), protect the full overfeed budget. */
    int immigrationBudget = overfeedNeed - totalNeed;
    if (immigrationBudget > 0)
    {
        if (aPlayer->yearsSinceImmigration >= 2)
            reserve += immigrationBudget;       /* Protect full overfeed */
        else
            reserve += immigrationBudget / 2;   /* Protect half */
    }
    bool listedNewGrain = false;
    int surplus = aPlayer->grain - reserve;
    if (surplus > 500)
    {
        int sellAmount = surplus * (100 - effError) / 100;
        if (sellAmount > 100)
        {
            float price = ComputeGrainTargetPrice(aPlayer, effError);
            ListGrainForSale(aPlayer, sellAmount, price);
            listedNewGrain = true;
        }
    }

    /*
     * REPRICE existing grain listing based on market trends — but only if
     * we didn't just list new grain (which would skew the calculation).
     * Use a ±15% threshold to avoid constant micro-adjustments.
     */
    if (!listedNewGrain && aPlayer->grainForSale > 0)
    {
        float targetPrice = ComputeGrainTargetPrice(aPlayer, effError);
        if (targetPrice > aPlayer->grainPrice * 1.15f ||
            targetPrice < aPlayer->grainPrice * 0.85f)
        {
            aPlayer->grainPrice = targetPrice;
            GameLog("  Repriced grain to %.4f\n", targetPrice);
        }
    }

    /*
     * SELL LAND only as a last resort when treasury is empty and we have
     * far more land than we can farm.
     */
    int population = aPlayer->serfCount + aPlayer->merchantCount
                     + aPlayer->nobleCount + aPlayer->soldierCount;
    if (aPlayer->treasury < COST_SOLDIER &&
        aPlayer->land > population * CPU_LAND_SURPLUS_MULT)
    {
        int acresNeeded = (COST_MARKETPLACE - aPlayer->treasury + 1)
                          / static_cast<int>(LAND_SELL_PRICE);
        int maxSell = aPlayer->land / 20;
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
    int effError = ComputeErrorPct(aPlayer->cpuDifficulty);
    int wasteBudget = aPlayer->treasury * effError / 100;
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

    /* Power disparity pushes toward guns even with friendly diplomacy.
     * If any player has 2x+ our power, we need to arm up regardless. */
    float myPower = ComputePlayerPower(aPlayer);
    float maxEnvy = 0.0f;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == pi || playerList[i].dead)
            continue;
        float ratio = ComputePlayerPower(&playerList[i]) / myPower;
        if (ratio - 1.0f > maxEnvy) maxEnvy = ratio - 1.0f;
    }
    /* Each 1.0 of power ratio above 1.0 adds ~15% to guns budget. */
    gunsPct += static_cast<int>(maxEnvy * 15.0f);

    if (gunsPct < 20) gunsPct = 20;
    if (gunsPct > 80) gunsPct = 80;

    /* When guns dominate, sell grain reserves to fund military.
     * The more guns-heavy, the deeper we dip into reserves.
     * At 80% guns, willing to sell down to 50% of safe reserve.
     * At 50% guns, no grain selling for military. */
    if (gunsPct > 50 && aPlayer->grain > aPlayer->peopleGrainNeed)
    {
        float urgency = static_cast<float>(gunsPct - 50) / 30.0f;
        int minKeep = aPlayer->peopleGrainNeed
                      + static_cast<int>((1.0f - urgency * 0.5f)
                        * (aPlayer->grain - aPlayer->peopleGrainNeed));
        int canSell = aPlayer->grain - minKeep;
        if (canSell > 500)
        {
            /* Find market price or use base price. */
            float price = GRAIN_PRICE_BASE;
            for (int i = 0; i < COUNTRY_COUNT; i++)
            {
                if (&playerList[i] != aPlayer && !playerList[i].dead
                    && playerList[i].grainForSale > 0)
                {
                    price = playerList[i].grainPrice;
                    break;
                }
            }
            ListGrainForSale(aPlayer, canSell, price);
            GameLog("  Sold %d grain for military funding (urgency %.0f%%)\n",
                    canSell, urgency * 100.0f);
        }
    }

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
        int noblesCap = NOBLE_LEADERSHIP * aPlayer->nobleCount;
        float equipRatio = EQUIP_RATIO_BASE + EQUIP_RATIO_PER_FOUNDRY * aPlayer->foundryCount;
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

    /* Grain mills — only if we had a grain surplus and proportional to farmland. */
    if (aPlayer->grainHarvest > aPlayer->peopleGrainNeed + aPlayer->armyGrainNeed)
    {
        int usableLand = aPlayer->land - aPlayer->serfCount
                         - 2 * aPlayer->nobleCount - aPlayer->merchantCount
                         - 2 * aPlayer->soldierCount - aPlayer->palaceCount;
        if (usableLand < 0) usableLand = 0;
        int millsWanted = usableLand / CPU_ACRES_PER_GRAIN_MILL;
        int millDeficit = millsWanted - aPlayer->grainMillCount;
        if (millDeficit > 0)
        {
            desired = MIN(MIN(aPlayer->treasury / COST_GRAIN_MILL, millDeficit),
                          RandRange(2));
            bought = PurchaseInvestment(aPlayer, &aPlayer->grainMillCount,
                                        desired, COST_GRAIN_MILL);
            if (bought > 0)
                GameLog("  Bought %d grain mills (-%d)  Treasury: %d\n",
                        bought, bought * COST_GRAIN_MILL, aPlayer->treasury);
        }
    }

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
    int count = RandRange(aPlayer->soldierCount);
    if (count < CPU_MIN_ATTACK_FORCE) return 0;
    return count;
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
    int count = (aPlayer->soldierCount * 3 / 10)
                + RandRange(aPlayer->soldierCount * 3 / 10);
    if (count < CPU_MIN_ATTACK_FORCE) return 0;
    return count;
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
    int count = (aPlayer->soldierCount * 4 / 10)
                + RandRange(aPlayer->soldierCount * 3 / 10);
    if (count < CPU_MIN_ATTACK_FORCE) return 0;
    return count;
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

    /* Go all-in against vulnerable targets. */
    if (aTarget != nullptr)
    {
        float vuln = ComputeVulnerability(aTarget - playerList);
        if (vuln >= 2.0f)
            count = aPlayer->soldierCount * 3 / 4;
    }

    if (count > (aPlayer->soldierCount * 3 / 4))
        count = aPlayer->soldierCount * 3 / 4;
    if (count < CPU_MIN_ATTACK_FORCE) return 0;
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
        int targetIdx = aTarget - playerList;
        float vuln = ComputeVulnerability(targetIdx);
        int alliedTroops = ComputeAlliedStrength(aPlayer, targetIdx);

        int targetStrength = aTarget->soldierCount;
        if (targetStrength <= 0)
            targetStrength = aTarget->serfCount / 3;

        /* Base: 120% of target strength. */
        count = (targetStrength * 12 / 10) + RandRange(targetStrength / 5);

        /* Go all-in against vulnerable targets (0 soldiers, recently wounded). */
        if (vuln >= 2.0f)
            count = aPlayer->soldierCount * 3 / 4;
        /* Commit more when backed by allies. */
        else if (alliedTroops > targetStrength)
            count = MIN(count * 3 / 2, aPlayer->soldierCount * 3 / 4);

        if (count > (aPlayer->soldierCount * 3 / 4))
            count = aPlayer->soldierCount * 3 / 4;
    }
    else
    {
        count = aPlayer->soldierCount / 2;
    }
    if (count < CPU_MIN_ATTACK_FORCE) return 0;
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
     * ARBITRAGE: scan the market for grain priced below 2x base price
     * and buy it to relist at a profit.  Only if we have treasury to spare.
     */
    if (aPlayer->treasury > 3 * COST_MARKETPLACE)
    {
        for (int i = 0; i < COUNTRY_COUNT; i++)
        {
            Player *seller = &playerList[i];
            if (seller == aPlayer || seller->dead || seller->grainForSale <= 0)
                continue;

            /* Buy grain priced below 2x base price and relist higher. */
            if (seller->grainPrice < GRAIN_PRICE_BASE * 2.0f)
            {
                /* Spend up to 20% of treasury on arbitrage. */
                int budget = aPlayer->treasury / 5;
                float markupFactor = 1.0f - GRAIN_MARKUP;
                int canBuy = static_cast<int>(
                    (static_cast<float>(budget) * markupFactor) / seller->grainPrice);
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
                        float relistPrice = seller->grainPrice * 2.0f;
                        if (relistPrice < GRAIN_PRICE_BASE * 3.0f)
                            relistPrice = GRAIN_PRICE_BASE * 3.0f;
                        ListGrainForSale(aPlayer, bought, relistPrice);
                    }
                }
            }
        }
    }
}
