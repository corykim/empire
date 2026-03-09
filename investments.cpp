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

static void DisplayInvestments(Player *aPlayer);


/*
 * Tax prototypes.
 */

static void SetTaxes(Player *aPlayer);

static void SetCustomsTax(Player *aPlayer);

static void SetSalesTax(Player *aPlayer);

static void SetIncomeTax(Player *aPlayer);

static void DisplayTaxRevenues(Player *aPlayer);


/*
 * Buy investment prototypes.
 */

static void BuyInvestments(Player *aPlayer);

static void BuyMarketplaces(Player *aPlayer);

static void BuyGrainMills(Player *aPlayer);

static void BuyFoundries(Player *aPlayer);

static void BuyShipyards(Player *aPlayer);

static void BuySoldiers(Player *aPlayer);

static void BuyPalaces(Player *aPlayer);

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

void DisplayInvestments(Player *aPlayer)
{
    /* Display investments. */
    UISeparator();
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
                SetCustomsTax(aPlayer);
                break;

            case 2 :
                SetSalesTax(aPlayer);
                break;

            case 3 :
                SetIncomeTax(aPlayer);
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

void SetCustomsTax(Player *aPlayer)
{
    char input[80];
    int  customsTax;
    bool validCustomsTax;

    /* Get the new customs tax. */
    validCustomsTax = false;
    do
    {
        CLEAR_MSG_AREA();
        printw("Customs tax (now %d%%, max 50%%)? ", aPlayer->customsTax);
        getnstr(input, sizeof(input));
        customsTax = ParseNum(input);
        if ((customsTax >= 0) && (customsTax <= 50))
            validCustomsTax = true;
    } while (!validCustomsTax);
    aPlayer->customsTax = customsTax;
    GameLog("  Set customs tax: %d%%\n", customsTax);
}


/*
 * Set sales tax for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

void SetSalesTax(Player *aPlayer)
{
    char input[80];
    int  salesTax;
    bool validSalesTax;

    /* Get the new sales tax. */
    validSalesTax = false;
    do
    {
        CLEAR_MSG_AREA();
        printw("Sales tax (now %d%%, max 20%%)? ", aPlayer->salesTax);
        getnstr(input, sizeof(input));
        salesTax = ParseNum(input);
        if ((salesTax >= 0) && (salesTax <= 20))
            validSalesTax = true;
    } while (!validSalesTax);
    aPlayer->salesTax = salesTax;
    GameLog("  Set sales tax: %d%%\n", salesTax);
}


/*
 * Set income tax for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

void SetIncomeTax(Player *aPlayer)
{
    char input[80];
    int  incomeTax;
    bool validIncomeTax;

    /* Get the new income tax. */
    validIncomeTax = false;
    do
    {
        CLEAR_MSG_AREA();
        printw("Income tax (now %d%%, max 35%%)? ", aPlayer->incomeTax);
        getnstr(input, sizeof(input));
        incomeTax = ParseNum(input);
        if ((incomeTax >= 0) && (incomeTax <= 35))
            validIncomeTax = true;
    } while (!validIncomeTax);
    aPlayer->incomeTax = incomeTax;
    GameLog("  Set income tax: %d%%\n", incomeTax);
}


/*
 * Display tax revenues for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

void DisplayTaxRevenues(Player *aPlayer)
{
    Country *country;

    /* Get the player country. */
    country = aPlayer->country;

    /* Display tax revenues. */
    UIColor(UIC_HEADING);
    printw("%-21s %-21s %s\n",
           "Customs Duty", "Sales Tax", "Income Tax");
    UIColorOff();
    printw("%20d%% %20d%% %20d%%\n",
           aPlayer->customsTax, aPlayer->salesTax, aPlayer->incomeTax);
    printw("%21s %21s %21s\n",
           FmtNum(aPlayer->customsTaxRevenue),
           FmtNum(aPlayer->salesTaxRevenue),
           FmtNum(aPlayer->incomeTaxRevenue));
    UISeparator();
}


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
                BuyMarketplaces(aPlayer);
                break;

            case INVESTMENT_GRAIN_MILL :
                BuyGrainMills(aPlayer);
                break;

            case INVESTMENT_FOUNDRY :
                BuyFoundries(aPlayer);
                break;

            case INVESTMENT_SHIPYARD :
                BuyShipyards(aPlayer);
                break;

            case INVESTMENT_SOLDIER :
                BuySoldiers(aPlayer);
                break;

            case INVESTMENT_PALACE :
                BuyPalaces(aPlayer);
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

void BuyMarketplaces(Player *aPlayer)
{
    char     input[80];
    int      marketplaceCount;
    bool     validMarketplaceCount;

    /* Get the number of marketplaces to buy. */
    validMarketplaceCount = false;
    do
    {
        /* Get user input. */
        CLEAR_MSG_AREA();
        printw("How many? ");
        getnstr(input, sizeof(input));
        marketplaceCount = ParseNum(input);

        /* Validate the number of marketplaces to buy. */
        validMarketplaceCount = ValidateInvestment(aPlayer,
                                                   INVESTMENT_MARKETPLACE,
                                                   marketplaceCount);
    } while (!validMarketplaceCount);

    /* Purchase using shared function. */
    int bought = PurchaseInvestment(aPlayer, &aPlayer->marketplaceCount,
                                    marketplaceCount, COST_MARKETPLACE);
    GameLog("  Bought %d marketplaces (-%d)  Treasury: %d\n",
            bought, bought * COST_MARKETPLACE, aPlayer->treasury);

    /* Merchants gained with every marketplace purchase action. */
    ApplyMarketplaceBonus(aPlayer);
}


