/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * TRS-80 Empire game header file.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#ifndef __EMPIRE_H__
#define __EMPIRE_H__

/*------------------------------------------------------------------------------
 *
 * Defs.
 */

/*
 * Game defs.
 *
 *   COUNTRY_COUNT          Count of the number of countries.
 *   TITLE_COUNT            Count of the number of ruler titles.
 *   DIFFICULTY_COUNT       Count of the number of difficulty levels.
 *   DELAY_TIME             Time in microseconds to delay.
 *   SERF_EFFICIENCY        Serf fighting efficiency (10 = 100%).
 */

constexpr int COUNTRY_COUNT       = 6;
constexpr int TITLE_COUNT         = 4;
constexpr int DIFFICULTY_COUNT    = 5;

/* DELAY_TIME and SERF_EFFICIENCY are defined in economy.h with all other
 * game-wide constants.  COUNTRY_COUNT, TITLE_COUNT, and DIFFICULTY_COUNT
 * remain here because they're used in struct/array definitions below. */


/*------------------------------------------------------------------------------
 *
 * Structure defs.
 */

/*
 * This structure contains fields for a country record.
 *
 *   name                   Country name.
 *   rulerName              Default ruler name.
 *   currency               Currency.
 *   titleList              List of ruler titles.
 */

struct Country
{
    char                    name[80];
    char                    rulerName[80];
    char                    currency[80];
    char                    titleList[TITLE_COUNT][80];
};


/*
 * This structure contains fields for a player record.
 *
 *   name                   Player name.
 *   number                 Player number.
 *   country                Player country.
 *   level                  Player level.
 *   title                  Player title.
 *   human                  If true, player is human.
 *   dead                   If true, player is dead.
 *   land                   Land in acres.
 *   grain                  Grain reserves in bushels.
 *   treasury               Currency treasury.
 *   serfCount              Count of number of serfs.
 *   soldierCount           Count of number of soldiers.
 *   soldierRevenue         Revenue from soldiers.
 *   nobleCount             Count of number of nobles.
 *   merchantCount          Count of number of merchants.
 *   immigrated             Number of people who immigrated into country.
 *   armyEfficiency         Efficiency of army.
 *   customsTax             Customs tax.
 *   customsTaxRevenue      Revenue from customs tax.
 *   salesTax               Sales tax.
 *   salesTaxRevenue        Revenue from sales tax.
 *   incomeTax              Income tax.
 *   incomeTaxRevenue       Revenue from income tax.
 *   marketplaceCount       Count of number of marketplaces.
 *   marketplaceRevenue     Revenue from marketplaces.
 *   grainMillCount         Count of number of grain mills.
 *   grainMillRevenue       Revenue from grain mills.
 *   foundryCount           Count of number of foundries.
 *   foundryRevenue         Revenue from foundries.
 *   shipyardCount          Count of number of shipyards.
 *   shipyardRevenue        Revenue from shipyards.
 *   palaceCount            Count of work done on palace.
 *   grainForSale           Bushels of grain for sale.
 *   grainPrice             Grain price.
 *   ratPct                 Percent of grain eaten by rats.
 *   grainHarvest           Grain harvest for year.
 *   peopleGrainNeed        How much grain people need for year.
 *   peopleGrainFeed        How much grain to feed people for year.
 *   armyGrainNeed          How much grain army needs for year.
 *   armyGrainFeed          How much grain to feed army for year.
 *   diedStarvation         How many people died of starvation.
 *   attackCount            Count of the number of attacks this year.
 *   attackedTargets        Bitmask of players attacked this turn (by index).
 *   diplomacy                Diplomacy scores toward each other player (-1..1+).
 */

struct Player
{
    char                    name[80];
    int                     number;
    Country                *country;
    int                     level;
    char                    title[80];
    bool                    human;
    bool                    dead;
    int                     land;
    int                     grain;
    int                     treasury;
    int                     serfCount;
    int                     soldierCount;
    int                     soldierRevenue;
    int                     nobleCount;
    int                     merchantCount;
    int                     immigrated;
    int                     armyEfficiency;
    int                     customsTax;
    int                     customsTaxRevenue;
    int                     salesTax;
    int                     salesTaxRevenue;
    int                     incomeTax;
    int                     incomeTaxRevenue;
    int                     marketplaceCount;
    int                     marketplaceRevenue;
    int                     grainMillCount;
    int                     grainMillRevenue;
    int                     foundryCount;
    int                     foundryRevenue;
    int                     shipyardCount;
    int                     shipyardRevenue;
    int                     palaceCount;
    int                     grainForSale;
    float                   grainPrice;
    int                     ratPct;
    int                     grainHarvest;
    int                     peopleGrainNeed;
    int                     peopleGrainFeed;
    int                     armyGrainNeed;
    int                     armyGrainFeed;
    int                     diedStarvation;
    int                     attackCount;
    int                     attackedTargets;
    int                     landTakenFrom[COUNTRY_COUNT];
    int                     desiredTroops;
    int                     soldiersPrevTurn;
    int                     yearsSinceImmigration;
    float                   cpuDifficulty;
    int                     strategyIndex;
    float                   diplomacy[COUNTRY_COUNT];
    /* Opening capital allocation (set once on turn 1, sums to 100). */
    int                     openMarketPct;
    int                     openMillPct;
    int                     openMilitaryPct;
};


