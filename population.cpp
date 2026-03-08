/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * TRS-80 Empire game population screen source file.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 *
 * Includes.
 */

/* System includes. */
#include <math.h>
#include <ncurses.h>
#include <unistd.h>

/* Local includes. */
#include "empire.h"
#include "economy.h"
#include "ui.h"


/*------------------------------------------------------------------------------
 *
 * Prototypes.
 */

static void PlayerDeathUI(Player *aPlayer, DeathCause cause);


/*------------------------------------------------------------------------------
 *
 * External population screen functions.
 */

/*
 * Manage population for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

void PopulationScreen(Player *aPlayer)
{
    char     input[80];
    Country *country;

    /* Get the player country. */
    country = aPlayer->country;

    /* Title. */
    char rulerLine[160];
    snprintf(rulerLine, sizeof(rulerLine), "%s %s of %s",
             aPlayer->title, aPlayer->name, country->name);
    UITitle("Population", rulerLine);
    printw("In year %d:\n", year);

    /* Compute population changes using shared formula. */
    PopulationResult result = ComputePopulation(aPlayer);

    /* Display the number of babies born. */
    UIColor(UIC_GOOD);
    printw(" %s babies were born\n", FmtNum(result.born));
    UIColorOff();

    /* Display the number of people who died of disease. */
    UIColor(UIC_BAD);
    printw(" %s people died of disease\n", FmtNum(result.diedDisease));
    UIColorOff();

    /* Display the number of people who immigrated. */
    if (result.immigrated > 0)
    {
        UIColor(UIC_GOOD);
        printw(" %s people immigrated into your country.\n",
               FmtNum(result.immigrated));
        if (result.merchantsImmigrated > 0)
            printw("   Including %s merchants.\n",
                   FmtNum(result.merchantsImmigrated));
        if (result.noblesImmigrated > 0)
            printw("   Including %s nobles.\n",
                   FmtNum(result.noblesImmigrated));
        UIColorOff();
    }

    /* Display the number of people who died of starvation and malnutrition. */
    UIColor(UIC_BAD);
    if (result.diedMalnutrition > 0)
        printw(" %s people died of malnutrition.\n",
               FmtNum(result.diedMalnutrition));
    if (result.diedStarvation > 0)
        printw(" %s people starved to death.\n",
               FmtNum(result.diedStarvation));

    /* Display the number of soldiers who starved to death. */
    if (result.armyDiedStarvation > 0)
        printw(" %s soldiers starved to death.\n",
               FmtNum(result.armyDiedStarvation));
    UIColorOff();

    UISeparator();

    /* Display the army efficiency. */
    printw("Your army will fight at %d%% efficiency.\n",
           10 * aPlayer->armyEfficiency);

    /* Display the population gain or loss. */
    if (result.populationGain >= 0)
    {
        UIColor(UIC_GOOD);
        printw("Your population gained %s citizens.\n",
               FmtNum(result.populationGain));
        UIColorOff();
    }
    else
    {
        UIColor(UIC_BAD);
        printw("Your population lost %s citizens.\n",
               FmtNum(-result.populationGain));
        UIColorOff();
    }

    /* Apply population changes using shared formula. */
    ApplyPopulationChanges(aPlayer, result);

    UISeparator();
    printw("<Enter>? ");
    getnstr(input, 80);

    /* Check if player died using shared formula, then show UI if so. */
    DeathCause cause = CheckPlayerDeath(aPlayer);
    if (cause != DEATH_NONE)
        PlayerDeathUI(aPlayer, cause);
}


/*
 * Display the death and funeral UI for the player specified by aPlayer.
 *
 *   aPlayer                Player.
 *   cause                  How the player died.
 */

static void PlayerDeathUI(Player *aPlayer, DeathCause cause)
{
    Country *country = aPlayer->country;

    clear();
    move(0, 0);
    UIColor(UIC_BAD);
    printw("Very sad news ...\n\n");

    if (cause == DEATH_STARVATION)
    {
        printw("%s %s of %s has been assassinated\n",
               aPlayer->title, aPlayer->name, country->name);
        printw("by a crazed mother whose child had starved to death. . .\n\n");
    }
    else
    {
        printw("%s %s ", aPlayer->title, aPlayer->name);
        switch (RandRange(4))
        {
            case 1:
                printw("has been assassinated by an ambitious noble\n\n");
                break;
            case 2:
                printw("has been killed from a fall during the annual "
                       "fox-hunt.\n\n");
                break;
            case 3:
                printw("died of acute food poisoning.\n"
                       "The royal cook was summarily executed.\n\n");
                break;
            case 4:
            default:
                printw("passed away this winter from a weak heart.\n\n");
                break;
        }
    }
    UIColorOff();

    printw("The other nation-states have sent representatives "
           "to the funeral\n\n");

    char input[80];
    printw("<Enter>? ");
    getnstr(input, sizeof(input));
}
