# Empire

TRS-80 Empire game using ncurses.

This fork is heavily refactored from the original C codebase, migrated to C++17, and extended with CPU opponents, a difficulty system, and numerous gameplay and UI improvements.

See [PLAYING.md](PLAYING.md) for how to play, strategy tips, and a detailed breakdown of the game systems.

## Prerequisites

- A C++17 compiler (`g++` or `clang++`)
- ncurses development library
- Math library (`libm`, typically included by default)

### Installing ncurses

**macOS** (included with Xcode Command Line Tools):
```
xcode-select --install
```

**Debian / Ubuntu**:
```
sudo apt install libncurses-dev
```

**Fedora / RHEL**:
```
sudo dnf install ncurses-devel
```

**Arch Linux**:
```
sudo pacman -S ncurses
```

## Building

```
make
./empire
```

## Command-Line Flags

| Flag | Description |
|------|-------------|
| `--omniscient` / `-o` | After each CPU turn, display their full economy screen (treasury, grain, investments, taxes, revenues). Press Enter to continue. |
| `--log` / `-l` | Write all player decisions to `empire.log` with running treasury/grain totals after each transaction. Covers grain, feeding, trading, taxes, purchases, and combat. Log is overwritten on each run. |
| `--fast` / `-f` | Skip all delays (splash, messages, battle rounds, CPU notifications) for rapid testing. |

Flags can be combined: `./empire --fast --log --omniscient`

## Architecture

### Language Migration

The codebase has been migrated from C to C++17. All source files use `.cpp` extensions and are compiled with `g++ -std=c++17`. The migration enables proper object-oriented design for the CPU strategy system while keeping the ncurses UI and game logic largely unchanged.

### Source Files

| File | Purpose |
|------|---------|
| `empire.h` | Shared types (Country, Player, Battle), globals, macros, forward declarations |
| `economy.h` / `economy.cpp` | Centralized economic rules: all formulas for grain, population, revenue, purchases, trading |
| `cpu_strategy.h` | `CPUStrategy` abstract base class and five derived classes |
| `ui.h` / `ui.cpp` | UI helper library: colors, dynamic sizing, screen templates, separators, column formatting |
| `empire.cpp` | Main game loop, setup screens, CPU economic phases, ShowMessage, FmtNum, ParseNum |
| `cpu.cpp` | Strategy class implementations (target selection, troops, investments, taxes, grain trading) |
| `attack.cpp` | Human and CPU attack screens, battle engine |
| `grain.cpp` | Grain screen UI: harvest display, trading prompts, feeding prompts |
| `investments.cpp` | Investments screen UI: tax setting prompts, purchase prompts |
| `population.cpp` | Population screen UI: births, deaths, immigration display |
| `grain.h` / `population.h` | Prototypes for GrainScreen / PopulationScreen |

### Centralized Economy (`economy.h` / `economy.cpp`)

All economic formulas live in one place. Both human UI screens and CPU strategies call the same functions, guaranteeing identical rules with no duplication:

| Function | Purpose |
|----------|---------|
| `ComputeGrainPhase()` | Rats, usable land, harvest, grain needs |
| `ComputePopulation()` | Births, deaths, immigration, army attrition, efficiency |
| `ApplyPopulationChanges()` | Apply civilian population changes from result |
| `CheckPlayerDeath()` | Starvation assassination and random death checks |
| `ComputeRevenues()` | All tax and investment revenue, treasury update |
| `ComputeSoldierCap()` | Max recruitable soldiers with limiting factor |
| `PurchaseInvestment()` | Generic investment purchase with treasury validation |
| `PurchaseSoldiers()` | Soldier recruitment with all constraints |
| `ApplyMarketplaceBonus()` | Merchant gain from marketplace purchase (once per action) |
| `ApplyPalaceBonus()` | Noble gain from palace purchase (once per action) |
| `TradeGrain()` | Buy grain from another player's market listing |
| `ListGrainForSale()` | Put grain on the market at a price |
| `SellLandToBarbarians()` | Sell land at 2 currency per acre |
| `GameLog()` | Write to empire.log (no-op if logging disabled) |

Investment costs are defined as `constexpr` constants in `economy.h` — the single source of truth used by both human and CPU code.

### Strategy Pattern

CPU decision-making uses the Strategy pattern with C++ inheritance and virtual dispatch. The abstract base class `CPUStrategy` defines the interface:

```cpp
class CPUStrategy
{
public:
    const char *name;
    int         attackChanceBase;
    int         attackChancePerYear;
    int         errorPct;       // deviation from optimal: 0=perfect, 50=random

    virtual int  selectTarget(...) = 0;
    virtual int  chooseSoldiersToSend(...) = 0;
    virtual void manageInvestments(...) = 0;
    virtual void manageTaxes(...) = 0;
    virtual void manageGrainTrade(...);  // default uses errorPct

protected:
    int  deviate(int optimal, int maxVal);  // apply random error
    void cpuInvest(Player *aPlayer);        // shared investment engine
};
```

Five concrete classes derive from it, each with an `errorPct` that controls how far decisions deviate from optimal:

```
CPUStrategy (abstract)
├── VillageFool      — errorPct=50, random targets, erratic decisions
├── LandedPeasant    — errorPct=35, picks weaker of two random targets
├── MinorNoble       — errorPct=20, only attacks weaker players
├── RoyalAdvisor     — errorPct=10, needs 1.5:1 odds, methodical growth
└── Machiavelli      — errorPct=5,  hunts the human leader, exploits wounded, arbitrage
```

