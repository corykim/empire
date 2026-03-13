/*------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 *
 * Diplomacy system implementation for the Empire game.
 *
 *------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "diplomacy.h"
#include "economy.h"
#include "empire.h"



/*
 * Generate a shuffled array of indices [0..COUNTRY_COUNT-1].
 * Used to eliminate ordering bias in diplomacy loops.
 */

static void ShuffleIndices(int *indices)
{
    for (int i = 0; i < COUNTRY_COUNT; i++)
        indices[i] = i;
    for (int i = COUNTRY_COUNT - 1; i > 0; i--)
    {
        int j = RandRange(i + 1) - 1;
        int tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }
}


/*------------------------------------------------------------------------------
 *
 * Diplomacy.
 */

void InitDiplomacy()
{
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        for (int j = 0; j < COUNTRY_COUNT; j++)
        {
            if (i == j)
            {
                playerList[i].diplomacy[j] = 0.0f;
            }
            else
            {
                /* Random value in [-DIPLOMACY_INIT_RANGE, +DIPLOMACY_INIT_RANGE]. */
                float r = static_cast<float>(RandRange(1001) - 501) / 500.0f;
                playerList[i].diplomacy[j] = r * DIPLOMACY_INIT_RANGE;
            }
        }
        playerList[i].attackedTargets = 0;
    }
    LogAllDiplomacy("Diplomacy Initialized");
}


void DecayDiplomacy(Player *aPlayer)
{
    /* Update the CALLING PLAYER's scores toward everyone else.
     * This ensures every CPU's scores toward ALL players (including
     * the human) are updated each turn, not just once per year. */
    if (aPlayer->human)
        return;  /* Human diplomacy is implicit. */

    int pi = aPlayer - playerList;
    float myPower = ComputePlayerPower(aPlayer);
    int order[COUNTRY_COUNT];
    ShuffleIndices(order);

    for (int k = 0; k < COUNTRY_COUNT; k++)
    {
        int j = order[k];
        if (j == pi || playerList[j].dead)
            continue;

        /* Asymmetric decay based on power disparity.
         * ratio = their power / my power.
         *
         * Negative scores toward a powerful player: suppress decay.
         * Positive scores toward a powerful player: amplify decay. */
        float theirPower = ComputePlayerPower(&playerList[j]);
        float ratio = theirPower / myPower;
        float decayRate = DIPLOMACY_DECAY_RATE;

        if (ratio > CPU_LEADER_POWER_MULT)
        {
            if (aPlayer->diplomacy[j] < 0.0f)
            {
                float suppress = 1.0f / (ratio * ratio);
                if (suppress < 0.05f) suppress = 0.05f;
                decayRate *= suppress;
            }
            else
            {
                decayRate *= ratio;
                if (decayRate > 0.40f) decayRate = 0.40f;
            }
        }

        /* Ally threat: if player j is much stronger than someone I
         * care about (ally b), I become wary of j.
         *   drift = sum over allies b: friendship(me→b) × threat(j→b) / 5 */
        if (ratio > CPU_LEADER_POWER_MULT)
        {
            float allyThreatDrift = 0.0f;
            for (int b = 0; b < COUNTRY_COUNT; b++)
            {
                if (b == j || b == pi || playerList[b].dead)
                    continue;
                float myFriendship = aPlayer->diplomacy[b];
                if (myFriendship <= 0.0f)
                    continue;
                float powerB = ComputePlayerPower(&playerList[b]);
                float threat = (theirPower / powerB) - 1.0f;
                if (threat > 0.0f)
                    allyThreatDrift -= myFriendship * threat / 5.0f;
            }
            if (allyThreatDrift < -0.20f) allyThreatDrift = -0.20f;
            aPlayer->diplomacy[j] = ClampDiplomacy(
                aPlayer->diplomacy[j] + allyThreatDrift);
        }

        aPlayer->diplomacy[j] *= (1.0f - decayRate);
    }
}


