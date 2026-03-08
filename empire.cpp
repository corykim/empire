/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * TRS-80 Empire game source file.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 *
 * Includes.
 */

/* System includes. */
#include <ctype.h>
#include <math.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Local includes. */
#include "empire.h"
#include "cpu_strategy.h"
#include "grain.h"
#include "population.h"


/*------------------------------------------------------------------------------
 *
 * Prototypes.
 */

static void StartScreen();

static void GameSetupScreen();

static void NewYearScreen();

static void SummaryScreen();

static void PlayHuman(Player *aPlayer);

static void PlayCPU(Player *aPlayer);

static void CPUGrainPhase(Player *aPlayer);

static void CPUPopulationPhase(Player *aPlayer);

static void CPUInvestmentsPhase(Player *aPlayer);


/*------------------------------------------------------------------------------
 *
 * Globals.
 */

/*
 * Country  list.
 */

Country countryList[COUNTRY_COUNT] =
{
    /* AUVEYRON. */
    {
        .name = "AUVEYRON",
        .rulerName = "MONTAIGNE",
        .currency = "FRANCS",
        .titleList = { "CHEVALIER", "PRINCE", "ROI", "EMPEREUR", },
    },

    /* BRITTANY. */
    {
        .name = "BRITTANY",
        .rulerName = "ARTHUR",
        .currency = "FRANCS",
        .titleList = { "SIR", "PRINCE", "KING", "EMPEROR", },
    },

    /* BAVARIA. */
    {
        .name = "BAVARIA",
        .rulerName = "MUNSTER",
        .currency = "MARKS",
        .titleList = { "RITTER", "PRINZ", "KONIG", "KAISER", },
    },

    /* QUATARA. */
    {
        .name = "QUATARA",
        .rulerName = "KHOTAN",
        .currency = "DINARS",
        .titleList = { "HASID", "CALIPH", "SHEIK", "SHAH", },
    },

    /* BARCELONA. */
    {
        .name = "BARCELONA",
        .rulerName = "FERDINAND",
        .currency = "PESETA",
        .titleList = { "CABALLERO", "PRINCIPE", "REY", "EMPERADORE", },
    },

    /* SVEALAND. */
    {
        .name = "SVEALAND",
        .rulerName = "HJODOLF",
        .currency = "KRONA",
        .titleList = { "RIDDARE", "PRINS", "KUNG", "KEJSARE", },
    },
};


/*
 * Weather list.
 */

const char *weatherList[] =
{
    "POOR WEATHER. NO RAIN. LOCUSTS MIGRATE.",
    "EARLY FROSTS. ARID CONDITIONS.",
    "FLASH FLOODS. TOO MUCH RAIN.",
    "AVERAGE WEATHER. GOOD YEAR.",
    "FINE WEATHER. LONG SUMMER.",
    "FANTASTIC WEATHER ! GREAT YEAR !",
};


/*
 * Difficulty level names.
 */

const char *difficultyList[DIFFICULTY_COUNT] =
{
    "VILLAGE FOOL",
    "LANDED PEASANT",
    "MINOR NOBLE",
    "ROYAL ADVISOR",
    "MACHIAVELLI",
};


/*
 * Game state variables.
 *
 *   playerList             List of players.
 *   playerCount            Count of the number of human players.
 *   year                   Current year.
 *   weather                Weather for year, 1 based.
 *   barbarianLand          Amount of barbarian land in acres.
 *   gameOver               If true, game is over.
 *   difficulty             Difficulty level (0-4).
 */

Player playerList[COUNTRY_COUNT];
int playerCount = 0;
int year = 0;
int weather;
int barbarianLand = 6000;
int gameOver = false;
int difficulty = 2;


/*------------------------------------------------------------------------------
 *
 * Main entry point.
 */

