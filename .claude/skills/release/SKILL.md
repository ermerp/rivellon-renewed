---
name: release
description: Checks before a Rivellon Renewed release or release candidate – third-party dependency check and docs hygiene check, both as a report to the human. Use when a release, release candidate, tag or GitHub release is being prepared.
---

# Release checks

Run both checks before every release candidate goes to the human and again before the tag. Report in German; change nothing silently – the human decides on every proposal. The release path itself (release script, candidate test, tag) is in `local/PLAN.md`.

## 1. Dependency check

For every third-party component we use (`THIRD-PARTY-NOTICES.md`: dgVoodoo2, Ultimate ASI Loader, MinHook, stb_truetype, fonts):

- Look for a newer version, official sources only (the project's GitHub releases or repository).
- If there is one: download it to `C:\dev\downloads`, verify its SHA-256 against the published digest, test the candidate with it (DE and EN; graphics on PC and Deck). Fonts: the human compares the texts in game – metrics can shift.
- Tests pass → pin the new version (version + SHA-256 where it is pinned: `setup/setup.ps1`, the release script, `docs/re/environment.md`, `THIRD-PARTY-NOTICES.md`).
- Tests fail → keep the old version, one line with the reason in `docs/rejected.md`.

## 2. Docs hygiene check

(a) Are the rules in `CLAUDE.md` and its documentation rules still current and useful? Name outdated or never-applied rules.

(b) Are they kept?
- File sizes: ≤ 15 KB except `docs/archive/`; `CLAUDE.md` within its limit.
- Every fact in one place only, no dead links.
- `local/PLAN.md` status current; nothing planned or unfinished in the public files (CLAUDE.md, documentation rules).
- `CHANGELOG.md` "Unreleased" complete as release notes; `README.md` "Features" and `docs/features.md` match what is built.
- `docs/decisions.md` / `docs/rejected.md` without contradictions to the code.

Result: one report with proposals to the human.