void UpdateDiplomacyAfterTurn(Player *aPlayer)
{
    int pi = aPlayer - playerList;
    float targetPower = ComputePlayerPower(aPlayer);

    int order[COUNTRY_COUNT];
    ShuffleIndices(order);

    if (aPlayer->attackedTargets == 0)
    {
        /* Peaceful turn: all CPUs increase diplomacy toward this player.
         * No peace bonus during treaty years — peace is mandatory. */
        if (year > treatyYears)
        {
            for (int k = 0; k < COUNTRY_COUNT; k++)
            {
                int i = order[k];
                if (i == pi || playerList[i].dead || playerList[i].human)
                    continue;

                /* Envy factor: power disparity dampens positive shifts and
                 * amplifies negative ones.  A player 2× your power gets
                 * half the peace bonus; 3× gets a third.
                 *   envyFactor = max(1.0, targetPower / observerPower) */
                float observerPower = ComputePlayerPower(&playerList[i]);
                float envyFactor = targetPower / observerPower;
                if (envyFactor < 1.0f) envyFactor = 1.0f;

                float old = playerList[i].diplomacy[pi];
                float bonus = DIPLOMACY_PEACE_BONUS / (envyFactor * envyFactor);
                playerList[i].diplomacy[pi] = ClampDiplomacy(
                    playerList[i].diplomacy[pi] + bonus);
                if (playerList[i].diplomacy[pi] != old)
                    GameLog("  Diplomacy peace: %s→%s %.3f→%.3f "
                            "(envy=%.1f)\n",
                            playerList[i].country->name,
                            aPlayer->country->name,
                            old, playerList[i].diplomacy[pi], envyFactor);
            }
        }
    }
    else
    {
        /* Aggressive turn: adjust diplomacy iteratively.
         * Pass 0: direct reactions (attack penalty, third-party, solidarity).
         * Pass 1+: solidarity feedback — observers see updated scores from
         * previous passes and may shift further.  Solidarity strength halves
         * each pass (0.3, 0.15, 0.075...) so the series converges.
         * Stop when max shift in a pass drops below 0.01. */
        float solidarityScale = 0.3f;
        for (int pass = 0; pass < 10; pass++)
        {
            ShuffleIndices(order);
            float maxShift = 0.0f;

            for (int k = 0; k < COUNTRY_COUNT; k++)
            {
                int i = order[k];
                if (i == pi || playerList[i].dead || playerList[i].human)
                    continue;

                float observerPower = ComputePlayerPower(&playerList[i]);
                float envyFactor = targetPower / observerPower;
                if (envyFactor < 1.0f) envyFactor = 1.0f;

                float old = playerList[i].diplomacy[pi];

                /* Check each target that was attacked. */
                for (int t = 0; t < COUNTRY_COUNT; t++)
                {
                    if (!(aPlayer->attackedTargets & (1 << t)))
                        continue;

                    if (t == i)
                    {
                        /* Attacked me — apply on pass 0 only. */
                        if (pass == 0)
                        {
                            int landBefore = playerList[i].land
                                             + aPlayer->landTakenFrom[i];
                            float landPct = (landBefore > 0)
                                ? static_cast<float>(aPlayer->landTakenFrom[i])
                                  / static_cast<float>(landBefore)
                                : 1.0f;
                            float penalty = (landPct / 0.20f)
                                            * (2.0f * DIPLOMACY_CLAMP);
                            if (penalty > 2.0f * DIPLOMACY_CLAMP)
                                penalty = 2.0f * DIPLOMACY_CLAMP;
                            playerList[i].diplomacy[pi] -= penalty * envyFactor;
                        }
                    }
                    else
                    {
                        /* Third-party shift — pass 0 only.
                         * Positive shifts dampened quadratically by envy:
                         * trust erodes fast toward the powerful.
                         * Negative shifts amplified linearly. */
                        if (pass == 0)
                        {
                            float shift = PredictThirdPartyShift(i, t, pi);
                            if (shift > 0.0f)
                                shift /= (envyFactor * envyFactor);
                            else
                                shift *= envyFactor;
                            playerList[i].diplomacy[pi] += shift;
                        }

                        /* Alliance solidarity — every pass, halving.
                         * Observer's preference between attacker and target
                         * determines who gets penalized:
                         *   preference = C→attacker - C→target
                         *   Positive: C sides with attacker → target penalized
                         *   Negative: C sides with target → attacker penalized
                         * One of the two shifts is always zero. */
                        float preference = playerList[i].diplomacy[pi]
                                           - playerList[i].diplomacy[t];
                        float pileOn = MAX(0.0f, preference) * solidarityScale;
                        float punish = MAX(0.0f, -preference) * solidarityScale;

                        playerList[i].diplomacy[t] = ClampDiplomacy(
                            playerList[i].diplomacy[t] - pileOn);
                        playerList[i].diplomacy[pi] = ClampDiplomacy(
                            playerList[i].diplomacy[pi] - punish);

                        float shift = MAX(pileOn, punish);
                        if (shift > maxShift)
                            maxShift = shift;
                    }
                }

                playerList[i].diplomacy[pi] = ClampDiplomacy(
                    playerList[i].diplomacy[pi]);
                if (pass == 0 && playerList[i].diplomacy[pi] != old)
                {
                    GameLog("  Diplomacy attack: %s→%s %.3f→%.3f "
                            "(envy=%.1f)\n",
                            playerList[i].country->name,
                            aPlayer->country->name,
                            old, playerList[i].diplomacy[pi], envyFactor);
                }
            }

            solidarityScale *= 0.5f;
            if (maxShift < 0.01f)
            {
                if (pass > 0)
                    GameLog("  Diplomacy converged after %d passes\n", pass + 1);
                break;
            }
        }
    }

    /* Attacker's own diplomacy drops toward anyone they attacked.
     * The act of aggression commits you to hostility — you can't attack
     * someone and still claim to be their friend. */
    for (int t = 0; t < COUNTRY_COUNT; t++)
    {
        if (!(aPlayer->attackedTargets & (1 << t)))
            continue;
        float landPct = 0.0f;
        if (aPlayer->landTakenFrom[t] > 0)
        {
            int landBefore = playerList[t].land + aPlayer->landTakenFrom[t];
            landPct = (landBefore > 0)
                ? static_cast<float>(aPlayer->landTakenFrom[t])
                  / static_cast<float>(landBefore)
                : 1.0f;
        }
        /* Scales from DIPLOMACY_CLAMP/4 (small raid) to full DIPLOMACY_CLAMP
         * (major assault) based on land taken. */
        float selfPenalty = DIPLOMACY_CLAMP / 4.0f
                            + landPct * (DIPLOMACY_CLAMP * 3.0f / 4.0f);
        aPlayer->diplomacy[t] = ClampDiplomacy(
            aPlayer->diplomacy[t] - selfPenalty);
    }

    /* Reset attacked targets for next turn. */
    aPlayer->attackedTargets = 0;
    for (int j = 0; j < COUNTRY_COUNT; j++)
        aPlayer->landTakenFrom[j] = 0;
}


