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


/*------------------------------------------------------------------------------
 *
 * Logging.
 */

bool logging = false;
static FILE *logFile = nullptr;

void GameLog(const char *fmt, ...)
{
    if (!logging)
        return;

    if (logFile == nullptr)
    {
        logFile = fopen("empire.log", "w");
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
 * Diplomacy.
 */

void InitDiplomacy()
{
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        for (int j = 0; j < COUNTRY_COUNT; j++)
        {
            if (i == j)
            {
                playerList[i].diplomacy[j] = 0.0f;
            }
            else
            {
                /* Random value in [-DIPLOMACY_INIT_RANGE, +DIPLOMACY_INIT_RANGE]. */
                float r = static_cast<float>(RandRange(1001) - 501) / 500.0f;
                playerList[i].diplomacy[j] = r * DIPLOMACY_INIT_RANGE;
            }
        }
        playerList[i].attackedTargets = 0;
    }
    LogAllDiplomacy("Diplomacy Initialized");
}


void DecayDiplomacy(Player *aPlayer)
{
    int pi = aPlayer - playerList;

    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == pi || playerList[i].dead || playerList[i].human)
            continue;

        float old = playerList[i].diplomacy[pi];
        playerList[i].diplomacy[pi] *= (1.0f - DIPLOMACY_DECAY_RATE);
        GameLog("  Diplomacy decay: %s→%s %.3f→%.3f\n",
                playerList[i].country->name, aPlayer->country->name,
                old, playerList[i].diplomacy[pi]);
    }
}


void UpdateDiplomacyAfterTurn(Player *aPlayer)
{
    int pi = aPlayer - playerList;

    if (aPlayer->attackedTargets == 0)
    {
        /* Peaceful turn: all CPUs increase diplomacy toward this player.
         * No peace bonus during treaty years — peace is mandatory. */
        if (year >= treatyYears)
        {
            for (int i = 0; i < COUNTRY_COUNT; i++)
            {
                if (i == pi || playerList[i].dead || playerList[i].human)
                    continue;

                float old = playerList[i].diplomacy[pi];
                playerList[i].diplomacy[pi] += DIPLOMACY_PEACE_BONUS;
                GameLog("  Diplomacy peace: %s→%s %.3f→%.3f\n",
                        playerList[i].country->name, aPlayer->country->name,
                        old, playerList[i].diplomacy[pi]);
            }
        }
    }
    else
    {
        /* Aggressive turn: adjust diplomacy based on who was attacked. */
        for (int i = 0; i < COUNTRY_COUNT; i++)
        {
            if (i == pi || playerList[i].dead || playerList[i].human)
                continue;

            float old = playerList[i].diplomacy[pi];

            /* Check each target that was attacked. */
            for (int t = 0; t < COUNTRY_COUNT; t++)
            {
                if (!(aPlayer->attackedTargets & (1 << t)))
                    continue;

                if (t == i)
                {
                    /* Attacked me — diplomacy drops to floor. */
                    playerList[i].diplomacy[pi] = DIPLOMACY_ATTACKED_ME;
                }
                else
                {
                    playerList[i].diplomacy[pi] +=
                        PredictThirdPartyShift(i, t, pi);
                }
            }

            if (playerList[i].diplomacy[pi] != old)
            {
                GameLog("  Diplomacy attack: %s→%s %.3f→%.3f\n",
                        playerList[i].country->name, aPlayer->country->name,
                        old, playerList[i].diplomacy[pi]);
            }
        }
    }

    /* Reset attacked targets for next turn. */
    aPlayer->attackedTargets = 0;
}


void LogAllDiplomacy(const char *label)
{
    GameLog("--- %s ---\n", label);
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (playerList[i].dead)
            continue;
        LogPlayerDiplomacy(&playerList[i]);
    }
}


void LogPlayerDiplomacy(Player *aPlayer)
{
    int pi = aPlayer - playerList;
    GameLog("  %s:", aPlayer->country->name);
    for (int j = 0; j < COUNTRY_COUNT; j++)
    {
        if (j != pi && !playerList[j].dead)
            GameLog(" %s=%+.3f", playerList[j].country->name,
                    aPlayer->diplomacy[j]);
    }
    GameLog("\n");
}


