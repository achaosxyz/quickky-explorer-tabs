# Validation and Review

## Scaffold checks

1. Review new files as well as tracked diffs. `git diff` alone omits untracked
   file contents:

   ```powershell
   git status --short
   git diff --check
   git diff --stat
   ```

2. Check that relative Markdown links resolve to existing repository files.
   Keep module paths and document indexes consistent with the actual directories.
3. Parse both `resources/locales/` JSON files as UTF-8 with duplicate-key
   rejection. Compare their complete leaf-key sets, nonempty string values, and
   named placeholders. A normal JSON parse alone does not prove key uniqueness.
4. Check that English is used in development material, with Chinese limited to
   `README.zh-CN.md` and Chinese localization values. Check both user READMEs for
   equivalent feature and availability statements.
5. Confirm every source directory is represented in the overall design and that
   documentation does not claim a working application, build, or runtime test.

## Code changes

Once build/test targets exist, run the documented commands from
[Development](DEVELOPMENT.md) and tests proportionate to the affected behavior.
Keep pure grouping/capture policy tests separate from live Explorer integration.
Use [Explorer testing](EXPLORER_TESTING.md) for OS-dependent behavior; a clean
compile does not validate shell compatibility or recovery.

## Report

Report the changed boundary, checks actually executed, their results, and
unverified behavior. At the scaffold stage, static documentation/catalog checks
are valid evidence; native build and Explorer runtime validation are unavailable.
Do not stage, commit, or publish merely to run a review.
