# Consistency checks

Zero-dependency (Python 3.11+ stdlib) repository consistency checks. They encode
rules that are already documented by the maintainers
(`docs/experimental/RELEASE-WORKFLOW.md`, `docs/EXPERIMENTAL_HANDOFF_ZH.md`) so
mechanical drift is caught before a release or PR, not during it.

```powershell
python tests/consistency/run.py
# exit code 1 on any FAIL; --json writes consistency-report.json
```

Rules:

| Rule | Level semantics | What it checks |
| --- | --- | --- |
| R1 | FAIL/WARN | `version.json` vs `docs/RELEASE_NOTES_NEXT.md` — the version NEXT points at must not already be released (still marked 草稿/draft) |
| R2 | FAIL | main `RELEASE_NOTES_v*-experimental.md` files keep the bilingual structure (Chinese block, `---`, English block) |
| R3 | FAIL/WARN | `presets/*.json` reference existing `src/Effects/**.hlsl` files, and their parameter keys exist in the effect's `//!PARAMETER` metadata. Keys that `ScalingModesService` migrates at load time (the V065 normalization table, mirrored here) are WARN; keys in neither place would be silently ignored by the runtime and stay FAIL |
| R4 | FAIL/INFO/WARN | every `Resources.language-*.resw` parses, has no duplicate keys; per-language coverage vs en-US is reported (zh-* gaps are WARN; other languages INFO). Translations themselves are managed on Weblate — this suite never edits them |
| R5 | INFO/WARN | effect-parameter label keys (`ScalingModes_EffectParam_<effect>_<param>`, PR #16 convention) — coverage report |
| R6 | FAIL/INFO | XAML `x:Uid` references must exist in en-US; en-US keys with zero code/XAML references are listed as cleanup candidates (maintainer decision — resw files are Weblate-managed) |
| R7 | FAIL/WARN | relative links in `docs/**` resolve to real files (angle-bracket and space-containing targets supported; fragments and absolute local paths like `C:/...` ignored). Dated review records under `docs/**/reviews/` may legitimately point at files a later refactor renamed, so missing targets there are WARN |
| R8 | FAIL | files referenced by `.github/workflows/build.yml` exist |
| R9 | FAIL | `scripts/Build-Release.ps1` keeps the required runtime layout entries |

Adding a rule = one function named `rN_*` plus one entry in `RULES`. A rule that
crashes is reported as FAIL rather than silently skipped.