int MaxSoldiers()
{
    int max = 1;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (!playerList[i].dead && playerList[i].soldierCount > max)
            max = playerList[i].soldierCount;
    }
    return max;
}


float MilitaryWeakness(int soldierCount)
{
    return 1.0f - (static_cast<float>(soldierCount)
                   / static_cast<float>(MaxSoldiers()));
}


float PredictThirdPartyShift(int observerIdx, int targetIdx, int attackerIdx)
{
    float k = playerList[observerIdx].diplomacy[targetIdx];
    float scale = DIPLOMACY_THIRD_PARTY_SCALE;
    if (k > 0.0f)
    {
        float targetWeakness = MilitaryWeakness(playerList[targetIdx].soldierCount);
        float attackerStrength = 1.0f - MilitaryWeakness(playerList[attackerIdx].soldierCount);
        scale *= (1.0f + targetWeakness * attackerStrength
                         * DIPLOMACY_WEAKNESS_SCALE);
    }
    return -k * scale;
}


float DiplomacyAttackWeight(float diplomacy, int targetSoldierCount)
{
    float w = 1.0f - diplomacy;
    if (w < 0.01f) w = 0.01f;
    if (diplomacy < 0.0f)
        w *= (1.0f + MilitaryWeakness(targetSoldierCount) * DIPLOMACY_WEAKNESS_SCALE);
    return w;
}


int ComputeRetaliationReserve(Player *aPlayer)
{
    int pi = aPlayer - playerList;
    float netThreat = 0.0f;

    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == pi || playerList[i].dead)
            continue;

        /* Theory of mind: read the other player's diplomacy toward us.
         * Enemies (negative diplomacy) add threat proportional to their
         * soldiers and hostility.  Allies (positive diplomacy) reduce
         * the perceived threat — their presence makes us safer. */
        float theirDiplomacy = playerList[i].diplomacy[pi];
        float soldiers = static_cast<float>(playerList[i].soldierCount);
        netThreat -= theirDiplomacy * soldiers;
    }

    if (netThreat < 0.0f) netThreat = 0.0f;
    int reserve = static_cast<int>(netThreat * DIPLOMACY_RESERVE_SCALE);
    GameLog("  Retaliation reserve: %d soldiers (net threat: %.0f)\n",
            reserve, netThreat);
    return reserve;
}


float ComputePlayerPower(Player *aPlayer)
{
    float power = static_cast<float>(aPlayer->soldierCount)
                  + static_cast<float>(aPlayer->land) / 50.0f
                  + static_cast<float>(aPlayer->treasury) / 500.0f
                  + static_cast<float>(aPlayer->merchantCount) / 5.0f
                  + static_cast<float>(aPlayer->nobleCount) * 2.0f
                  + static_cast<float>(aPlayer->marketplaceCount)
                  + static_cast<float>(aPlayer->foundryCount) * 2.0f
                  + static_cast<float>(aPlayer->shipyardCount) * 3.0f
                  + static_cast<float>(aPlayer->grain) / 1000.0f;
    if (power < 1.0f) power = 1.0f;
    return power;
}


int ComputeDesiredTroopStrength(Player *aPlayer)
{
    int reserve = ComputeRetaliationReserve(aPlayer);

    /* Estimate troops needed for attacks.  Look at potential targets
     * and estimate how many soldiers we'd want to send.  Use the
     * weakest enemy (lowest diplomacy) as the likely target. */
    int pi = aPlayer - playerList;
    int attackTroops = 0;
    float worstDiplomacy = 0.0f;
    int worstTargetSoldiers = 0;

    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == pi || playerList[i].dead)
            continue;
        if (aPlayer->diplomacy[i] < worstDiplomacy)
        {
            worstDiplomacy = aPlayer->diplomacy[i];
            worstTargetSoldiers = playerList[i].soldierCount;
        }
    }

    /* If we have an enemy, plan to send ~1.5x their soldiers. */
    if (worstDiplomacy < 0.0f)
        attackTroops = worstTargetSoldiers * 3 / 2;

    aPlayer->desiredTroops = reserve + attackTroops;
    GameLog("  Desired troops: %d (reserve=%d, attack=%d, current=%d)\n",
            aPlayer->desiredTroops, reserve, attackTroops,
            aPlayer->soldierCount);
    return aPlayer->desiredTroops;
}


