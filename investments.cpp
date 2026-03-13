/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * TRS-80 Empire game investments screen source file.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 *
 * Includes.
 */

/* System includes. */
#include <math.h>
#include "platform.h"
#include <stdlib.h>
#include <string.h>

/* Local includes. */
#include "empire.h"
#include "economy.h"
#include "ui.h"


/*------------------------------------------------------------------------------
 *
 * Prototypes.
 */

/*
 * Investment prototypes.
 */

static void DrawInvestmentsScreen(Player *aPlayer);



/*
 * Tax prototypes.
 */

static void SetTaxes(Player *aPlayer);

static void SetTaxRate(Player *aPlayer, int *taxPtr,
                       int maxRate, const char *taxName);



/*
 * Buy investment prototypes.
 */

static void BuyInvestments(Player *aPlayer);

static void BuyBuilding(Player *aPlayer, int investmentType);

static void BuySoldiers(Player *aPlayer);

static bool ValidateInvestment(Player *aPlayer,
                               int investment,
                               int investmentCount);


/*------------------------------------------------------------------------------
 *
 * Globals.
 */

/*
 * Investment list.
 */

#define INVESTMENT_MARKETPLACE 1
#define INVESTMENT_GRAIN_MILL  2
#define INVESTMENT_FOUNDRY     3
#define INVESTMENT_SHIPYARD    4
#define INVESTMENT_SOLDIER     5
#define INVESTMENT_PALACE      6


/*
 * Investment cost list — uses constants from economy.h.
 */

static const int investmentCost[] =
{
    COST_MARKETPLACE, COST_GRAIN_MILL, COST_FOUNDRY,
    COST_SHIPYARD, COST_SOLDIER, COST_PALACE,
};


/*------------------------------------------------------------------------------
 *
 * External investments screen functions.
 */

/*
 * Manage investments for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

void InvestmentsScreen(Player *aPlayer)
{
    /* Compute revenues. */
    ComputeRevenues(aPlayer);

    /* Draw the investments screen. */
    DrawInvestmentsScreen(aPlayer);

    /* Set taxes. */
    SetTaxes(aPlayer);

    /* Buy investments. */
    BuyInvestments(aPlayer);
}


/*------------------------------------------------------------------------------
 *
 * Investments functions.
 */

/*
 * Draw the investments screen for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void DrawInvestmentsScreen(Player *aPlayer)
{
    char rulerLine[160];
    snprintf(rulerLine, sizeof(rulerLine), "%s %s of %s",
             aPlayer->title, aPlayer->name, aPlayer->country->name);
    UITitle("Investments", rulerLine);

    /* Tax rates and revenues — compact 2-line format. */
    printw("Customs %2d%%  %7s | Sales %2d%%  %7s | Income %2d%%  %7s\n",
           aPlayer->customsTax, FmtNum(aPlayer->customsTaxRevenue),
           aPlayer->salesTax, FmtNum(aPlayer->salesTaxRevenue),
           aPlayer->incomeTax, FmtNum(aPlayer->incomeTaxRevenue));
    printw("Treasury: %s %s\n",
           FmtNum(aPlayer->treasury), aPlayer->country->currency);
    UISeparator();

    /* Investment table. */
    UIColor(UIC_HEADING);
    printw("                 Count    Revenue     Cost\n");
    UIColorOff();
    printw("1) Marketplaces  %5s    %7s     1,000\n",
           FmtNum(aPlayer->marketplaceCount), FmtNum(aPlayer->marketplaceRevenue));
    printw("2) Grain Mills   %5s    %7s     2,000\n",
           FmtNum(aPlayer->grainMillCount), FmtNum(aPlayer->grainMillRevenue));
    printw("3) Foundries     %5s    %7s     7,000\n",
           FmtNum(aPlayer->foundryCount), FmtNum(aPlayer->foundryRevenue));
    printw("4) Shipyards     %5s    %7s     8,000\n",
           FmtNum(aPlayer->shipyardCount), FmtNum(aPlayer->shipyardRevenue));
    printw("5) Soldiers      %5s    %7s         8\n",
           FmtNum(aPlayer->soldierCount), FmtNum(aPlayer->soldierRevenue));
    printw("6) Palace        %5d%%              5,000\n",
           10 * aPlayer->palaceCount);
    UISeparator();
}