/*
 * This structure contains fields used to managed a battle.
 *
 *   player                 Player initiating the battle.
 *   soldiersToAttackCount  Number of player soldiers to attack.
 *   soldierCount           Remaining number of player soldiers.
 *   soldierEfficiency      Efficiency of player soldiers.
 *   soldierLabel           Label to use to show remaining player soldiers.
 *   targetPlayer           Target players being attacked.
 *   targetSoldierCount     Number of target soldiers being attacked.
 *   targetSoldierEfficiency
 *                          Efficiency of target soldiers.
 *   targetSoldierLabel     Label to use to show remaining target soldiers.
 *   targetLand             Acres of land of target.
 *   targetSerfs            If true, target is defending with serfs.
 *   targetDefeated         If true, target has been defeated.
 *   targetOverrun          If true, target has been overrun.
 */

struct Battle
{
    Player                 *player;
    int                     soldiersToAttackCount;
    int                     soldierCount;
    int                     soldierEfficiency;
    char                    soldierLabel[80];
    Player                 *targetPlayer;
    int                     targetSoldierCount;
    int                     targetSoldierEfficiency;
    char                    targetSoldierLabel[80];
    int                     targetLand;
    bool                    targetSerfs;
    int                     landCaptured;
    bool                    targetDefeated;
    bool                    targetOverrun;
};


/*
 * Forward declaration of the CPU strategy base class.  See cpu_strategy.h
 * for the full class hierarchy.
 */

class CPUStrategy;

extern CPUStrategy *cpuStrategies[DIFFICULTY_COUNT];


/*------------------------------------------------------------------------------
 *
 * Globals.
 */

/*
 * Country  list.
 */

extern Country countryList[COUNTRY_COUNT];


/*
 * Game state variables.
 *
 *   playerList             List of players.
 *   playerCount            Count of the number of human players.
 *   year                   Current year.
 *   weather                Weather for year, 1 based.
 *   barbarianLand          Amount of barbarian land in acres.
 *   done                   If true, game is done.
 */

extern Player playerList[COUNTRY_COUNT];
extern int playerCount;
extern int year;
extern int weather;
extern int barbarianLand;
extern int done;
extern int difficulty;
extern int treatyYears;
extern int marketGrainHistory[3];  /* Rolling 3-year market grain inventory */
extern bool omniscient;
extern bool fastMode;
extern bool logging;
extern const char *logFileName;


/*------------------------------------------------------------------------------
 *
 * Prototypes.
 */

int RandRange(int range);

void InvestmentsScreen(Player *aPlayer);

void AttackScreen(Player *aPlayer);

void CPUAttackScreen(Player *aPlayer);

void ShowMessage(const char *fmt, ...);

const char *FmtNum(int value);

int ParseNum(const char *str);


/*------------------------------------------------------------------------------
 *
 * Macros.
 */

/*
 * Return the maximum of the two values specified by a and b.
 *
 *   a, b                   Values for which to return the maximum.
 */

#define MAX(a, b) (((a) > (b)) ? (a) : (b))


/*
 * Return the minimum of the two values specified by a and b.
 *
 *   a, b                   Values for which to return the minimum.
 */

#define MIN(a, b) (((a) < (b)) ? (a) : (b))


/*
 * Return the length of the array specified by aArray.
 *
 *   aArray                 Array to get length.
 */

#define ArraySize(aArray) (sizeof((aArray)) / sizeof((aArray)[0]))


/*
 * Clear the two-line message area at the bottom of the screen and position
 * the cursor at the start of the prompt row.  Uses winrows for dynamic sizing.
 */

#define CLEAR_MSG_AREA() do { flushinp(); move(winrows-2, 0); clrtoeol(); move(winrows-1, 0); clrtoeol(); move(winrows-2, 0); } while (0)


#endif /* __EMPIRE_H__ */

