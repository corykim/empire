/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * Centralized economic rules for the Empire game.
 *
 * Every economic formula lives here exactly once.  Human UI screens and CPU
 * strategy code both call these functions, guaranteeing identical rules.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "economy.h"
#include "empire.h"


#include "diplomacy.h"

/*------------------------------------------------------------------------------
 *
 * Logging.
 */

bool logging = false;
const char *logFileName = "empire.log";
static FILE *logFile = nullptr;

void GameLog(const char *fmt, ...)
{
    if (!logging)
        return;

    if (logFile == nullptr)
    {
        logFile = fopen(logFileName, "w");
        if (logFile == nullptr)
        {
            logging = false;
            return;
        }
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(logFile, fmt, args);
    va_end(args);
    fflush(logFile);
}

/*------------------------------------------------------------------------------
 *
 * Grain phase.
 */

int ComputeExpectedRevenue(Player *aPlayer, int salesTax, int incomeTax)
{
    /* Expected values for RandRange(n) = (n+1)/2. */
    int expMerchantRand = 18 + 18;  /* RandRange(35) + RandRange(35) */
    int expSerfRand = 301 + 301;    /* RandRange(600) + RandRange(600) */
    int expFoundryRand = 76 + 400;  /* RandRange(150) + 400 */

    /* Marketplace revenue. */
    int mktPerUnit = (12 * (aPlayer->merchantCount + expMerchantRand)
                      / (salesTax + 1)) + 5;
    int mktRev = static_cast<int>(
        pow(aPlayer->marketplaceCount * mktPerUnit, 0.9));

    /* Grain mill revenue — serfs operate mills, penalized by income tax. */
    int millPerUnit = static_cast<int>(
        MILL_REV_MULT * static_cast<float>(aPlayer->serfCount + expSerfRand)
        / static_cast<float>(incomeTax + 1))
        + MILL_REV_ADD;
    int millRev = static_cast<int>(
        pow(aPlayer->grainMillCount * millPerUnit, 0.9));

    /* Foundry revenue (tax-independent). */
    int fndRev = static_cast<int>(
        pow(aPlayer->foundryCount * expFoundryRand, 0.9));

    /* Shipyard revenue (tax-independent, use avg weather = 3.5). */
    int shipBase = 4 * aPlayer->merchantCount
                   + 9 * aPlayer->marketplaceCount
                   + 15 * aPlayer->foundryCount;
    int shipRev = static_cast<int>(
        pow(static_cast<int>(aPlayer->shipyardCount * shipBase * 3.5f), 0.9));

    /* Army cost. */
    int armyCost = 8 * aPlayer->soldierCount;

    /* Sales tax revenue. */
    int salesBase = static_cast<int>(1.8f * aPlayer->merchantCount)
                    + 33 * mktRev + 17 * millRev
                    + 50 * fndRev + 70 * shipRev;
    int salesTaxRev = salesTax
                      * (static_cast<int>(pow(salesBase, 0.85))
                         + 5 * aPlayer->nobleCount + aPlayer->serfCount)
                      / 100;

    /* Income tax revenue. */
    int incomeBase = static_cast<int>(1.3f * aPlayer->serfCount)
                     + 145 * aPlayer->nobleCount
                     + 39 * aPlayer->merchantCount
                     + 99 * aPlayer->marketplaceCount
                     + INCOMETAX_MILL_MULT * millRev
                     + 425 * aPlayer->foundryCount
                     + 965 * aPlayer->shipyardCount;
    int incomeTaxRev = static_cast<int>(
        pow(incomeTax * incomeBase / 100, 0.97));

    return mktRev + millRev + fndRev + shipRev
           + salesTaxRev + incomeTaxRev - armyCost;
}


void OptimizeTaxRates(Player *aPlayer)
{
    int bestSales = aPlayer->salesTax;
    int bestIncome = aPlayer->incomeTax;
    int bestRevenue = ComputeExpectedRevenue(aPlayer, bestSales, bestIncome);

    for (int s = 0; s <= 20; s++)
    {
        for (int i = 0; i <= 35; i++)
        {
            int rev = ComputeExpectedRevenue(aPlayer, s, i);
            if (rev > bestRevenue)
            {
                bestRevenue = rev;
                bestSales = s;
                bestIncome = i;
            }
        }
    }

    GameLog("  Optimal taxes: sales=%d%% income=%d%% (expected revenue=%d)\n",
            bestSales, bestIncome, bestRevenue);
    aPlayer->salesTax = bestSales;
    aPlayer->incomeTax = bestIncome;
}


float EffectiveYieldMult(Player *aPlayer)
{
    /* Mill yield bonus scales inversely with weather: mills matter most
     * during poor harvests (insurance against bad weather streaks).
     * At weather=1: bonus × 1.5.  At weather=4 (avg): bonus × 0.75.
     * At weather=6: bonus × 0.25.  Weather 0 = pre-game, use neutral 1.0. */
    float weatherScale = (weather > 0)
                         ? static_cast<float>(7 - weather) / 4.0f
                         : 1.0f;
    return GRAIN_YIELD_MULT
           + MILL_YIELD_BONUS * sqrtf(static_cast<float>(aPlayer->grainMillCount))
             * weatherScale;
}


int EffectiveSeedRate(Player *aPlayer)
{
    return GRAIN_SEED_PER_ACRE
           + static_cast<int>(MILL_SEED_BONUS
             * sqrtf(static_cast<float>(aPlayer->grainMillCount)));
}


int ComputeWorstCaseHarvest(Player *aPlayer)
{
    int harvest = static_cast<int>(
        ComputeUsableLand(aPlayer) * EffectiveYieldMult(aPlayer));
    return (harvest > 0) ? harvest : 0;
}


int ComputeSafeGrainReserve(Player *aPlayer)
{
    /* Reserve enough to SURVIVE a worst-case year at 100% feeding.
     * Overfeeding is aspirational — don't reserve grain for it.
     * This frees grain for immigration-enabling overfeeds. */
    int totalNeed = aPlayer->peopleGrainNeed + aPlayer->armyGrainNeed;
    int worstHarvest = ComputeWorstCaseHarvest(aPlayer);
    int grainNeededFromReserve = totalNeed - worstHarvest;
    if (grainNeededFromReserve < 0) grainNeededFromReserve = 0;
    int reserve = static_cast<int>(grainNeededFromReserve / 0.70f);
    if (reserve < totalNeed) reserve = totalNeed;
    return reserve;
}





int ComputeUsableLand(Player *aPlayer)
{
    int usable = aPlayer->land
                 - aPlayer->serfCount
                 - 2 * aPlayer->nobleCount
                 - aPlayer->palaceCount
                 - aPlayer->merchantCount
                 - 2 * aPlayer->soldierCount;
    return (usable > 0) ? usable : 0;
}


int ComputeLandSustainabilityFloor(Player *aPlayer)
{
    int totalNeed = aPlayer->peopleGrainNeed + aPlayer->armyGrainNeed;
    float yieldMult = EffectiveYieldMult(aPlayer);
    return static_cast<int>(
        static_cast<float>(totalNeed) / (CPU_AVG_WEATHER * yieldMult));
}


float ComputeMarketplaceROI(Player *aPlayer)
{
    /* Evaluate marketplace ROI at salesTax=0 (its optimal environment).
     * This captures the true value: if we buy marketplaces, the tax
     * optimizer will lower sales tax, boosting all marketplace revenue. */
    int expRand = 36;
    int perUnit = (MKT_REV_MULT * (aPlayer->merchantCount + expRand)
                   / (0 + 1)) + MKT_REV_ADD;  /* salesTax=0 */
    int count = aPlayer->marketplaceCount;
    float revNow = (count > 0) ? pow(count * perUnit, REV_EXP_INVESTMENT) : 0.0f;
    float revNext = pow((count + 1) * perUnit, REV_EXP_INVESTMENT);
    return (revNext - revNow) / static_cast<float>(COST_MARKETPLACE);
}


float ComputeMillROI(Player *aPlayer)
{
    /* Evaluate mill ROI at incomeTax=0 (its optimal environment).
     * Symmetric with marketplace ROI — each investment evaluated at
     * the tax rate that maximizes its returns. */
    int expRand = 602;
    int perUnit = static_cast<int>(
        MILL_REV_MULT * static_cast<float>(aPlayer->serfCount + expRand)
        / static_cast<float>(0 + 1))  /* incomeTax=0 */
        + MILL_REV_ADD;
    int count = aPlayer->grainMillCount;
    float revNow = (count > 0) ? pow(count * perUnit, REV_EXP_INVESTMENT) : 0.0f;
    float revNext = pow((count + 1) * perUnit, REV_EXP_INVESTMENT);
    return (revNext - revNow) / static_cast<float>(COST_GRAIN_MILL);
}


void ComputeGrainPhase(Player *aPlayer)
{
    int usableLand;

    /* Rats eat grain. */
    aPlayer->ratPct = RandRange(MAX_RAT_PERCENT);
    aPlayer->grain -= (aPlayer->grain * aPlayer->ratPct) / 100;

    /* Usable land = total land minus space occupied by people and buildings. */
    usableLand = ComputeUsableLand(aPlayer);

    /* Each bushel of grain can seed acres based on mill count. */
    {
        int seedRate = EffectiveSeedRate(aPlayer);
        if (usableLand > (seedRate * aPlayer->grain))
            usableLand = seedRate * aPlayer->grain;
    }

    /* Each serf can farm 5 acres. */
    if (usableLand > (ACRES_PER_SERF * aPlayer->serfCount))
        usableLand = ACRES_PER_SERF * aPlayer->serfCount;

    /* Harvest. */
    aPlayer->grainHarvest =   (weather * usableLand * EffectiveYieldMult(aPlayer))
                            + RandRange(FOUNDRY_POLLUTION)
                            - (aPlayer->foundryCount * FOUNDRY_POLLUTION);
    if (aPlayer->grainHarvest < 0)
        aPlayer->grainHarvest = 0;
    aPlayer->grain += aPlayer->grainHarvest;

    /* Grain needs. */
    aPlayer->peopleGrainNeed = GRAIN_PER_PERSON * (  aPlayer->serfCount
                                    + aPlayer->merchantCount
                                    + (GRAIN_PER_NOBLE_MULT * aPlayer->nobleCount));
    aPlayer->armyGrainNeed = GRAIN_PER_SOLDIER * aPlayer->soldierCount;

    GameLog("  Weather: %d/6  Rats ate %d%%\n", weather, aPlayer->ratPct);
    GameLog("  Usable land: %d  Harvest: %d  Grain: %d\n",
            usableLand, aPlayer->grainHarvest, aPlayer->grain);
    GameLog("  People need: %d  Army need: %d\n",
            aPlayer->peopleGrainNeed, aPlayer->armyGrainNeed);
}


/*------------------------------------------------------------------------------
 *
 * Population phase.
 */

PopulationResult ComputePopulation(Player *aPlayer)
{
    PopulationResult r = {};

    int population = aPlayer->serfCount
                     + aPlayer->merchantCount
                     + aPlayer->nobleCount;

    /* Births. */
    r.born = RandRange(static_cast<int>(static_cast<float>(population) / BIRTH_RATE_DIV));

    /* Disease deaths. */
    r.diedDisease = RandRange(population / DISEASE_DEATH_DIV);

    /* Starvation and malnutrition. */
    if (aPlayer->peopleGrainNeed > (2 * aPlayer->peopleGrainFeed))
    {
        r.diedMalnutrition = RandRange(population / MALNUTRITION_DEATH_DIV_SEVERE + 1);
        r.diedStarvation = RandRange(population / STARVATION_DEATH_DIV + 1);
    }
    else if (aPlayer->peopleGrainNeed > aPlayer->peopleGrainFeed)
    {
        r.diedMalnutrition = RandRange(population / MALNUTRITION_DEATH_DIV + 1);
    }
    aPlayer->diedStarvation = r.diedStarvation;

    /* Immigration (requires feed > 1.5x need). */
    if (static_cast<float>(aPlayer->peopleGrainFeed) >=
        (IMMIGRATION_FEED_MULT * static_cast<float>(aPlayer->peopleGrainNeed)))
    {
        r.immigrated =
              static_cast<int>(sqrt(aPlayer->peopleGrainFeed
                                    - aPlayer->peopleGrainNeed))
            - RandRange(static_cast<int>(
                  IMMIGRATION_CUSTOMS_MULT * static_cast<float>(aPlayer->customsTax)));
        if (r.immigrated > 0)
            r.immigrated = RandRange(2 * r.immigrated + 1);
        else
            r.immigrated = 0;
    }
    aPlayer->immigrated = r.immigrated;

    /* Merchant and noble immigration. */
    if ((r.immigrated / IMMIGRANT_MERCHANT_RATIO) > 0)
        r.merchantsImmigrated = RandRange(r.immigrated / IMMIGRANT_MERCHANT_RATIO);
    if ((r.immigrated / IMMIGRANT_NOBLE_RATIO) > 0)
        r.noblesImmigrated = RandRange(r.immigrated / IMMIGRANT_NOBLE_RATIO);

    /* Army starvation and desertion. */
    if (aPlayer->armyGrainNeed > (2 * aPlayer->armyGrainFeed))
    {
        r.armyDiedStarvation = RandRange(aPlayer->soldierCount / 2 + 1);
        aPlayer->soldierCount -= r.armyDiedStarvation;
        r.armyDeserted = RandRange(aPlayer->soldierCount / 5);
        aPlayer->soldierCount -= r.armyDeserted;
    }

    /* Army efficiency: 10 = 100%, scales with feeding ratio.
     * When there's no army to feed, carry forward previous efficiency
     * with a floor of 100%.  This preserves the momentum of good army
     * management — a player who was overfeeding at 150% keeps that
     * efficiency for fresh recruits.  Underfed armies get reset to 100%. */
    if (aPlayer->armyGrainNeed > 0)
    {
        aPlayer->armyEfficiency = (10 * aPlayer->armyGrainFeed)
                                  / aPlayer->armyGrainNeed;
        if (aPlayer->armyEfficiency < 5)
            aPlayer->armyEfficiency = 5;
        else if (aPlayer->armyEfficiency > 15)
            aPlayer->armyEfficiency = 15;
    }
    else if (aPlayer->armyEfficiency < 10)
    {
        aPlayer->armyEfficiency = 10;
    }

    /* Net population gain. */
    r.populationGain = r.born + r.immigrated
                       - r.diedDisease - r.diedMalnutrition - r.diedStarvation;

    GameLog("  Born: %d  Disease: %d  Malnutrition: %d  Starvation: %d\n",
            r.born, r.diedDisease, r.diedMalnutrition, r.diedStarvation);
    GameLog("  Immigrated: %d (merchants: %d, nobles: %d)\n",
            r.immigrated, r.merchantsImmigrated, r.noblesImmigrated);
    GameLog("  Army starvation: %d  Deserted: %d  Efficiency: %d%%\n",
            r.armyDiedStarvation, r.armyDeserted, 10 * aPlayer->armyEfficiency);
    GameLog("  Population gain: %d\n", r.populationGain);

    return r;
}


void ApplyPopulationChanges(Player *aPlayer, const PopulationResult &result)
{
    aPlayer->serfCount += result.populationGain
                          - result.merchantsImmigrated
                          - result.noblesImmigrated;
    aPlayer->merchantCount += result.merchantsImmigrated;
    aPlayer->nobleCount += result.noblesImmigrated;
}


DeathCause CheckPlayerDeath(Player *aPlayer)
{
    DeathCause cause = DEATH_NONE;

    /*
     * Starvation assassination — probability proportional to the percentage
     * of population that starved.  If 10% starved, ~10% chance of
     * assassination.  Roll 1..100; if it lands at or below the starvation
     * percentage, the ruler is assassinated.
     */
    if (aPlayer->diedStarvation > 0)
    {
        int population = aPlayer->serfCount + aPlayer->merchantCount
                         + aPlayer->nobleCount;
        if (population < 1) population = 1;
        int starvePct = (100 * aPlayer->diedStarvation) / population;
        if (starvePct > 0 && RandRange(100) <= starvePct)
        {
            aPlayer->dead = true;
            cause = DEATH_STARVATION;
            OnPlayerDeath(aPlayer - playerList);
            GameLog("  *** ASSASSINATED (starvation: %d died, %d%% of pop) ***\n",
                    aPlayer->diedStarvation, starvePct);
        }
    }

    /* Random death — 0.25% chance per year (1 in 400). */
    if (RandRange(RANDOM_DEATH_CHANCE) == 1)
    {
        aPlayer->dead = true;
        cause = DEATH_RANDOM;
        OnPlayerDeath(aPlayer - playerList);
        GameLog("  *** DIED (random event) ***\n");
    }

    /* Remove grain from market when a player dies. */
    if (aPlayer->dead)
    {
        aPlayer->grainForSale = 0;
        aPlayer->grainPrice = 0.0;
    }

    return cause;
}


/*------------------------------------------------------------------------------
 *
 * Revenue computation.
 */

void ComputeRevenues(Player *aPlayer)
{
    int marketplaceRevenue;
    int grainMillRevenue;
    int foundryRevenue;
    int shipyardRevenue;
    int salesTaxRevenue;
    int incomeTaxRevenue;

    /* Marketplace revenue. */
    marketplaceRevenue =
          (  MKT_REV_MULT
           * (aPlayer->merchantCount + RandRange(MKT_REV_RAND) + RandRange(MKT_REV_RAND))
           / (aPlayer->salesTax + 1))
        + MKT_REV_ADD;
    marketplaceRevenue = aPlayer->marketplaceCount * marketplaceRevenue;
    aPlayer->marketplaceRevenue = pow(marketplaceRevenue, REV_EXP_INVESTMENT);

    /* Grain mill revenue — serfs operate mills, penalized by income tax.
     * Symmetric with marketplaces (merchants / sales tax). */
    grainMillRevenue =
          static_cast<int>(MILL_REV_MULT
           * static_cast<float>(aPlayer->serfCount
                                + RandRange(MILL_REV_RAND) + RandRange(MILL_REV_RAND))
           / static_cast<float>(aPlayer->incomeTax + 1))
        + MILL_REV_ADD;
    grainMillRevenue = aPlayer->grainMillCount * grainMillRevenue;
    aPlayer->grainMillRevenue = pow(grainMillRevenue, REV_EXP_INVESTMENT);

    /* Foundry revenue. */
    foundryRevenue = aPlayer->soldierCount + RandRange(FOUNDRY_REV_RAND) + FOUNDRY_REV_BASE;
    foundryRevenue = aPlayer->foundryCount * foundryRevenue;
    foundryRevenue = pow(foundryRevenue, REV_EXP_INVESTMENT);
    aPlayer->foundryRevenue = foundryRevenue;

    /* Shipyard revenue. */
    shipyardRevenue =
        (  SHIP_MULT_MERCHANT * aPlayer->merchantCount
         + SHIP_MULT_MARKETPLACE * aPlayer->marketplaceCount
         + SHIP_MULT_FOUNDRY * aPlayer->foundryCount);
    shipyardRevenue = aPlayer->shipyardCount * shipyardRevenue * weather;
    shipyardRevenue = pow(shipyardRevenue, REV_EXP_INVESTMENT);
    aPlayer->shipyardRevenue = shipyardRevenue;

    /* Army cost (negative revenue). */
    aPlayer->soldierRevenue = -GRAIN_PER_SOLDIER * aPlayer->soldierCount;

    /* Customs tax revenue. */
    aPlayer->customsTaxRevenue = aPlayer->customsTax
                                 * aPlayer->immigrated
                                 * (RandRange(40) + RandRange(40))
                                 / 100;

    /* Sales tax revenue. */
    salesTaxRevenue =
        (  static_cast<int>(SALESTAX_MERCHANT_MULT * static_cast<float>(aPlayer->merchantCount))
         + SALESTAX_MKT_MULT * aPlayer->marketplaceRevenue
         + SALESTAX_MILL_MULT * aPlayer->grainMillRevenue
         + SALESTAX_FOUNDRY_MULT * aPlayer->foundryRevenue
         + SALESTAX_SHIPYARD_MULT * aPlayer->shipyardRevenue);
    salesTaxRevenue = pow(salesTaxRevenue, REV_EXP_SALES_TAX);
    aPlayer->salesTaxRevenue =
          aPlayer->salesTax
        * (salesTaxRevenue + SALESTAX_NOBLE_MULT * aPlayer->nobleCount + aPlayer->serfCount)
        / 100;

    /* Income tax revenue. */
    incomeTaxRevenue =
          static_cast<int>(INCOMETAX_SERF_MULT * static_cast<float>(aPlayer->serfCount))
        + INCOMETAX_NOBLE_MULT * aPlayer->nobleCount
        + INCOMETAX_MERCHANT_MULT * aPlayer->merchantCount
        + INCOMETAX_MKT_MULT * aPlayer->marketplaceCount
        + INCOMETAX_MILL_MULT * aPlayer->grainMillRevenue
        + INCOMETAX_FOUNDRY_MULT * aPlayer->foundryCount
        + INCOMETAX_SHIPYARD_MULT * aPlayer->shipyardCount;
    incomeTaxRevenue = aPlayer->incomeTax * incomeTaxRevenue / 100;
    aPlayer->incomeTaxRevenue = pow(incomeTaxRevenue, REV_EXP_INCOME_TAX);

    /* Update treasury. */
    int totalRevenue = aPlayer->customsTaxRevenue
                       + aPlayer->salesTaxRevenue
                       + aPlayer->incomeTaxRevenue
                       + aPlayer->marketplaceRevenue
                       + aPlayer->grainMillRevenue
                       + aPlayer->foundryRevenue
                       + aPlayer->shipyardRevenue
                       + aPlayer->soldierRevenue;
    aPlayer->treasury += totalRevenue;

    GameLog("  Revenue: Customs %d, Sales %d, Income %d\n",
            aPlayer->customsTaxRevenue, aPlayer->salesTaxRevenue,
            aPlayer->incomeTaxRevenue);
    GameLog("  Revenue: Mkt %d, Mill %d, Fnd %d, Ship %d, Army %d\n",
            aPlayer->marketplaceRevenue, aPlayer->grainMillRevenue,
            aPlayer->foundryRevenue, aPlayer->shipyardRevenue,
            aPlayer->soldierRevenue);
    GameLog("  Total revenue: %d  Treasury: %d\n", totalRevenue,
            aPlayer->treasury);
}


/*------------------------------------------------------------------------------
 *
 * Purchase functions.
 */

SoldierCap ComputeSoldierCap(Player *aPlayer)
{
    SoldierCap cap;

    int totalPeople = aPlayer->serfCount
                      + aPlayer->merchantCount
                      + aPlayer->nobleCount;
    float equipRatio = EQUIP_RATIO_BASE + EQUIP_RATIO_PER_FOUNDRY * aPlayer->foundryCount;
    int maxEquip = MAX(0, static_cast<int>(equipRatio * totalPeople)
                          - aPlayer->soldierCount);
    int maxNobles = MAX(0, (NOBLE_LEADERSHIP * aPlayer->nobleCount)
                           - aPlayer->soldierCount);
    int maxTreasury = aPlayer->treasury / COST_SOLDIER;
    int maxSerfs = aPlayer->serfCount;

    cap.maxSoldiers = MIN(MIN(maxTreasury, maxSerfs),
                          MIN(maxEquip, maxNobles));
    if (cap.maxSoldiers < 0)
        cap.maxSoldiers = 0;

    /* Determine the limiting factor. */
    if (cap.maxSoldiers == maxNobles)
        cap.limiter = SoldierCap::NOBLES;
    else if (cap.maxSoldiers == maxEquip)
        cap.limiter = SoldierCap::FOUNDRIES;
    else if (cap.maxSoldiers == maxSerfs)
        cap.limiter = SoldierCap::SERFS;
    else
        cap.limiter = SoldierCap::TREASURY;

    return cap;
}


int PurchaseInvestment(Player *aPlayer, int *count, int desired, int cost)
{
    if (desired <= 0)
        return 0;

    int affordable = aPlayer->treasury / cost;
    if (desired > affordable)
        desired = affordable;
    if (desired <= 0)
        return 0;

    aPlayer->treasury -= desired * cost;
    (*count) += desired;
    return desired;
}


int PurchaseSoldiers(Player *aPlayer, int desired)
{
    if (desired <= 0)
        return 0;

    SoldierCap cap = ComputeSoldierCap(aPlayer);
    if (desired > cap.maxSoldiers)
        desired = cap.maxSoldiers;
    if (desired <= 0)
        return 0;

    aPlayer->soldierCount += desired;
    aPlayer->serfCount -= desired;
    aPlayer->treasury -= desired * COST_SOLDIER;

    GameLog("  Recruited %d soldiers (-%d)  Treasury: %d\n",
            desired, desired * COST_SOLDIER, aPlayer->treasury);

    return desired;
}


void ApplyMarketplaceBonus(Player *aPlayer)
{
    int newMerch = MIN(RandRange(7), aPlayer->serfCount);
    aPlayer->merchantCount += newMerch;
    aPlayer->serfCount -= newMerch;
    GameLog("    +%d merchants (marketplace bonus)\n", newMerch);
}


void ApplyPalaceBonus(Player *aPlayer)
{
    int newNobles = MIN(RandRange(4), aPlayer->serfCount);
    aPlayer->nobleCount += newNobles;
    aPlayer->serfCount -= newNobles;
    GameLog("    +%d nobles (palace bonus)\n", newNobles);
}


/*------------------------------------------------------------------------------
 *
 * Grain and land trading.
 */

int TradeGrain(Player *buyer, Player *seller, int amount)
{
    if (amount <= 0 || seller->grainForSale <= 0 || seller->grainPrice <= 0)
        return 0;

    /* Clamp to what's available. */
    if (amount > seller->grainForSale)
        amount = seller->grainForSale;

    /* Price includes marketplace markup. */
    float markupFactor = 1.0f - GRAIN_MARKUP;
    int totalPrice = static_cast<int>(
        (static_cast<float>(amount) * seller->grainPrice) / markupFactor);
    if (totalPrice > buyer->treasury)
    {
        /* Buy only what we can afford. */
        amount = static_cast<int>(
            (static_cast<float>(buyer->treasury) * markupFactor) / seller->grainPrice);
        if (amount <= 0)
            return 0;
        totalPrice = static_cast<int>(
            (static_cast<float>(amount) * seller->grainPrice) / markupFactor);
    }

    buyer->grain += amount;
    buyer->treasury -= totalPrice;
    seller->treasury += static_cast<int>(amount * seller->grainPrice);
    seller->grainForSale -= amount;

    GameLog("  Bought %d grain from %s at %.2f (-%d)  Treasury: %d  Grain: %d\n",
            amount, seller->country->name, seller->grainPrice,
            totalPrice, buyer->treasury, buyer->grain);
    return amount;
}


void ListGrainForSale(Player *aPlayer, int amount, float price)
{
    if (amount <= 0 || price <= 0.0)
        return;
    if (amount > aPlayer->grain)
        amount = aPlayer->grain;
    if (amount <= 0)
        return;

    /* Blend price with any existing listing. */
    aPlayer->grainPrice =
          (  (aPlayer->grainPrice * static_cast<float>(aPlayer->grainForSale))
           + (price * static_cast<float>(amount)))
        / static_cast<float>(aPlayer->grainForSale + amount);
    aPlayer->grainForSale += amount;
    aPlayer->grain -= amount;

    GameLog("  Listed %d grain at %.2f (for sale: %d)  Grain: %d\n",
            amount, price, aPlayer->grainForSale, aPlayer->grain);
}


int WithdrawGrain(Player *aPlayer, int amount)
{
    if (amount <= 0 || aPlayer->grainForSale <= 0)
        return 0;
    if (amount > aPlayer->grainForSale)
        amount = aPlayer->grainForSale;

    int returned = static_cast<int>(amount * (1.0f - GRAIN_WITHDRAW_FEE));
    aPlayer->grainForSale -= amount;
    aPlayer->grain += returned;
    int lost = amount - returned;

    /* Reset price if nothing left on market. */
    if (aPlayer->grainForSale <= 0)
        aPlayer->grainPrice = 0.0f;

    GameLog("  Withdrew %d grain (-%d spoilage)  For sale: %d  Grain: %d\n",
            amount, lost, aPlayer->grainForSale, aPlayer->grain);
    return returned;
}


int SellLandToBarbarians(Player *aPlayer, int acres)
{
    if (acres <= 0)
        return 0;

    /* Can't sell more than LAND_MAX_SELL_PCT of land. */
    int maxSell = static_cast<int>(LAND_MAX_SELL_PCT * static_cast<float>(aPlayer->land));
    if (acres > maxSell)
        acres = maxSell;
    if (acres <= 0)
        return 0;

    aPlayer->treasury += static_cast<int>(LAND_SELL_PRICE) * acres;
    aPlayer->land -= acres;
    barbarianLand += acres;

    GameLog("  Sold %d acres (+%d)  Treasury: %d  Land: %d\n",
            acres, 2 * acres, aPlayer->treasury, aPlayer->land);
    return acres;
}
