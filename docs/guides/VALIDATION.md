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
   documentation distinguishes the working bootstrap/build from unimplemented
   Explorer integration, tabs, recovery, and settings.

## Code changes

Run the Debug/Release build and CTest commands from
[Development](DEVELOPMENT.md). `app.smoke` must create and close the hidden
application window and exit successfully before its timeout. Check unsupported
arguments return 160 without leaving a running application. These checks must
not attach to Explorer. There are no grouping/capture policy unit tests yet.
Keep those tests separate from live Explorer integration when they are added.
Use [Explorer testing](EXPLORER_TESTING.md) for OS-dependent behavior; a clean
compile does not validate shell compatibility or recovery.

## Report

Report the changed boundary, checks actually executed, their results, and
unverified behavior. Native builds and the hidden-window smoke test are available;
they do not prove visible presentation or Explorer compatibility. Report those
integration checks as unverified until they are actually implemented and tested.
Do not stage, commit, or publish merely to run a review.
