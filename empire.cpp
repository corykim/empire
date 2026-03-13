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
#include "platform.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Local includes. */
#include "empire.h"
#include "economy.h"
#include "diplomacy.h"
#include "cpu_strategy.h"
#include "grain.h"
#include "population.h"
#include "ui.h"


/*------------------------------------------------------------------------------
 *
 * Prototypes.
 */

static void StartScreen();

static void GameSetupScreen();

static void NewYearScreen();

static void SummaryScreen();

static void LogRoundSummary();

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
    /* Auveyron. */
    {
        .name = "Auveyron",
        .rulerName = "Montaigne",
        .currency = "Francs",
        .titleList = { "Chevalier", "Prince", "Roi", "Empereur", },
    },

    /* Brittany. */
    {
        .name = "Brittany",
        .rulerName = "Arthur",
        .currency = "Francs",
        .titleList = { "Sir", "Prince", "King", "Emperor", },
    },

    /* Bavaria. */
    {
        .name = "Bavaria",
        .rulerName = "Munster",
        .currency = "Marks",
        .titleList = { "Ritter", "Prinz", "Konig", "Kaiser", },
    },

    /* Quatara. */
    {
        .name = "Quatara",
        .rulerName = "Khotan",
        .currency = "Dinars",
        .titleList = { "Hasid", "Caliph", "Sheik", "Shah", },
    },

    /* Barcelona. */
    {
        .name = "Barcelona",
        .rulerName = "Ferdinand",
        .currency = "Peseta",
        .titleList = { "Caballero", "Principe", "Rey", "Emperadore", },
    },

    /* Svealand. */
    {
        .name = "Svealand",
        .rulerName = "Hjodolf",
        .currency = "Krona",
        .titleList = { "Riddare", "Prins", "Kung", "Kejsare", },
    },
};


/*
 * Weather list.
 */

const char *weatherList[] =
{
    "Poor weather. No rain. Locusts migrate.",
    "Early frosts. Arid conditions.",
    "Flash floods. Too much rain.",
    "Average weather. Good year.",
    "Fine weather. Long summer.",
    "Fantastic weather! Great year!",
};


/*
 * Difficulty level names.
 */

