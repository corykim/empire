# CLAUDE.md

## Project Overview

TRS-80 Empire game — a medieval strategy game using ncurses. Migrated from C to C++17. Players manage a country: harvest grain, feed people, set taxes, buy investments, recruit soldiers, and attack rivals. CPU opponents use the Strategy pattern with five difficulty levels.

## Build

```
make          # builds ./empire
rm -f empire && make   # force rebuild (header changes need this)
```

Requires: `g++` (C++17), `ncurses`, `libm`.

## Architecture

### Source Files

| File | Purpose |
|------|---------|
| `empire.h` | All shared types (Country, Player, Battle), globals, macros, prototypes |
| `cpu_strategy.h` | CPUStrategy abstract base class + 5 derived classes |
| `ui.h` / `ui.cpp` | UI helper library: screen templates, separators, column formatting |
| `empire.cpp` | Main loop, setup screens, CPU economic phases, ShowMessage, FmtNum, ParseNum |
| `cpu.cpp` | Strategy class implementations (target selection, troops, investments, taxes) |
| `attack.cpp` | Human/CPU attack screens, battle engine (RunBattle, InitBattle, etc.) |
| `grain.cpp` | Grain harvest, trading, feeding |
| `investments.cpp` | Revenue computation, tax setting, investment purchasing |
| `population.cpp` | Births, deaths, immigration, army efficiency |
| `grain.h` / `population.h` | Prototypes for GrainScreen / PopulationScreen |

### UI System (`ui.h` / `ui.cpp`)

All screens use a shared UI library for consistent layout:

- **`UITitle(title, rulerLine)`** — Clears the screen, prints a title (with optional ruler name), then a separator line. Every screen starts with this.
- **`UISeparator()`** — Prints a 64-character dash line. Used between sections and before prompt areas.
- **`UICol(value, width)`** — Right-aligned string in a fixed-width column.
- **`UIColNum(value, width)`** — Right-aligned integer with thousands separators.
- **`UIColLabel(label, width)`** — Left-aligned label in a fixed-width column.

Constants: `SCREEN_WIDTH = 64`, `SCREEN_ROWS = 16`.

### Number Formatting

- **`FmtNum(int)`** — Returns a string with thousands separators (e.g., `"12,345"`). Uses 4 rotating static buffers so multiple calls work in one `printw()`. Use `%s` format specifier with the return value.
- **`ParseNum(str)`** — Strips commas and parses an integer. Use for all user numeric input instead of `strtol()`.

### Screen Layout Pattern

Every game screen follows this structure:
```
UITitle("Screen Name", "Ruler Title Name of Country");   // row 0-1
[screen content — tables, messages]                       // rows 2-12
UISeparator();                                            // row 13
[prompt on rows 14-15, managed via CLEAR_MSG_AREA()]
```

The window is 16 rows x 64 columns. Rows 14-15 are the prompt/message area. Content must fit within rows 2-12 (~11 rows of data).

### Key Design Decisions

- **Fair economics**: CPU players run through identical formulas as humans (grain, population, investments). No hidden bonuses.
- **Strategy pattern**: `CPUStrategy` is an abstract C++ class. Five concrete subclasses implement `selectTarget()`, `chooseSoldiersToSend()`, `manageInvestments()`, `manageTaxes()`. Difficulty only affects decisions, not outcomes.
- **Investment waste**: The only parameter varying between difficulty levels for investments is `wastePct` (50% → 0%), passed to the shared `cpuInvest()` base class method.
- **`ShowMessage(fmt, ...)`**: Printf-style function that prints, refreshes, and delays proportional to word count (200ms/word, min DELAY_TIME). Use this instead of `printw()` + `refresh()` + `usleep()` for any message the player needs to read.
- **`CLEAR_MSG_AREA()`**: Macro that clears rows 14-15 and positions cursor at row 14. Used before every prompt or error.
- **`RandRange(n)`**: Returns 1..n (1-based, not 0-based). Returns 0 if n <= 0.
- **Battle helpers**: `InitBattle()`, `SetBattleTarget()`, `ApplyBattleResults()` — shared between human and CPU attack paths.

### Constants

Defined as `constexpr int` in `empire.h`:
- `COUNTRY_COUNT = 6`, `TITLE_COUNT = 4`, `DIFFICULTY_COUNT = 5`
- `DELAY_TIME = 2000000` (2 seconds, in microseconds)
- `SERF_EFFICIENCY = 5`

Investment costs in `cpu_strategy.h` and `investments.cpp`:
- Marketplace=1000, Grain mill=2000, Foundry=7000, Shipyard=8000, Soldier=8, Palace=5000

## Conventions

### C++ Style
- Use `true`/`false`, not `TRUE`/`FALSE`
- Use `nullptr`, not `NULL`
- Use `static_cast<>()`, not C-style casts
- Use `constexpr` for constants, not `#define` (except macros)
- Structs use C++ style (`struct Name {}`) not `typedef struct`

### UI Text Style
- **Proper case** for all user-facing strings (sentence case for messages, title case for headers)
- ALL CAPS only for emphasis
- Country names, ruler names, and titles are stored in proper case (e.g., "Brittany", "King", "Arthur")
- Error messages state the constraint directly: `"You only have 500 bushels!"` not `"Think again..."`
- Prompts show `0) Done` or `0 to skip` where applicable

### Number Display
- All displayed counts use `FmtNum()` with `%s` format: `printw("%s bushels", FmtNum(amount))`
- All user input uses `ParseNum()` to accept optional commas
- Right-align numbers in table columns using `%Ns` with `FmtNum()`

### Screen Layout
- Every screen starts with `UITitle()`
- Sections separated by `UISeparator()`
- Prompts in rows 14-15 via `CLEAR_MSG_AREA()`
- Content must fit within the 16-row window — no scrolling

## User Preferences

- No hidden CPU bonuses — all economic formulas must be identical for human and CPU
- Fog of war is intentional — don't show enemy soldier counts on attack screen
- Combat delay: mathematical curves only (`37.5ms * sqrt(smaller_force)`), recalculated each round
- Message delays scale by word count via `ShowMessage()`, never below DELAY_TIME (2s)
- Grain feeding defaults to required amount on empty ENTER; only confirm if army feed > 2x need or people feed > 4x need
- Soldier purchase caps should explain the limiting factor (nobles, foundries, serfs, treasury)
- Prefer clean code patterns: strategy pattern, no switch-on-difficulty, no duplicated strings
- Use generalized UI helpers (UITitle, UISeparator) rather than hand-coded layout per screen