float SimulateAttackOutcome(Player *attacker, int targetIdx)
{
    int ai = attacker - playerList;
    float allyScore = 0.0f;
    float retaliationRisk = 0.0f;

    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == ai || i == targetIdx || playerList[i].dead)
            continue;

        float predictedShift = PredictThirdPartyShift(i, targetIdx, ai);
        float predictedDiplomacy = playerList[i].diplomacy[ai] + predictedShift;

        if (predictedDiplomacy >= 0.0f)
            allyScore += predictedDiplomacy;
        else
            retaliationRisk += -predictedDiplomacy
                               * static_cast<float>(playerList[i].soldierCount);
    }

    /* The target itself will set diplomacy to DIPLOMACY_ATTACKED_ME toward us. */
    retaliationRisk += -DIPLOMACY_ATTACKED_ME
                       * static_cast<float>(playerList[targetIdx].soldierCount);

    /* Normalize retaliation risk to a comparable scale with ally scores.
     * Raw risk can be in the hundreds (soldiers * diplomacy), so scale down. */
    float score = allyScore - retaliationRisk * 0.005f;

    GameLog("    Simulate attack %s→%s: allies=%.2f retaliation=%.1f score=%.3f\n",
            attacker->country->name, playerList[targetIdx].country->name,
            allyScore, retaliationRisk, score);

    return score;
}


/*------------------------------------------------------------------------------
 *
 * Grain phase.
 */

