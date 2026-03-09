# CLAUDE.md

## Project Overview

TRS-80 Empire game — a medieval strategy game using ncurses. Migrated from C to C++17. Players manage a country: harvest grain, feed people, set taxes, buy investments, recruit soldiers, and attack rivals. CPU opponents use the Strategy pattern with five difficulty levels.

## Build

```
make          # builds ./empire (Unix/macOS)
rm -f empire && make   # force rebuild (header changes need this)
cmake -B build && cmake --build build   # cross-platform (also works on Windows with PDCurses)
```

Requires: `g++` or any C++17 compiler, `ncurses` (Unix/macOS) or `PDCurses` (Windows), `libm`.

## Architecture

### Source Files

| File | Purpose |
|------|---------|
| `platform.h` | Cross-platform header: ncurses/PDCurses selection, `SleepUs()` replacing `usleep()` |
| `empire.h` | All shared types (Country, Player, Battle), globals, macros, prototypes |
| `economy.h` / `economy.cpp` | Centralized economic rules: grain, population, revenue, purchases, trading, diplomacy, tax optimization, military planning. Single source of truth for investment costs and tuning constants. |
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
- **Strategy pattern**: `CPUStrategy` is an abstract C++ class. Five concrete subclasses implement `selectTarget()`, `chooseSoldiersToSend()`, `manageInvestments()`. Base class provides default `manageTaxes()` and `manageGrainTrade()`. Difficulty only affects decisions, not outcomes.
- **Error rate system**: Each difficulty has `errorPct` (50% → 5%). The `deviate(optimal, maxVal)` helper applies random error. `errorPct` also controls: investment waste budget, attack weight blending toward uniform, tax adaptation rate (`adaptPct = 100 - 2*errorPct`).
- **`ShowMessage(fmt, ...)`**: Printf-style function that prints, refreshes, and delays proportional to word count (200ms/word, min DELAY_TIME). Use this instead of `printw()` + `refresh()` + `usleep()` for any message the player needs to read.
- **`CLEAR_MSG_AREA()`**: Macro that clears rows 14-15 and positions cursor at row 14. Used before every prompt or error.
- **`RandRange(n)`**: Returns 1..n (1-based, not 0-based). Returns 0 if n <= 0.
- **Battle helpers**: `InitBattle()`, `SetBattleTarget()`, `ApplyBattleResults()` — shared between human and CPU attack paths.

### Diplomacy System (`economy.h` / `economy.cpp`)

CPU players track diplomacy scores (float) toward every other player. Scores drive attack targeting, investment priorities, and retaliation planning.

**Diplomacy lifecycle:**
- **Init**: Random values in `[-DIPLOMACY_INIT_RANGE, +DIPLOMACY_INIT_RANGE]` (±0.25)
- **Decay**: All CPU scores decay 10% toward zero at the start of each player's turn
- **Peace bonus**: +0.03 per peaceful turn (skipped during treaty years)
- **Direct attack**: Penalty proportional to land taken: `(landPct / 0.20) × 4.0`, capped at 4.0. Small raids (1%) cost -0.2; taking 20%+ costs the full -4.0. Tracked via `Player::landTakenFrom[]`.
- **Third-party**: `PredictThirdPartyShift()` — attacking someone's enemy raises score; attacking their friend lowers it (amplified by target weakness × attacker strength)
- **Clamping**: All diplomacy scores clamped to `[-DIPLOMACY_CLAMP, +DIPLOMACY_CLAMP]` (±2.0) via `ClampDiplomacy()`.

**Tuning constants** (in `economy.h`):
`DIPLOMACY_INIT_RANGE`, `DIPLOMACY_PEACE_BONUS`, `DIPLOMACY_DECAY_RATE`, `DIPLOMACY_THIRD_PARTY_SCALE`, `DIPLOMACY_WEAKNESS_SCALE`, `DIPLOMACY_ENVY_SCALE`, `DIPLOMACY_RESERVE_SCALE`, `DIPLOMACY_CLAMP`, `CPU_OVERFEED_PCT`

**Key helpers:**
- `MaxSoldiers()`, `MilitaryWeakness(soldiers)`, `DiplomacyAttackWeight(diplomacy, soldiers)` — shared building blocks
- `PredictThirdPartyShift(observer, target, attacker)` — used by both `UpdateDiplomacyAfterTurn` and `SimulateAttackOutcome`
- `ComputePlayerPower(player)` — composite score (soldiers + land/50 + treasury/500 + merchants/5 + nobles×2 + marketplaces + foundries×2 + shipyards×3 + grain/1000)
- `ComputeRetaliationReserve(player)` — net threat from scores × soldiers, reduced by allies
- `ComputeDesiredTroopStrength(player)` — reserve + 1.5× worst enemy's soldiers
- `SimulateAttackOutcome(attacker, targetIdx)` — predicts diplomatic fallout and retaliation risk
- `ComputeExpectedRevenue(player, salesTax, incomeTax)` — deterministic revenue prediction
- `OptimizeTaxRates(player)` — brute-force 756 combinations for optimal sales/income rates
- `LogAllDiplomacy(label)`, `LogPlayerDiplomacy(player)` — structured logging

