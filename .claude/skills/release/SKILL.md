---
name: release
description: Checks before a Rivellon Renewed release or release candidate – third-party dependency check, test coverage check and docs hygiene check, each as a report to the human. Use when a release, release candidate, tag or GitHub release is being prepared.
---

# Release checks

Run all three checks before every release candidate goes to the human and again before the tag. Report in German; change nothing silently – the human decides on every proposal. The release path itself (release script, candidate test, tag) is in `local/PLAN.md`.

## 1. Dependency check

For every third-party component we use (`THIRD-PARTY-NOTICES.md`: dgVoodoo2, Ultimate ASI Loader, MinHook, stb_truetype, fonts):

- Look for a newer version, official sources only (the project's GitHub releases or repository).
- If there is one: download it to `C:\dev\downloads`, verify its SHA-256 against the published digest, test the candidate with it (DE and EN; graphics on PC and Deck). Fonts: the human compares the texts in game – metrics can shift.
- Tests pass → pin the new version (version + SHA-256 where it is pinned: `setup/setup.ps1`, the release script, `docs/re/environment.md`, `THIRD-PARTY-NOTICES.md`).
- Tests fail → keep the old version, one line with the reason in `docs/rejected.md`.

## 2. Test coverage check

No game start – a comparison of the docs and test files. For every feature in `README.md` "Features" and every setting in `docs/features.md`:

- Which test covers it: unit test (`tests/`), automated sequence (`docs/dev/test-sequences.md`, run by `local/Run-ReleaseCheck.ps1`), or only the human's checklist (`local/testing.md`). Name features without any test and propose one (or say why manual is enough).
- Is the test still current: sequences, coordinates and INI keys match the code and the default `RivellonRenewed.ini`; tests of removed or changed behavior are named for deletion or update.
- Platforms: Windows DE and EN, Deck – name a feature tested on only one where it matters (graphics, input).

## 3. Docs hygiene check

(a) Are the rules in `CLAUDE.md` and its documentation rules still current and useful? Name outdated or never-applied rules.

(b) Are they kept?
- File sizes: ≤ 15 KB except `docs/archive/`; `CLAUDE.md` within its limit.
- Every fact in one place only, no dead links.
- `local/PLAN.md` status current; nothing planned or unfinished in the public files (CLAUDE.md, documentation rules).
- `CHANGELOG.md` "Unreleased" complete as release notes; `README.md` "Features" and `docs/features.md` match what is built.
- `docs/decisions.md` / `docs/rejected.md` without contradictions to the code.

Result: one report with proposals to the human.
