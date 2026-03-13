# How to Play Empire

## Part 1: Basic Mechanics

### Overview

You rule a medieval country. Each year you manage your grain supply, grow your population, collect taxes, build investments, and wage war against rival nations. You win by conquering all five CPU opponents.

### The Turn Cycle

Each year, every country (human and CPU) takes a turn. Turn order is randomized. Your turn has four phases:

**1. Grain**

Your granary is the foundation of your country. Each year:

- Rats eat 1-30% of your stored grain (random).
- Your serfs harvest grain from available farmland. Better weather means a bigger harvest.
- You can buy grain from other countries, sell grain on the market, withdraw listed grain (with a 15% spoilage penalty), reprice your listings, or sell land to barbarians for quick cash.
- You must feed your army and your people from your grain reserves. Press Enter to feed the required amount, type a number to feed more or less, or use shortcuts: `+` feeds 150% of the requirement, and `++` feeds 200% (people only).

Underfeeding your people causes malnutrition and starvation. Overfeeding attracts immigrants. Underfeeding your army causes soldiers to starve and desert, and lowers their combat effectiveness.

**2. Population**

Your population changes each year through births, disease, immigration, and starvation. You have four classes of people:

- **Serfs** are your workers. They farm your land, and they're converted into soldiers when you recruit. They're the backbone of your country.
- **Merchants** generate marketplace revenue. You gain merchants through immigration and by purchasing marketplaces.
- **Nobles** lead your army. Each noble can command up to 20 soldiers. You gain nobles through immigration and by building palace sections.
- **Soldiers** fight your wars. They cost 8 per year in upkeep and consume 8 bushels of grain each. They don't produce anything.

If too many people starve, you risk assassination.

**3. Investments**

You collect tax revenue and investment income, then spend your treasury on buildings and troops:

| Investment | Cost | What It Does |
|------------|------|--------------|
| Marketplace | 1,000 | Generates revenue based on your merchant count. Also attracts new merchants. |
| Grain Mill | 2,000 | Boosts harvest yield and seeding efficiency. Generates revenue based on your serf count, penalized by income tax. Best insurance against bad weather. |
| Foundry | 7,000 | Generates revenue based on your army size. Also lets you equip more soldiers. Reduces grain harvest by 500 bushels per foundry. |
| Shipyard | 8,000 | Generates revenue based on your merchants, marketplaces, and foundries. Affected by weather. |
| Soldiers | 8 each | Fight battles. Cost 8/year upkeep plus 8 bushels grain. Limited by serfs, nobles, foundries, and treasury. |
| Palace | 5,000 | No direct revenue, but attracts new nobles who can lead more soldiers. |

You also set three tax rates:

- **Customs tax** (0-50%): Taxes immigrants. Higher customs discourages immigration.
- **Sales tax** (0-20%): Taxes commerce. Higher sales tax reduces marketplace revenue.
- **Income tax** (0-35%): Taxes everyone based on your population and investments. Higher income tax reduces grain mill revenue.

**4. Attack**

You can attack barbarian lands (unclaimed territory) or rival countries. You choose how many soldiers to send. Battles play out in real time until one side is eliminated.

- If you win, you capture land.
- If you lose, you may still capture a small amount of land.
- If you capture more than a third of the enemy's land, your army sacks their country, destroying buildings, grain, and killing serfs and nobles.
- If you wipe out all their forces, you overrun them entirely and absorb their serfs. Their country is destroyed.

You can't attack other countries during the treaty period (varies by difficulty level). The number of attacks per year is limited by your noble count.

### How You Can Die

- **Assassination**: If people starve, a grieving mother may assassinate you. The more starvation deaths, the higher the chance.
- **Overrun**: If an enemy destroys all your soldiers and serfs defend unsuccessfully, your country falls.
- **Random events**: There's a 1% chance each year of dying from an accident, poisoning, or assassination by a noble.

