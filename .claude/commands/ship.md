---
description: Build, document, commit, and push all changes
allowed-tools: Bash, Read, Edit, Write, Glob, Grep
---

## Context

This project requires documentation updates whenever functionality or design changes. The build must pass before committing.

## Your Task

Build, update docs, commit, and push all current changes in one workflow.

### Steps

1. **Build**: Run `rm -f empire && make` and verify it compiles cleanly.

2. **Check what changed**: Run `git status -u` and `git diff --stat` to understand the scope of changes.

3. **Update documentation** if any functionality or design changed:
   - `README.md` — update if features, flags, UI, or architecture changed
   - `PLAYING.md` — update if game mechanics, strategy implications, or system behavior changed
   - `CLAUDE.md` — update if conventions, architecture, or file purposes changed
   - Skip docs that aren't affected by the changes

4. **Commit**: Stage all changed files by name (not `git add -A`). Write a concise commit message summarizing the "why", using a HEREDOC. Always end with:
   ```
   Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
   ```

5. **Push**: Run `git push`.

6. **Confirm**: Run `git status` and report the result.

### Guardrails

- Never use `git add -A` or `git add .` — add files by name
- Never amend commits — always create new ones
- Never skip hooks with `--no-verify`
- Don't commit `.env`, credentials, or log files
- If the build fails, fix the issue before committing
- Only update docs that are actually affected by the changes
