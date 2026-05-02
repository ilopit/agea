---
name: main-only
description: Enforce that all work happens on the `main` branch. Verifies current branch and switches/rebases as needed. Use BEFORE starting any code change, and AUTO-INVOKE at the start of any session that begins on a non-main branch. Repo policy: no feature branches — every change goes on main, history stays linear via rebase (never merge).
allowed-tools: Bash
---

# Repo policy: only `main`

This repo does not use feature branches. All work — features, fixes, refactors, experiments — goes directly onto `main`. History stays linear; merge commits are forbidden.

## What this skill does

1. Runs `git branch --show-current` to check the current branch.
2. If already on `main` — confirm and exit.
3. If on another branch:
   - Show `git status --short` so the user sees any uncommitted work.
   - Stop and ASK before doing anything. Do NOT auto-switch — the user may have in-progress work that needs to be committed, stashed, or moved onto main first.
   - Possible follow-ups (only after explicit user OK):
     - `git checkout main && git pull --rebase origin main`
     - If the other branch has commits ahead of main, propose `git rebase main` from that branch then fast-forward main, OR cherry-pick. **Never `git merge`.**
     - If working tree is dirty, propose `git stash` first.

## Invariants

- **NEVER `git merge`.** Always rebase. See `feedback_no_merge.md` in user memory.
- **Force-push uses `--force-with-lease`, never plain `--force`.** Plain `--force` blindly overwrites the remote ref; `--force-with-lease` only succeeds if the remote tip matches what was last fetched, so it refuses to clobber commits pushed by someone else in the meantime. Still requires explicit user confirmation per push.
- If the user is on another branch and asks for code work, surface the policy first ("you're on `<branch>`, repo policy is main-only — switch?") instead of silently working on the wrong branch.

## When to invoke automatically

- Start of any session where `git branch --show-current` returns something other than `main`.
- Before any commit, if not on `main`.
- When the user says "let's start", "ready to work", or anything that signals beginning a new task on a non-main branch.

## When NOT to invoke

- The user is mid-rebase, mid-cherry-pick, or otherwise in a known transient git state — wait for them to finish.
- The user explicitly asked to operate on a non-main branch for a one-off purpose (e.g. inspecting an old branch's history).