When any ruler dies — yours or a rival's — you'll see a notification and be prompted to continue. A ruler who dies mid-turn immediately loses control; their turn ends with no further investments or attacks. When a ruler dies from non-military causes (starvation or random events), their land reverts to barbarian control and can be conquered by any player.

### Winning

Eliminate all five CPU countries by overrunning them in battle. You are the last ruler standing.

---

## Part 2: Strategy Guide

### Early Game (Years 1-5): Build Your Economy

Your starting treasury is only 1,000. Spend it on **marketplaces** immediately. They're cheap (1,000 each), generate revenue, and each purchase attracts new merchants who boost future marketplace revenue. This is the most important compounding loop in the game.

Set your taxes to moderate levels. A good starting point:
- Customs: 15-20%
- Sales: 5-8%
- Income: 25-30%

Resist the urge to raise taxes too high. Sales tax reduces marketplace revenue and income tax reduces grain mill revenue. High customs tax discourages immigration, which is your main source of population growth (and nobles).

**Overfeed your people.** Feed them at least 1.5x their requirement. This triggers immigration, which brings new serfs, merchants, and nobles. Immigration is the only way to grow your noble count without building palaces, and nobles are the bottleneck for your military.

Don't build soldiers yet. You can't be attacked during the treaty period, and your starting 20 soldiers are enough for early defense. But don't turtle for too long — CPU players grow envious of powerful neighbors and will eventually gang up on you regardless of how peaceful you've been.

### Mid Game (Years 5-12): Expand Your Military

Once your marketplace revenue is generating a healthy treasury, start building:

1. **Palaces** to gain nobles (each palace attracts 1-4 nobles, each noble leads 20 soldiers)
2. **Foundries** to increase your equip ratio (each foundry lets you arm 1.5% more of your population). Be aware that each foundry reduces your grain harvest by 500 bushels.
3. **Soldiers** once you have the nobles and foundries to support them

Attack **barbarian lands** first. They have no organized army structure, the land you capture is free, and there's no diplomatic consequence. Barbarian territory is a safe way to grow your land holdings, which increases your farming capacity.

**Be aware of diplomacy.** CPU players track how they feel about every other country. Attacking someone's ally will make that CPU hostile toward you, while attacking someone's enemy will make them friendlier. Choose your targets wisely — you can create useful alliances by attacking countries that multiple CPUs already dislike.

### Late Game (Years 12+): Conquer Rivals

Target the weakest CPU player first. Check the summary screen (sorted by power score) to identify them. Send enough soldiers to overwhelm their defenses — you want at least a 1.5:1 advantage.

When you overrun a country, you absorb their serfs, which boosts your population and farming capacity. Their buildings are destroyed in the victory riot, but the land and people are yours.

Keep building soldiers between wars. Don't leave yourself vulnerable while your army recovers.

### Key Principles

- **Diversify your investments.** No single building type wins alone. The strongest economy combines grain mills (grain sustainability), marketplaces (trade revenue), and shipyards (multiplier). Pure marketplace or pure mill strategies both lose to diversified ones.
- **Build grain mills early.** They boost your harvest yield and seeding efficiency, keeping your grain supply sustainable as your population grows. They also generate revenue based on your serf count. A few mills early can prevent the grain crisis that cripples pure marketplace economies.
- **Match your taxes to your buildings.** Mill-heavy economies thrive with low income tax. Marketplace-heavy economies prefer low sales tax. The CPU tax optimizer discovers this automatically.
- **Overfeed for immigration.** Feed your people 1.5-2x their need. The immigrants pay for themselves.
- **Foundries are a tradeoff.** They enable soldiers and generate revenue, but each one costs you 500 bushels of grain per year. Don't build more than your harvest can absorb.
- **Nobles are the bottleneck.** Without nobles you can't recruit soldiers. Build palaces and keep customs low for immigration.
- **Sell surplus grain.** If your reserves are much larger than your needs, list grain on the market. Grain is valuable — surplus sales can fund your early marketplace expansion.
- **Don't sell too much land.** Land sells for 5 per acre, which is tempting, but you lose the grain production forever. Selling 10-20% for an early marketplace boost is reasonable; selling half your land creates a permanent grain deficit that eats your revenue.
- **Shipyards are endgame.** They're expensive and their revenue depends on having lots of merchants, marketplaces, and foundries already. Don't build them early.
- **Don't turtle too long.** Staying peaceful builds goodwill, but growing too powerful triggers envy. CPU players will eventually unite against you if your power score far exceeds theirs. Use your strength before the window closes.
- **Pick enemies strategically.** Attacking a country that others already dislike earns you friends. Attacking a popular country turns everyone against you.