void LogAllDiplomacy(const char *label)
{
    /* Collect living player indices. */
    int living[COUNTRY_COUNT];
    int lc = 0;
    for (int i = 0; i < COUNTRY_COUNT; i++)
        if (!playerList[i].dead)
            living[lc++] = i;
    if (lc == 0) return;

    /* Header row. */
    GameLog("%-12s", (label && label[0]) ? label : "Diplomacy:");
    for (int c = 0; c < lc; c++)
        GameLog(" %10s", playerList[living[c]].country->name);
    GameLog("\n");

    /* One row per living player. */
    for (int r = 0; r < lc; r++)
    {
        int ri = living[r];
        GameLog("%-12s", playerList[ri].country->name);
        for (int c = 0; c < lc; c++)
        {
            int ci = living[c];
            if (ci == ri)
                GameLog("        ---");
            else
                GameLog("     %+.3f", playerList[ri].diplomacy[ci]);
        }
        GameLog("\n");
    }
}


void LogPlayerDiplomacy(Player *aPlayer)
{
    int pi = aPlayer - playerList;
    GameLog("  %s:", aPlayer->country->name);
    for (int j = 0; j < COUNTRY_COUNT; j++)
    {
        if (j != pi && !playerList[j].dead)
            GameLog(" %s=%+.3f", playerList[j].country->name,
                    aPlayer->diplomacy[j]);
    }
    GameLog("\n");
}


int MaxSoldiers()
{
    int max = 1;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (!playerList[i].dead && playerList[i].soldierCount > max)
            max = playerList[i].soldierCount;
    }
    return max;
}


float MilitaryWeakness(int soldierCount)
{
    return 1.0f - (static_cast<float>(soldierCount)
                   / static_cast<float>(MaxSoldiers()));
}


float PredictThirdPartyShift(int observerIdx, int targetIdx, int attackerIdx)
{
    float k = playerList[observerIdx].diplomacy[targetIdx];
    float scale = DIPLOMACY_THIRD_PARTY_SCALE;
    if (k > 0.0f)
    {
        float targetWeakness = MilitaryWeakness(playerList[targetIdx].soldierCount);
        float attackerStrength = 1.0f - MilitaryWeakness(playerList[attackerIdx].soldierCount);
        scale *= (1.0f + targetWeakness * attackerStrength
                         * DIPLOMACY_WEAKNESS_SCALE);
    }

    /* Betrayal penalty: if the attacker was allied with the target,
     * observers view this much more negatively.  Attacking a friend
     * is worse than attacking an enemy. */
    /* Betrayal amplification: attacking someone you were friendly with
     * is viewed more harshly by observers.  Scales continuously — the
     * closer the attacker was to the target, the worse the betrayal. */
    float betrayal = MAX(0.0f, playerList[attackerIdx].diplomacy[targetIdx]);
    scale *= (1.0f + betrayal);

    return -k * scale;
}


