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
 * Game-wide constants — single source of truth.
 *
 * All tunable numbers live here.  Grouped by system.
 */

/* Core game. */
constexpr int DELAY_TIME          = 2000000;  /* Base message delay (microseconds) */
constexpr int SERF_EFFICIENCY     = 5;        /* Serf fighting efficiency (10 = 100%) */
constexpr int RANDOM_DEATH_CHANCE = 400;      /* 1 in N chance of random death per year */

/* CPU difficulty scaling.  errorPct and attackChanceBase are interpolated
 * from continuous difficulty (0.0–4.0) rather than discrete levels.
 *   errorPct: 50 at diff=0, 5 at diff=4 → 50 - 11.25*diff
 *   attackChanceBase: 20 at diff=0, 80 at diff=4 → 20 + 15*diff
 */
inline int ComputeErrorPct(float diff)
{
    int e = static_cast<int>(50.0f - 11.25f * diff);
    if (e < 3) e = 3;
    if (e > 55) e = 55;
    return e;
}

inline int ComputeAttackChanceBase(float diff)
{
    int a = static_cast<int>(20.0f + 15.0f * diff);
    if (a < 15) a = 15;
    if (a > 85) a = 85;
    return a;
}

/* Starting values. */
constexpr int STARTING_LAND       = 10000;
constexpr int STARTING_GRAIN_BASE = 15000;
constexpr int STARTING_GRAIN_RAND = 10000;
constexpr int STARTING_TREASURY   = 1000;
constexpr int STARTING_SERFS      = 2000;
constexpr int STARTING_SOLDIERS   = 20;
constexpr int STARTING_NOBLES     = 1;
constexpr int STARTING_MERCHANTS  = 25;

/* Investment costs. */
constexpr int COST_MARKETPLACE = 1000;
constexpr int COST_GRAIN_MILL  = 2000;
constexpr int COST_FOUNDRY     = 7000;
constexpr int COST_SHIPYARD    = 8000;
constexpr int COST_SOLDIER     = 8;
constexpr int COST_PALACE      = 5000;

/* Grain and farming. */
constexpr float GRAIN_YIELD_MULT    = 0.72f;  /* Base harvest = weather * land * this */
constexpr float MILL_YIELD_BONUS    = 0.08f;  /* Yield bonus per sqrt(grain mill count) */
constexpr int   GRAIN_SEED_PER_ACRE = 3;      /* Acres that can be seeded per bushel */
constexpr int   MILL_SEED_BONUS     = 1;      /* Extra seed acres per sqrt(grain mill count) */
constexpr int   ACRES_PER_SERF      = 5;      /* Max acres one serf can farm */
constexpr int   GRAIN_PER_PERSON    = 5;      /* Bushels per civilian per year */
constexpr int   GRAIN_PER_NOBLE_MULT = 3;     /* Nobles need this × GRAIN_PER_PERSON */
constexpr int   GRAIN_PER_SOLDIER   = 8;      /* Bushels per soldier per year */
constexpr int   FOUNDRY_POLLUTION   = 500;    /* Harvest penalty per foundry */
constexpr int   MAX_RAT_PERCENT     = 30;     /* Max % of grain eaten by rats */

/* Trading. */
constexpr float GRAIN_MARKUP        = 0.10f;  /* Marketplace markup on grain purchases */
constexpr float GRAIN_WITHDRAW_FEE  = 0.15f;  /* Penalty to withdraw grain from market */
constexpr float GRAIN_PRICE_BASE    = 0.50f;  /* CPU base grain price per bushel */
constexpr float GRAIN_PRICE_MAX     = 5.0f;   /* Max price humans can list grain at */
constexpr float LAND_SELL_PRICE     = 5.0f;   /* Currency per acre when selling to barbarians */
constexpr float LAND_MAX_SELL_PCT   = 0.95f;  /* Max fraction of land that can be sold */