---

## Part 3: How the Systems Work

### Land and Farming

Your land serves two purposes: it provides space for your people and buildings, and the rest is farmed for grain.

**Usable farmland** is your total land minus the space occupied by your population:

- Each serf takes 1 acre
- Each noble takes 2 acres
- Each merchant takes 1 acre
- Each soldier takes 2 acres
- Each palace section takes 1 acre

Usable land is further capped by two constraints:
- You can only seed 3 acres per bushel of grain in reserve (grain mills increase this: +1 per sqrt(mills))
- Each serf can only farm 5 acres

**Harvest** depends on weather (rated 1-6, with mild regression toward the mean) and usable land. Grain mills boost the yield multiplier with diminishing returns (+0.08 per sqrt(mills)), and the bonus is stronger in bad weather — making mills valuable insurance against drought. Each foundry reduces your harvest by 500 bushels, representing the pollution and land disruption from industrial activity.

### Population Growth

**Births**: Up to 1/9.5 of your population is born each year (random).

**Disease**: Up to 1/22 of your population dies from disease each year (random).

**Starvation**: If you feed less than your people need, some die of malnutrition. If you feed less than half the need, people also starve to death. Starvation deaths create assassination risk.

**Immigration**: If you feed your people more than 1.5x their need, immigrants arrive. The number is based on the square root of the surplus grain, reduced by your customs tax rate. Among the immigrants, roughly 1 in 5 is a merchant and 1 in 25 is a noble.

### Army

**Recruitment**: Soldiers are recruited from serfs at 8 each. The number you can recruit is limited by four factors:

- **Treasury**: 8 per soldier
- **Serfs**: one serf becomes one soldier
- **Foundry equip ratio**: you can maintain soldiers up to (5% + 1.5% per foundry) of your total civilian population
- **Noble leadership**: each noble commands up to 20 soldiers

**Feeding**: Each soldier needs 8 bushels of grain per year. If you feed the army less than half their need, soldiers starve and desert. Army efficiency (combat effectiveness) ranges from 50% to 150% based on how well-fed they are. Use `+` at the feeding prompt to quickly feed 150% for maximum army efficiency.

**Attacks per year**: Limited to (nobles / 4) + 1.

### Combat

Each battle round, the game rolls the attacker's army efficiency against the defender's. The side that loses the roll takes casualties. When the attacker wins a round, they also capture land proportional to the casualties inflicted.

**Casualties per round** scale with the attacker's army size:
- Under 200 soldiers: ~1/15 of the force per round
- 200-1000 soldiers: ~1/8 per round
- Over 1000 soldiers: ~1/5 per round

If a country has no soldiers, its **serfs defend** at 50% efficiency. If the serfs are defeated, the country is overrun.

**Sacking**: If you capture more than 1/3 of the enemy's total land in a single battle, your army sacks their country. This randomly destroys a portion of their serfs, marketplaces, grain, grain mills, foundries, shipyards, and nobles.

### Revenue

Revenue is computed once per year at the start of your investment phase. Each investment type generates revenue that goes directly to your treasury. All revenue amounts have diminishing returns (raised to a power less than 1.0).

**Marketplace revenue** scales with your merchant count and number of marketplaces. It is divided by (sales tax + 1), so higher sales tax directly reduces it.

