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
#include "ui.h"


/*------------------------------------------------------------------------------
 *
 * Prototypes.
 */

static void PlayerDeath(Player *aPlayer);


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
    int      population;
    int      populationGain;
    int      born;
    int      immigrated;
    int      merchantsImmigrated;
    int      noblesImmigrated;
    int      diedDisease;
    int      diedMalnutrition;
    int      diedStarvation;
    int      armyDiedStarvation;
    int      armyDeserted;

    /* Get the player country. */
    country = aPlayer->country;

    /* Title. */
    char rulerLine[160];
    snprintf(rulerLine, sizeof(rulerLine), "%s %s of %s",
             aPlayer->title, aPlayer->name, country->name);
    UITitle("Population", rulerLine);
    printw("In year %d:\n", year);

    /* Determine the total population. */
    population =   aPlayer->serfCount
                 + aPlayer->merchantCount
                 + aPlayer->nobleCount;

    /* Determine the number of babies born. */
    born = RandRange((static_cast<int>(static_cast<float>(population) / 9.5)));

    /* Determine the number of people who died from disease. */
    diedDisease = RandRange(population / 22);

    /* Determine the number of people who died of starvation and */
    /* malnutrition.                                             */
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

    /* Determine the number of people who immigrated. */
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

    /* Determine the number of merchants and nobles who immigrated. */
    merchantsImmigrated = 0;
    noblesImmigrated = 0;
    if ((immigrated / 5) > 0)
        merchantsImmigrated = RandRange(immigrated / 5);
    if ((immigrated / 25) > 0)
        noblesImmigrated = RandRange(immigrated / 25);

    /* Determine the number of soldiers who died of starvation or deserted. */
    armyDiedStarvation = 0;
    armyDeserted = 0;
    if (aPlayer->armyGrainNeed > (2 * aPlayer->armyGrainFeed))
    {
        armyDiedStarvation = RandRange(aPlayer->soldierCount/2 + 1);
        aPlayer->soldierCount -= armyDiedStarvation;
        armyDeserted = RandRange(aPlayer->soldierCount / 5);
        aPlayer->soldierCount -= armyDeserted;
    }

    /* Determine the army's efficiency. */
    aPlayer->armyEfficiency =   (10 * aPlayer->armyGrainFeed)
                              / aPlayer->armyGrainNeed;
    if (aPlayer->armyEfficiency < 5)
        aPlayer->armyEfficiency = 5;
    else if (aPlayer->armyEfficiency > 15)
        aPlayer->armyEfficiency = 15;

    /* Display the number of babies born. */
    printw(" %s babies were born\n", FmtNum(born));

    /* Display the number of people who died of disease. */
    printw(" %s people died of disease\n", FmtNum(diedDisease));

    /* Display the number of people who immigrated. */
    if (immigrated > 0)
        printw(" %s people immigrated into your country.\n", FmtNum(immigrated));

    /* Display the number of people who died of starvation and malnutrition. */
    if (diedMalnutrition > 0)
        printw(" %s people died of malnutrition.\n", FmtNum(diedMalnutrition));
    if (diedStarvation > 0)
        printw(" %s people starved to death.\n", FmtNum(diedStarvation));

    /* Display the number of soldiers who starved to death. */
    if (armyDiedStarvation > 0)
        printw(" %s soldiers starved to death.\n", FmtNum(armyDiedStarvation));

    UISeparator();

    /* Display the army efficiency. */
    printw("Your army will fight at %d%% efficiency.\n",
           10 * aPlayer->armyEfficiency);

    /* Display the population gain or loss. */
    populationGain =
        born + immigrated - diedDisease - diedMalnutrition - diedStarvation;
    if (populationGain >= 0)
        printw("Your population gained %s citizens.\n", FmtNum(populationGain));
    else
        printw("Your population lost %s citizens.\n", FmtNum(-populationGain));

    /* Update population. */
    aPlayer->serfCount +=
        populationGain - merchantsImmigrated - noblesImmigrated;
    aPlayer->merchantCount += merchantsImmigrated;
    aPlayer->nobleCount += noblesImmigrated;

    UISeparator();
    printw("<Enter>? ");
    getnstr(input, 80);

    /* Check if player died. */
    PlayerDeath(aPlayer);
}


/*
 * Check if any event happened that killed the player specified by aPlayer.
 *
 *   aPlayer                Player.
 */

static void PlayerDeath(Player *aPlayer)
{
    Country *country;

    /* Get the player country. */
    country = aPlayer->country;

    /* If anyone starved to death, their mother might assassinate the ruler. */
    if ((aPlayer->diedStarvation > 0) &&
        (RandRange(aPlayer->diedStarvation) > RandRange(110)))
    {
        aPlayer->dead = true;
        clear();
        move(0, 0);
        printw("Very sad news ...\n\n");
        printw("%s %s of %s has been assassinated\n",
            aPlayer->title,
            aPlayer->name,
            country->name);
        printw("by a crazed mother whose child had starved to death. . .\n\n");
    }

    /* Check if the player died for any other reason. */
    if (RandRange(100) == 1)
    {
        aPlayer->dead = true;
        clear();
        move(0, 0);
        printw("Very sad news ...\n\n");
        printw("%s %s ", aPlayer->title, aPlayer->name);
        switch(RandRange(4))
        {
            case 1 :
                printw("has been assassinated by an ambitious noble\n\n");
                break;

            case 2 :
                printw("has been killed from a fall during the annual fox-hunt.\n\n");
                break;

            case 3 :
                printw("died of acute food poisoning.\n"
                       "The royal cook was summarily executed.\n\n");
                break;

            case 4 :
            default :
                printw("passed away this winter from a weak heart.\n\n");
                break;
        }
    }

    /* If the player died, display the funeral. */
    if (aPlayer->dead)
    {
        char input[80];
        printw("The other nation-states have sent representatives to the funeral\n\n");
        printw("<Enter>? ");
        getnstr(input, sizeof(input));
    }
}


