/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * TRS-80 Empire game grain screen source file.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 *
 * Includes.
 */

/* System includes. */
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

static void DrawGrainScreen(Player *aPlayer);

static void TradeGrainAndLand(Player *aPlayer);

static void BuyGrain(Player *aPlayer);

static void SellGrain(Player *aPlayer);

static void SellLand(Player *aPlayer);

static void WithdrawGrainScreen(Player *aPlayer);

static void RepriceGrainScreen(Player *aPlayer);

static int  ParseFeedInput(const char *input, int need, bool isPeople);
static void FeedCountry(Player *aPlayer);

static void UpdateTreasuryDisplay(Player *aPlayer);


/*
 * Return true if input is a valid numeric string (digits, optional commas,
 * optional leading minus).  Empty strings return false.
 */

static bool IsValidNumericInput(const char *input)
{
    if (input[0] == '\0')
        return false;
    int i = 0;
    if (input[i] == '-') i++;
    bool hasDigit = false;
    for (; input[i]; i++)
    {
        if (input[i] >= '0' && input[i] <= '9')
            hasDigit = true;
        else if (input[i] != ',')
            return false;
    }
    return hasDigit;
}


/*
 * Return true if input consists entirely of '+' characters.
 */

static bool IsAllPlus(const char *input)
{
    if (input[0] != '+')
        return false;
    for (int i = 0; input[i]; i++)
    {
        if (input[i] != '+')
            return false;
    }
    return true;
}


/*------------------------------------------------------------------------------
 *
 * External grain screen functions.
 */

/*
 * Manage grain for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

void GrainScreen(Player *aPlayer)
{
    /* Shared grain computation (rats, harvest, needs). */
    ComputeGrainPhase(aPlayer);

    /* Draw the grain screen. */
    DrawGrainScreen(aPlayer);

    /* Trade grain and land. */
    TradeGrainAndLand(aPlayer);

    /* Feed country. */
    FeedCountry(aPlayer);
}


/*------------------------------------------------------------------------------
 *
 * Internal grain screen functions.
 */

/*
 * Draw the grain screen for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void DrawGrainScreen(Player *aPlayer)
{
    Player  *player;
    Country *country;
    bool     anyGrainForSale;
    int      i;

    /* Get the player country. */
    country = aPlayer->country;

    /* Title. */
    char rulerLine[160];
    snprintf(rulerLine, sizeof(rulerLine), "%s %s of %s",
             aPlayer->title, aPlayer->name, country->name);
    UITitle("Grain", rulerLine);

    UIColor(UIC_BAD);
    printw("Rats ate %d%% of the grain reserve\n", aPlayer->ratPct);
    UIColorOff();

    /* Grain stats table. */
    UIColor(UIC_HEADING);
    printw("Harvest   Reserve   People     Army       Treasury\n");
    UIColorOff();
    printw("%7s   %7s   %7s   %7s",
           FmtNum(aPlayer->grainHarvest),
           FmtNum(aPlayer->grain),
           FmtNum(aPlayer->peopleGrainNeed),
           FmtNum(aPlayer->armyGrainNeed));
    printw("    %7s %s\n",
           FmtNum(aPlayer->treasury),
           country->currency);
    UISeparator();

    /* Grain market. */
    UIColor(UIC_HEADING);
    printw("Grain For Sale:\n");
    printw("  #  Country           Bushels    Price\n");
    UIColorOff();
    anyGrainForSale = false;
    for (i = 0; i < COUNTRY_COUNT; i++)
    {
        player = &(playerList[i]);
        if (player->grainForSale > 0)
        {
            anyGrainForSale = true;
            printw("  %d  %-18s %6s    %6.4f\n",
                   player->number,
                   player->country->name,
                   FmtNum(player->grainForSale),
                   player->grainPrice);
        }
    }

    if (!anyGrainForSale)
    {
        printw("  (none)\n");
    }
    UISeparator();

    /* Refresh screen. */
    refresh();
}


/*
 * Update the treasury value on screen without redrawing everything.
 *
 *   aPlayer                Player.
 */

static void UpdateTreasuryDisplay(Player *aPlayer)
{
    int row, col;
    getyx(stdscr, row, col);
    mvprintw(4, 43, " %6s", FmtNum(aPlayer->treasury));
    move(row, col);
    refresh();
}