const char *difficultyList[DIFFICULTY_COUNT] =
{
    "Village Fool",
    "Landed Peasant",
    "Minor Noble",
    "Royal Advisor",
    "Machiavelli",
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
int treatyYears = 3;
/* Seeded with estimated year-1 market: ~5 CPUs listing ~10k each. */
int marketGrainHistory[3] = {50000, 50000, 50000};
bool omniscient = false;
bool fastMode = false;


/*------------------------------------------------------------------------------
 *
 * Main entry point.
 */

int main(int argc, char *argv[])
{
    Player *player;
    int     i, j;

    /* Parse command-line arguments. */
    for (int a = 1; a < argc; a++)
    {
        if (strcmp(argv[a], "--omniscient") == 0 ||
            strcmp(argv[a], "-o") == 0)
        {
            omniscient = true;
        }
        if (strcmp(argv[a], "--log") == 0 ||
            strcmp(argv[a], "-l") == 0)
        {
            logging = true;
            /* Optional filename argument: -l <filename> */
            if (a + 1 < argc && argv[a + 1][0] != '-')
            {
                logFileName = argv[a + 1];
                a++;
            }
        }
        if (strcmp(argv[a], "--fast") == 0 ||
            strcmp(argv[a], "-f") == 0)
        {
            fastMode = true;
        }
    }

    /* Initialize the screen, colors, and terminal size. */
    UIInit();

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

        /* Year header in log (before turns). */
        GameLog("\n################################################################"
                "################\n");
        GameLog("###  YEAR %d  —  Weather: %d/6 (%s)  ###\n",
                year, weather, weatherList[weather - 1]);
        GameLog("################################################################"
                "################\n");
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

            /* Record soldier count for weakness detection next turn. */
            player->soldiersPrevTurn = player->soldierCount;

            /* Decay diplomacy scores toward zero at the start of each turn. */
            DecayDiplomacy(player);

            /* Play as human or CPU. */
            if (player->human)
                PlayHuman(player);
            else
                PlayCPU(player);

            /* Update diplomacy based on whether this player attacked. */
            UpdateDiplomacyAfterTurn(player);

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
                    UITitle("Victory");
                    printw("\n");
                    UIColor(UIC_GOOD);
                    printw("All rival nations have fallen!\n\n");
                    printw("%s %s of %s has conquered the known world!\n\n",
                           lastHuman->title,
                           lastHuman->name,
                           lastHuman->country->name);
                    printw("Long live %s %s!\n",
                           lastHuman->title,
                           lastHuman->name);
                    UIColorOff();
                    UISeparator();
                    flushinp();
    printw("<Enter>? ");
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

        /* Track market grain inventory for CPU pricing trends (3-year rolling). */
        {
            marketGrainHistory[2] = marketGrainHistory[1];
            marketGrainHistory[1] = marketGrainHistory[0];
            marketGrainHistory[0] = 0;
            for (i = 0; i < COUNTRY_COUNT; i++)
                marketGrainHistory[0] += playerList[i].grainForSale;
        }

        /* Log end-of-round stats and diplomacy. */
        LogRoundSummary();

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
 * Format an integer with thousands separators.  Returns a pointer to a
 * static buffer (not thread-safe, but fine for single-threaded ncurses).
 * Cycles through 4 buffers so multiple calls can be used in one printw().
 *
 *   value                  Integer to format.
 */

const char *FmtNum(int value)
{
    static char bufs[4][32];
    static int  idx = 0;
    char *buf = bufs[idx++ & 3];
    char tmp[32];
    int  len, pos, i, neg;

    neg = (value < 0);
    if (neg) value = -value;

    snprintf(tmp, sizeof(tmp), "%d", value);
    len = static_cast<int>(strlen(tmp));

    pos = 0;
    if (neg) buf[pos++] = '-';
    for (i = 0; i < len; i++)
    {
        if (i > 0 && (len - i) % 3 == 0)
            buf[pos++] = ',';
        buf[pos++] = tmp[i];
    }
    buf[pos] = '\0';
    return buf;
}


/*
 * Parse a number string that may contain commas as thousands separators.
 * Returns the parsed integer value.
 *
 *   str                    String to parse.
 */

int ParseNum(const char *str)
{
    char cleaned[80];
    int  pos = 0;

    for (int i = 0; str[i] && pos < 79; i++)
    {
        if (str[i] != ',')
            cleaned[pos++] = str[i];
    }
    cleaned[pos] = '\0';
    return static_cast<int>(strtol(cleaned, nullptr, 0));
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

    /* Delay 200ms per word, minimum DELAY_TIME.  Skipped in fast mode. */
    if (!fastMode)
    {
        delayUs = words * 200000;
        if (delayUs < DELAY_TIME)
            delayUs = DELAY_TIME;
        SleepUs(delayUs);
    }
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
    /* Update terminal size and clear screen. */
    getmaxyx(stdscr, winrows, wincols);
    clear();

    /* Display splash screen centered on the terminal. */
    UIColor(UIC_TITLE);
    mvprintw(winrows / 3, (wincols - 11) / 2, "E M P I R E");
    UIColorOff();
    mvprintw(winrows / 2, (wincols - 32) / 2, "(Always hit <Enter> to continue)");
    refresh();

    /* Delay. */
    if (!fastMode)
        SleepUs(DELAY_TIME);
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
        printw("How many people are playing? ");
        refresh();
        getnstr(input, sizeof(input));
        playerCount = ParseNum(input);
    } while ((playerCount < 1) || (playerCount > COUNTRY_COUNT));

    /* Get the difficulty level. */
    printw("\n\nChoose the cunning of your rivals:\n\n");
    for (i = 0; i < DIFFICULTY_COUNT; i++)
    {
        printw("  %d. %s\n", i + 1, difficultyList[i]);
    }
    printw("\n");
    do
    {
        printw("Difficulty (1-%d)? ", DIFFICULTY_COUNT);
        refresh();
        getnstr(input, sizeof(input));
        difficulty = ParseNum(input) - 1;
    } while ((difficulty < 0) || (difficulty >= DIFFICULTY_COUNT));

    /* Treaty period scales inversely with difficulty:
     * Level 1 → 5, Level 2 → 4, Level 3 → 3, Level 4 → 2, Level 5 → 1. */
    treatyYears = 5 - difficulty;

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
        printw("Who is the ruler of %s? ", country->name);
        getnstr(player->name, sizeof(player->name));
        for (j = 0; player->name[j]; j++)
        {
            if (j == 0 || player->name[j - 1] == ' ')
                player->name[j] = toupper(player->name[j]);
            else
                player->name[j] = tolower(player->name[j]);
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
        player->land = STARTING_LAND;
        player->grain = STARTING_GRAIN_BASE + RandRange(STARTING_GRAIN_RAND);
        player->treasury = STARTING_TREASURY;
        player->serfCount = STARTING_SERFS;
        player->soldierCount = STARTING_SOLDIERS;
        player->nobleCount = STARTING_NOBLES;
        player->merchantCount = STARTING_MERCHANTS;
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
        player->attackedTargets = 0;
        player->soldiersPrevTurn = 0;
        player->yearsSinceImmigration = 0;
        for (int j = 0; j < COUNTRY_COUNT; j++)
            player->landTakenFrom[j] = 0;

        /* Per-CPU difficulty: base ± 0.5, clamped to [0, 4]. */
        if (!player->human)
        {
            float offset = (static_cast<float>(RandRange(101)) - 51.0f) / 100.0f;
            player->cpuDifficulty = static_cast<float>(difficulty) + offset;
            if (player->cpuDifficulty < 0.0f) player->cpuDifficulty = 0.0f;
            if (player->cpuDifficulty > 4.0f) player->cpuDifficulty = 4.0f;
            player->strategyIndex = static_cast<int>(player->cpuDifficulty + 0.5f);
            if (player->strategyIndex > 4) player->strategyIndex = 4;
            /* Random opening capital allocation: three weighted random
             * values normalized to sum to 100%.  Higher difficulty biases
             * strongly toward mills (the strongest economic strategy).
             *   diff=0: all equal weights (uniform random)
             *   diff=4: mill draw is 3.5× base (strong mill preference,
             *           ~28% chance of non-mill opening for diversity) */
            float millBias = 1.0f + 0.625f * player->cpuDifficulty;
            int r1 = RandRange(100);
            int r2 = static_cast<int>(RandRange(100) * millBias);
            int r3 = RandRange(100);
            int rsum = r1 + r2 + r3;
            player->openMarketPct = r1 * 100 / rsum;
            player->openMillPct = r2 * 100 / rsum;
            player->openMilitaryPct = 100 - player->openMarketPct
                                          - player->openMillPct;

            GameLog("  %s: difficulty=%.2f strategy=%d (%s) "
                    "opening=mkt%d/mill%d/mil%d\n",
                    player->country->name, player->cpuDifficulty,
                    player->strategyIndex,
                    cpuStrategies[player->strategyIndex]->name,
                    player->openMarketPct, player->openMillPct,
                    player->openMilitaryPct);
        }
        else
        {
            player->cpuDifficulty = 0.0f;
            player->strategyIndex = 0;
            player->openMarketPct = 0;
            player->openMillPct = 0;
            player->openMilitaryPct = 0;
        }
    }

    /* Initialize diplomacy scores between all players. */
    InitDiplomacy();
}


/*
 * Start a new year.
 */

static void NewYearScreen()
{
    /* Update year. */
    year++;

    /* Update weather — mild regression toward the mean (3.5).
     * Only the most extreme weather (1 or 6) nudges the next roll,
     * so bad/good streaks can persist longer.  This makes grain mills
     * more valuable as insurance against sustained poor harvests. */
    int rawWeather = RandRange(ArraySize(weatherList));
    if (weather > 0) {
        if (weather == 1) rawWeather = MIN(6, rawWeather + 1);
        else if (weather == 6) rawWeather = MAX(1, rawWeather - 1);
    }
    weather = rawWeather;

    /* Display the year and weather. */
    char yearTitle[32];
    snprintf(yearTitle, sizeof(yearTitle), "Year %d", year);
    UITitle(yearTitle);
    printw("\n");
    ShowMessage("%s\n", weatherList[weather - 1]);
}


/*
 * Display a summary of all players.
 */

/*
 * Sort player indices by power score descending.  Returns count.
 * Includes all players (dead players sort to the bottom with power 0).
 */

static int SortPlayersByPower(int *indices)
{
    int count = 0;
    for (int i = 0; i < COUNTRY_COUNT; i++)
        indices[count++] = i;
    for (int a = 0; a < count - 1; a++)
        for (int b = a + 1; b < count; b++)
        {
            float pa = playerList[indices[a]].dead ? 0 :
                       ComputePlayerPower(&playerList[indices[a]]);
            float pb = playerList[indices[b]].dead ? 0 :
                       ComputePlayerPower(&playerList[indices[b]]);
            if (pb > pa)
            {
                int tmp = indices[a];
                indices[a] = indices[b];
                indices[b] = tmp;
            }
        }
    return count;
}


static void LogRoundSummary()
{
    int logOrder[COUNTRY_COUNT];
    int logCount = SortPlayersByPower(logOrder);

    /* Stats table. */
    GameLog("\n\n--- End of Round Stats ---\n");
    GameLog("%-14s %6s %6s %7s %6s %5s %4s %5s  %3s %3s %3s %3s %3s  %3s/%3s/%3s  %6s\n",
            "Country", "Land", "Grain", "Gold", "Serfs", "Merch",
            "Nob", "Army", "Mkt", "Mil", "Fnd", "Shp", "Pal",
            "Cus", "Sal", "Inc", "Power");
    GameLog("%-14s %6s %6s %7s %6s %5s %4s %5s  %3s %3s %3s %3s %3s  %3s/%3s/%3s  %6s\n",
            "--------------", "------", "------", "-------",
            "------", "-----", "----", "-----",
            "---", "---", "---", "---", "---",
            "---", "---", "---", "------");
    for (int s = 0; s < logCount; s++)
    {
        Player *p = &playerList[logOrder[s]];
        if (p->dead)
        {
            GameLog("%-14s  -- DEAD --\n", p->country->name);
            continue;
        }
        GameLog("%-13s%s %6d %6d %7d %6d %5d %4d %5d  %3d %3d %3d %3d %3d  %3d/%3d/%3d  %6.0f\n",
                p->country->name, p->human ? "*" : " ",
                p->land, p->grain, p->treasury,
                p->serfCount, p->merchantCount, p->nobleCount,
                p->soldierCount,
                p->marketplaceCount, p->grainMillCount,
                p->foundryCount, p->shipyardCount, p->palaceCount,
                p->customsTax, p->salesTax, p->incomeTax,
                ComputePlayerPower(p));
    }
    GameLog("\n");

    /* Diplomacy table (columns sorted by power). */
    GameLog("Diplomacy:       ");
    for (int s = 0; s < logCount; s++)
    {
        if (!playerList[logOrder[s]].dead)
            GameLog(" %10s", playerList[logOrder[s]].country->name);
    }
    GameLog("\n");
    for (int sr = 0; sr < logCount; sr++)
    {
        int ri = logOrder[sr];
        if (playerList[ri].dead || playerList[ri].human)
            continue;
        GameLog("%-14s ", playerList[ri].country->name);
        for (int sc = 0; sc < logCount; sc++)
        {
            int ci = logOrder[sc];
            if (playerList[ci].dead)
                continue;
            if (ri == ci)
                GameLog(" %10s", "---");
            else
                GameLog(" %+10.3f", playerList[ri].diplomacy[ci]);
        }
        GameLog("\n");
    }
    GameLog("\n");
}


static void SummaryScreen()
{
    char    input[80];
    Player *player;
    int     i;

    UITitle("Summary");

    /* Sort players by power score descending. */
    int sorted[COUNTRY_COUNT];
    int livingCount = SortPlayersByPower(sorted);

    /* Draw each living player — sorted, compact, two lines each. */
    for (int s = 0; s < livingCount; s++)
    {
        player = &playerList[sorted[s]];
        if (player->dead)
            continue;

        /* Line 1: player identity + land. */
        UIColor(UIC_HEADING);
        printw(" %s %s of %s", player->title, player->name,
               player->country->name);
        UIColorOff();
        printw("  %s acres\n", FmtNum(player->land));

        /* Line 2: stats. */
        printw("   %4s nobles", FmtNum(player->nobleCount));
        printw("  %5s soldiers", FmtNum(player->soldierCount));
        printw("  %5s merchants", FmtNum(player->merchantCount));
        printw("  %5s serfs", FmtNum(player->serfCount));
        printw("  %3d%% palace\n", 10 * player->palaceCount);
    }

    /* Wait for player. */
    UISeparator();
    flushinp();
    printw("<Enter>? ");
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

    GameLog("\n================================================================"
            "================\n");
    GameLog("Year %d -- %s (%s %s) [HUMAN]\n", year, aPlayer->country->name,
            aPlayer->title, aPlayer->name);
    GameLog("================================================================"
            "================\n");
    LogAllDiplomacy("Diplomacy");

    /* Show grain screen. */
    GameLog("--- Grain Phase ---\n");
    GrainScreen(aPlayer);

    /* Show population screen. */
    GameLog("--- Population Phase ---\n");
    PopulationScreen(aPlayer);

    /* If this player died, end their turn. */
    if (aPlayer->dead)
        return;

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
    GameLog("--- Investments Phase ---\n");
    InvestmentsScreen(aPlayer);

    /* Show attack screen. */
    GameLog("--- Attack Phase ---\n");
    AttackScreen(aPlayer);
}


/*
 * Play the CPU player specified by aPlayer.
 *
 *   aPlayer                CPU player.
 */

static void OmniscientScreen(Player *aPlayer);

static void PlayCPU(Player *aPlayer)
{
    /* Reset screen. */
    clear();
    move(0, 0);

    /* Announce player's turn. */
    GameLog("\n================================================================"
            "================\n");
    GameLog("Year %d -- %s (%s %s) [CPU]\n", year, aPlayer->country->name,
            aPlayer->title, aPlayer->name);
    GameLog("================================================================"
            "================\n");
    LogAllDiplomacy("Diplomacy");
    printw("\n\n");
    ShowMessage("One moment -- %s %s's turn . . .", aPlayer->title,
                aPlayer->name);
    printw("\n");

    /*
     * Run the CPU through the same economic pipeline as a human player:
     * grain, population, investments, then attack.  The CPU makes choices
     * through its difficulty strategy; the formulas and outcomes are
     * identical to the human path.
     */

    GameLog("--- Grain Phase ---\n");
    CPUGrainPhase(aPlayer);

    GameLog("--- Population Phase ---\n");
    CPUPopulationPhase(aPlayer);
    if (aPlayer->dead)
        return;

    GameLog("--- Military Planning ---\n");
    ComputeDesiredTroopStrength(aPlayer);

    GameLog("--- Investments Phase ---\n");
    CPUInvestmentsPhase(aPlayer);

    GameLog("--- Attack Phase ---\n");
    CPUAttackScreen(aPlayer);

    /* Show full economy in omniscient mode. */
    if (omniscient && !aPlayer->dead)
        OmniscientScreen(aPlayer);
}


/*
 * Run the grain phase for a CPU player.  Same formulas as GrainScreen:
 * rats, harvest, feeding.  CPU feeds army and people at the required amount.
 *
 *   aPlayer                CPU player.
 */

static void CPUGrainPhase(Player *aPlayer)
{
    /* Shared grain computation (rats, harvest, needs). */
    ComputeGrainPhase(aPlayer);

    /* Compute a safe grain reserve: enough to survive a worst-case year. */
    int totalNeed = aPlayer->armyGrainNeed + aPlayer->peopleGrainNeed;
    int worstHarvest = ComputeWorstCaseHarvest(aPlayer);
    int safeReserve = ComputeSafeGrainReserve(aPlayer);

    /* Discount reserve if cheap grain is on the market. */
    int marketGrain = 0;
    float cheapestPrice = GRAIN_PRICE_MAX + 1.0f;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        Player *p = &playerList[i];
        if (p != aPlayer && !p->dead && p->grainForSale > 0)
        {
            marketGrain += p->grainForSale;
            if (p->grainPrice < cheapestPrice)
                cheapestPrice = p->grainPrice;
        }
    }
    if (marketGrain > 0 && cheapestPrice <= GRAIN_PRICE_MAX)
    {
        int canBuy = static_cast<int>(
            (aPlayer->treasury * CPU_GRAIN_TREASURY_PCT * (1.0f - GRAIN_MARKUP)) / cheapestPrice);
        if (canBuy > marketGrain) canBuy = marketGrain;
        safeReserve -= canBuy;
        if (safeReserve < totalNeed) safeReserve = totalNeed;
    }

    GameLog("  Safe grain reserve: %d (totalNeed=%d, worstHarvest=%d, "
            "marketDiscount=%d)\n",
            safeReserve, totalNeed, worstHarvest,
            marketGrain > 0 ? static_cast<int>(
                (aPlayer->treasury * CPU_GRAIN_TREASURY_PCT * (1.0f - GRAIN_MARKUP))
                / cheapestPrice) : 0);

    /* List surplus grain for sale BEFORE feeding, but only when:
     * 1. Market price is attractive (above base price)
     * 2. After listing, remaining grain covers BOTH this year's
     *    overfeed (immigration threshold) AND a prudent reserve for
     *    next year (totalNeed, enough to survive a bad year).
     * This mirrors the human's grain management: sell high, keep reserves. */
    {
        int effErr = ComputeErrorPct(aPlayer->cpuDifficulty);
        float price = cpuStrategies[aPlayer->strategyIndex]
                      ->ComputeGrainTargetPrice(aPlayer, effErr);

        if (price >= GRAIN_PRICE_BASE)
        {
            int feedBudget = static_cast<int>(
                aPlayer->peopleGrainNeed * IMMIGRATION_FEED_MULT) + 1
                + aPlayer->armyGrainNeed;
            int reserveBudget = totalNeed;  /* full year's need as reserve */
            int mustKeep = feedBudget + reserveBudget;
            int surplus = aPlayer->grain - mustKeep;

            if (surplus > CPU_MIN_GRAIN_SURPLUS)
            {
                int sellAmount = surplus * (100 - effErr) / 100;
                if (sellAmount > CPU_MIN_GRAIN_SURPLUS / 5)
                {
                    ListGrainForSale(aPlayer, sellAmount, price);
                    GameLog("  Listed %d grain at %.2f (surplus %d, "
                            "keeping %d for feed+reserve)\n",
                            sellAmount, price, surplus, mustKeep);
                }
            }
        }
    }

    /* Buy/withdraw/emergency grain trading. */
    cpuStrategies[aPlayer->strategyIndex]->manageGrainTrade(aPlayer);

    /* Buy grain to enable immigration — but only at reasonable prices.
     * Don't enrich the dominant player by buying at 5.0/bushel. */
    int immigrationTarget = static_cast<int>(aPlayer->peopleGrainNeed * IMMIGRATION_FEED_MULT)
                            + aPlayer->armyGrainNeed;
    if (aPlayer->grain < immigrationTarget && aPlayer->treasury > COST_SOLDIER)
    {
        int deficit = immigrationTarget - aPlayer->grain;
        Player *cheapest = nullptr;
        for (int j = 0; j < COUNTRY_COUNT; j++) {
            Player *p = &playerList[j];
            if (p != aPlayer && !p->dead && p->grainForSale > 0
                && p->grainPrice <= GRAIN_PRICE_BASE * CPU_GRAIN_MAX_BUY_MULT)
                if (!cheapest || p->grainPrice < cheapest->grainPrice)
                    cheapest = p;
        }
        if (cheapest) {
            int canAfford = static_cast<int>((aPlayer->treasury * CPU_GRAIN_TREASURY_PCT * (1.0f - GRAIN_MARKUP)) / cheapest->grainPrice);
            int toBuy = MIN(deficit, MIN(canAfford, cheapest->grainForSale));
            if (toBuy > 0) {
                TradeGrain(aPlayer, cheapest, toBuy);
                GameLog("  Bought %d grain for immigration at %.2f (treasury: %d)\n",
                        toBuy, cheapest->grainPrice, aPlayer->treasury);
            }
        }
    }

    /* CPU feeds army.  Higher difficulties overfeed for army efficiency,
     * scaling from 100% at difficulty 2 to 150% at difficulty 4.
     * The 50% range maps to (IMMIGRATION_FEED_MULT - 1.0) × 100. */
    int armyOverfeedPct = 100 + static_cast<int>(
        ((IMMIGRATION_FEED_MULT - 1.0f) * 100.0f / 2.0f)
        * MAX(0.0f, aPlayer->cpuDifficulty - 2.0f));
    int armyFeedTarget = aPlayer->armyGrainNeed;
    if (armyOverfeedPct > 100)
    {
        int overfedArmy = (aPlayer->armyGrainNeed * armyOverfeedPct + 99) / 100;
        if (aPlayer->grain - overfedArmy >= safeReserve + aPlayer->peopleGrainNeed)
            armyFeedTarget = overfedArmy;
    }
    aPlayer->armyGrainFeed = MIN(armyFeedTarget, aPlayer->grain);
    aPlayer->grain -= aPlayer->armyGrainFeed;

    /*
     * CPU feeds people above the required amount to attract immigrants.
     *
     * Three constraints:
     * 1. Sustainability: after feeding, remaining grain (with 10% rats)
     *    plus WORST-CASE harvest must cover next year's feeding.
     * 2. Prudence: overfeed above 150% only when grain remaining after
     *    feeding >= 100% of annual need.  No hard cap — just smart.
     * 3. Floor: always feed at least 100% of need.
     */
    int optimalOverfeed = CPU_OVERFEED_PCT;
    int errorRange = ComputeErrorPct(aPlayer->cpuDifficulty);
    int overfeedPct = optimalOverfeed + RandRange(2 * errorRange + 1)
                      - errorRange - 1;
    if (overfeedPct < 100)
        overfeedPct = 100;
    int desiredFeed = aPlayer->peopleGrainNeed * overfeedPct / 100;

    /* Sustainability clamp using worst-case harvest (weather=1).
     * Formula: (grain × (1 - ratLoss) + worstHarvest - armyNeed)
     *          / (1 + (1 - ratLoss))
     * Derived from: remaining × (1 - rats) + harvest >= feed + armyNeed */
    int worstHarvestEst = ComputeWorstCaseHarvest(aPlayer);
    float ratSurvival = 1.0f - CPU_EXPECTED_RAT_LOSS;
    int sustainableFeed = static_cast<int>(
        (aPlayer->grain * ratSurvival
         + static_cast<float>(worstHarvestEst)
         - static_cast<float>(aPlayer->armyGrainNeed))
        / (1.0f + ratSurvival));
    if (sustainableFeed < aPlayer->peopleGrainNeed)
        sustainableFeed = aPlayer->peopleGrainNeed;

    if (desiredFeed > sustainableFeed)
    {
        GameLog("  Sustainability clamp: %d → %d (grain=%d worstHarvest=%d)\n",
                desiredFeed, sustainableFeed, aPlayer->grain,
                worstHarvestEst);
        desiredFeed = sustainableFeed;
    }

    /* Prudence: only overfeed above the immigration threshold when
     * grain after feeding still covers a full year's need (totalNeed).
     * immigrationThreshold = peopleNeed × IMMIGRATION_FEED_MULT.
     * This mirrors the human strategy of keeping annual reserves intact. */
    int immigrationFeed = static_cast<int>(
        aPlayer->peopleGrainNeed * IMMIGRATION_FEED_MULT) + 1;
    if (desiredFeed > immigrationFeed)
    {
        int grainAfter = aPlayer->grain - desiredFeed;
        if (grainAfter < totalNeed)
        {
            desiredFeed = MAX(immigrationFeed,
                              aPlayer->grain - totalNeed);
            if (desiredFeed < aPlayer->peopleGrainNeed)
                desiredFeed = aPlayer->peopleGrainNeed;
            GameLog("  Prudence clamp: capped at %d (reserve %d < need %d)\n",
                    desiredFeed, aPlayer->grain - desiredFeed, totalNeed);
        }
    }

    aPlayer->peopleGrainFeed = MIN(desiredFeed, aPlayer->grain);
    aPlayer->grain -= aPlayer->peopleGrainFeed;

    GameLog("  Feed army: %d (need %d)  Feed people: %d (need %d, %d%%)\n",
            aPlayer->armyGrainFeed, aPlayer->armyGrainNeed,
            aPlayer->peopleGrainFeed, aPlayer->peopleGrainNeed,
            aPlayer->peopleGrainNeed > 0
                ? aPlayer->peopleGrainFeed * 100 / aPlayer->peopleGrainNeed
                : 100);
    GameLog("  Grain remaining: %d (safe reserve: %d)\n",
            aPlayer->grain, safeReserve);

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
    PopulationResult result = ComputePopulation(aPlayer);
    ApplyPopulationChanges(aPlayer, result);

    /* Track immigration for overfeeding decisions. */
    if (result.immigrated > 0)
        aPlayer->yearsSinceImmigration = 0;
    else
        aPlayer->yearsSinceImmigration++;

    DeathCause cause = CheckPlayerDeath(aPlayer);

    /* Notify the human player when a CPU ruler dies. */
    if (cause != DEATH_NONE)
    {
        char input[80];
        UITitle("News");
        printw("\n");
        UIColor(UIC_BAD);
        if (cause == DEATH_STARVATION)
        {
            printw("%s %s of %s has been assassinated\n",
                   aPlayer->title, aPlayer->name,
                   aPlayer->country->name);
            printw("by a crazed mother whose child had starved "
                   "to death.\n");
        }
        else
        {
            printw("%s %s of %s ", aPlayer->title, aPlayer->name,
                   aPlayer->country->name);
            switch (RandRange(4))
            {
                case 1:
                    printw("has been assassinated by an ambitious "
                           "noble.\n");
                    break;
                case 2:
                    printw("has been killed in a fall during the "
                           "annual fox-hunt.\n");
                    break;
                case 3:
                    printw("died of acute food poisoning.\n"
                           "The royal cook was summarily executed.\n");
                    break;
                case 4:
                default:
                    printw("passed away this winter from a weak "
                           "heart.\n");
                    break;
            }
        }
        UIColorOff();

        /* Dead ruler's land reverts to barbarian control. */
        printw("\nThe lands of %s fall into disarray and revert to "
               "barbarian control.\n", aPlayer->country->name);
        GameLog("  %s land (%d acres) reverts to barbarians\n",
                aPlayer->country->name, aPlayer->land);
        barbarianLand += aPlayer->land;
        aPlayer->land = 0;
        aPlayer->grainForSale = 0;
        aPlayer->grainPrice = 0.0;

        printw("The other nation-states have sent representatives "
               "to the funeral.\n");
        UISeparator();
        flushinp();
        printw("<Enter>? ");
        getnstr(input, sizeof(input));
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
    cpuStrategies[aPlayer->strategyIndex]->manageTaxes(aPlayer);
    GameLog("  Set taxes: Customs %d%%, Sales %d%%, Income %d%%\n",
            aPlayer->customsTax, aPlayer->salesTax, aPlayer->incomeTax);

    /* Make investment purchases via the strategy. */
    cpuStrategies[aPlayer->strategyIndex]->manageInvestments(aPlayer);
}


/*
 * Display the full economy screen for a CPU player in omniscient mode.
 * Shows the same information a human player sees on their investments screen.
 *
 *   aPlayer                CPU player.
 */

static void OmniscientScreen(Player *aPlayer)
{
    char input[80];
    char rulerLine[160];

    snprintf(rulerLine, sizeof(rulerLine), "%s %s of %s",
             aPlayer->title, aPlayer->name, aPlayer->country->name);
    UITitle("Intelligence", rulerLine);

    /* Economy overview. */
    printw("Land: %s   Grain: %s   Treasury: %s %s\n",
           FmtNum(aPlayer->land), FmtNum(aPlayer->grain),
           FmtNum(aPlayer->treasury), aPlayer->country->currency);
    printw("Serfs: %s  Merchants: %s  Nobles: %s  Soldiers: %s\n",
           FmtNum(aPlayer->serfCount), FmtNum(aPlayer->merchantCount),
           FmtNum(aPlayer->nobleCount), FmtNum(aPlayer->soldierCount));
    UISeparator();

    /* Tax rates and revenues. */
    printw("Customs %2d%%  %7s | Sales %2d%%  %7s | Income %2d%%  %7s\n",
           aPlayer->customsTax, FmtNum(aPlayer->customsTaxRevenue),
           aPlayer->salesTax, FmtNum(aPlayer->salesTaxRevenue),
           aPlayer->incomeTax, FmtNum(aPlayer->incomeTaxRevenue));
    UISeparator();

    /* Investment table. */
    printw("                 Count    Revenue     Cost\n");
    printw("Marketplaces     %5s    %7s     1,000\n",
           FmtNum(aPlayer->marketplaceCount),
           FmtNum(aPlayer->marketplaceRevenue));
    printw("Grain Mills      %5s    %7s     2,000\n",
           FmtNum(aPlayer->grainMillCount),
           FmtNum(aPlayer->grainMillRevenue));
    printw("Foundries        %5s    %7s     7,000\n",
           FmtNum(aPlayer->foundryCount),
           FmtNum(aPlayer->foundryRevenue));
    printw("Shipyards        %5s    %7s     8,000\n",
           FmtNum(aPlayer->shipyardCount),
           FmtNum(aPlayer->shipyardRevenue));
    printw("Soldiers         %5s    %7s         8\n",
           FmtNum(aPlayer->soldierCount),
           FmtNum(aPlayer->soldierRevenue));
    printw("Palace           %5d%%              5,000\n",
           10 * aPlayer->palaceCount);
    UISeparator();

    flushinp();
    printw("<Enter>? ");
    getnstr(input, sizeof(input));
}