int main(int argc, char *argv[])
{
    Player *player;
    int     i, j;

    /* Initialize the screen. */
    initscr();

    /* Resize the window to 16x64 and set it to scroll. */
    wresize(stdscr, 16, 64);
    scrollok(stdscr, true);

    /* Seed the random number generator. */
    srand(time(nullptr));

    /* Display the game start screen. */
    StartScreen();

    /* Set up game options. */
    GameSetupScreen();

    /* Run game until it's over. */
    while (!gameOver)
    {
        /* Start a new year. */
        NewYearScreen();

        /* Randomize the turn order for this year. */
        int turnOrder[COUNTRY_COUNT];
        for (i = 0; i < COUNTRY_COUNT; i++)
            turnOrder[i] = i;
        for (i = COUNTRY_COUNT - 1; i > 0; i--)
        {
            /* RandRange returns 1..n, so subtract 1 for a 0-based index. */
            j = RandRange(i + 1) - 1;
            int tmp = turnOrder[i];
            turnOrder[i] = turnOrder[j];
            turnOrder[j] = tmp;
        }

        /* Go through each player. */
        for (i = 0; i < COUNTRY_COUNT; i++)
        {
            /* Get player. */
            player = &(playerList[turnOrder[i]]);

            /* Skip dead players. */
            if (player->dead)
                continue;

            /* Play as human or CPU. */
            if (player->human)
                PlayHuman(player);
            else
                PlayCPU(player);

            /* Check for game over conditions. */
            {
                int h;
                int humansAlive = 0;
                int cpuAlive = 0;
                Player *lastHuman = nullptr;
                for (h = 0; h < COUNTRY_COUNT; h++)
                {
                    if (playerList[h].dead)
                        continue;
                    if (playerList[h].human)
                    {
                        humansAlive++;
                        lastHuman = &playerList[h];
                    }
                    else
                    {
                        cpuAlive++;
                    }
                }
                /* All humans eliminated. */
                if (humansAlive == 0)
                    gameOver = true;
                /* Victory: one human remains, all CPU players defeated. */
                if (humansAlive == 1 && cpuAlive == 0 && lastHuman != nullptr)
                {
                    char input[80];
                    clear();
                    move(0, 0);
                    printw("ALL RIVAL NATIONS HAVE FALLEN!\n\n");
                    printw("%s %s OF %s HAS CONQUERED THE KNOWN WORLD!\n\n",
                           lastHuman->title,
                           lastHuman->name,
                           lastHuman->country->name);
                    printw("LONG LIVE %s %s!\n\n",
                           lastHuman->title,
                           lastHuman->name);
                    printw("<ENTER>? ");
                    getnstr(input, sizeof(input));
                    gameOver = true;
                }
            }

            /* Stop if game over. */
            if (gameOver)
                break;
        }

        /* Stop if game over. */
        if (gameOver)
            break;

        /* Display summary. */
        SummaryScreen();
    }

    /* End nCurses. */
    endwin();

    return 0;
}


/*------------------------------------------------------------------------------
 *
 * External functions.
 */

/*
 *   Return a random integer value between 1 and the value specified by range,
 * inclusive (i.e., [1, range]).  If range is 0, return 0.
 *
 *   range                  Range of random value.
 */

int RandRange(int range)
{
    return (range > 0) ? (rand() % range) + 1 : 0;
}


/*
 * Display a message, refresh the screen, and delay proportional to the
 * number of words.  200ms per word, never less than DELAY_TIME.  Accepts
 * printf-style format strings.
 *
 *   fmt                    Format string (printf-style).
 *   ...                    Format arguments.
 */

void ShowMessage(const char *fmt, ...)
{
    char    buf[256];
    va_list args;
    int     words = 0;
    int     inWord = 0;
    int     delayUs;
    const char *p;

    /* Format the message. */
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* Display and refresh. */
    printw("%s", buf);
    refresh();

    /* Count words in the rendered message. */
    for (p = buf; *p; p++)
    {
        if (*p == ' ' || *p == '\n' || *p == '\t')
            inWord = 0;
        else if (!inWord)
        {
            inWord = 1;
            words++;
        }
    }

    /* Delay 200ms per word, minimum DELAY_TIME. */
    delayUs = words * 200000;
    if (delayUs < DELAY_TIME)
        delayUs = DELAY_TIME;
    usleep(delayUs);
}


/*------------------------------------------------------------------------------
 *
 * Internal functions.
 */

/*
 * Display the game start splash screen.
 */

static void StartScreen()
{
    /* Clear screen. */
    clear();

    /* Display splash screen. */
    move(0, 20);
    printw("E M P I R E\n");
    move(7, 0);
    printw("(ALWAYS HIT <ENTER> TO CONTINUE)");
    refresh();

    /* Delay. */
    usleep(DELAY_TIME);
}


/*
 * Set up the game options.
 */