**Grain mill revenue** scales with your serf count and number of mills. It is divided by (income tax + 1), so higher income tax directly reduces it — symmetric with how sales tax reduces marketplace revenue. This creates a distinct tax strategy: mill-heavy economies prefer low income tax, while marketplace economies prefer low sales tax.

**Foundry revenue** scales with your soldier count and number of foundries. Tax rates don't affect it directly.

**Shipyard revenue** scales with your merchants, marketplaces, foundries, number of shipyards, AND the current year's weather. In a bad weather year, shipyards produce almost nothing. In a great year, they're the most productive investment.

**Soldier upkeep** costs 8 per soldier per year, deducted from your treasury as negative revenue.

**Customs tax** generates revenue proportional to your customs rate times the number of immigrants that year. No immigrants means no customs revenue, regardless of the rate.

**Sales tax** generates revenue based on your sales rate applied to a base that includes your investment revenues, noble count, and serf count.

**Income tax** generates revenue based on your income rate applied to a base that includes your entire population and every investment type. Foundries (425 each) and shipyards (965 each) contribute the most per unit.

### The Tax Paradox

Higher tax rates don't always mean more revenue. Sales tax appears in the denominator of marketplace revenue, while income tax appears in the denominator of grain mill revenue. The two are symmetric: marketplaces use merchants as the numerator and sales tax as the penalty, while mills use serfs as the numerator and income tax as the penalty. A marketplace-heavy economy wants low sales / high income tax. A mill-heavy economy wants low income / high sales tax. A balanced economy finds the sweet spot in between. Additionally, marketplace revenue feeds back into the sales tax base, and mill revenue feeds back into the income tax base — creating a self-balancing loop where the tax you want to lower also benefits from the revenue it suppresses.

### Diplomacy

CPU players track a **diplomacy score** toward every other player, ranging from deeply negative (hostile) to positive (friendly). These scores drive who they attack, how they invest, and how many troops they hold in reserve.

**How diplomacy changes:**
- **Peace**: Each turn you don't attack, all CPUs increase diplomacy toward you by a small amount. Over many turns, this builds up to moderate goodwill (~0.5).
- **Direct attack**: If you attack a CPU, their diplomacy toward you drops proportional to the land you captured — a small raid barely dents relations, but taking 20% or more of their land triggers the maximum penalty. Diplomacy scores are clamped to the range [-2, +2].
- **Third-party effects**: If you attack a country that a CPU dislikes, that CPU likes you more. If you attack someone they like, they like you less — especially if the target is militarily weak.
- **Decay**: All scores drift toward zero over time. Old grudges fade; old friendships erode.
- **Treaty period**: Peace bonuses don't accumulate during the treaty period — peace is mandatory, so it earns no goodwill.

**How diplomacy affects CPU behavior:**
- **Target selection**: CPUs are most likely to attack the player with the lowest score. Positive scores make you an unlikely target; negative scores paint a bullseye.
- **Envy**: Power disparity (including revenue) affects all diplomacy. Friendliness toward a dominant player erodes faster; hostility persists longer. CPUs also develop preemptive hostility toward players who threaten their allies — even before an attack happens. When any player exceeds 1.5× average power, CPUs suppress inter-CPU attacks and focus on the leader.
- **Investment priorities**: CPUs with many enemies prioritize military infrastructure (palaces, foundries, soldiers). CPUs with mostly allies invest in their economy (marketplaces, mills, shipyards). CPUs compare mill vs marketplace ROI and buy whichever is more profitable.
- **Reserves**: CPUs hold soldiers in reserve proportional to the hostility and military strength of their enemies, offset by the strength of their allies. A garrison floor keeps 25% of the army as defense. Exception: CPUs can go all-in against a nemesis (maximum hostility, major power imbalance) when backed by allies.
- **Defensive turtling**: When outmatched 3:1+ with fewer than 50 soldiers, CPUs skip attacks entirely and focus on rebuilding.
- **Theory of mind**: Before attacking, smarter CPUs simulate the diplomatic consequences — who would be angered, who would approve, and how likely retaliation would be. Betraying an ally is viewed far more negatively by observers than attacking an enemy.
- **Alliance solidarity**: When an ally attacks someone, CPUs side with whichever player they prefer. If you like your ally more than the target, your view of the target drops. If you like the target more, no effect. This creates realistic alliance dynamics where strong alliances pull CPUs into conflicts.
- **Vulnerability detection**: CPUs detect when a player has recently lost soldiers or has zero defenders, and prioritize attacking them with full force.
- **Barbarian expansion**: CPUs prioritize conquering barbarian lands in the early game when it's free growth with no diplomatic consequences.

