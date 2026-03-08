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
| `economy.h` / `economy.cpp` | Centralized economic rules: all formulas for grain, population, revenue, purchases, trading. Single source of truth for investment costs. |
| `cpu_strategy.h` | CPUStrategy abstract base class + 5 derived classes. Each has `errorPct` for decision quality. |
| `ui.h` / `ui.cpp` | UI helper library: colors, dynamic sizing, screen templates, separators, column formatting |
| `empire.cpp` | Main loop, setup screens, CPU economic phases, ShowMessage, FmtNum, ParseNum |
| `cpu.cpp` | Strategy class implementations (target selection, troops, investments, taxes, grain trading) |
| `attack.cpp` | Human/CPU attack screens, battle engine (RunBattle, InitBattle, etc.) |
| `grain.cpp` | Grain screen UI: harvest display, trading prompts, feeding prompts |
| `investments.cpp` | Investments screen UI: tax setting prompts, purchase prompts |
| `population.cpp` | Population screen UI: births, deaths, immigration display |
| `grain.h` / `population.h` | Prototypes for GrainScreen / PopulationScreen |

### UI System (`ui.h` / `ui.cpp`)

All screens use a shared UI library for consistent layout:

- **`UIInit()`** — Initializes ncurses, colors, and terminal size. Call once at startup instead of `initscr()`.
- **`UIColor(UIC_xxx)`** / **`UIColorOff()`** — Set/clear text color. Colors defined via X-macro with off/dark themes.
- **`UITitle(title, rulerLine)`** — Clears the screen, updates `winrows`/`wincols`, draws a full-width colored title bar, then a separator. Every screen starts with this.
- **`UISeparator()`** — Prints a full-width dash line. Used between sections and before prompt areas.
- **`UICol(value, width)`** — Right-aligned string in a fixed-width column.
- **`UIColNum(value, width)`** — Right-aligned integer with thousands separators.
- **`UIColLabel(label, width)`** — Left-aligned label in a fixed-width column.

Color types: `UIC_DEFAULT`, `UIC_TITLE`, `UIC_SEPARATOR`, `UIC_HEADING`, `UIC_PROMPT`, `UIC_GOOD`, `UIC_BAD`.

Globals: `winrows`, `wincols` — terminal dimensions, updated on each `UITitle()` call.

### Number Formatting

- **`FmtNum(int)`** — Returns a string with thousands separators (e.g., `"12,345"`). Uses 4 rotating static buffers so multiple calls work in one `printw()`. Use `%s` format specifier with the return value.
- **`ParseNum(str)`** — Strips commas and parses an integer. Use for all user numeric input instead of `strtol()`.

### Screen Layout Pattern

Every game screen follows this structure:
```
UITitle("Screen Name", "Ruler Title Name of Country");   // row 0-1 (colored bar + separator)
[screen content — tables, messages]                       // rows 2..winrows-4
UISeparator();                                            // optional content separator
[prompt at bottom, managed via CLEAR_MSG_AREA()]          // rows winrows-2..winrows-1
```

The game uses the full terminal (dynamic `winrows` x `wincols`). Content flows from the top; prompts are pinned to the bottom via `CLEAR_MSG_AREA()`. Separators span the full terminal width.

### Key Design Decisions

- **Fair economics**: CPU players run through identical formulas as humans (grain, population, investments). No hidden bonuses.
- **Strategy pattern**: `CPUStrategy` is an abstract C++ class. Five concrete subclasses implement `selectTarget()`, `chooseSoldiersToSend()`, `manageInvestments()`, `manageTaxes()`. Difficulty only affects decisions, not outcomes.
- **Error rate system**: Each difficulty has `errorPct` (50% → 5%). The `deviate(optimal, maxVal)` helper applies random error to taxes, investments, and grain trading. Investment waste budget = `errorPct`% of treasury.
- **`ShowMessage(fmt, ...)`**: Printf-style function that prints, refreshes, and delays proportional to word count (200ms/word, min DELAY_TIME). Use this instead of `printw()` + `refresh()` + `usleep()` for any message the player needs to read.
- **`CLEAR_MSG_AREA()`**: Macro that clears rows 14-15 and positions cursor at row 14. Used before every prompt or error.
- **`RandRange(n)`**: Returns 1..n (1-based, not 0-based). Returns 0 if n <= 0.
- **Battle helpers**: `InitBattle()`, `SetBattleTarget()`, `ApplyBattleResults()` — shared between human and CPU attack paths.

### Constants

Defined as `constexpr int` in `empire.h`:
- `COUNTRY_COUNT = 6`, `TITLE_COUNT = 4`, `DIFFICULTY_COUNT = 5`
- `DELAY_TIME = 2000000` (2 seconds, in microseconds)
- `SERF_EFFICIENCY = 5`

Investment costs defined as `constexpr` in `economy.h` (single source of truth):
- `COST_MARKETPLACE`=1000, `COST_GRAIN_MILL`=2000, `COST_FOUNDRY`=7000, `COST_SHIPYARD`=8000, `COST_SOLDIER`=8, `COST_PALACE`=5000

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

- No hidden CPU bonuses — all economic formulas must be centralized in `economy.h`/`economy.cpp` so CPU and human use identical code paths
- Fog of war is intentional — don't show enemy soldier counts on attack screen
- Combat delay: mathematical curves only (`37.5ms * sqrt(smaller_force)`), recalculated each round
- Message delays scale by word count via `ShowMessage()`, never below DELAY_TIME (2s)
- Grain feeding defaults to required amount on empty ENTER; only confirm if excess > 2x army need or 4x people need AND absolute difference > 10 bushels
- Soldier purchase caps should explain the limiting factor (nobles, foundries, serfs, treasury)
- Purchase prompts should show the maximum affordable/available amount
- Prefer clean code patterns: strategy pattern, no switch-on-difficulty, no duplicated strings
- Use generalized UI helpers (UITitle, UISeparator) rather than hand-coded layout per screen
- Consistent screen templates: every screen uses `UITitle()`, `UISeparator()` before prompts, `<Enter>?` for continuation
- No abbreviations in user-facing text — spell out "nobles", "soldiers", "merchants", "serfs", "palace"
- No graphs, bars, or visual charts unless explicitly requested
- Dead players' turns must end immediately — no investments or attacks after death
- When a ruler dies (any cause), always show a notification screen with Enter prompt
- CPU players should actively trade grain (buy when starving, sell surplus, sell land when broke) — not just hoard
- Always update README and other docs when changing functionality or design
- Always gitignore log files and build artifacts
- `FmtNum()` has only 4 rotating buffers — never exceed 4 calls per single `printw()`