/*
 * Trade grain and land for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void TradeGrainAndLand(Player *aPlayer)
{
    char input[80];
    bool doneTrading;

    /* Trade grain and land. */
    doneTrading = false;
    while (!doneTrading)
    {
        /* Draw grain screen. */
        DrawGrainScreen(aPlayer);

        /* Display options. */
        CLEAR_MSG_AREA();
        if (aPlayer->grainForSale > 0)
            printw("1) Buy  2) Sell  3) Land  4) Withdraw  5) Reprice  0) Done? ");
        else
            printw("1) Buy Grain  2) Sell Grain  3) Sell Land  0) Done? ");
        getnstr(input, sizeof(input));

        /* Parse command. */
        switch (ParseNum(input))
        {
            case 0 :
                doneTrading = true;
                break;

            case 1 :
                BuyGrain(aPlayer);
                break;

            case 2 :
                SellGrain(aPlayer);
                break;

            case 3 :
                SellLand(aPlayer);
                break;

            case 4 :
                WithdrawGrainScreen(aPlayer);
                break;

            case 5 :
                RepriceGrainScreen(aPlayer);
                break;

            default :
                break;
        }
    }
}


/*
 * Buy grain for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void BuyGrain(Player *aPlayer)
{
    Player *seller;
    int     sellerIndex;
    int     grain;
    int     maxGrain;
    int     totalPrice;
    char    input[80];
    bool    validSeller;
    bool    validGrain;

    /* Get the seller from which to buy grain. */
    validSeller = false;
    do
    {
        CLEAR_MSG_AREA();
        printw("Buy from which country (0 to cancel)? ");
        getnstr(input, sizeof(input));
        if (!IsValidNumericInput(input) && input[0] != '\0')
        {
            CLEAR_MSG_AREA();
            ShowMessage("Enter a country number");
            continue;
        }
        sellerIndex = ParseNum(input);
        if ((sellerIndex >= 0) && (sellerIndex <= COUNTRY_COUNT))
        {
            validSeller = true;
        }
    } while (!validSeller);
    if (sellerIndex > 0)
        seller = &(playerList[sellerIndex - 1]);
    else
        seller = nullptr;

    /* Validate that the seller has grain for sale. */
    if ((seller == nullptr) || (seller->dead) || (seller->grainForSale == 0))
    {
        ShowMessage("That country has none for sale!");
        return;
    }

    /* Cannot buy grain from self. */
    if (seller == aPlayer)
    {
        ShowMessage("You cannot buy grain that you have put onto the market!");
        return;
    }

    /* Get the amount of grain to buy. */
    validGrain = false;
    maxGrain = static_cast<int>(
        (static_cast<float>(aPlayer->treasury) * (1.0f - GRAIN_MARKUP))
        / seller->grainPrice);
    if (maxGrain > seller->grainForSale)
        maxGrain = seller->grainForSale;
    do
    {
        /* Get the number of bushels to purchase. */
        CLEAR_MSG_AREA();
        printw("How many bushels (up to %s, ENTER for max)? ",
               FmtNum(maxGrain));
        getnstr(input, sizeof(input));
        if (input[0] == '\0')
        {
            grain = maxGrain;
        }
        else if (!IsValidNumericInput(input))
        {
            CLEAR_MSG_AREA();
            ShowMessage("Enter a number of bushels to buy");
            continue;
        }
        else
        {
            grain = ParseNum(input);
        }
        if (grain <= 0)
            return;

        /* Compute the total grain purchase price, including marketplace markup. */
        totalPrice = static_cast<int>(
            (static_cast<float>(grain) * seller->grainPrice)
            / (1.0f - GRAIN_MARKUP));

        if (grain > seller->grainForSale)
        {
            ShowMessage("They only have %s bushels for sale!",
                        FmtNum(seller->grainForSale));
        }
        else if (totalPrice > aPlayer->treasury)
        {
            CLEAR_MSG_AREA();
            ShowMessage("You can only afford %s bushels, %s!",
                        FmtNum(maxGrain), aPlayer->title);
        }
        else
        {
            validGrain = true;
        }
    } while (!validGrain);

    /* Execute trade using shared function. */
    TradeGrain(aPlayer, seller, grain);
    UpdateTreasuryDisplay(aPlayer);
}


/*
 * Sell grain for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void SellGrain(Player *aPlayer)
{
    int   grainToSell;
    float grainPrice;
    char  input[80];
    bool  validGrainToSell;
    bool  validGrainPrice;

    /* Get the amount of grain to sell. */
    validGrainToSell = false;
    do
    {
        CLEAR_MSG_AREA();
        printw("How many bushels do you wish to sell (0 to cancel)? ");
        getnstr(input, sizeof(input));
        if (!IsValidNumericInput(input) && input[0] != '\0')
        {
            CLEAR_MSG_AREA();
            ShowMessage("Enter a number of bushels");
            continue;
        }
        grainToSell = ParseNum(input);
        if (grainToSell <= 0)
            return;
        if (grainToSell > aPlayer->grain)
        {
            CLEAR_MSG_AREA();
            ShowMessage("You only have %s bushels!", FmtNum(aPlayer->grain));
        }
        else
        {
            validGrainToSell = true;
        }
    } while (!validGrainToSell);

    /* Get the price at which to sell grain. */
    validGrainPrice = false;
    do
    {
        CLEAR_MSG_AREA();
        printw("What will be the price per bushel? ");
        getnstr(input, sizeof(input));
        if (input[0] == '\0')
            continue;
        {
            char *endp;
            grainPrice = strtod(input, &endp);
            if (endp == input || (*endp != '\0' && *endp != ' '))
            {
                CLEAR_MSG_AREA();
                ShowMessage("Enter a price (e.g. 0.50)");
                continue;
            }
        }
        if (grainPrice > GRAIN_PRICE_MAX)
        {
            ShowMessage("Be reasonable . . .even gold costs less than that!");
        }
        else if (grainPrice > 0.0)
        {
            validGrainPrice = true;
        }
    } while (!validGrainPrice);

    /* List grain using shared function. */
    ListGrainForSale(aPlayer, grainToSell, grainPrice);
}


