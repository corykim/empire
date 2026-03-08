/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * UI helper library implementation.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#include <ncurses.h>
#include <string.h>
#include <stdio.h>

#include "empire.h"
#include "ui.h"


void UISeparator()
{
    printw("----------------------------------------------------------------\n");
}


void UITitle(const char *title, const char *rulerLine)
{
    clear();
    move(0, 0);
    if (rulerLine)
        printw("%s -- %s\n", rulerLine, title);
    else
        printw("%s\n", title);
    UISeparator();
}


void UICol(const char *value, int width)
{
    printw("%*s", width, value);
}


void UIColNum(int value, int width)
{
    printw("%*s", width, FmtNum(value));
}


void UIColLabel(const char *label, int width)
{
    printw("%-*s", width, label);
}
