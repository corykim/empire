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
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Local includes. */
#include "empire.h"
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

static void FeedCountry(Player *aPlayer);

static void UpdateTreasuryDisplay(Player *aPlayer);


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
    int usableLand;

    /* Determine what percentage of grain the rats ate. */
    aPlayer->ratPct = RandRange(30);
    aPlayer->grain -= (aPlayer->grain * aPlayer->ratPct) / 100;

    /* Determine the amount of usable land for grain. */
    usableLand =   aPlayer->land
                 - aPlayer->serfCount
                 - (2 * aPlayer->nobleCount)
                 - aPlayer->palaceCount
                 - aPlayer->merchantCount
                 - (2 * aPlayer->soldierCount);

    /* Each bushel of grain in the reserves can be used to seed 3 acres of */
    /* land.                                                               */
    if (usableLand > (3 * aPlayer->grain))
    {
        usableLand = 3 * aPlayer->grain;
    }

    /* Each serf can farm 5 acres of land. */
    if (usableLand > (5 * aPlayer->serfCount))
    {
        usableLand = 5 * aPlayer->serfCount;
    }

    /* Determine the grain harvest. */
    aPlayer->grainHarvest =   (weather * usableLand * 0.72)
                            + RandRange(500)
                            - (aPlayer->foundryCount * 500);
    if (aPlayer->grainHarvest < 0)
        aPlayer->grainHarvest = 0;
    aPlayer->grain += aPlayer->grainHarvest;

    /* Determine the amount of grain required by the people. */
    aPlayer->peopleGrainNeed = 5 * (  aPlayer->serfCount
                                    + aPlayer->merchantCount
                                    + (3 * aPlayer->nobleCount));

    /* Determine the amount of grain required by the army. */
    aPlayer->armyGrainNeed = 8 * aPlayer->soldierCount;

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

    printw("Rats ate %d%% of the grain reserve\n", aPlayer->ratPct);

    /* Grain stats table. */
    printw("Harvest   Reserve   People     Army       Treasury\n");
    printw("%7s   %7s   %7s   %7s    %7s %s\n",
           FmtNum(aPlayer->grainHarvest),
           FmtNum(aPlayer->grain),
           FmtNum(aPlayer->peopleGrainNeed),
           FmtNum(aPlayer->armyGrainNeed),
           FmtNum(aPlayer->treasury),
           country->currency);
    UISeparator();

    /* Grain market. */
    printw("Grain For Sale:\n");
    printw("  #  Country           Bushels    Price\n");
    anyGrainForSale = false;
    for (i = 0; i < COUNTRY_COUNT; i++)
    {
        player = &(playerList[i]);
        if (player->grainForSale > 0)
        {
            anyGrainForSale = true;
            printw("  %d  %-18s %6s    %5.2f\n",
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
        printw("Buy from which country? ");
        getnstr(input, sizeof(input));
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
    maxGrain = (static_cast<float>(aPlayer->treasury) * 0.9) / seller->grainPrice;
    do
    {
        /* Get the number of bushels to purchase. */
        CLEAR_MSG_AREA();
        printw("How many bushels? ");
        getnstr(input, sizeof(input));
        grain = ParseNum(input);

        /* Compute the total grain purchase price, including marketplace */
        /* markup.                                                       */
        totalPrice = (static_cast<float>(grain) * seller->grainPrice) / 0.9;

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

    /* Update player and seller state. */
    aPlayer->grain += grain;
    aPlayer->treasury -= totalPrice;
    seller->treasury += grain * seller->grainPrice;
    seller->grainForSale -= grain;
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
        printw("How many bushels do you wish to sell? ");
        getnstr(input, sizeof(input));
        grainToSell = ParseNum(input);
        if (grainToSell > aPlayer->grain)
        {
            CLEAR_MSG_AREA();
            ShowMessage("You only have %s bushels!", FmtNum(aPlayer->grain));
        }
        else if (grainToSell > 0)
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
        grainPrice = strtod(input, nullptr);
        if (grainPrice > 15.0)
        {
            ShowMessage("Be reasonable . . .even gold costs less than that!");
        }
        else if (grainPrice > 0.0)
        {
            validGrainPrice = true;
        }
    } while (!validGrainPrice);

    /* Update the total grain for sale and price. */
    aPlayer->grainPrice =
          (  (aPlayer->grainPrice * static_cast<float>(aPlayer->grainForSale))
           + (grainPrice * static_cast<float>(grainToSell)))
        / static_cast<float>(aPlayer->grainForSale + grainToSell);
    aPlayer->grainForSale += grainToSell;
    aPlayer->grain -= grainToSell;
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
        ShowMessage("The barbarians will give you 2 %s per acre",
                    aPlayer->country->currency);

        /* Get the number of acres to sell. */
        CLEAR_MSG_AREA();
        printw("How many acres will you sell? ");
        getnstr(input, sizeof(input));
        landToSell = ParseNum(input);
        if (landToSell < 0)
        {
            validLandToSell = false;
        }
        else if (static_cast<float>(landToSell) <= (0.95 * static_cast<float>(aPlayer->land)))
        {
            validLandToSell = true;
        }
        else
        {
            ShowMessage("You must keep some land for the royal palace!");
        }
    } while (!validLandToSell);

    /* Update land and treasury. */
    aPlayer->treasury += 2 * landToSell;
    aPlayer->land -= landToSell;
    barbarianLand += landToSell;
    UpdateTreasuryDisplay(aPlayer);
}


/*
 * Feed country for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void FeedCountry(Player *aPlayer)
{
    int   grainToFeed;
    int   peopleCount;
    char  input[80];
    bool  validGrainToFeed;

    /* Feed army. */
    validGrainToFeed = false;
    do
    {
        /* Get the amount of grain to feed to army. */
        CLEAR_MSG_AREA();
        printw("Feed army of %s men (need %s, ENTER for default)? ",
               FmtNum(aPlayer->soldierCount), FmtNum(aPlayer->armyGrainNeed));
        getnstr(input, sizeof(input));
        if (input[0] == '\0')
            grainToFeed = aPlayer->armyGrainNeed;
        else
            grainToFeed = ParseNum(input);
        if (grainToFeed > aPlayer->grain)
        {
            CLEAR_MSG_AREA();
            ShowMessage("You cannot give your army more grain than you have!");
        }
        else if (grainToFeed > aPlayer->armyGrainNeed * 2)
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
        if (input[0] == '\0')
            grainToFeed = aPlayer->peopleGrainNeed;
        else
            grainToFeed = ParseNum(input);
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
        else if (grainToFeed > aPlayer->peopleGrainNeed * 4)
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
}

