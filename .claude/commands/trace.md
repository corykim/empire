---
description: Trace a game formula to verify correctness
allowed-tools: Bash, Read, Grep, Glob
---

## Context

Empire has complex economic formulas in `economy.cpp` that interact in non-obvious ways. When game behavior seems wrong, it could be a formula bug, a display bug (e.g., FmtNum buffer overflow), or working as intended but surprising.

## Your Task

Trace a specific game formula or behavior to determine if it's correct.

### Steps

1. **Identify the formula**: Read the relevant function in `economy.cpp` (or `attack.cpp` for combat). Understand the inputs and outputs.

2. **Check the display path**: If the issue might be a display bug, check the `printw` call. Verify FmtNum is not called more than 4 times in a single `printw`.

3. **Check the game log**: If `--log` was used, read `empire.log` for the relevant turn. Look for the actual computed values vs what was displayed.

4. **Write a test program**: Create a small C++ program in `/tmp/` that runs the formula with representative inputs. Compile with `g++ -std=c++17 -lm` and run it.

5. **Report findings**: Explain whether the behavior is:
   - **Formula bug**: the calculation is wrong
   - **Display bug**: the value is correct but shown incorrectly
   - **Working as intended**: the formula is correct, explain why the result is surprising

### Guardrails

- Always read the actual formula code — don't assume from memory
- Check for integer overflow in multiplications
- Check FmtNum buffer overflow (max 4 calls per printw)
- Check division-by-zero edge cases
- Remember `RandRange(n)` returns 1..n, not 0..n-1
- Clean up any test programs in /tmp/ when done