static void GameSetupScreen()
{
    Country *country;
    Player  *player;
    char     input[80];
    int      i, j;

    /* Reset screen. */
    clear();
    move(0, 0);

    /* Get the number of players. */
    do
    {
        printw("HOW MANY PEOPLE ARE PLAYING? ");
        refresh();
        getnstr(input, sizeof(input));
        playerCount = strtol(input, nullptr, 0);
    } while ((playerCount < 1) || (playerCount > COUNTRY_COUNT));

    /* Get the difficulty level. */
    printw("\nCHOOSE THE CUNNING OF YOUR RIVALS:\n");
    for (i = 0; i < DIFFICULTY_COUNT; i++)
    {
        printw("  %d. %s\n", i + 1, difficultyList[i]);
    }
    do
    {
        printw("DIFFICULTY (1-%d)? ", DIFFICULTY_COUNT);
        refresh();
        getnstr(input, sizeof(input));
        difficulty = strtol(input, nullptr, 0) - 1;
    } while ((difficulty < 0) || (difficulty >= DIFFICULTY_COUNT));
    printw("\n");

    /* Get the player names. */
    for (i = 0; i < playerCount; i++)
    {
        /* Get the country and player records. */
        country = &(countryList[i]);
        player = &(playerList[i]);

        /* Player is human. */
        player->human = true;

        /* Get the country's ruler's name. */
        printw("WHO IS THE RULER OF %s? ", country->name);
        getnstr(player->name, sizeof(player->name));
        for (j = 0; j < strlen(player->name); j++)
        {
            player->name[j] = toupper(player->name[j]);
        }
    }
    for (; i < COUNTRY_COUNT; i++)
    {
        /* Get the country and player records. */
        country = &(countryList[i]);
        player = &(playerList[i]);

        /* Player is not human. */
        player->human = false;

        /* Get the country's ruler's name. */
        snprintf(player->name, sizeof(player->name), "%s", country->rulerName);
    }

    /* Initialize the player records. */
    for (i = 0; i < COUNTRY_COUNT; i++)
    {
        /* Get the country and player records. */
        country = &(countryList[i]);
        player = &(playerList[i]);

        /* Initialize the player's number. */
        player->number = i + 1;

        /* Initialize the player's country. */
        player->country = country;

        /* Initialize the player's level and title. */
        player->level = 0;
        snprintf(player->title,
                 sizeof(player->title),
                 "%s",
                 country->titleList[player->level]);

        /* Initialize the player's state. */
        player->dead = false;
        player->land = 10000;
        player->grain = 15000 + RandRange(10000);
        player->treasury = 1000;
        player->serfCount = 2000;
        player->soldierCount = 20;
        player->nobleCount = 1;
        player->merchantCount = 25;
        player->armyEfficiency = 15;
        player->customsTax = 20;
        player->salesTax = 5;
        player->incomeTax = 35;
        player->marketplaceCount = 0;
        player->grainMillCount = 0;
        player->foundryCount = 0;
        player->shipyardCount = 0;
        player->palaceCount = 0;
        player->grainForSale = 0;
        player->grainPrice = 0.0;
    }
}


/*
 * Start a new year.
 */

static void NewYearScreen()
{
    /* Update year. */
    year++;

    /* Update weather. */
    weather = RandRange(ArraySize(weatherList));

    /* Reset screen. */
    clear();
    move(0, 0);

    /* Display the year. */
    printw("YEAR %d\n\n", year);

    /* Display the weather and delay based on message length. */
    ShowMessage("%s\n", weatherList[weather - 1]);
}


/*
 * Display a summary of all players.
 */

static void SummaryScreen()
{
    char     input[80];
    Player  *player;
    Country *country;
    int      i;

    /* Reset screen. */
    clear();
    move(0, 0);

    /* Display summary. */
    printw("SUMMARY\n");
    printw("NOBLES   SOLDIERS   MERCHANTS   SERFS   LAND    PALACE\n\n");
    for (i = 0; i < COUNTRY_COUNT; i++)
    {
        /* Get the country and player records. */
        country = &(countryList[i]);
        player = &(playerList[i]);

        /* Skip dead players. */
        if (player->dead)
            continue;

        /* Display player summary. */
        printw("%s %s OF %s\n", player->title, player->name, country->name);
        printw(" %3d       %5d       %5d    %6d   %5d  %3d%%\n",
               player->nobleCount,
               player->soldierCount,
               player->merchantCount,
               player->serfCount,
               player->land,
               10 * player->palaceCount);
    }

    /* Wait for player. */
    printw("<ENTER>? ");
    getnstr(input, sizeof(input));
}


