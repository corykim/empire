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

    /* Reset screen. */
    clear();
    move(0, 0);

    /* Display the ruler and country. */
    printw("%s %s OF %s\n", aPlayer->title, aPlayer->name, country->name);

    /* Display how much grain the rats ate. */
    printw("RATS ATE %d %% OF THE GRAIN RESERVE\n", aPlayer->ratPct);

    /* Display grain levels and treasury. */
    printw("GRAIN     GRAIN     PEOPLE     ARMY       ROYAL\n");
    printw("HARVEST   RESERVE   REQUIRE    REQUIRES   TREASURY\n");
    printw(" %6d    %6d    %6d     %6d     %6d\n",
           aPlayer->grainHarvest,
           aPlayer->grain,
           aPlayer->peopleGrainNeed,
           aPlayer->armyGrainNeed,
           aPlayer->treasury);
    printw("BUSHELS   BUSHELS   BUSHELS    BUSHELS    %s\n", country->currency);

    /* Display grain for sale. */
    printw("------GRAIN FOR SALE:\n");
    printw("  #  COUNTRY           BUSHELS    PRICE\n");
    anyGrainForSale = false;
    for (i = 0; i < COUNTRY_COUNT; i++)
    {
        player = &(playerList[i]);
        if (player->grainForSale > 0)
        {
            anyGrainForSale = true;
            printw("  %d  %-18s %6d    %5.2f\n",
                   player->number,
                   player->country->name,
                   player->grainForSale,
                   player->grainPrice);
        }
    }

    /* Display if no grain is for sale. */
    if (!anyGrainForSale)
    {
        printw("\n\nNO GRAIN FOR SALE . . .\n\n");
    }

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
    mvprintw(4, 43, " %6d", aPlayer->treasury);
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
        printw("1) BUY GRAIN  2) SELL GRAIN  3) SELL LAND  0) DONE? ");
        getnstr(input, sizeof(input));

        /* Parse command. */
        switch (strtol(input, nullptr, 0))
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
        printw("BUY FROM WHICH COUNTRY? ");
        getnstr(input, sizeof(input));
        sellerIndex = strtol(input, nullptr, 0);
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
        ShowMessage("THAT COUNTRY HAS NONE FOR SALE!");
        return;
    }

    /* Cannot buy grain from self. */
    if (seller == aPlayer)
    {
        ShowMessage("YOU CANNOT BUY GRAIN THAT YOU HAVE PUT ONTO THE MARKET!");
        return;
    }

    /* Get the amount of grain to buy. */
    validGrain = false;
    maxGrain = (static_cast<float>(aPlayer->treasury) * 0.9) / seller->grainPrice;
    do
    {
        /* Get the number of bushels to purchase. */
        CLEAR_MSG_AREA();
        printw("HOW MANY BUSHELS? ");
        getnstr(input, sizeof(input));
        grain = strtol(input, nullptr, 0);

        /* Compute the total grain purchase price, including marketplace */
        /* markup.                                                       */
        totalPrice = (static_cast<float>(grain) * seller->grainPrice) / 0.9;

        if (grain > seller->grainForSale)
        {
            ShowMessage("THEY ONLY HAVE %d BUSHELS FOR SALE!",
                        seller->grainForSale);
        }
        else if (totalPrice > aPlayer->treasury)
        {
            CLEAR_MSG_AREA();
            ShowMessage("YOU CAN ONLY AFFORD %d BUSHELS, %s!",
                        maxGrain, aPlayer->title);
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
        printw("HOW MANY BUSHELS DO YOU WISH TO SELL? ");
        getnstr(input, sizeof(input));
        grainToSell = strtol(input, nullptr, 0);
        if (grainToSell > aPlayer->grain)
        {
            CLEAR_MSG_AREA();
            ShowMessage("YOU ONLY HAVE %d BUSHELS!", aPlayer->grain);
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
        printw("WHAT WILL BE THE PRICE PER BUSHEL? ");
        getnstr(input, sizeof(input));
        grainPrice = strtod(input, nullptr);
        if (grainPrice > 15.0)
        {
            ShowMessage("BE REASONABLE . . .EVEN GOLD COSTS LESS THAN THAT!");
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
        ShowMessage("THE BARBARIANS WILL GIVE YOU 2 %s PER ACRE",
                    aPlayer->country->currency);

        /* Get the number of acres to sell. */
        CLEAR_MSG_AREA();
        printw("HOW MANY ACRES WILL YOU SELL THEM? ");
        getnstr(input, sizeof(input));
        landToSell = strtol(input, nullptr, 0);
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
            ShowMessage("YOU MUST KEEP SOME LAND FOR THE ROYAL PALACE!");
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
        printw("FEED ARMY OF %d MEN (NEED %d, ENTER FOR DEFAULT)? ",
               aPlayer->soldierCount, aPlayer->armyGrainNeed);
        getnstr(input, sizeof(input));
        if (input[0] == '\0')
            grainToFeed = aPlayer->armyGrainNeed;
        else
            grainToFeed = strtol(input, nullptr, 0);
        if (grainToFeed > aPlayer->grain)
        {
            CLEAR_MSG_AREA();
            ShowMessage("YOU CANNOT GIVE YOUR ARMY MORE GRAIN THAN YOU HAVE!");
        }
        else if (grainToFeed > aPlayer->armyGrainNeed * 2)
        {
            CLEAR_MSG_AREA();
            printw("YOUR ARMY NEEDS %d BUSHELS. GIVE %d? (Y/N) ",
                   aPlayer->armyGrainNeed, grainToFeed);
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
        printw("FEED %d PEOPLE (NEED %d, ENTER FOR DEFAULT)? ",
               peopleCount, aPlayer->peopleGrainNeed);
        getnstr(input, sizeof(input));
        if (input[0] == '\0')
            grainToFeed = aPlayer->peopleGrainNeed;
        else
            grainToFeed = strtol(input, nullptr, 0);
        if (grainToFeed > aPlayer->grain)
        {
            CLEAR_MSG_AREA();
            ShowMessage("BUT YOU ONLY HAVE %d BUSHELS OF GRAIN!",
                        aPlayer->grain);
        }
        else if (static_cast<float>(grainToFeed) < (0.1 * static_cast<float>(aPlayer->grain)))
        {
            CLEAR_MSG_AREA();
            ShowMessage("YOU MUST RELEASE AT LEAST 10%% OF THE STORED GRAIN");
        }
        else if (grainToFeed > aPlayer->peopleGrainNeed * 4)
        {
            CLEAR_MSG_AREA();
            printw("YOUR PEOPLE NEED %d BUSHELS. GIVE %d? (Y/N) ",
                   aPlayer->peopleGrainNeed, grainToFeed);
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

