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
| `empire.cpp` | Main loop, setup screens, CPU grain/population/investment phases |
| `cpu.cpp` | Strategy class implementations (target selection, troops, investments, taxes) |
| `attack.cpp` | Human/CPU attack screens, battle engine (RunBattle, InitBattle, etc.) |
| `grain.cpp` | Grain harvest, trading, feeding |
| `investments.cpp` | Revenue computation, tax setting, investment purchasing |
| `population.cpp` | Births, deaths, immigration, army efficiency |
| `grain.h` / `population.h` | Prototypes for GrainScreen / PopulationScreen |

### Key Design Decisions

- **Fair economics**: CPU players run through identical formulas as humans (grain, population, investments). No hidden bonuses.
- **Strategy pattern**: `CPUStrategy` is an abstract C++ class. Five concrete subclasses implement `selectTarget()`, `chooseSoldiersToSend()`, `manageInvestments()`, `manageTaxes()`. Difficulty only affects decisions, not outcomes.
- **Investment waste**: The only parameter varying between difficulty levels for investments is `wastePct` (50% → 0%), passed to the shared `cpuInvest()` base class method.
- **`ShowMessage(fmt, ...)`**: Printf-style function that prints, refreshes, and delays proportional to word count. Use this instead of `printw()` + `refresh()` + `usleep()` for any message the player needs to read.
- **`CLEAR_MSG_AREA()`**: Macro that clears rows 14-15 and positions cursor at row 14. Used before every prompt or error in the 16x64 window.
- **`RandRange(n)`**: Returns 1..n (1-based, not 0-based). Returns 0 if n <= 0.
- **Battle helpers**: `InitBattle()`, `SetBattleTarget()`, `ApplyBattleResults()` — shared between human and CPU attack paths.

### Constants

Defined as `constexpr int` in `empire.h`:
- `COUNTRY_COUNT = 6`, `TITLE_COUNT = 4`, `DIFFICULTY_COUNT = 5`
- `DELAY_TIME = 2000000` (2 seconds, in microseconds)
- `SERF_EFFICIENCY = 5`

Investment costs in `cpu_strategy.h` and `investments.cpp`:
- Marketplace=1000, Grain mill=2000, Foundry=7000, Shipyard=8000, Soldier=8, Palace=5000

### Window

ncurses window is 16 rows x 64 columns. Rows 14-15 are the message/prompt area.

## Conventions

- Use `true`/`false`, not `TRUE`/`FALSE`
- Use `nullptr`, not `NULL`
- Use `static_cast<>()`, not C-style casts
- Use `constexpr` for constants, not `#define` (except macros)
- Structs use C++ style (`struct Name {}`) not `typedef struct`
- Error messages state the constraint directly: `"YOU ONLY HAVE %d BUSHELS!"` not `"THINK AGAIN..."`
- Prompts show `0) DONE` or `0 TO SKIP` where applicable
- All delays use `usleep()` with microsecond values

## User Preferences

- No hidden CPU bonuses — all economic formulas must be identical for human and CPU
- Combat delay should feel natural, not forced — use mathematical curves, not conditional tiers
- Message delays scale by word count, never below DELAY_TIME
- Grain feeding defaults to required amount on empty ENTER; only confirm if army feed > 2x need or people feed > 4x need
- Soldier purchase caps should explain the limiting factor
- Fog of war is intentional — don't show enemy soldier counts on attack screen
