/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * UI helper library for ncurses screen layout.
 *
 * Provides:
 *   - Table: right-aligned columnar data with headers
 *   - Screen: title bar, separator lines, consistent layout
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#ifndef __UI_H__
#define __UI_H__

#include <ncurses.h>

constexpr int SCREEN_WIDTH = 64;
constexpr int SCREEN_ROWS  = 16;


/*
 * Print a horizontal separator line across the full screen width.
 */

void UISeparator();


/*
 * Print a screen title with the player's name, then a separator.
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
