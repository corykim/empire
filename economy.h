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
 * Diplomacy tuning constants.
 */

constexpr float DIPLOMACY_INIT_RANGE       = 0.25f;  /* Initial range: [-0.25, 0.25] */
constexpr float DIPLOMACY_PEACE_BONUS      = 0.05f;  /* Per-turn bonus for not attacking */
constexpr float DIPLOMACY_ATTACKED_ME      = -1.0f;  /* Set diplomacy to this if attacked */
constexpr float DIPLOMACY_DECAY_RATE       = 0.10f;  /* Decay toward zero per turn */
constexpr float DIPLOMACY_THIRD_PARTY_SCALE = 1.0f;  /* Scale for third-party attack effects */
constexpr float DIPLOMACY_WEAKNESS_SCALE   = 1.0f;   /* How much weakness amplifies effects */
constexpr float DIPLOMACY_ENVY_SCALE      = 1.5f;   /* Scale for envy toward powerful targets */
constexpr float DIPLOMACY_RESERVE_SCALE    = 0.5f;   /* Fraction of worst threat to hold in reserve */


/*------------------------------------------------------------------------------
 *
 * Diplomacy functions.
 */

/*
 * Initialize diplomacy scores for all players to random values in
 * [-DIPLOMACY_INIT_RANGE, +DIPLOMACY_INIT_RANGE].  Self-diplomacy is 0.
 */

void InitDiplomacy();


/*
 * Decay diplomacy scores toward zero for all CPU players' views of the
 * given player.  Called at the start of each player's turn.
 *
 *   aPlayer                Player whose turn is starting.
 */

void DecayDiplomacy(Player *aPlayer);


/*
 * Update diplomacy scores after a player finishes their turn.
 * If the player attacked no one, all CPUs increase diplomacy toward them.
 * If the player attacked, diplomacy is adjusted based on who was attacked.
 *
 *   aPlayer                Player whose turn just ended.
 */

void UpdateDiplomacyAfterTurn(Player *aPlayer);


/*
 * Log all diplomacy scores for all CPU players.
 *
 *   label                  Header label for the log section.
 */

void LogAllDiplomacy(const char *label);


/*
 * Log a single player's diplomacy scores toward all other players.
 *
 *   aPlayer                Player whose diplomacy to log.
 */

void LogPlayerDiplomacy(Player *aPlayer);


/*
 * Return the highest soldier count among all living players (min 1).
 */

int MaxSoldiers();


/*
 * Return military weakness of a player: 0.0 = strongest, 1.0 = no soldiers.
 * Uses MaxSoldiers() as the baseline.
 *
 *   soldierCount           Soldier count of the player to evaluate.
 */

float MilitaryWeakness(int soldierCount);


/*
 * Compute the diplomacy-based attack weight for a target.
 *   weight = max(0.01, 1 - diplomacy)
 * If diplomacy is negative, the target's weakness amplifies the weight:
 *   weight *= (1 + weakness * DIPLOMACY_WEAKNESS_SCALE)
 *
 *   diplomacy                Attacker's diplomacy toward the target.
 *   targetSoldierCount     Target's soldier count (for weakness calc).
 */

float DiplomacyAttackWeight(float diplomacy, int targetSoldierCount);


/*
 * Predict how an observer's diplomacy toward the attacker would shift
 * if the attacker attacks the target (a third party, not the observer).
 * Returns the shift amount (positive = observer likes attacker more,
 * negative = observer likes attacker less).
 *
 *   observerIdx            Index of the observing player.
 *   targetIdx              Index of the attack target.
 *   attackerIdx             Index of the attacker.
 */

float PredictThirdPartyShift(int observerIdx, int targetIdx, int attackerIdx);


/*
 * Compute how many soldiers a CPU player should hold in reserve to
 * withstand retaliation.  Uses theory of mind: reads other players'
 * diplomacy scores toward this player to estimate threat.  Each enemy's
 * soldier count is weighted by how negative their diplomacy is.
 *
 *   aPlayer                CPU player.
 */

int ComputeRetaliationReserve(Player *aPlayer);


/*
 * Return a composite power score for a player:
 *   soldiers + land/50 + treasury/500
 * Minimum 1.0 to avoid division by zero.
 *
 *   aPlayer                Player to evaluate.
 */

float ComputePlayerPower(Player *aPlayer);


/*
 * Estimate desired troop strength for a CPU player based on:
 *   - Retaliation reserve (defense against enemies)
 *   - Estimated troops needed for attack phase (offensive ambitions)
 * Sets aPlayer->desiredTroops and returns the value.
 *
 *   aPlayer                CPU player.
 */

int ComputeDesiredTroopStrength(Player *aPlayer);


/*
 * Simulate the diplomatic consequences of an attack and return a net
 * score.  Positive = diplomatically favorable, negative = risky.
 *
 * For each observer, predicts diplomacy shifts using the same rules as
 * UpdateDiplomacyAfterTurn, then estimates retaliation risk from any
 * player whose predicted diplomacy toward us would be negative.
 *
 *   attacker               Attacking CPU player.
 *   targetIdx              Index into playerList of the attack target.
 */

float SimulateAttackOutcome(Player *attacker, int targetIdx);


/*
 * Compute expected total revenue for a player at given tax rates,
 * using expected values for random components.  Does not modify player
 * state.  Used by CPU to optimize tax rates.
 *
 *   aPlayer                Player to evaluate.
 *   salesTax               Sales tax rate (0-20).
 *   incomeTax              Income tax rate (0-35).
 */

int ComputeExpectedRevenue(Player *aPlayer, int salesTax, int incomeTax);


/*
 * Find the sales and income tax rates that maximize expected revenue
 * for a CPU player.  Sets aPlayer->salesTax and aPlayer->incomeTax.
 *
 *   aPlayer                CPU player.
 */

void OptimizeTaxRates(Player *aPlayer);


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