/*
 * Sell land for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void SellLand(Player *aPlayer)
{
    int   landToSell;
    char  input[80];
    bool  validLandToSell;

    /* Sell land. */
    validLandToSell = false;
    do
    {
        /* Display the price per acre. */
        CLEAR_MSG_AREA();
        ShowMessage("The barbarians will give you %d %s per acre",
                    static_cast<int>(LAND_SELL_PRICE), aPlayer->country->currency);

        /* Get the number of acres to sell. */
        CLEAR_MSG_AREA();
        printw("How many of your %s acres will you sell (0 to cancel)? ",
               FmtNum(aPlayer->land));
        getnstr(input, sizeof(input));
        if (!IsValidNumericInput(input) && input[0] != '\0')
        {
            CLEAR_MSG_AREA();
            ShowMessage("Enter a number of acres");
            continue;
        }
        landToSell = ParseNum(input);
        if (landToSell <= 0)
            return;
        if (static_cast<float>(landToSell) <= (LAND_MAX_SELL_PCT * static_cast<float>(aPlayer->land)))
        {
            validLandToSell = true;
        }
        else
        {
            ShowMessage("You must keep some land for the royal palace!");
        }
    } while (!validLandToSell);

    /* Sell land using shared function. */
    SellLandToBarbarians(aPlayer, landToSell);
    UpdateTreasuryDisplay(aPlayer);
}


/*
 * Withdraw grain from the market for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void WithdrawGrainScreen(Player *aPlayer)
{
    int   grainToWithdraw;
    char  input[80];
    bool  valid;

    if (aPlayer->grainForSale <= 0)
    {
        ShowMessage("You have no grain on the market!");
        return;
    }

    valid = false;
    do
    {
        CLEAR_MSG_AREA();
        printw("Withdraw how many of your %s listed bushels (%d%% penalty, 0 to cancel)? ",
               FmtNum(aPlayer->grainForSale),
               static_cast<int>(GRAIN_WITHDRAW_FEE * 100));
        getnstr(input, sizeof(input));
        if (!IsValidNumericInput(input) && input[0] != '\0')
        {
            CLEAR_MSG_AREA();
            ShowMessage("Enter a number of bushels");
            continue;
        }
        grainToWithdraw = ParseNum(input);
        if (grainToWithdraw <= 0)
        {
            return;
        }
        else if (grainToWithdraw > aPlayer->grainForSale)
        {
            ShowMessage("You only have %s bushels on the market!",
                        FmtNum(aPlayer->grainForSale));
        }
        else
        {
            valid = true;
        }
    } while (!valid);

    int returned = WithdrawGrain(aPlayer, grainToWithdraw);
    CLEAR_MSG_AREA();
    ShowMessage("Withdrew %s bushels (%s lost to spoilage)",
                FmtNum(returned), FmtNum(grainToWithdraw - returned));
}


/*
 * Reprice grain on the market for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void RepriceGrainScreen(Player *aPlayer)
{
    float newPrice;
    char  input[80];

    if (aPlayer->grainForSale <= 0)
    {
        ShowMessage("You have no grain on the market!");
        return;
    }

    CLEAR_MSG_AREA();
    printw("Current price: %6.4f.  New price per bushel (0 to cancel)? ",
           aPlayer->grainPrice);
    getnstr(input, sizeof(input));
    {
        char *endp;
        newPrice = strtod(input, &endp);
        if (endp == input)
            return;
    }

    if (newPrice <= 0.0)
        return;
    if (newPrice > GRAIN_PRICE_MAX)
    {
        ShowMessage("Be reasonable . . .even gold costs less than that!");
        return;
    }

    aPlayer->grainPrice = newPrice;
    GameLog("  Repriced grain to %.4f (%d bushels)\n",
            newPrice, aPlayer->grainForSale);
}


/*
 * Feed country for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

/*
 * Parse a feed input string.  Returns the grain amount, or -1 for invalid.
 * Handles: empty (default), '+' shortcuts, numeric input.
 */