/*
 * Play the human player specified by aPlayer.
 *
 *   aPlayer                Human player.
 */

static void PlayHuman(Player *aPlayer)
{
    int i;

    /* Show grain screen. */
    GrainScreen(aPlayer);

    /* Show population screen. */
    PopulationScreen(aPlayer);

    /* If all human players have died, end game. */
    for (i = 0; i < playerCount; i++)
    {
        if (!playerList[i].dead)
            break;
    }
    if (i == playerCount)
    {
        gameOver = true;
        return;
    }

    /* Show investments screen. */
    InvestmentsScreen(aPlayer);

    /* Show attack screen. */
    AttackScreen(aPlayer);
}


/*
 * Play the CPU player specified by aPlayer.
 *
 *   aPlayer                CPU player.
 */

static void PlayCPU(Player *aPlayer)
{
    /* Reset screen. */
    clear();
    move(0, 0);

    /* Announce player's turn. */
    ShowMessage("ONE MOMENT -- %s %s'S TURN . . .", aPlayer->title,
                aPlayer->name);

    /*
     * Run the CPU through the same economic pipeline as a human player:
     * grain, population, investments, then attack.  The CPU makes choices
     * through its difficulty strategy; the formulas and outcomes are
     * identical to the human path.
     */

    /* --- Grain phase (same as GrainScreen minus UI) --- */
    CPUGrainPhase(aPlayer);

    /* --- Population phase (same as PopulationScreen minus UI) --- */
    CPUPopulationPhase(aPlayer);

    /* --- Investments phase (same as InvestmentsScreen minus UI) --- */
    CPUInvestmentsPhase(aPlayer);

    /* --- Attack phase --- */
    CPUAttackScreen(aPlayer);
}


/*
 * Run the grain phase for a CPU player.  Same formulas as GrainScreen:
 * rats, harvest, feeding.  CPU feeds army and people at the required amount.
 *
 *   aPlayer                CPU player.
 */

static void CPUGrainPhase(Player *aPlayer)
{
    int usableLand;

    /* Rats eat grain (same formula as human). */
    aPlayer->ratPct = RandRange(30);
    aPlayer->grain -= (aPlayer->grain * aPlayer->ratPct) / 100;

    /* Determine usable land (same formula as human). */
    usableLand =   aPlayer->land
                 - aPlayer->serfCount
                 - (2 * aPlayer->nobleCount)
                 - aPlayer->palaceCount
                 - aPlayer->merchantCount
                 - (2 * aPlayer->soldierCount);
    if (usableLand > (3 * aPlayer->grain))
        usableLand = 3 * aPlayer->grain;
    if (usableLand > (5 * aPlayer->serfCount))
        usableLand = 5 * aPlayer->serfCount;

    /* Harvest (same formula as human). */
    aPlayer->grainHarvest =   (weather * usableLand * 0.72)
                            + RandRange(500)
                            - (aPlayer->foundryCount * 500);
    if (aPlayer->grainHarvest < 0)
        aPlayer->grainHarvest = 0;
    aPlayer->grain += aPlayer->grainHarvest;

    /* Compute grain needs (same formula as human). */
    aPlayer->peopleGrainNeed = 5 * (  aPlayer->serfCount
                                    + aPlayer->merchantCount
                                    + (3 * aPlayer->nobleCount));
    aPlayer->armyGrainNeed = 8 * aPlayer->soldierCount;

    /* CPU feeds army and people at the required amount. */
    aPlayer->armyGrainFeed = aPlayer->armyGrainNeed;
    if (aPlayer->armyGrainFeed > aPlayer->grain)
        aPlayer->armyGrainFeed = aPlayer->grain;
    aPlayer->grain -= aPlayer->armyGrainFeed;

    aPlayer->peopleGrainFeed = aPlayer->peopleGrainNeed;
    if (aPlayer->peopleGrainFeed > aPlayer->grain)
        aPlayer->peopleGrainFeed = aPlayer->grain;
    aPlayer->grain -= aPlayer->peopleGrainFeed;
}


/*
 * Run the population phase for a CPU player.  Same formulas as
 * PopulationScreen: births, deaths, immigration, army starvation/desertion,
 * army efficiency.
 *
 *   aPlayer                CPU player.
 */

