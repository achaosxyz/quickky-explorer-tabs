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
[Development](DEVELOPMENT.md). Require `core.model`, `capture.discovery`, and
`app.smoke` to pass. The smoke test must close its message-only window before
its timeout. Discovery fixtures are hidden windows in the test process, not
real Explorer attachment. Check invalid arguments return 160 and bounded modes
exit without leaving a running application.

Read-only snapshot/observation may run on the development desktop. Confirm the
output contains no captions or folder paths, real managed count remains zero,
and no Explorer window is modified. Resident/duplicate/stop tests must first
exclude an existing controller and clean up only the process started by the test.
Do not interpret these observations or pure acknowledgment simulations as live
window capture. Keep mutating integration tests separate.
Use [Explorer testing](EXPLORER_TESTING.md) for OS-dependent behavior; a clean
compile does not validate shell compatibility or recovery.

## Report

Report the changed boundary, checks actually executed, their results, and
unverified behavior. Native builds, pure models, and synthetic discovery tests are available;
they do not prove visible presentation or Explorer compatibility. Report those
integration checks as unverified until they are actually implemented and tested.
Do not stage, commit, or publish merely to run a review.