### CPU Opponents

CPU players use the same economic rules as you. They buy and sell grain, set taxes, build investments, recruit soldiers, and attack. Each CPU gets a difficulty level close to your chosen setting (±0.5 random variation), so no two opponents behave identically. At higher difficulty levels they make smarter decisions:

- **Level 1 (Village Fool)**: Makes random decisions, often skips trading entirely, wastes half their investment budget on random purchases. Picks targets nearly at random, ignoring diplomacy scores. Never adjusts tax rates from defaults.
- **Level 3 (Minor Noble)**: Makes reasonable decisions with moderate error. Scores moderately influence target selection. Slowly adapts tax rates toward optimal.
- **Level 5 (Machiavelli)**: Near-optimal tax rates (computed by simulating all 756 sales/income combinations), efficient investment with guns-vs-butter prioritization, exploits arbitrage on the grain market, overfeeds for immigration (withdrawing own market grain and buying grain if needed), uses full theory-of-mind simulation, coordinates with allies, and goes all-in against vulnerable targets. Strongly favors mill-heavy opening strategies (~72% chance), selling land to fund 2-3 mills on turn 1.

Each CPU gets a random opening capital allocation (market/mill/military) at game start, biased toward mills by difficulty. CPUs sell land on turn 1 to fund their allocation. This produces diverse strategies: some CPUs rush mills, some rush marketplaces, some go military. At Level 5, most CPUs favor mills but ~28% try alternative openings for strategic diversity.

CPU players can attack multiple times per turn (up to nobles/4 + 1), re-evaluating targets and reserves after each battle. Attacks require a minimum force (25% of estimated target strength) to prevent wasteful suicide raids. The treaty period varies by difficulty: 5 years at Level 1, down to 1 year at Level 5.

CPUs use a sustainability formula for grain management: they won't overfeed beyond what the worst-case harvest can sustain, and won't feed above 150% unless reserves cover a full year's need. They prefer buying grain mills over expensive market grain when the economics favor it, and refuse to buy grain above 2× base price except in true emergencies. CPUs never sell land for emergency grain — instead they attack for land when starving.

All difficulty levels use identical economic formulas. The only difference is decision quality, not rules.

---

## Part 4: The Gory Details

### Harvest

```
yieldMult  = 0.72 + 0.08 × sqrt(mills) × (7 - weather) / 4
seedRate   = 3 + floor(sqrt(mills))
usableLand = land - serfs - 2×nobles - merchants - 2×soldiers - palaces
usableLand = min(usableLand, seedRate × grainReserve) ← seed cap
usableLand = min(usableLand, 5 × serfs)               ← labor cap
harvest    = weather × usableLand × yieldMult + rand(1..500) - foundries × 500
```