/* Population. */
constexpr float BIRTH_RATE_DIV              = 9.5f;  /* Pop divisor for births */
constexpr int   DISEASE_DEATH_DIV           = 22;    /* Pop divisor for disease */
constexpr int   MALNUTRITION_DEATH_DIV      = 15;    /* Moderate underfeed */
constexpr int   MALNUTRITION_DEATH_DIV_SEVERE = 12;  /* Severe underfeed */
constexpr int   STARVATION_DEATH_DIV        = 16;    /* Severe underfeed */
constexpr float IMMIGRATION_FEED_MULT       = 1.5f;  /* Feed above this × need for immigration */
constexpr float IMMIGRATION_CUSTOMS_MULT    = 1.5f;  /* Customs tax penalty on immigration */
constexpr int   IMMIGRANT_MERCHANT_RATIO    = 5;     /* 1 in N immigrants is a merchant */
constexpr int   IMMIGRANT_NOBLE_RATIO       = 25;    /* 1 in N immigrants is a noble */

/* Military. */
constexpr float EQUIP_RATIO_BASE       = 0.05f;   /* Base soldier-to-civilian ratio */
constexpr float EQUIP_RATIO_PER_FOUNDRY = 0.015f; /* Additional ratio per foundry */
constexpr int   NOBLE_LEADERSHIP        = 20;      /* Soldiers per noble */
constexpr int   MAX_ATTACK_CHANCE       = 95;      /* Cap on CPU attack probability */
constexpr int   ATTACKS_PER_NOBLE_DIV   = 4;       /* Max attacks = nobles / this + 1 */

/* Revenue exponents (diminishing returns). */
constexpr float REV_EXP_INVESTMENT = 0.9f;   /* Marketplace, grain mill, foundry, shipyard */
constexpr float REV_EXP_SALES_TAX = 0.85f;   /* Sales tax base */
constexpr float REV_EXP_INCOME_TAX = 0.97f;  /* Income tax */

/* Revenue multipliers — marketplace. */
constexpr int   MKT_REV_MULT      = 12;      /* Merchant contribution */
constexpr int   MKT_REV_ADD       = 5;       /* Base per marketplace */
constexpr int   MKT_REV_RAND      = 35;      /* Random merchant range (×2) */

/* Revenue multipliers — grain mill.
 * Mill revenue benefits from sales tax (processed grain sells at premium)
 * and is hurt only by income tax.  This creates a distinct tax optimization:
 * mill-heavy players prefer high sales / low income tax, while marketplace
 * players prefer low sales / high income tax. */
constexpr float MILL_REV_MULT     = 5.8f;    /* Harvest contribution */
constexpr int   MILL_REV_RAND     = 250;     /* Random harvest range */
constexpr int   MILL_DIV_INCOME   = 20;      /* Income tax divisor component */
constexpr int   MILL_DIV_BASE     = 150;     /* Base divisor component */
constexpr float MILL_SALES_BONUS  = 0.08f;   /* +8% mill revenue per sales tax point */

/* Revenue multipliers — foundry. */
constexpr int   FOUNDRY_REV_RAND  = 150;     /* Random range */
constexpr int   FOUNDRY_REV_BASE  = 400;     /* Base per foundry */

/* Revenue multipliers — shipyard. */
constexpr int   SHIP_MULT_MERCHANT    = 4;   /* Per merchant */
constexpr int   SHIP_MULT_MARKETPLACE = 9;   /* Per marketplace */
constexpr int   SHIP_MULT_FOUNDRY     = 15;  /* Per foundry */

/* Revenue multipliers — sales tax base. */
constexpr float SALESTAX_MERCHANT_MULT   = 1.8f;
constexpr int   SALESTAX_MKT_MULT        = 33;
constexpr int   SALESTAX_MILL_MULT       = 17;
constexpr int   SALESTAX_FOUNDRY_MULT    = 50;
constexpr int   SALESTAX_SHIPYARD_MULT   = 70;
constexpr int   SALESTAX_NOBLE_MULT      = 5;

/* Revenue multipliers — income tax base. */
constexpr float INCOMETAX_SERF_MULT      = 1.3f;
constexpr int   INCOMETAX_NOBLE_MULT     = 145;
constexpr int   INCOMETAX_MERCHANT_MULT  = 39;
constexpr int   INCOMETAX_MKT_MULT       = 99;
constexpr int   INCOMETAX_MILL_MULT      = 99;
constexpr int   INCOMETAX_FOUNDRY_MULT   = 425;
constexpr int   INCOMETAX_SHIPYARD_MULT  = 965;