The `errorPct` drives all decision quality through `deviate(optimal, maxVal)`, which returns a value near `optimal` with random deviation scaled by `errorPct`. This affects:

- **Tax rates**: all strategies target the same optimal rates (~18% customs, ~8% sales, ~28% income), but lower difficulties deviate more
- **Investment waste**: `errorPct`% of treasury is spent on random purchases before strategic buying
- **Grain feeding**: optimal overfeed of 190% (to attract immigrants), deviated by difficulty

Adding a new difficulty level requires subclassing `CPUStrategy`, setting `errorPct`, implementing the four pure virtual methods, creating an instance, and adding a pointer to the `cpuStrategies[]` table.

### Fair Economics

CPU players run through the exact same economic functions as human players each turn:

1. **Grain phase** — `ComputeGrainPhase()` for rats/harvest/needs, then CPU trading via `manageGrainTrade()`, then feeding decisions.
2. **Population phase** — `ComputePopulation()` + `ApplyPopulationChanges()` + `CheckPlayerDeath()`.
3. **Investments phase** — `ComputeRevenues()` for revenue, then `manageTaxes()` and `manageInvestments()` for CPU decisions, all purchases through `PurchaseInvestment()` / `PurchaseSoldiers()`.
4. **Attack phase** — Target selection and troop commitment delegated to the strategy class. Battle engine (`RunBattle`, `ApplyBattleResults`) is fully shared.

No hidden bonuses, no free resources. Every formula, cost, and constraint is identical for CPU and human players. Marketplace and palace bonuses (merchants/nobles gained) are applied exactly once per purchase action for both paths.

## Game Logic

### CPU Grain Trading

CPU players now participate in the grain market. Behavior scales with difficulty:

- **Buy grain** when they can't feed their people and army — finds the cheapest seller
- **Sell surplus grain** when reserves exceed ~2x needs — smarter CPUs sell more at better prices
- **Sell land** as emergency fundraising when treasury is too low for investments
- **Arbitrage** (Machiavelli only): buys grain priced below 2.0 and relists at 3.0+ for profit
- **Inaction chance**: Village Fool skips trading 50% of turns, Machiavelli only 5%

### CPU Attacks

CPU players attack other countries. Attack behavior is governed by difficulty level. Attacks follow the same rules as human attacks: no attacks before year 3, soldiers are committed to battle, land is seized or lost, and countries can be overrun.

### Difficulty Levels

| Level | Name | Error | Aggression | Attack Strategy |
|-------|------|-------|------------|-----------------|
| 1 | Village Fool | 50% | 20% base | Random targets, unpredictable troop commitments |
| 2 | Landed Peasant | 35% | 35% base | Picks weaker of two random targets, sends 30-60% |
| 3 | Minor Noble | 20% | 50% base | Avoids stronger players, sends 40-70% |
| 4 | Royal Advisor | 10% | 65% base | Targets weakest with most land, needs 1.5:1, keeps 25% reserve |
| 5 | Machiavelli | 5% | 80% base | Targets human leader, pounces on wounded, needs 2:1, arbitrage |

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

This creates a natural rhythm: large armies clash quickly, but as forces dwindle the pace slows and each round becomes more dramatic. Kill rates scale with attacker army size so battles resolve in a reasonable number of rounds. Skipped entirely with `--fast`.

### Soldier Purchasing

When a player requests more soldiers than they can afford, train, equip, or lead, the count is automatically capped to the maximum possible using `ComputeSoldierCap()`. A message explains the limiting factor (nobles, foundries, serfs, or treasury).

### Grain Feeding

- Pressing ENTER with no value feeds the required amount (default).
- Confirmation is only prompted for excessive amounts (more than 2x the army's need or 4x the people's need) AND when the absolute difference exceeds 10 bushels.

### Immigration

When people immigrate, the population screen shows a breakdown of how many merchants and nobles arrived among the immigrants.

## UI

- Full-screen dynamic layout using `winrows`/`wincols` (updated on each screen).
- Color support with off/dark themes via X-macro pattern.
- `ShowMessage(fmt, ...)` prints, refreshes, and delays proportional to word count (200ms per word, minimum 2 seconds).
- Summary screen sorted by land holdings, largest first.
- Treasury updates in real-time on the grain trading screen.
- Tax prompts show the current rate: `Customs tax (now 20%, max 50%)?`
- Land sell prompt shows current holdings: `How many of your 9,445 acres will you sell?`
- All prompts show `0) Done` or `0 to skip` where applicable.
- Error messages state the constraint directly.

## Code Quality

- Migrated from C to C++17 (`g++ -std=c++17`).
- Replaced all C idioms with C++ equivalents:
  - `NULL` → `nullptr`
  - `TRUE` / `FALSE` → `true` / `false`
  - C-style casts `(int)` → `static_cast<int>()`
  - `typedef struct` → `struct`
  - `#define` constants → `constexpr int`
  - `memset()` → aggregate initialization `= {}`
- `FmtNum()` uses 4 rotating static buffers — never exceed 4 calls per `printw()`.
- All economic formulas centralized in `economy.cpp` — no duplication between human and CPU paths.
- Battle setup shared via `InitBattle()`, `SetBattleTarget()`, `ApplyBattleResults()`.
- `CLEAR_MSG_AREA()` macro for consistent prompt area management.