float DiplomacyAttackWeight(float diplomacy, int targetSoldierCount)
{
    /* Base weight: 1 - diplomacy (enemies have high weight, friends low).
     * Weakness amplification scales continuously with hostility —
     * the more hostile, the more a weak target is attractive. */
    float w = MAX(0.01f, 1.0f - diplomacy);
    float hostility = MAX(0.0f, -diplomacy);
    w *= (1.0f + hostility * MilitaryWeakness(targetSoldierCount)
                            * DIPLOMACY_WEAKNESS_SCALE);
    return w;
}


int ComputeRetaliationReserve(Player *aPlayer)
{
    int pi = aPlayer - playerList;
    float netThreat = 0.0f;

    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == pi || playerList[i].dead)
            continue;

        /* Theory of mind: read the other player's diplomacy toward us.
         * Enemies (negative diplomacy) add threat proportional to their
         * soldiers and hostility.  Allies (positive diplomacy) reduce
         * the perceived threat — their presence makes us safer. */
        float theirDiplomacy = playerList[i].diplomacy[pi];
        float soldiers = static_cast<float>(playerList[i].soldierCount);
        netThreat -= theirDiplomacy * soldiers;
    }

    if (netThreat < 0.0f) netThreat = 0.0f;
    int reserve = static_cast<int>(netThreat * DIPLOMACY_RESERVE_SCALE);
    GameLog("  Retaliation reserve: %d soldiers (net threat: %.0f)\n",
            reserve, netThreat);
    return reserve;
}


float ComputePlayerPower(Player *aPlayer)
{
    /* Use anticipated revenue (from current infrastructure + optimal taxes)
     * rather than last year's actual revenue.  This detects economic threats
     * immediately when investments are built, not a year later.
     * ComputeExpectedRevenue averages weather at 3.5 for shipyards. */
    int anticipatedRevenue = ComputeExpectedRevenue(
        aPlayer, aPlayer->salesTax, aPlayer->incomeTax);
    if (anticipatedRevenue < 0) anticipatedRevenue = 0;

    float power = static_cast<float>(anticipatedRevenue) / 50.0f
                  + static_cast<float>(aPlayer->soldierCount)
                  + static_cast<float>(aPlayer->land) / 50.0f
                  + static_cast<float>(aPlayer->treasury) / 500.0f
                  + static_cast<float>(aPlayer->merchantCount) / 5.0f
                  + static_cast<float>(aPlayer->nobleCount) * 2.0f
                  + static_cast<float>(aPlayer->marketplaceCount)
                  + static_cast<float>(aPlayer->foundryCount) * 2.0f
                  + static_cast<float>(aPlayer->shipyardCount) * 3.0f
                  + static_cast<float>(aPlayer->grain) / 1000.0f;
    if (power < 1.0f) power = 1.0f;
    return power;
}


int ComputeDesiredTroopStrength(Player *aPlayer)
{
    int reserve = ComputeRetaliationReserve(aPlayer);

    /* Estimate troops needed for attacks.  Consider both enemies
     * (negative diplomacy) and envy targets (powerful players). */
    int pi = aPlayer - playerList;
    int attackTroops = 0;
    float myPower = ComputePlayerPower(aPlayer);

    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == pi || playerList[i].dead)
            continue;

        int needed = 0;
        if (aPlayer->diplomacy[i] < 0.0f)
        {
            /* Enemy: plan to send 1.5x their soldiers. */
            needed = playerList[i].soldierCount * 3 / 2;
        }
        else
        {
            /* Neutral/friendly but powerful: plan expeditionary force. */
            float ratio = ComputePlayerPower(&playerList[i]) / myPower;
            if (ratio > 1.5f)
                needed = playerList[i].soldierCount / 2;
        }

        if (needed > attackTroops)
            attackTroops = needed;
    }

    aPlayer->desiredTroops = reserve + attackTroops;
    GameLog("  Desired troops: %d (reserve=%d, attack=%d, current=%d)\n",
            aPlayer->desiredTroops, reserve, attackTroops,
            aPlayer->soldierCount);
    return aPlayer->desiredTroops;
}


