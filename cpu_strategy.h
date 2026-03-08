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


/*------------------------------------------------------------------------------
 *
 * Investment costs (must match investmentCost[] in investments.cpp).
 */

#define COST_MARKETPLACE  1000
#define COST_GRAIN_MILL   2000
#define COST_FOUNDRY      7000
#define COST_SHIPYARD     8000
#define COST_SOLDIER      8
#define COST_PALACE       5000

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

    CPUStrategy(const char *aName, int aBase, int aPerYear)
        : name(aName), attackChanceBase(aBase), attackChancePerYear(aPerYear) {}

    virtual ~CPUStrategy() {}

    /* Pure virtual strategy methods — each derived class must implement. */
    virtual int  selectTarget(Player *aPlayer, int *livingIndices,
                              int livingCount) = 0;
    virtual int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) = 0;
    virtual void manageInvestments(Player *aPlayer) = 0;
    virtual void manageTaxes(Player *aPlayer) = 0;

protected:
    /* Shared helpers available to all derived classes. */
    int  cpuBuy(Player *aPlayer, int *count, int desired, int cost);
    void cpuBuySoldiers(Player *aPlayer, int desired);
    void cpuInvest(Player *aPlayer, int wastePct);
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
    VillageFool() : CPUStrategy("VILLAGE FOOL", 20, 2) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
    void manageTaxes(Player *aPlayer) override;
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
    LandedPeasant() : CPUStrategy("LANDED PEASANT", 35, 2) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
    void manageTaxes(Player *aPlayer) override;
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
    MinorNoble() : CPUStrategy("MINOR NOBLE", 50, 2) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
    void manageTaxes(Player *aPlayer) override;
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
    RoyalAdvisor() : CPUStrategy("ROYAL ADVISOR", 65, 2) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
    void manageTaxes(Player *aPlayer) override;
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
    Machiavelli() : CPUStrategy("MACHIAVELLI", 80, 2) {}

    int  selectTarget(Player *aPlayer, int *livingIndices,
                      int livingCount) override;
    int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) override;
    void manageInvestments(Player *aPlayer) override;
    void manageTaxes(Player *aPlayer) override;
};


#endif /* __CPU_STRATEGY_H__ */
