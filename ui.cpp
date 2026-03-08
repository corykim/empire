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


/*
 * Terminal dimensions.
 */

int winrows = 24;
int wincols = 80;


/*
 * Color state.
 */

struct ColorDef
{
    int fg, bg, attr;
};

static ColorDef colorDefs[] =
{
#define C(name, off_attr, dark_fg, dark_bg, dark_attr) \
    { -1, -1, off_attr }, { dark_fg, dark_bg, dark_attr },
    UI_COLORS
#undef C
};

static int colors[UIC_NONE];
static int lastColor = 0;


void UIInit()
{
    initscr();
    keypad(stdscr, true);
    scrollok(stdscr, true);

    /* Initialize colors if the terminal supports them. */
    if (has_colors())
    {
        start_color();
        use_default_colors();
        int theme = 1; /* 0=off, 1=dark */
        for (int i = 0; i < UIC_NONE; i++)
        {
            int j = i * 2 + theme;
            init_pair(i + 1, colorDefs[j].fg, colorDefs[j].bg);
            colors[i] = colorDefs[j].attr | COLOR_PAIR(i + 1);
        }
    }

    /* Get initial terminal size. */
    getmaxyx(stdscr, winrows, wincols);
}


void UIColor(UIColorType color)
{
    attroff(lastColor);
    lastColor = colors[color];
    attron(lastColor);
}


void UIColorOff()
{
    attroff(lastColor);
    lastColor = 0;
}


void UISeparator()
{
    int row, col;
    getyx(stdscr, row, col);
    UIColor(UIC_SEPARATOR);
    mvhline(row, 0, '-', wincols);
    move(row + 1, 0);
    UIColorOff();
}


void UITitle(const char *title, const char *rulerLine)
{
    /* Update terminal size. */
    getmaxyx(stdscr, winrows, wincols);

    clear();

    /* Draw full-width colored title bar. */
    UIColor(UIC_TITLE);
    mvhline(0, 0, ' ', wincols);
    move(0, 1);
    if (rulerLine)
        printw("%s -- %s", rulerLine, title);
    else
        printw("%s", title);
    UIColorOff();

    /* Separator. */
    move(1, 0);
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