int ComputeExpectedRevenue(Player *aPlayer, int salesTax, int incomeTax)
{
    /* Expected values for RandRange(n) = (n+1)/2. */
    int expMerchantRand = 18 + 18;  /* RandRange(35) + RandRange(35) */
    int expHarvestRand = 126;       /* RandRange(250) */
    int expFoundryRand = 76 + 400;  /* RandRange(150) + 400 */

    /* Marketplace revenue. */
    int mktPerUnit = (12 * (aPlayer->merchantCount + expMerchantRand)
                      / (salesTax + 1)) + 5;
    int mktRev = static_cast<int>(
        pow(aPlayer->marketplaceCount * mktPerUnit, 0.9));

    /* Grain mill revenue. */
    int millPerUnit = static_cast<int>(
        5.8f * static_cast<float>(aPlayer->grainHarvest + expHarvestRand))
        / (20 * incomeTax + 40 * salesTax + 150);
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
                     + 99 * aPlayer->grainMillCount
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


void ComputeGrainPhase(Player *aPlayer)
{
    int usableLand;

    /* Rats eat grain. */
    aPlayer->ratPct = RandRange(30);
    aPlayer->grain -= (aPlayer->grain * aPlayer->ratPct) / 100;

    /* Usable land = total land minus space occupied by people and buildings. */
    usableLand =   aPlayer->land
                 - aPlayer->serfCount
                 - (2 * aPlayer->nobleCount)
                 - aPlayer->palaceCount
                 - aPlayer->merchantCount
                 - (2 * aPlayer->soldierCount);

    /* Each bushel of grain can seed 3 acres. */
    if (usableLand > (3 * aPlayer->grain))
        usableLand = 3 * aPlayer->grain;

    /* Each serf can farm 5 acres. */
    if (usableLand > (5 * aPlayer->serfCount))
        usableLand = 5 * aPlayer->serfCount;

    /* Harvest. */
    aPlayer->grainHarvest =   (weather * usableLand * 0.72)
                            + RandRange(500)
                            - (aPlayer->foundryCount * 500);
    if (aPlayer->grainHarvest < 0)
        aPlayer->grainHarvest = 0;
    aPlayer->grain += aPlayer->grainHarvest;

    /* Grain needs. */
    aPlayer->peopleGrainNeed = 5 * (  aPlayer->serfCount
                                    + aPlayer->merchantCount
                                    + (3 * aPlayer->nobleCount));
    aPlayer->armyGrainNeed = 8 * aPlayer->soldierCount;

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
    r.born = RandRange(static_cast<int>(static_cast<float>(population) / 9.5));

    /* Disease deaths. */
    r.diedDisease = RandRange(population / 22);

    /* Starvation and malnutrition. */
    if (aPlayer->peopleGrainNeed > (2 * aPlayer->peopleGrainFeed))
    {
        r.diedMalnutrition = RandRange(population / 12 + 1);
        r.diedStarvation = RandRange(population / 16 + 1);
    }
    else if (aPlayer->peopleGrainNeed > aPlayer->peopleGrainFeed)
    {
        r.diedMalnutrition = RandRange(population / 15 + 1);
    }
    aPlayer->diedStarvation = r.diedStarvation;

    /* Immigration (requires feed > 1.5x need). */
    if (static_cast<float>(aPlayer->peopleGrainFeed) >
        (1.5 * static_cast<float>(aPlayer->peopleGrainNeed)))
    {
        r.immigrated =
              static_cast<int>(sqrt(aPlayer->peopleGrainFeed
                                    - aPlayer->peopleGrainNeed))
            - RandRange(static_cast<int>(
                  1.5 * static_cast<float>(aPlayer->customsTax)));
        if (r.immigrated > 0)
            r.immigrated = RandRange(2 * r.immigrated + 1);
        else
            r.immigrated = 0;
    }
    aPlayer->immigrated = r.immigrated;

    /* Merchant and noble immigration. */
    if ((r.immigrated / 5) > 0)
        r.merchantsImmigrated = RandRange(r.immigrated / 5);
    if ((r.immigrated / 25) > 0)
        r.noblesImmigrated = RandRange(r.immigrated / 25);

    /* Army starvation and desertion. */
    if (aPlayer->armyGrainNeed > (2 * aPlayer->armyGrainFeed))
    {
        r.armyDiedStarvation = RandRange(aPlayer->soldierCount / 2 + 1);
        aPlayer->soldierCount -= r.armyDiedStarvation;
        r.armyDeserted = RandRange(aPlayer->soldierCount / 5);
        aPlayer->soldierCount -= r.armyDeserted;
    }

    /* Army efficiency (with division-by-zero guard). */
    aPlayer->armyEfficiency = (10 * aPlayer->armyGrainFeed)
                              / (aPlayer->armyGrainNeed > 0
                                 ? aPlayer->armyGrainNeed : 1);
    if (aPlayer->armyEfficiency < 5)
        aPlayer->armyEfficiency = 5;
    else if (aPlayer->armyEfficiency > 15)
        aPlayer->armyEfficiency = 15;

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
            GameLog("  *** ASSASSINATED (starvation: %d died, %d%% of pop) ***\n",
                    aPlayer->diedStarvation, starvePct);
        }
    }

    /* Random death — 0.25% chance per year (1 in 400). */
    if (RandRange(400) == 1)
    {
        aPlayer->dead = true;
        cause = DEATH_RANDOM;
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
          (  12
           * (aPlayer->merchantCount + RandRange(35) + RandRange(35))
           / (aPlayer->salesTax + 1))
        + 5;
    marketplaceRevenue = aPlayer->marketplaceCount * marketplaceRevenue;
    aPlayer->marketplaceRevenue = pow(marketplaceRevenue, 0.9);

    /* Grain mill revenue. */
    grainMillRevenue =
          static_cast<int>(5.8 * static_cast<float>(
              aPlayer->grainHarvest + RandRange(250)))
        / (20 * aPlayer->incomeTax + 40 * aPlayer->salesTax + 150);
    grainMillRevenue = aPlayer->grainMillCount * grainMillRevenue;
    aPlayer->grainMillRevenue = pow(grainMillRevenue, 0.9);

    /* Foundry revenue. */
    foundryRevenue = aPlayer->soldierCount + RandRange(150) + 400;
    foundryRevenue = aPlayer->foundryCount * foundryRevenue;
    foundryRevenue = pow(foundryRevenue, 0.9);
    aPlayer->foundryRevenue = foundryRevenue;

    /* Shipyard revenue. */
    shipyardRevenue =
        (  4 * aPlayer->merchantCount
         + 9 * aPlayer->marketplaceCount
         + 15 * aPlayer->foundryCount);
    shipyardRevenue = aPlayer->shipyardCount * shipyardRevenue * weather;
    shipyardRevenue = pow(shipyardRevenue, 0.9);
    aPlayer->shipyardRevenue = shipyardRevenue;

    /* Army cost (negative revenue). */
    aPlayer->soldierRevenue = -8 * aPlayer->soldierCount;

    /* Customs tax revenue. */
    aPlayer->customsTaxRevenue = aPlayer->customsTax
                                 * aPlayer->immigrated
                                 * (RandRange(40) + RandRange(40))
                                 / 100;

    /* Sales tax revenue. */
    salesTaxRevenue =
        (  static_cast<int>(1.8 * static_cast<float>(aPlayer->merchantCount))
         + 33 * aPlayer->marketplaceRevenue
         + 17 * aPlayer->grainMillRevenue
         + 50 * aPlayer->foundryRevenue
         + 70 * aPlayer->shipyardRevenue);
    salesTaxRevenue = pow(salesTaxRevenue, 0.85);
    aPlayer->salesTaxRevenue =
          aPlayer->salesTax
        * (salesTaxRevenue + 5 * aPlayer->nobleCount + aPlayer->serfCount)
        / 100;

    /* Income tax revenue. */
    incomeTaxRevenue =
          static_cast<int>(1.3 * static_cast<float>(aPlayer->serfCount))
        + 145 * aPlayer->nobleCount
        + 39 * aPlayer->merchantCount
        + 99 * aPlayer->marketplaceCount
        + 99 * aPlayer->grainMillCount
        + 425 * aPlayer->foundryCount
        + 965 * aPlayer->shipyardCount;
    incomeTaxRevenue = aPlayer->incomeTax * incomeTaxRevenue / 100;
    aPlayer->incomeTaxRevenue = pow(incomeTaxRevenue, 0.97);

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
    float equipRatio = 0.05 + 0.015 * aPlayer->foundryCount;
    int maxEquip = MAX(0, static_cast<int>(equipRatio * totalPeople)
                          - aPlayer->soldierCount);
    int maxNobles = MAX(0, (20 * aPlayer->nobleCount)
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

    /* Price includes 10% marketplace markup (same as human path). */
    int totalPrice = static_cast<int>(
        (static_cast<float>(amount) * seller->grainPrice) / 0.9);
    if (totalPrice > buyer->treasury)
    {
        /* Buy only what we can afford. */
        amount = static_cast<int>(
            (static_cast<float>(buyer->treasury) * 0.9) / seller->grainPrice);
        if (amount <= 0)
            return 0;
        totalPrice = static_cast<int>(
            (static_cast<float>(amount) * seller->grainPrice) / 0.9);
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


int SellLandToBarbarians(Player *aPlayer, int acres)
{
    if (acres <= 0)
        return 0;

    /* Can't sell more than 95% of land. */
    int maxSell = static_cast<int>(0.95 * static_cast<float>(aPlayer->land));
    if (acres > maxSell)
        acres = maxSell;
    if (acres <= 0)
        return 0;

    aPlayer->treasury += 2 * acres;
    aPlayer->land -= acres;
    barbarianLand += acres;

    GameLog("  Sold %d acres (+%d)  Treasury: %d  Land: %d\n",
            acres, 2 * acres, aPlayer->treasury, aPlayer->land);
    return acres;
}
