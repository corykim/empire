/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * CPU strategy class hierarchy.
 *
 * Each difficulty level is a concrete class derived from the abstract
 * CPUStrategy base class.  The base class provides shared helper methods
 * for purchasing and soldier recruitment.  Each derived class implements
 * the four pure virtual strategy methods.
 *
 *   CPUStrategy (abstract)
 *   ├── VillageFool
 *   ├── LandedPeasant
 *   ├── MinorNoble
 *   ├── RoyalAdvisor
 *   └── Machiavelli
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#ifndef __CPU_STRATEGY_H__
#define __CPU_STRATEGY_H__

#include "empire.h"
#include "economy.h"

#define TARGET_NONE       -1
#define TARGET_BARBARIANS -2


/*------------------------------------------------------------------------------
 *
 * Abstract base class.
 */

class CPUStrategy
{
public:
    const char *name;
    int         attackChanceBase;
    int         attackChancePerYear;
    int         errorPct;       /* Deviation from optimal: 0=perfect, 50=random */

    CPUStrategy(const char *aName, int aBase, int aPerYear, int aError)
        : name(aName), attackChanceBase(aBase), attackChancePerYear(aPerYear),
          errorPct(aError) {}

    virtual ~CPUStrategy() {}

    /* Pure virtual strategy methods — each derived class must implement. */
    virtual int  selectTarget(Player *aPlayer, int *livingIndices,
                              int livingCount) = 0;
    virtual int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) = 0;
    virtual void manageInvestments(Player *aPlayer) = 0;
    virtual void manageTaxes(Player *aPlayer);

    /* Grain/land trading — default uses errorPct, overridable. */
    virtual void manageGrainTrade(Player *aPlayer);

    /*
     * Unified attack decision and target selection, driven by diplomacy.
     *
     * For each eligible candidate, computes a weight from diplomacy:
     *   weight = max(0.01, 1 - diplomacy)
     * The strongest weight scales the strategy's base aggression to
     * determine the overall probability of attacking at all:
     *   effectiveChance = aggression * maxWeight (capped at 95%)
     * If the roll says attack, a weighted random pick selects the target.
     *
     * Returns a playerList index, TARGET_BARBARIANS, or TARGET_NONE.
     *
     *   aPlayer          Attacking CPU player.
     *   livingIndices    Array of living player indices.
     *   livingCount      Number of living players.
     *   requireOdds      Minimum soldier ratio required (e.g. 1.5 = 3:2 odds).
     *                    Set to 0 to skip the soldier check.
     */
    int selectTargetByDiplomacy(Player *aPlayer, int *livingIndices,
                              int livingCount, float requireOdds);

    /* Grain pricing — public so CPUGrainPhase can use it for post-feed listing. */
    float ComputeGrainTargetPrice(Player *aPlayer, int effError);

protected:
    /* Shared helpers available to all derived classes. */
    int   deviate(int optimal, int maxVal);          /* Uses this->errorPct */
    int   deviate(int optimal, int maxVal, int err); /* Uses explicit error */
    void  cpuInvest(Player *aPlayer);
};


/*------------------------------------------------------------------------------
 *
 * Class 1: Village Fool
 *
 * Acts randomly in all things.  50% investment waste.
 */

class VillageFool : public CPUStrategy
{
public:
    VillageFool() : CPUStrategy("VILLAGE FOOL", 20, 2, 50) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
};


/*------------------------------------------------------------------------------
 *
 * Class 2: Landed Peasant
 *
 * Picks the weaker of two random targets.  35% investment waste.
 */

class LandedPeasant : public CPUStrategy
{
public:
    LandedPeasant() : CPUStrategy("LANDED PEASANT", 35, 2, 35) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
};


/*------------------------------------------------------------------------------
 *
 * Class 3: Minor Noble
 *
 * Only attacks weaker players.  20% investment waste.
 */

class MinorNoble : public CPUStrategy
{
public:
    MinorNoble() : CPUStrategy("MINOR NOBLE", 50, 2, 20) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
};


/*------------------------------------------------------------------------------
 *
 * Class 4: Royal Advisor
 *
 * Targets weakest with most land, needs 1.5:1 odds.  10% investment waste.
 */

class RoyalAdvisor : public CPUStrategy
{
public:
    RoyalAdvisor() : CPUStrategy("ROYAL ADVISOR", 65, 2, 10) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
};


/*------------------------------------------------------------------------------
 *
 * Class 5: Machiavelli
 *
 * Hunts the human leader, exploits wounded players.  0% investment waste.
 */

class Machiavelli : public CPUStrategy
{
public:
    Machiavelli() : CPUStrategy("MACHIAVELLI", 80, 2, 5) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
    void manageGrainTrade(Player *aPlayer) override;
};


#endif /* __CPU_STRATEGY_H__ */
