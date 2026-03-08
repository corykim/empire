# Empire

TRS-80 Empire game using ncurses.

This fork is heavily refactored from the original C codebase, migrated to C++17, and extended with CPU opponents, a difficulty system, and numerous gameplay and UI improvements.

## Building

```
make
./empire
```

Requires `g++` (C++17), `ncurses`, and `libm`.

## Architecture

### Language Migration

The codebase has been migrated from C to C++17. All source files use `.cpp` extensions and are compiled with `g++ -std=c++17`. The migration enables proper object-oriented design for the CPU strategy system while keeping the ncurses UI and game logic largely unchanged.

### Source Files

| File | Purpose |
|------|---------|
| `empire.h` | Shared types, globals, macros, forward declarations |
| `cpu_strategy.h` | `CPUStrategy` abstract base class and five derived classes |
| `empire.cpp` | Main game loop, setup, CPU economic phases |
| `cpu.cpp` | Strategy class implementations and global instances |
| `attack.cpp` | Human and CPU attack screens, battle engine |
| `grain.cpp` | Grain harvesting, trading, and feeding |
| `investments.cpp` | Revenue computation, tax setting, investment purchasing |
| `population.cpp` | Births, deaths, immigration, army efficiency |

### Strategy Pattern

CPU decision-making uses the Strategy pattern with C++ inheritance and virtual dispatch. The abstract base class `CPUStrategy` defines the interface:

```cpp
class CPUStrategy
{
public:
    const char *name;
    int         attackChanceBase;
    int         attackChancePerYear;

    virtual int  selectTarget(Player *aPlayer, int *livingIndices,
                              int livingCount) = 0;
    virtual int  chooseSoldiersToSend(Player *aPlayer, Player *aTarget) = 0;
    virtual void manageInvestments(Player *aPlayer) = 0;
    virtual void manageTaxes(Player *aPlayer) = 0;

protected:
    int  cpuBuy(Player *aPlayer, int *count, int desired, int cost);
    void cpuBuySoldiers(Player *aPlayer, int desired);
    void cpuInvest(Player *aPlayer, int wastePct);
};
```

Five concrete classes derive from it:

```
CPUStrategy (abstract)
├── VillageFool      — 50% waste, random targets, erratic taxes
├── LandedPeasant    — 35% waste, picks weaker of two random targets
├── MinorNoble       — 20% waste, only attacks weaker players
├── RoyalAdvisor     — 10% waste, needs 1.5:1 odds, keeps reserves
└── Machiavelli      —  0% waste, hunts the human leader, exploits wounded
```

Each class is a self-contained unit in `cpu.cpp` with all four strategy methods defined together. One static instance per class is created at startup and stored in the `cpuStrategies[]` pointer array, indexed by difficulty level:

```cpp
cpuStrategies[difficulty]->selectTarget(player, indices, count);
cpuStrategies[difficulty]->chooseSoldiersToSend(player, target);
cpuStrategies[difficulty]->manageInvestments(player);
cpuStrategies[difficulty]->manageTaxes(player);
```

Adding a new difficulty level requires subclassing `CPUStrategy`, implementing the four virtual methods, creating an instance, and adding a pointer to the table.

### Shared Base Class Helpers

The base class provides three protected methods available to all derived classes:

- **`cpuBuy()`** — Purchases an investment at cost, limited by treasury. Same validation as human purchases.
- **`cpuBuySoldiers()`** — Recruits soldiers constrained by treasury, available serfs, foundry equip ratio, and noble leadership cap. Identical rules to the human `BuySoldiers` path.
- **`cpuInvest(wastePct)`** — Core investment engine. Wastes a percentage of the budget on random purchases, then spends the rest in optimal order (foundries, palaces, marketplaces, grain mills, shipyards, soldiers). Each derived class calls this with its own waste percentage.

### Fair Economics

CPU players run through the exact same economic pipeline as human players each turn:

1. **Grain phase** — Rats eat grain, harvest computed from land/weather/serfs/foundries, army and people fed from actual grain stores (same formulas as `GrainScreen`).
2. **Population phase** — Births, disease deaths, starvation, malnutrition, immigration, army desertion, army efficiency (same formulas as `PopulationScreen`).
3. **Investments phase** — `ComputeRevenues()` calculates tax and investment income using identical formulas, treasury updated, then purchases paid at real cost.
4. **Attack phase** — Target selection and troop commitment delegated to the strategy class.

No hidden bonuses, no averaging of human holdings, no free resources. Every unit costs the same for CPU and human players.

## Game Logic Changes

### CPU Attacks

CPU players now attack other countries. Previously they only updated their holdings each turn and never initiated combat. CPU attack behavior is governed by the difficulty level. Attacks follow the same rules as human attacks: no attacks before year 3, soldiers are committed to battle, land is seized or lost, and countries can be overrun.