static void CPUPopulationPhase(Player *aPlayer)
{
    int population;
    int born;
    int immigrated;
    int merchantsImmigrated;
    int noblesImmigrated;
    int diedDisease;
    int diedMalnutrition;
    int diedStarvation;
    int armyDiedStarvation;
    int armyDeserted;
    int populationGain;

    /* Total population. */
    population =   aPlayer->serfCount
                 + aPlayer->merchantCount
                 + aPlayer->nobleCount;

    /* Births (same formula). */
    born = RandRange((static_cast<int>(static_cast<float>(population) / 9.5)));

    /* Disease deaths (same formula). */
    diedDisease = RandRange(population / 22);

    /* Starvation and malnutrition (same formula). */
    diedStarvation = 0;
    diedMalnutrition = 0;
    if (aPlayer->peopleGrainNeed > (2 * aPlayer->peopleGrainFeed))
    {
        diedMalnutrition = RandRange(population/12 + 1);
        diedStarvation = RandRange(population/16 + 1);
    }
    else if (aPlayer->peopleGrainNeed > aPlayer->peopleGrainFeed)
    {
        diedMalnutrition = RandRange(population/15 + 1);
    }
    aPlayer->diedStarvation = diedStarvation;

    /* Immigration (same formula). */
    if (static_cast<float>(aPlayer->peopleGrainFeed) >
        (1.5 * static_cast<float>(aPlayer->peopleGrainNeed)))
    {
        immigrated =
              static_cast<int>(sqrt(aPlayer->peopleGrainFeed - aPlayer->peopleGrainNeed))
            - RandRange(static_cast<int>(1.5 * static_cast<float>(aPlayer->customsTax)));
        if (immigrated > 0)
            immigrated = RandRange(((2 * immigrated) + 1));
        else
            immigrated = 0;
    }
    else
    {
        immigrated = 0;
    }
    aPlayer->immigrated = immigrated;

    /* Merchant and noble immigration (same formula). */
    merchantsImmigrated = 0;
    noblesImmigrated = 0;
    if ((immigrated / 5) > 0)
        merchantsImmigrated = RandRange(immigrated / 5);
    if ((immigrated / 25) > 0)
        noblesImmigrated = RandRange(immigrated / 25);

    /* Army starvation and desertion (same formula). */
    armyDiedStarvation = 0;
    armyDeserted = 0;
    if (aPlayer->armyGrainNeed > (2 * aPlayer->armyGrainFeed))
    {
        armyDiedStarvation = RandRange(aPlayer->soldierCount/2 + 1);
        aPlayer->soldierCount -= armyDiedStarvation;
        armyDeserted = RandRange(aPlayer->soldierCount / 5);
        aPlayer->soldierCount -= armyDeserted;
    }

    /* Army efficiency (same formula). */
    aPlayer->armyEfficiency =   (10 * aPlayer->armyGrainFeed)
                              / (aPlayer->armyGrainNeed > 0
                                 ? aPlayer->armyGrainNeed : 1);
    if (aPlayer->armyEfficiency < 5)
        aPlayer->armyEfficiency = 5;
    else if (aPlayer->armyEfficiency > 15)
        aPlayer->armyEfficiency = 15;

    /* Update population (same formula). */
    populationGain =
        born + immigrated - diedDisease - diedMalnutrition - diedStarvation;
    aPlayer->serfCount +=
        populationGain - merchantsImmigrated - noblesImmigrated;
    aPlayer->merchantCount += merchantsImmigrated;
    aPlayer->nobleCount += noblesImmigrated;

    /* Check for death (same as human). */
    if ((aPlayer->diedStarvation > 0) &&
        (RandRange(aPlayer->diedStarvation) > RandRange(110)))
    {
        aPlayer->dead = true;
    }
    if (RandRange(100) == 1)
    {
        aPlayer->dead = true;
    }
}


/*
 * Run the investments phase for a CPU player.  Uses the same revenue
 * formulas as ComputeRevenues, then delegates purchase decisions to the
 * difficulty strategy.  Purchases are constrained by treasury and the same
 * validation rules as human players.
 *
 *   aPlayer                CPU player.
 */

static void CPUInvestmentsPhase(Player *aPlayer)
{
    /* Compute revenues using the same formula as human players. */
    ComputeRevenues(aPlayer);

    /* Set taxes via the strategy. */
    cpuStrategies[difficulty]->manageTaxes(aPlayer);

    /* Make investment purchases via the strategy. */
    cpuStrategies[difficulty]->manageInvestments(aPlayer);
}