static int ParseFeedInput(const char *input, int need, bool isPeople)
{
    if (input[0] == '\0')
        return need;

    if (IsAllPlus(input))
    {
        if (isPeople)
        {
            int plusCount = 0;
            for (int c = 0; input[c] == '+'; c++)
                plusCount++;
            return need * (100 + 50 * plusCount) / 100 + 1;
        }
        else
        {
            return need * 3 / 2 + 1;
        }
    }

    if (!IsValidNumericInput(input))
        return -1;

    return ParseNum(input);
}


static void FeedCountry(Player *aPlayer)
{
    int   grainToFeed;
    int   peopleCount;
    char  input[80];
    bool  validGrainToFeed;

    /* Feed army — skip if no soldiers. */
    if (aPlayer->soldierCount == 0)
    {
        aPlayer->armyGrainFeed = 0;
        GameLog("  Feed army: 0 (no soldiers)\n");
    }
    else
    {
    validGrainToFeed = false;
    do
    {
        /* Get the amount of grain to feed to army. */
        CLEAR_MSG_AREA();
        printw("Feed army of %s men (need %s, ENTER for default)? ",
               FmtNum(aPlayer->soldierCount), FmtNum(aPlayer->armyGrainNeed));
        getnstr(input, sizeof(input));
        grainToFeed = ParseFeedInput(input, aPlayer->armyGrainNeed, false);
        if (grainToFeed < 0)
        {
            CLEAR_MSG_AREA();
            ShowMessage("Enter a number of bushels, or + for 150%%");
            continue;
        }
        if (grainToFeed > aPlayer->grain)
        {
            CLEAR_MSG_AREA();
            ShowMessage("You cannot give your army more grain than you have!");
        }
        else if (grainToFeed > aPlayer->armyGrainNeed * 2 &&
                 grainToFeed - aPlayer->armyGrainNeed > 10)
        {
            CLEAR_MSG_AREA();
            printw("Your army needs %s bushels. Give %s? (Y/N) ",
                   FmtNum(aPlayer->armyGrainNeed), FmtNum(grainToFeed));
            getnstr(input, sizeof(input));
            if (input[0] == 'Y' || input[0] == 'y')
                validGrainToFeed = true;
        }
        else
        {
            validGrainToFeed = true;
        }
    } while (!validGrainToFeed);
    aPlayer->grain -= grainToFeed;
    aPlayer->armyGrainFeed = grainToFeed;
    GameLog("  Feed army: %d (need %d)  Grain: %d\n",
            grainToFeed, aPlayer->armyGrainNeed, aPlayer->grain);
    } /* end else (soldierCount > 0) */

    /* Feed people. */
    validGrainToFeed = false;
    peopleCount =
        aPlayer->serfCount + aPlayer->merchantCount + aPlayer->nobleCount;
    do
    {
        /* Get the amount of grain to feed to people. */
        CLEAR_MSG_AREA();
        printw("Feed %s people (need %s, ENTER for default)? ",
               FmtNum(peopleCount), FmtNum(aPlayer->peopleGrainNeed));
        getnstr(input, sizeof(input));
        grainToFeed = ParseFeedInput(input, aPlayer->peopleGrainNeed, true);
        if (grainToFeed < 0)
        {
            CLEAR_MSG_AREA();
            ShowMessage("Enter a number of bushels, or + for 150%%, ++ for 200%%");
            continue;
        }
        if (grainToFeed > aPlayer->grain)
        {
            CLEAR_MSG_AREA();
            ShowMessage("But you only have %s bushels of grain!",
                        FmtNum(aPlayer->grain));
        }
        else if (static_cast<float>(grainToFeed) < (0.1 * static_cast<float>(aPlayer->grain)))
        {
            CLEAR_MSG_AREA();
            ShowMessage("You must release at least 10%% of the stored grain");
        }
        else if (grainToFeed > aPlayer->peopleGrainNeed * 4 &&
                 grainToFeed - aPlayer->peopleGrainNeed > 10)
        {
            CLEAR_MSG_AREA();
            printw("Your people need %s bushels. Give %s? (Y/N) ",
                   FmtNum(aPlayer->peopleGrainNeed), FmtNum(grainToFeed));
            getnstr(input, sizeof(input));
            if (input[0] == 'Y' || input[0] == 'y')
                validGrainToFeed = true;
        }
        else
        {
            validGrainToFeed = true;
        }
    } while (!validGrainToFeed);
    aPlayer->grain -= grainToFeed;
    aPlayer->peopleGrainFeed = grainToFeed;
    GameLog("  Feed people: %d (need %d)  Grain: %d\n",
            grainToFeed, aPlayer->peopleGrainNeed, aPlayer->grain);
}