Weather is 1-6 (random, with mild regression — only weather 1 or 6 nudge the next year's roll). Rats eat 1-30% of stored grain before harvest is added. The mill yield bonus is strongest in bad weather (×1.5 at weather 1) and weakest in good weather (×0.25 at weather 6), making mills insurance against drought.

### Feeding

- **People need**: 5 per civilian (serfs + merchants), 15 per noble
- **Army need**: 8 per soldier
- **Immigration threshold**: feed people > 1.5× their need
- **Immigration formula**: `sqrt(feed - need) - rand(1.5 × customsTax)`; if positive, `rand(2 × base + 1)` immigrants arrive
- Of immigrants: 1 in 5 becomes a merchant, 1 in 25 becomes a noble

### Population

| Event | Formula |
|-------|---------|
| Births | `rand(population / 9.5)` |
| Disease | `rand(population / 22)` |
| Malnutrition (feed < need) | `rand(population / 15)` |
| Malnutrition (feed < need/2) | `rand(population / 12)` |
| Starvation (feed < need/2) | `rand(population / 16)` |

### Army

- **Equip ratio**: `(5% + 1.5% per foundry) × civilian population`
- **Noble leadership**: 20 soldiers per noble
- **Recruitment cost**: 8 per soldier (from serfs)
- **Army efficiency**: `10 × (grainFed / grainNeed)`, clamped to 50%-150%
- **Attacks per year**: `nobles / 4 + 1`
- **Army upkeep**: 8 per soldier per year (deducted from treasury)

### Revenue

All investment revenues use diminishing returns: `pow(base, 0.9)`.

**Marketplace**: `(12 × (merchants + rand(35) + rand(35)) / (salesTax + 1) + 5) × count`

**Grain Mill**: `(0.50 × (serfs + rand(600) + rand(600)) / (incomeTax + 1) + 13) × count`

**Foundry**: `(soldiers + rand(150) + 400) × count`

**Shipyard**: `(4×merchants + 9×marketplaces + 15×foundries) × count × weather`

**Sales Tax**: `salesTax × (pow(1.8×merchants + 140×mktRev + 17×millRev + 50×fndRev + 70×shipRev, 0.85) + 5×nobles + serfs) / 100`

**Income Tax**: `pow(incomeTax × (1.3×serfs + 145×nobles + 39×merchants + 99×mkt + 55×millRev + 425×fnd + 965×ship) / 100, 0.97)`

### Combat

Each round:
- **Casualties**: `soldiers/15 + 1` (under 200), `soldiers/8 + 1` (200-1000), `soldiers/5 + 1` (over 1000)
- **Round winner**: `rand(attackerEfficiency) vs rand(defenderEfficiency)`
- **Land captured** (on attacker win): `rand(26 × casualties) - rand(casualties + 5)`
- **Sacking**: triggers if land captured > 1/3 of defender's total land
- **Overrun**: all defender soldiers (or serfs) eliminated
- **Serf defense**: 50% efficiency when no soldiers remain

### Trading

- **Grain markup**: 10% on purchases
- **Grain withdrawal**: 15% spoilage penalty
- **Land sale**: 5 per acre (max 95% of total land)
- **Grain price cap**: 5.0 per bushel

### Diplomacy

- **Range**: clamped to [-2, +2]
- **Peace bonus**: +0.03 per peaceful turn (post-treaty only)
- **Attack penalty**: `(landTakenPct / 20%) × 4.0`, capped at 4.0
- **Third-party shift**: `-observerDiplomacyToTarget × scale`; attacking an ally multiplies scale by `(1 + attacker's diplomacy toward target)` (betrayal penalty)
- **Decay**: all scores × 0.9 per turn
- **Envy**: `(powerRatio - 1)³ × 1.5` — bypasses military caution
- **Power score**: `soldiers + land/50 + treasury/500 + merchants/5 + nobles×2 + marketplaces + foundries×2 + shipyards×3 + grain/1000`

### Starting Values

| Resource | Amount |
|----------|--------|
| Land | 10,000 acres |
| Grain | 15,000-25,000 bushels |
| Treasury | 1,000 |
| Serfs | 2,000 |
| Merchants | 25 |
| Nobles | 1 |
| Soldiers | 20 |
| Taxes | Customs 20%, Sales 5%, Income 35% |

### Investment Costs

| Investment | Cost |
|------------|------|
| Marketplace | 1,000 |
| Grain Mill | 2,000 |
| Palace | 5,000 |
| Foundry | 7,000 |
| Shipyard | 8,000 |
| Soldier | 8 |