/*
 * Display investments for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */


/*------------------------------------------------------------------------------
 *
 * Tax functions.
 */

/*
 * Set taxes for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

void SetTaxes(Player *aPlayer)
{
    char input[80];
    bool done;

    /* Set taxes. */
    done = false;
    while (!done)
    {
        /* Draw the investments screen. */
        DrawInvestmentsScreen(aPlayer);

        /* Get tax to set. */
        CLEAR_MSG_AREA();
        printw("1) Customs  2) Sales Tax  3) Income Tax  0) Done? ");
        getnstr(input, sizeof(input));

        /* Parse input. */
        switch (ParseNum(input))
        {
            case 0 :
                done = true;
                break;

            case 1 :
                SetTaxRate(aPlayer, &aPlayer->customsTax, 50, "Customs");
                break;

            case 2 :
                SetTaxRate(aPlayer, &aPlayer->salesTax, 20, "Sales");
                break;

            case 3 :
                SetTaxRate(aPlayer, &aPlayer->incomeTax, 35, "Income");
                break;

            default :
                break;
        }
    }
}


/*
 * Set customs tax for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

void SetTaxRate(Player *aPlayer, int *taxPtr,
                int maxRate, const char *taxName)
{
    char input[80];
    int  newRate;
    bool valid = false;

    do
    {
        CLEAR_MSG_AREA();
        printw("%s tax (now %d%%, max %d%%)? ", taxName, *taxPtr, maxRate);
        getnstr(input, sizeof(input));
        if (input[0] == '\0')
        {
            newRate = *taxPtr;  /* Keep current rate on empty ENTER */
            valid = true;
        }
        else
        {
            /* Strip non-numeric except digits and commas. */
            bool isNum = true;
            for (int i = 0; input[i]; i++)
            {
                if ((input[i] < '0' || input[i] > '9') && input[i] != ',')
                    isNum = false;
            }
            if (!isNum)
            {
                CLEAR_MSG_AREA();
                ShowMessage("Enter a tax rate (0-%d)", maxRate);
                continue;
            }
            newRate = ParseNum(input);
            if ((newRate >= 0) && (newRate <= maxRate))
                valid = true;
            else
            {
                CLEAR_MSG_AREA();
                ShowMessage("Tax rate must be between 0 and %d", maxRate);
            }
        }
    } while (!valid);
    *taxPtr = newRate;
    GameLog("  Set %s tax: %d%%\n", taxName, newRate);
}


/*
 * Display tax revenues for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */


/*------------------------------------------------------------------------------
 *
 * Buy investment functions.
 */

/*
 * Buy investments for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

void BuyInvestments(Player *aPlayer)
{
    char input[80];
    bool done;

    /* Buy investments. */
    done = false;
    while (!done)
    {
        /* Draw the investments screen. */
        DrawInvestmentsScreen(aPlayer);

        /* Get investment to buy. */
        CLEAR_MSG_AREA();
        printw("Buy which investment (0 to skip)? ");
        getnstr(input, sizeof(input));

        /* Parse input. */
        switch (ParseNum(input))
        {
            case 0 :
                done = true;
                break;

            case INVESTMENT_MARKETPLACE :
            case INVESTMENT_GRAIN_MILL :
            case INVESTMENT_FOUNDRY :
            case INVESTMENT_SHIPYARD :
            case INVESTMENT_PALACE :
                BuyBuilding(aPlayer, ParseNum(input));
                break;

            case INVESTMENT_SOLDIER :
                BuySoldiers(aPlayer);
                break;

            default :
                break;
        }
    }
}


/*
 * Buy marketplaces for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

/*
 * Lookup tables for building purchases.
 */

static const char *investmentName[] =
{
    "marketplaces", "grain mills", "foundries",
    "shipyards", "soldiers", "palaces",
};