### Difficulty Levels

A difficulty selection screen is presented at game start with five levels:

| Level | Name | Aggression | Attack Strategy | Investment Waste | Taxes |
|-------|------|------------|-----------------|------------------|-------|
| 1 | **Village Fool** | 20% base | Random targets, unpredictable troop commitments. | 50% random | Random rates. |
| 2 | **Landed Peasant** | 35% base | Picks weaker of two random targets. Sends 30-60%. | 35% random | Moderate, slightly random. |
| 3 | **Minor Noble** | 50% base | Avoids stronger players. Sends 40-70%. | 20% random | Reasonable rates. |
| 4 | **Royal Advisor** | 65% base | Targets weakest with most land. Needs 1.5:1. Keeps 25% reserve. | 10% random | Optimized for growth. |
| 5 | **Machiavelli** | 80% base | Targets human leader. Pounces on wounded. Needs 2:1. Minimum force + margin. | 0% (optimal) | Pushes taxes to the limit. |

Difficulty affects only CPU decision-making. All players use the same formulas for outcomes. Attack frequency increases with difficulty and escalates by 2% per year.

### Victory and Defeat

- **Victory**: When only one human player remains and all CPU nations have been conquered, a victory screen announces the winner.
- **Defeat**: When the last human player is eliminated (by CPU attack, assassination, or starvation), the game ends immediately.
- Game-over checks run after every player's turn, not just human turns.

### Turn Order

Player turn order is randomized each year using a Fisher-Yates shuffle. This applies to both human and CPU players.

### Combat Pacing

Per-round battle delay is recalculated each round based on the current smaller force:

```
delay = 37.5ms * sqrt(smaller_force)
```

This creates a natural rhythm: large armies clash quickly, but as forces dwindle the pace slows and each round becomes more dramatic. Kill rates scale with attacker army size so battles resolve in a reasonable number of rounds.

### Soldier Purchasing

When a player requests more soldiers than they can afford, train, equip, or lead, the count is automatically capped to the maximum possible. A message explains the limiting factor:

- `LIMITED BY NOBLES (3 LEAD UP TO 60 TROOPS).`
- `LIMITED BY FOUNDRIES (2). BUILD MORE TO EQUIP TROOPS.`
- `LIMITED BY SERFS (45 AVAILABLE TO TRAIN).`
- `LIMITED BY TREASURY (500 KRONA).`

### Grain Feeding

- Pressing ENTER with no value feeds the required amount (default).
- Confirmation is only prompted for excessive amounts: more than 2x the army's need, or more than 4x the people's need.

### Player Death

When a human player dies (assassination, overrun in battle, or random death), the game prompts for ENTER so the player can read the message before it is dismissed.

## UI Improvements

- **`ShowMessage(fmt, ...)`** — Single function that prints a message, refreshes the screen, and delays proportional to word count (200ms per word, minimum 2 seconds). Eliminates duplicated strings and manual delay calculations.
- Treasury updates in real-time on the grain trading screen after each purchase or sale.
- Tax prompts show the current rate: `CUSTOMS TAX (NOW 20%, MAX 50%)?`
- Grain market display uses fixed-width columns for clean alignment.
- All prompts show `0) DONE` or `0 TO SKIP` where applicable.
- Attack prompt shows current soldier count: `HOW MANY OF YOUR 500 SOLDIERS TO SEND?`
- Error messages state the constraint directly instead of generic phrasing.

## Code Quality

- Migrated from C to C++17 (`g++ -std=c++17`).
- Replaced all C idioms with C++ equivalents:
  - `NULL` → `nullptr`
  - `TRUE` / `FALSE` → `true` / `false`
  - C-style casts `(int)` → `static_cast<int>()`
  - `typedef struct` → `struct`
  - `#define` constants → `constexpr int`
  - `memset()` → aggregate initialization `= {}`
  - `(void)` parameters → `()`
  - `char *` string literals → `const char *`
- Replaced 30+ repeated screen-clearing sequences with `CLEAR_MSG_AREA()` macro.
- Extracted duplicated battle setup into `InitBattle()`, `SetBattleTarget()`, and `ApplyBattleResults()`.
- Added `MIN` macro alongside `MAX`.
- Removed unused variable declarations from six investment functions.
- Fixed bitwise AND (`&`) used where logical AND (`&&`) was intended.
- Fixed `printw(invalidMessage)` to `printw("%s", invalidMessage)` for format string safety.
- Fixed missing `#include` directives that caused implicit function declaration errors.
- Fixed K&R-style `main()` to standard C prototype.
- Fixed out-of-bounds array access that caused a bus error when all human players were dead.
- All delays use `usleep()` with microsecond precision. Base message delay is 2 seconds.
- Player count prompt rejects 0 and non-numeric input.