### CPU Turn Architecture

CPU turns follow: Grain → Population → **Military Planning** → Investments → Attack.

**Military Planning** (`ComputeDesiredTroopStrength`): Runs before investments to estimate army needs. Sets `player->desiredTroops = reserve + attackTroops`.

**Investments** (`cpuInvest`): Guns-vs-butter split based on aggregate diplomacy. `gunsPct = 50 - avgDiplomacy * 30`, clamped to [20%, 80%]. Guns budget buys palaces/foundries (whichever bottlenecks troop capacity). Butter buys economic infrastructure normally.

**Attack** (`selectTargetByDiplomacy`): Unified decision combining:
1. **Diplomacy weight**: `max(0.01, 1 - score)`, amplified by weakness for enemies. Military caution penalty applied only to this component.
2. **Envy**: `(targetPower/attackerPower - 1)³ × ENVY_SCALE` — cubic growth, bypasses caution entirely. CPUs will send suicide raids to weaken dominant players.
3. **Theory of mind**: `SimulateAttackOutcome` predicts diplomatic consequences and retaliation
4. **Barbarians**: Weighted by attacker's military strength, competes in the same pool
5. **Error blend**: `w = w * (1 - err/100) + 1.0 * (err/100)` — computed from continuous `cpuDifficulty`
6. **Attack probability**: `effectiveChance = aggression * maxWeight`, capped at 95%

CPUs attack multiple times per turn (up to `nobles/4 + 1`), re-evaluating targets and reserves after each battle.

**Tax optimization** (`manageTaxes`): Base class finds optimal sales/income rates via `OptimizeTaxRates`, then blends toward previous rates by `adaptPct`. Customs uses `deviate(18, 50)`. All parameters derived from per-CPU continuous difficulty.

### Per-CPU Continuous Difficulty

Each CPU gets `cpuDifficulty = baseDifficulty ± random(0.5)`, clamped [0, 4]. `strategyIndex = round(cpuDifficulty)` selects the behavioral class. All proportional parameters computed via `ComputeErrorPct(diff)` and `ComputeAttackChanceBase(diff)` from the continuous value, so two CPUs using the same strategy class still behave differently.

### Constants

Struct sizing constants in `empire.h` (needed before `economy.h` is included):
- `COUNTRY_COUNT = 6`, `TITLE_COUNT = 4`, `DIFFICULTY_COUNT = 5`

All other game-wide constants in `economy.h`, organized by category:
- **Core game**: `DELAY_TIME`, `SERF_EFFICIENCY`, `RANDOM_DEATH_CHANCE`
- **Starting values**: `STARTING_LAND`=10000, `STARTING_GRAIN_BASE`=15000, `STARTING_TREASURY`=1000, etc.
- **Investment costs**: `COST_MARKETPLACE`=1000 through `COST_PALACE`=5000
- **Grain/farming**: `GRAIN_YIELD_MULT`=0.72, `GRAIN_PER_PERSON`=5, `GRAIN_PER_SOLDIER`=8, `FOUNDRY_POLLUTION`=500, etc.
- **Trading**: `GRAIN_MARKUP`=10%, `GRAIN_WITHDRAW_FEE`=15%, `GRAIN_PRICE_BASE`=0.50, `LAND_SELL_PRICE`=5.0
- **Population**: birth/death/immigration rates and ratios
- **Military**: `EQUIP_RATIO_BASE`, `NOBLE_LEADERSHIP`=20, `MAX_ATTACK_CHANCE`=95
- **Revenue**: exponents (`REV_EXP_INVESTMENT`=0.9, etc.) and per-investment multipliers
- **Combat**: `BATTLE_DELAY_MS`, casualty thresholds and divisors, `SACK_THRESHOLD_DIV`
- **Diplomacy**: all `DIPLOMACY_*` constants

Additional globals in `empire.h` / `empire.cpp`:
- `treatyYears` — no player-vs-player attacks before this year, `5 - difficulty` (L1=5, L5=1)
- `Player::diplomacy[COUNTRY_COUNT]` — per-player scores toward each other player
- `Player::attackedTargets` — bitmask of players attacked this turn (for post-turn updates)
- `Player::desiredTroops` — military planning target, set before investments phase

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
- Grain feeding defaults to required amount on empty ENTER; `+` feeds 150%, `++` feeds 200% (people only). Only confirm if excess > 2x army need or 4x people need AND absolute difference > 10 bushels
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
- Summary screen ranks players by `ComputePlayerPower` (same score that drives CPU envy)
- Round-start logs use ASCII tables for country stats and diplomacy matrix
- Tuning constants (`DIPLOMACY_*`) should be kept in `economy.h` for easy tweaking
