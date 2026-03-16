/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * Diplomacy system for the Empire game.
 *
 * All diplomacy formulas, scoring, and strategic assessment live here.
 * CPU strategy code and the main game loop both call these functions.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#ifndef __DIPLOMACY_H__
#define __DIPLOMACY_H__

#include "empire.h"


/*------------------------------------------------------------------------------
 *
 * Diplomacy constants — single source of truth.
 */

constexpr float DIPLOMACY_INIT_RANGE       = 0.05f;
constexpr float DIPLOMACY_PEACE_BONUS      = 0.01f;
constexpr float DIPLOMACY_ATTACKED_ME      = -1.0f;
constexpr float DIPLOMACY_DECAY_RATE       = 0.10f;
constexpr float DIPLOMACY_THIRD_PARTY_SCALE = 1.0f;
constexpr float DIPLOMACY_WEAKNESS_SCALE   = 1.0f;
constexpr float DIPLOMACY_ENVY_SCALE      = 1.5f;
constexpr float DIPLOMACY_RESERVE_SCALE    = 0.5f;
constexpr float DIPLOMACY_CLAMP            = 2.0f;  /* Max absolute diplomacy value */

/* Anti-leader and power-based diplomacy tuning. */
constexpr float CPU_LEADER_POWER_MULT    = 1.5f;   /* Power ratio to trigger anti-leader mode */
constexpr float CPU_LEADER_ATTACK_BOOST  = 2.0f;   /* Attack weight multiplier for leader target */
constexpr float CPU_LEADER_GUNS_BOOST    = 20.0f;  /* Extra guns% when leader detected */
constexpr float CPU_TURTLE_POWER_RATIO   = 3.0f;   /* Don't attack targets with this power ratio */
constexpr int   CPU_GUNS_BASE_PCT        = 35;      /* Base guns% at neutral diplomacy */
constexpr float CPU_GUNS_DIPLOMACY_SCALE = 25.0f;  /* Guns% swing per unit of avg diplomacy */
constexpr float CPU_GUNS_ENVY_SCALE      = 15.0f;  /* Guns% increase per unit of power envy */

inline float ClampDiplomacy(float d)
{
    if (d > DIPLOMACY_CLAMP) return DIPLOMACY_CLAMP;
    if (d < -DIPLOMACY_CLAMP) return -DIPLOMACY_CLAMP;
    return d;
}


/*------------------------------------------------------------------------------
 *
 * Diplomacy lifecycle.
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
 * Called when a player dies.  Erodes positive diplomacy between surviving
 * players that was built from shared hostility toward the dead player.
 * "Our shared enemy is gone — what do we have in common now?"
 *
 *   deadIdx                Index of the player who just died.
 */

void OnPlayerDeath(int deadIdx);


/*
 * Update diplomacy scores after a player finishes their turn.
 * If the player attacked no one, all CPUs increase diplomacy toward them.
 * If the player attacked, diplomacy is adjusted based on who was attacked.
 *
 *   aPlayer                Player whose turn just ended.
 */

void UpdateDiplomacyAfterTurn(Player *aPlayer);


/*------------------------------------------------------------------------------
 *
 * Diplomacy logging.
 */

/*
 * Log the full diplomacy matrix as a table.
 *
 *   label                  Column header for the first column.
 */

void LogAllDiplomacy(const char *label);


/*
 * Log a single player's diplomacy scores toward all other players.
 *
 *   aPlayer                Player whose diplomacy to log.
 */

void LogPlayerDiplomacy(Player *aPlayer);


/*------------------------------------------------------------------------------
 *
 * Military and power assessment.
 */

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
 * Weakness amplification scales continuously with hostility.
 *
 *   diplomacy              Attacker's diplomacy toward the target.
 *   targetSoldierCount     Target's soldier count (for weakness calc).
 */

float DiplomacyAttackWeight(float diplomacy, int targetSoldierCount);


/*
 * Predict how an observer's diplomacy toward the attacker would shift
 * if the attacker attacks the target (a third party, not the observer).
 *
 *   observerIdx            Index of the observing player.
 *   targetIdx              Index of the attack target.
 *   attackerIdx            Index of the attacker.
 */

float PredictThirdPartyShift(int observerIdx, int targetIdx, int attackerIdx);


/*
 * Compute how many soldiers a CPU player should hold in reserve to
 * withstand retaliation.
 *
 *   aPlayer                CPU player.
 */

int ComputeRetaliationReserve(Player *aPlayer);


/*
 * Return a composite power score for a player.  Revenue is the dominant
 * term (revenue/50), followed by soldiers, land, and infrastructure.
 *
 *   aPlayer                Player to evaluate.
 */

float ComputePlayerPower(Player *aPlayer);


/*
 * Estimate desired troop strength for a CPU player.
 * Sets aPlayer->desiredTroops and returns the value.
 *
 *   aPlayer                CPU player.
 */

int ComputeDesiredTroopStrength(Player *aPlayer);


/*
 * Simulate the diplomatic consequences of an attack and return a net
 * score.  Positive = diplomatically favorable, negative = risky.
 *
 *   attacker               Attacking CPU player.
 *   targetIdx              Index into playerList of the attack target.
 */

float SimulateAttackOutcome(Player *attacker, int targetIdx);


/*
 * Compute the total soldiers of players allied against a target.
 *
 *   attacker               The attacking player.
 *   targetIdx              Index of the target player.
 */

int ComputeAlliedStrength(Player *attacker, int targetIdx);


/*
 * Compute a vulnerability score for a target (0.0 = normal, 1.0+ = very
 * vulnerable).
 *
 *   targetIdx              Index of the target player.
 */

float ComputeVulnerability(int targetIdx);


/*
 * Compute average power score of surviving players, excluding the leader.
 */

float ComputeAverageSurvivorPower();


/*
 * Find the index of the player with the highest power score.
 * Returns -1 if no living players.
 */

int FindLeaderIdx();


/*
 * Compute average soldier count across all living players.
 */

float ComputeAverageSoldierCount();


#endif /* __DIPLOMACY_H__ */
