/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * Centralized economic rules for the Empire game.
 *
 * All economic formulas live here — grain, population, revenue, purchases.
 * Both human UI screens and CPU strategies call these same functions so
 * that no rule is duplicated and the CPU cannot cheat.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#ifndef __ECONOMY_H__
#define __ECONOMY_H__

#include "empire.h"


/*------------------------------------------------------------------------------
 *
 * Investment costs — single source of truth.
 */

constexpr int COST_MARKETPLACE = 1000;
constexpr int COST_GRAIN_MILL  = 2000;
constexpr int COST_FOUNDRY     = 7000;
constexpr int COST_SHIPYARD    = 8000;
constexpr int COST_SOLDIER     = 8;
constexpr int COST_PALACE      = 5000;


/*------------------------------------------------------------------------------
 *
 * Grain phase.
 */

/*
 * Run the grain computation phase: rats, usable land, harvest, grain needs.
 * Sets ratPct, grainHarvest, grain, peopleGrainNeed, armyGrainNeed on the
 * player.  Does NOT handle feeding decisions (those are player-specific).
 *
 *   aPlayer                Player.
 */

void ComputeGrainPhase(Player *aPlayer);


/*------------------------------------------------------------------------------
 *
 * Population phase.
 */

/*
 * Result of population computation.  Returned by ComputePopulation so the
 * caller (human UI or CPU) can display or log the results before applying.
 */

struct PopulationResult
{
    int born;
    int diedDisease;
    int diedMalnutrition;
    int diedStarvation;
    int immigrated;
    int merchantsImmigrated;
    int noblesImmigrated;
    int armyDiedStarvation;
    int armyDeserted;
    int populationGain;
};


/*
 * Compute population changes: births, deaths, immigration, army attrition,
 * army efficiency.  Updates soldierCount (for army starvation/desertion),
 * armyEfficiency, immigrated, and diedStarvation on the player.
 * Does NOT update civilian counts — call ApplyPopulationChanges for that.
 *
 *   aPlayer                Player.
 */

PopulationResult ComputePopulation(Player *aPlayer);


/*
 * Apply the civilian population changes from a PopulationResult.
 * Updates serfCount, merchantCount, nobleCount.
 *
 *   aPlayer                Player.
 *   result                 Population result from ComputePopulation.
 */

void ApplyPopulationChanges(Player *aPlayer, const PopulationResult &result);


/*
 * Death causes, returned by CheckPlayerDeath.
 */

enum DeathCause { DEATH_NONE, DEATH_STARVATION, DEATH_RANDOM };


/*
 * Check if the player died (starvation assassination or random death).
 * Sets aPlayer->dead if so.  No UI — caller handles display.
 * Returns DEATH_NONE if alive, or the cause of death.
 *
 *   aPlayer                Player.
 */

DeathCause CheckPlayerDeath(Player *aPlayer);


/*------------------------------------------------------------------------------
 *
 * Revenue computation.
 */

/*
 * Compute all revenues (tax + investment) and add them to the treasury.
 * Sets all revenue fields on the player.
 *
 *   aPlayer                Player.
 */

void ComputeRevenues(Player *aPlayer);


/*------------------------------------------------------------------------------
 *
 * Purchase functions.
 */

/*
 * Soldier capacity — how many soldiers the player can recruit, and what
 * the limiting factor is.
 */

struct SoldierCap
{
    int maxSoldiers;
    enum Limiter { NOBLES, FOUNDRIES, SERFS, TREASURY } limiter;
};


/*
 * Compute the maximum number of soldiers the player can currently recruit.
 *
 *   aPlayer                Player.
 */

SoldierCap ComputeSoldierCap(Player *aPlayer);


/*
 * Purchase a generic investment.  Validates treasury, clamps desired to
 * affordable amount.  Returns the number actually bought.
 *
 *   aPlayer                Player.
 *   count                  Pointer to the investment count field.
 *   desired                Number the player wants to buy.
 *   cost                   Cost per unit.
 */

int PurchaseInvestment(Player *aPlayer, int *count, int desired, int cost);


/*
 * Recruit soldiers.  Clamps to all constraints (treasury, serfs, foundries,
 * nobles).  Returns the number actually recruited.
 *
 *   aPlayer                Player.
 *   desired                Number the player wants to recruit.
 */

int PurchaseSoldiers(Player *aPlayer, int desired);


/*
 * Apply the marketplace purchase bonus: gain some merchants from serfs.
 * Call once per purchase action (not per marketplace bought).
 *
 *   aPlayer                Player.
 */

void ApplyMarketplaceBonus(Player *aPlayer);


/*
 * Apply the palace purchase bonus: gain some nobles from serfs.
 * Call once per purchase action (not per palace bought).
 *
 *   aPlayer                Player.
 */

void ApplyPalaceBonus(Player *aPlayer);


/*------------------------------------------------------------------------------
 *
 * Grain and land trading.
 */

/*
 * Buy grain from a seller on the market.  Applies the 10% marketplace markup.
 * Returns the amount actually bought (clamped to affordable / available).
 *
 *   buyer                  Buying player.
 *   seller                 Selling player (must have grainForSale > 0).
 *   amount                 Desired bushels.
 */

int TradeGrain(Player *buyer, Player *seller, int amount);


/*
 * Put grain on the market for sale at the given price.
 * Blends with any existing listing.
 *
 *   aPlayer                Selling player.
 *   amount                 Bushels to list.
 *   price                  Price per bushel.
 */

void ListGrainForSale(Player *aPlayer, int amount, float price);


/*
 * Sell land to the barbarians at 2 currency per acre.
 * Clamps to 95% of total land.  Returns acres actually sold.
 *
 *   aPlayer                Player.
 *   acres                  Desired acres to sell.
 */

int SellLandToBarbarians(Player *aPlayer, int acres);


/*------------------------------------------------------------------------------
 *
 * Logging.
 */

/*
 * Write a formatted message to the game log file (empire.log).
 * No-op if logging is disabled.  Accepts printf-style format strings.
 *
 *   fmt                    Format string (printf-style).
 *   ...                    Format arguments.
 */

void GameLog(const char *fmt, ...);


/*
 * Logging enabled flag and file handle.
 */

extern bool logging;


#endif /* __ECONOMY_H__ */