static int *InvestmentCountPtr(Player *p, int type)
{
    switch (type)
    {
        case INVESTMENT_MARKETPLACE: return &p->marketplaceCount;
        case INVESTMENT_GRAIN_MILL:  return &p->grainMillCount;
        case INVESTMENT_FOUNDRY:     return &p->foundryCount;
        case INVESTMENT_SHIPYARD:    return &p->shipyardCount;
        case INVESTMENT_PALACE:      return &p->palaceCount;
        default:                     return nullptr;
    }
}


void BuyBuilding(Player *aPlayer, int investmentType)
{
    char input[80];
    int  count;
    bool valid = false;

    do
    {
        CLEAR_MSG_AREA();
        printw("How many (0 to cancel)? ");
        getnstr(input, sizeof(input));
        if (input[0] == '\0')
            return;
        {
            bool isNum = true;
            for (int i = 0; input[i]; i++)
            {
                if ((input[i] < '0' || input[i] > '9') && input[i] != ',')
                    isNum = false;
            }
            if (!isNum)
            {
                CLEAR_MSG_AREA();
                ShowMessage("Enter a number");
                continue;
            }
        }
        count = ParseNum(input);
        if (count <= 0)
            return;
        valid = ValidateInvestment(aPlayer, investmentType, count);
    } while (!valid);

    int *countPtr = InvestmentCountPtr(aPlayer, investmentType);
    int  cost = investmentCost[investmentType - 1];
    int  bought = PurchaseInvestment(aPlayer, countPtr, count, cost);
    GameLog("  Bought %d %s (-%d)  Treasury: %d\n",
            bought, investmentName[investmentType - 1],
            bought * cost, aPlayer->treasury);

    /* Bonuses for specific investment types. */
    if (investmentType == INVESTMENT_MARKETPLACE)
        ApplyMarketplaceBonus(aPlayer);
    else if (investmentType == INVESTMENT_PALACE)
        ApplyPalaceBonus(aPlayer);
}


/*
 * Buy soldiers for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

void BuySoldiers(Player *aPlayer)
{
    char     input[80];
    int      soldierCount;

    /* Get the number of soldiers to buy. */
    CLEAR_MSG_AREA();
    printw("How many (0 to cancel)? ");
    getnstr(input, sizeof(input));
    {
        bool isNum = false;
        for (int i = 0; input[i]; i++)
        {
            if (input[i] >= '0' && input[i] <= '9') isNum = true;
            else if (input[i] != ',') { isNum = false; break; }
        }
        if (!isNum && input[0] != '\0')
        {
            CLEAR_MSG_AREA();
            ShowMessage("Enter a number of soldiers");
            return;
        }
    }
    soldierCount = ParseNum(input);
    if (soldierCount <= 0)
        return;

    /* Use shared soldier cap computation. */
    SoldierCap cap = ComputeSoldierCap(aPlayer);
    if (soldierCount > cap.maxSoldiers)
    {
        soldierCount = cap.maxSoldiers;
        CLEAR_MSG_AREA();
        switch (cap.limiter)
        {
            case SoldierCap::NOBLES:
                ShowMessage("Limited by nobles (%s lead up to %s troops).\n"
                            "You can recruit %s soldiers.",
                            FmtNum(aPlayer->nobleCount),
                            FmtNum(NOBLE_LEADERSHIP * aPlayer->nobleCount),
                            FmtNum(soldierCount));
                break;
            case SoldierCap::FOUNDRIES:
            {
                /* Determine whether more foundries or more population
                 * would help more.  Equip cap = ratio × people.
                 * If adding 1 foundry raises cap less than adding 100
                 * serfs, population is the real bottleneck. */
                int totalPeople = aPlayer->serfCount
                                  + aPlayer->merchantCount
                                  + aPlayer->nobleCount;
                float ratio = EQUIP_RATIO_BASE
                              + EQUIP_RATIO_PER_FOUNDRY
                                * aPlayer->foundryCount;
                int capFromFoundry = static_cast<int>(
                    EQUIP_RATIO_PER_FOUNDRY * totalPeople);
                int capFromPeople = static_cast<int>(ratio * 100);
                if (capFromPeople > capFromFoundry)
                    ShowMessage("Limited by equip ratio (%d%%). Your population "
                                "of %s is too low —\ngrow your people to "
                                "equip more troops. You can recruit %s.",
                                static_cast<int>(ratio * 100),
                                FmtNum(totalPeople),
                                FmtNum(soldierCount));
                else
                    ShowMessage("Limited by foundries (%s, equip ratio %d%%). "
                                "Build more\nfoundries to equip troops. "
                                "You can recruit %s.",
                                FmtNum(aPlayer->foundryCount),
                                static_cast<int>(ratio * 100),
                                FmtNum(soldierCount));
                break;
            }
            case SoldierCap::SERFS:
                ShowMessage("Limited by serfs (%s available to train).\n"
                            "You can recruit %s soldiers.",
                            FmtNum(aPlayer->serfCount),
                            FmtNum(soldierCount));
                break;
            case SoldierCap::TREASURY:
                ShowMessage("Limited by treasury (%s %s).\n"
                            "You can recruit %s soldiers.",
                            FmtNum(aPlayer->treasury),
                            aPlayer->country->currency,
                            FmtNum(soldierCount));
                break;
        }
    }
    if (soldierCount <= 0)
        return;

    /* Purchase using shared function. */
    PurchaseSoldiers(aPlayer, soldierCount);
}