float SimulateAttackOutcome(Player *attacker, int targetIdx)
{
    int ai = attacker - playerList;
    float allyScore = 0.0f;
    float retaliationRisk = 0.0f;

    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == ai || i == targetIdx || playerList[i].dead)
            continue;

        float predictedShift = PredictThirdPartyShift(i, targetIdx, ai);
        float predictedDiplomacy = playerList[i].diplomacy[ai] + predictedShift;

        if (predictedDiplomacy >= 0.0f)
            allyScore += predictedDiplomacy;
        else
            retaliationRisk += -predictedDiplomacy
                               * static_cast<float>(playerList[i].soldierCount);
    }

    /* The target itself will set diplomacy to DIPLOMACY_ATTACKED_ME toward us. */
    retaliationRisk += -DIPLOMACY_ATTACKED_ME
                       * static_cast<float>(playerList[targetIdx].soldierCount);

    /* Normalize retaliation risk to a comparable scale with ally scores.
     * Raw risk can be in the hundreds (soldiers * diplomacy), so scale down. */
    float score = allyScore - retaliationRisk * 0.005f;

    GameLog("    Simulate attack %s→%s: allies=%.2f retaliation=%.1f score=%.3f\n",
            attacker->country->name, playerList[targetIdx].country->name,
            allyScore, retaliationRisk, score);

    return score;
}


int ComputeAlliedStrength(Player *attacker, int targetIdx)
{
    int ai = attacker - playerList;
    float alliedSoldiers = 0.0f;

    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (i == ai || i == targetIdx || playerList[i].dead)
            continue;

        /* Weight by diplomacy magnitude: a strong ally who hates the
         * target contributes fully; a lukewarm ally contributes less.
         * hostility = how much they dislike the target (0 to CLAMP)
         * friendship = how much they like us (0 to CLAMP)
         * contribution = soldiers × hostility × friendship / CLAMP² */
        float hostility = MAX(0.0f, -playerList[i].diplomacy[targetIdx]);
        float friendship = MAX(0.0f, playerList[i].diplomacy[ai]);
        alliedSoldiers += playerList[i].soldierCount
                          * hostility * friendship
                          / (DIPLOMACY_CLAMP * DIPLOMACY_CLAMP);
    }
    return static_cast<int>(alliedSoldiers);
}


float ComputeVulnerability(int targetIdx)
{
    Player *target = &playerList[targetIdx];
    float vulnerability = 0.0f;

    /* Low absolute soldiers relative to power = vulnerable. */
    float power = ComputePlayerPower(target);
    if (power > 0.0f && target->soldierCount < 20)
        vulnerability += 1.0f;

    /* Recently lost many soldiers = wounded. */
    if (target->soldiersPrevTurn > 0)
    {
        float lossRatio = 1.0f - static_cast<float>(target->soldierCount)
                                 / static_cast<float>(target->soldiersPrevTurn);
        if (lossRatio > 0.3f)
            vulnerability += lossRatio * 3.0f;
    }

    /* Zero soldiers = maximally vulnerable. */
    if (target->soldierCount == 0)
        vulnerability += 3.0f;

    return vulnerability;
}


float ComputeAverageSurvivorPower()
{
    /* Exclude the strongest player from the average so that a dominant
     * leader's own power doesn't inflate the average and mask the gap.
     * Without this, a player at 400 with CPUs at 250 averages to 300,
     * yielding a ratio of only 1.33× — below the 1.5× threshold.
     * Excluding the leader: avg=250, ratio=1.6× — correctly detected. */
    int leaderIdx = -1;
    float leaderPower = 0.0f;
    float total = 0.0f;
    int count = 0;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (!playerList[i].dead)
        {
            float p = ComputePlayerPower(&playerList[i]);
            total += p;
            count++;
            if (p > leaderPower) { leaderPower = p; leaderIdx = i; }
        }
    }
    /* Return average of non-leader players. */
    if (count > 1)
        return (total - leaderPower) / (count - 1);
    return (count > 0) ? total / count : 1.0f;
}


int FindLeaderIdx()
{
    int best = -1;
    float bestPower = 0.0f;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (!playerList[i].dead)
        {
            float p = ComputePlayerPower(&playerList[i]);
            if (p > bestPower) { bestPower = p; best = i; }
        }
    }
    return best;
}


float ComputeAverageSoldierCount()
{
    int total = 0;
    int count = 0;
    for (int i = 0; i < COUNTRY_COUNT; i++)
    {
        if (!playerList[i].dead)
        {
            total += playerList[i].soldierCount;
            count++;
        }
    }
    return (count > 0) ? static_cast<float>(total) / count : 20.0f;
}
