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
   documentation distinguishes implemented experimental integration from
   verified host behavior and remaining compatibility gates.

## Code changes

Run the Debug/Release build and CTest commands from
[Development](DEVELOPMENT.md). Require `core.model`, `capture.discovery`,
`explorer.native_guard`, `workspace.bridge`, `workspace.performance`, `hook.staging`,
`app.refresh`, `app.instance`, `explorer.native_abi`, and `app.smoke` to pass. The smoke
test must close its message-only window before
its timeout. Discovery fixtures are hidden windows in the test process, not
real Explorer attachment. Check invalid arguments return 160 and bounded modes
exit without leaving a running application.

`workspace.performance` checks publication operation counts and hidden test-owned
strip resources/hover state, not live frame rates or Explorer resource usage.
`hook.staging` injects a failed runtime copy after creating a partial destination
and requires cleanup of that file and its per-attempt directory. Discovery tests
count deferred enumerations with controlled event/callback delivery, separate
from eventual real WinEvent delivery. They do not infer delivery completion or
isolation from a fixed-duration message pump.

For each locally archived supported native image, run
`QuickkyNativeAbiTests.exe --image PATH` in both configurations. Run `--unsupported-image PATH` for the
excluded compact-accessor images and images confirmed to lack transfer interfaces.
These checks independently compare mode-discovered tables, entries and derived
fields with exact-image oracles. Unknown identity/missing-version, relocated
tables/accessors, duplicate candidates and damaged typed entries are synthetic
cases, not compatibility claims for an untested Windows build. The old exact-map
selection/fallback checks remain test-only regression oracles, not product logic.
They do not attach to
Explorer or establish live transfer compatibility.

With the retained local acquisition inventories, run the whole evidence set:

```powershell
scripts/Test-NativeAbiSamples.ps1 -Configuration Debug
scripts/Test-NativeAbiSamples.ps1 -Configuration Release
```

The script requires the matching built test executable and ignored research
artifacts. It exercises every registered reference and explicit exclusion or
missing-interface sample, but skips failed acquisitions. It requires each
registered fixture exactly once and checks the image bytes against the inventory
checksum before counting it as evidence for that version. The negative version
set and its categories must match `tests/abi_reference/rejections.json` exactly;
omitting, duplicating or relabeling a required negative is an error. It downloads nothing.
Production binaries must not link `quickky_abi_reference`; build with
`-DBUILD_TESTING=OFF` to verify that fixture locators are unnecessary.

Read-only snapshot/observation may run on the development desktop. Confirm the
output contains no captions or folder paths, diagnostic managed count remains zero,
and no Explorer window is modified. Resident/duplicate/stop tests must first
exclude an existing controller and clean up only the process started by the test.
Do not interpret these observations or pure acknowledgment simulations as live
window capture. Keep mutating integration tests separate.
Use [Explorer testing](EXPLORER_TESTING.md) for OS-dependent behavior; a clean
compile does not validate shell compatibility or recovery.

## Release tooling

For release tooling, run `./scripts/test-release.ps1` and the package build in
[Release](RELEASE.md). Inspect the ZIP contents and version resources. Script
checks use local fixture repositories and a mocked GitHub CLI; they must not
publish or mutate the real repository's refs. The workflow minimum is
`app.smoke` plus a second smoke run from the extracted ZIP. This is not the
full integration test suite or proof of clean-machine runtime availability.

## Report

Report the changed boundary, checks actually executed, their results, and
unverified behavior. Native builds, pure models, and synthetic discovery tests are available;
they do not prove visible presentation or Explorer compatibility. Report those
integration checks as unverified unless the specific runtime path/build was
actually tested. Review critical class ownership/thread contracts, HWND lifetime
binding, reentrant callbacks, bounded IPC, and cancellation-before-restoration.
After fixing a review finding, review the changed path again; if that review
finds a defect, fix it and perform another review before completion.
Do not stage, commit, or publish merely to run a review.