/* Combat. */
constexpr float BATTLE_DELAY_MS       = 37.5f;  /* Base delay per round (× sqrt(force)) */
constexpr int   CASUALTY_DIV_BASE     = 15;     /* Kill rate: small armies */
constexpr int   CASUALTY_THRESHOLD_MID = 200;   /* Threshold for medium armies */
constexpr int   CASUALTY_DIV_MID      = 8;      /* Kill rate: medium armies */
constexpr int   CASUALTY_THRESHOLD_HIGH = 1000; /* Threshold for large armies */
constexpr int   CASUALTY_DIV_HIGH     = 5;      /* Kill rate: large armies */
constexpr int   SACK_THRESHOLD_DIV    = 3;      /* Sack if capture > 1/N of land */

/* CPU behavior tuning (shared across files). */
constexpr int   CPU_OVERFEED_PCT         = 190;    /* CPU target people overfeed percentage */
constexpr int   CPU_MIN_ATTACK_FORCE     = 10;     /* Minimum soldiers to send on attack */

/* Diplomacy. */
constexpr float DIPLOMACY_INIT_RANGE       = 0.05f;
constexpr float DIPLOMACY_PEACE_BONUS      = 0.03f;
constexpr float DIPLOMACY_ATTACKED_ME      = -1.0f;
constexpr float DIPLOMACY_DECAY_RATE       = 0.10f;
constexpr float DIPLOMACY_THIRD_PARTY_SCALE = 1.0f;
constexpr float DIPLOMACY_WEAKNESS_SCALE   = 1.0f;
constexpr float DIPLOMACY_ENVY_SCALE      = 1.5f;
constexpr float DIPLOMACY_RESERVE_SCALE    = 0.5f;
constexpr float DIPLOMACY_CLAMP            = 2.0f;  /* Max absolute diplomacy value */

inline float ClampDiplomacy(float d)
{
    if (d > DIPLOMACY_CLAMP) return DIPLOMACY_CLAMP;
    if (d < -DIPLOMACY_CLAMP) return -DIPLOMACY_CLAMP;
    return d;
}


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


/*
 * Compute the total soldiers of players allied against a target.
 * "Allied" means: has negative diplomacy toward the target AND
 * positive diplomacy toward the attacker.
 *
 *   attacker               The attacking player.
 *   targetIdx              Index of the target player.
 */

int ComputeAlliedStrength(Player *attacker, int targetIdx);


/*
 * Compute a vulnerability score for a target (0.0 = normal, 1.0+ = very
 * vulnerable).  Based on recent soldier losses and low soldier count
 * relative to land/power.
 *
 *   targetIdx              Index of the target player.
 */

float ComputeVulnerability(int targetIdx);


/*
 * Compute the effective grain yield multiplier for a player, including
 * the base yield plus a diminishing bonus per grain mill (sqrt scaling).
 *
 *   aPlayer                Player to evaluate.
 */

float EffectiveYieldMult(Player *aPlayer);


/*
 * Compute the effective seed rate for a player (acres per bushel),
 * including a diminishing bonus per grain mill (sqrt scaling).
 *
 *   aPlayer                Player to evaluate.
 */

int EffectiveSeedRate(Player *aPlayer);


/*
 * Compute the worst-case harvest for a player: weather=1, no seed/serf
 * limits, just raw land minus occupied space times effective yield.
 * Returns 0 if the result would be negative.
 *
 *   aPlayer                Player to evaluate.
 */

int ComputeWorstCaseHarvest(Player *aPlayer);


/*
 * Compute a safe grain reserve: enough to survive a worst-case year
 * (30% rats + weather=1).  Ensures the reserve is at least totalNeed.
 *
 *   aPlayer                Player to evaluate.
 */

int ComputeSafeGrainReserve(Player *aPlayer);


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
 * Withdraw grain from the market back to reserves.  Applies
 * GRAIN_WITHDRAW_FEE penalty (grain is lost to spoilage).
 * Returns the amount actually returned to reserves.
 *
 *   aPlayer                Player withdrawing grain.
 *   amount                 Bushels to withdraw (0 = all).
 */

int WithdrawGrain(Player *aPlayer, int amount);


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