/*
 *   Return true if the investment and investment count specified by investment
 * and investmentCount are valid for the player specified by aPlayer; return
 * false otherwise.  For example, if the investment cost times times the
 * investment count is greater than the player's treasury, this function returns
 * false.
 *
 *   aPlayer                Player.
 *   investment             Investment being purchased.
 *   investmentCount        Number of investments being purchased.
 */

bool ValidateInvestment(Player *aPlayer, int investment, int investmentCount)
{
    Country *country;
    int      totalCost;
    char     invalidMessage[128];
    bool     valid = true;

    /* Clear the invalid message. */
    invalidMessage[0] = '\0';

    /* Get the player country. */
    country = aPlayer->country;

    /* Validate that the count is a non-negative number. */
    if (investmentCount < 0)
        valid = false;

    /* Validate there's enough in the treasury for the purchase. */
    if (valid)
    {
        totalCost = investmentCount * investmentCost[investment - 1];
        if (totalCost > aPlayer->treasury)
        {
            valid = false;
            snprintf(invalidMessage,
                     sizeof(invalidMessage),
                     "You only have %s %s!",
                     FmtNum(aPlayer->treasury),
                     country->currency);
        }
    }

    /* Validate that there are enough serfs to train for the purchase. */
    if (valid && (investmentCount > aPlayer->serfCount))
    {
        valid = false;
        snprintf(invalidMessage,
                 sizeof(invalidMessage),
                 "You don't have enough serfs to train");
    }

    /*
     * Validate if there are enough foundries and nobles for the new
     * soldiers.
     */
    if (valid && (investment == INVESTMENT_SOLDIER))
    {
        int totalSoldierCount;
        int totalPeopleCount;

        /*
         * Determine the total number of soldiers if the investment is made
         * and the total number of civilians in the country.
         */
        totalSoldierCount = aPlayer->soldierCount + investmentCount;
        totalPeopleCount =   aPlayer->serfCount
                           + aPlayer->merchantCount
                           + aPlayer->nobleCount;

        /*
         * Validate if there are enough foundries and nobles for the new
         * soldiers.
         */
        if ((static_cast<float>(totalSoldierCount) / static_cast<float>(totalPeopleCount)) >
            (EQUIP_RATIO_BASE + EQUIP_RATIO_PER_FOUNDRY*aPlayer->foundryCount))
        {
            valid = false;
            snprintf(invalidMessage,
                     sizeof(invalidMessage),
                     "You cannot equip and maintain so many troops, %s",
                     aPlayer->title);
        }
        else if (totalSoldierCount > (NOBLE_LEADERSHIP * aPlayer->nobleCount))
        {
            valid = false;
            snprintf(invalidMessage,
                     sizeof(invalidMessage),
                     "You only have %s nobles to lead your troops!",
                     FmtNum(aPlayer->nobleCount));
        }
    }

    /* If investment is invalid, notify the player at the call site. */
    if (!valid && (strlen(invalidMessage) > 0))
    {
        CLEAR_MSG_AREA();
        ShowMessage("%s", invalidMessage);
    }

    return valid;
}

