---
name: release
description: Checks before a Rivellon Renewed release or release candidate – third-party dependency check (plus a dev tool report), test coverage check and docs hygiene check, each as a report to the human. Use when a release, release candidate, tag or GitHub release is being prepared.
---

# Release checks

Run all three checks before every release candidate goes to the human and again before the tag. Report in German; change nothing silently – the human decides on every proposal.

## Release path

1. Candidate: `New-Release.ps1 -Suffix rc<n>` (`docs/dev/workflow.md`), its files into `dd-test`/`-en`, `local/Run-ReleaseCheck.ps1` (DE and EN, every 2560×1440 sequence in borderless full screen incl. one new-game start).
2. The human's release test on a clean install on the PC (`dd-test-fresh` from `dd-steam-clean`) and the Deck: install following only the README – Claude does not help, every stumble is a README bug –, play with a checklist of every feature (`local/testing.md`), uninstall following the README (game runs original again). Passed when installation needs no help and no settings and the play test finds no defect.
3. `CHANGELOG.md` "Unreleased" → `## v<version> – <date>` as full release notes (features, known issues), commit; final zip from that commit (`New-Release.ps1`, default `build\dist`); the three checks below; secret scan of the history; `main` pushed.
4. `tools/release/Publish-Release.ps1 -Version <version>` – tag, GitHub release (`docs/dev/workflow.md`).

## 1. Dependency check

For every third-party component we use (`THIRD-PARTY-NOTICES.md`: dgVoodoo2, Ultimate ASI Loader, MinHook, stb_truetype, fonts):

- Look for a newer version, official sources only (the project's GitHub releases or repository).
- If there is one: download it to `C:\dev\downloads`, verify its SHA-256 against the published digest, test the candidate with it (DE and EN; graphics on PC and Deck). Fonts: the human compares the texts in game – metrics can shift.
- Tests pass → pin the new version (version + SHA-256 where it is pinned: `setup/setup.ps1`, the release script, `docs/re/environment.md`, `THIRD-PARTY-NOTICES.md`).
- Tests fail → keep the old version, one line with the reason in `docs/rejected.md`.

**Dev tools (report only):** for every tool in the table of `docs/re/environment.md` (compiler, CMake, Python, Ghidra, debuggers, Cheat Engine, `gh`, 7-Zip, video tools, …) look up the newest version from its official source and list installed vs. newest. Updating a dev tool needs no player test: show the command (winget or download with SHA-256), install only after the human's OK, then update the version in `docs/re/environment.md` (and in `docs/dev/` where a path holds the version). After an MSVC or CMake update, build the same commit twice and check the two zips are still byte-identical (reproducible builds, `docs/dev/workflow.md`) – if not, say so before the release.

## 2. Test coverage check

No game start – a comparison of the docs and test files. For every feature in `README.md` "Features" and every setting in `docs/features.md`:

- Which test covers it: unit test (`tests/`), automated sequence (`docs/dev/test-sequences.md`, run by `local/Run-ReleaseCheck.ps1`), or only the human's checklist (`local/testing.md`). Name features without any test and propose one (or say why manual is enough).
- Is the test still current: sequences, coordinates and INI keys match the code and the default `RivellonRenewed.ini`; tests of removed or changed behavior are named for deletion or update.
- Platforms: Windows DE and EN, Deck – name a feature tested on only one where it matters (graphics, input).

## 3. Docs hygiene check

(a) Are the rules in `CLAUDE.md` and its documentation rules still current and useful? Name outdated or never-applied rules.

(b) Are they kept?
- File sizes: doc files (`.md`) ≤ 15 KB except `docs/archive/`; `CLAUDE.md` within its limit.
- Every fact in one place only, no dead links.
- `local/PLAN.md` status current; nothing planned or unfinished in the public files (CLAUDE.md, documentation rules).
- `CHANGELOG.md` "Unreleased" complete as release notes; `README.md` "Features" and `docs/features.md` match what is built.
- `docs/decisions.md` / `docs/rejected.md` without contradictions to the code.
- `local/open-questions-re.md`: name the entries the next phase (`local/roadmap.md`) needs.

Result: one report with proposals to the human.
