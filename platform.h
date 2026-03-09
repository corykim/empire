/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * Cross-platform compatibility header.
 *
 * Abstracts platform differences:
 *   - ncurses (Unix/macOS) vs PDCurses (Windows)
 *   - usleep (POSIX) vs Sleep (Windows) vs std::this_thread::sleep_for (C++11)
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

/*------------------------------------------------------------------------------
 *
 * Terminal UI library.
 */

#ifdef _WIN32
#include <curses.h>       /* PDCurses on Windows */
#else
#include <ncurses.h>      /* ncurses on Unix/macOS */
#endif


/*------------------------------------------------------------------------------
 *
 * Sleep function (microseconds).
 */

#include <chrono>
#include <thread>

inline void SleepUs(int microseconds)
{
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}


#endif /* __PLATFORM_H__ */