/*
 * Buy grain mills for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

void BuyGrainMills(Player *aPlayer)
{
    char     input[80];
    int      grainMillCount;
    bool     validGrainMillCount;

    /* Get the number of grain mills to buy. */
    validGrainMillCount = false;
    do
    {
        /* Get user input. */
        CLEAR_MSG_AREA();
        printw("How many? ");
        getnstr(input, sizeof(input));
        grainMillCount = ParseNum(input);

        /* Validate the number of grain mills to buy. */
        validGrainMillCount = ValidateInvestment(aPlayer,
                                                 INVESTMENT_GRAIN_MILL,
                                                 grainMillCount);
    } while (!validGrainMillCount);

    int bought = PurchaseInvestment(aPlayer, &aPlayer->grainMillCount,
                                    grainMillCount, COST_GRAIN_MILL);
    GameLog("  Bought %d grain mills (-%d)  Treasury: %d\n",
            bought, bought * COST_GRAIN_MILL, aPlayer->treasury);
}


/*
 * Buy foundries for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

void BuyFoundries(Player *aPlayer)
{
    char     input[80];
    int      foundryCount;
    bool     validFoundryCount;

    /* Get the number of foundries to buy. */
    validFoundryCount = false;
    do
    {
        /* Get user input. */
        CLEAR_MSG_AREA();
        printw("How many? ");
        getnstr(input, sizeof(input));
        foundryCount = ParseNum(input);

        /* Validate the number of foundries to buy. */
        validFoundryCount = ValidateInvestment(aPlayer,
                                               INVESTMENT_FOUNDRY,
                                               foundryCount);
    } while (!validFoundryCount);

    int bought = PurchaseInvestment(aPlayer, &aPlayer->foundryCount,
                                    foundryCount, COST_FOUNDRY);
    GameLog("  Bought %d foundries (-%d)  Treasury: %d\n",
            bought, bought * COST_FOUNDRY, aPlayer->treasury);
}


/*
 * Buy shipyards for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

void BuyShipyards(Player *aPlayer)
{
    char     input[80];
    int      shipyardCount;
    bool     validShipyardCount;

    /* Get the number of shipyards to buy. */
    validShipyardCount = false;
    do
    {
        /* Get user input. */
        CLEAR_MSG_AREA();
        printw("How many? ");
        getnstr(input, sizeof(input));
        shipyardCount = ParseNum(input);

        /* Validate the number of shipyards to buy. */
        validShipyardCount = ValidateInvestment(aPlayer,
                                                INVESTMENT_SHIPYARD,
                                                shipyardCount);
    } while (!validShipyardCount);

    int bought = PurchaseInvestment(aPlayer, &aPlayer->shipyardCount,
                                    shipyardCount, COST_SHIPYARD);
    GameLog("  Bought %d shipyards (-%d)  Treasury: %d\n",
            bought, bought * COST_SHIPYARD, aPlayer->treasury);
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
    printw("How many? ");
    getnstr(input, sizeof(input));
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
                ShowMessage("Limited by foundries (%s). Build more to equip "
                            "troops.\nYou can recruit %s soldiers.",
                            FmtNum(aPlayer->foundryCount),
                            FmtNum(soldierCount));
                break;
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
 * Buy palaces for the player specified by aPlayer
 *
 *   aPlayer                Player.
 */

void BuyPalaces(Player *aPlayer)
{
    char     input[80];
    int      palaceCount;
    bool     validPalaceCount;

    /* Get the number of palaces to buy. */
    validPalaceCount = false;
    do
    {
        /* Get user input. */
        CLEAR_MSG_AREA();
        printw("How many? ");
        getnstr(input, sizeof(input));
        palaceCount = ParseNum(input);

        /* Validate the number of palaces to buy. */
        validPalaceCount = ValidateInvestment(aPlayer,
                                              INVESTMENT_PALACE,
                                              palaceCount);
    } while (!validPalaceCount);

    /* Purchase using shared function. */
    int bought = PurchaseInvestment(aPlayer, &aPlayer->palaceCount,
                                    palaceCount, COST_PALACE);
    GameLog("  Bought %d palaces (-%d)  Treasury: %d\n",
            bought, bought * COST_PALACE, aPlayer->treasury);

    /* Nobles gained with every palace purchase action. */
    ApplyPalaceBonus(aPlayer);
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

    /* If investment is invalid, notify the player. */
    if (!valid && (strlen(invalidMessage) > 0))
    {
        CLEAR_MSG_AREA();
        ShowMessage("%s", invalidMessage);
    }

    return valid;
}

