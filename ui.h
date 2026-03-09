/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * UI helper library for ncurses screen layout.
 *
 * Provides:
 *   - Color: theme-aware color pairs (off/dark) via X-macro
 *   - Layout: full-screen, dynamic sizing, title bars, separators
 *   - Table: right-aligned columnar data with headers
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#ifndef __UI_H__
#define __UI_H__

#include "platform.h"


/*
 * Terminal dimensions, updated on each UITitle() call.
 */

extern int winrows;
extern int wincols;


/*
 * Color definitions using X-macro pattern.
 *
 * Each entry defines a UI element's appearance for two themes:
 *   OFF  — monochrome, uses only ncurses attributes (A_BOLD, A_REVERSE, etc.)
 *   DARK — colored foreground/background pairs + attributes
 *
 * Fields: (name, off_attr, dark_fg, dark_bg, dark_attr)
 */

#define UI_COLORS \
    C(DEFAULT,    0,                -1,           -1,          0)      \
    C(TITLE,      A_BOLD|A_REVERSE, COLOR_WHITE,  COLOR_BLUE,  A_BOLD) \
    C(SEPARATOR,  0,                COLOR_CYAN,   -1,          0)      \
    C(HEADING,    A_BOLD,           COLOR_CYAN,   -1,          A_BOLD) \
    C(PROMPT,     A_BOLD,           COLOR_GREEN,  -1,          A_BOLD) \
    C(GOOD,       0,                COLOR_GREEN,  -1,          0)      \
    C(BAD,        A_BOLD,           COLOR_RED,    -1,          A_BOLD)


/*
 * Color type enum — one entry per UI element that can be colored.
 */

enum UIColorType {
#define C(name, ...) UIC_##name,
    UI_COLORS
#undef C
    UIC_NONE
};


/*
 * Initialize ncurses, colors, and terminal size.  Call once at startup
 * instead of initscr().
 */

void UIInit();


/*
 * Set the current text color/attribute.  Turns off the previous color
 * before applying the new one.
 *
 *   color                  Color type to apply.
 */

void UIColor(UIColorType color);


/*
 * Turn off the current text color, returning to default attributes.
 */

void UIColorOff();


/*
 * Print a horizontal separator line across the full screen width.
 */

void UISeparator();


/*
 * Print a screen title with the player's name, then a separator.
 * Clears screen, updates terminal dimensions, and draws a full-width
 * colored title bar.
 *
 *   title                  Screen title (e.g., "Investments").
 *   rulerLine              Ruler description (e.g., "King Arthur of Brittany").
 *                          If nullptr, no ruler line is shown.
 */

void UITitle(const char *title, const char *rulerLine = nullptr);


/*
 * Print a right-aligned value in a fixed-width column.
 *
 *   value                  String to print.
 *   width                  Column width.
 */

void UICol(const char *value, int width);


/*
 * Print a right-aligned integer with thousands separators in a fixed-width
 * column.
 *
 *   value                  Integer to print.
 *   width                  Column width.
 */

void UIColNum(int value, int width);


/*
 * Print a left-aligned label in a fixed-width column.
 *
 *   label                  String to print.
 *   width                  Column width.
 */

void UIColLabel(const char *label, int width);


#endif /* __UI_H__ */
