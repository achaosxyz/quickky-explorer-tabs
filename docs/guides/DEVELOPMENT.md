# Development Workflow

## Current scope

The repository builds a C++17 Windows x86_64 tray controller, integration DLL,
module libraries, model tests, synthetic Win32 tests, and a discovery-free smoke
test. Explicit tabs enablement activates experimental capture and presentation;
snapshot/observation remain read-only. Read-only host observations are not
evidence of safe injection, native-tab replacement, or recovery.

## Procedure

1. Read [AGENTS.md](../../AGENTS.md) and
   [PROJECT_DESIGN.md](../design/PROJECT_DESIGN.md), then the affected module docs.
2. Check local changes before editing:

   ```powershell
   git status --short
   git diff
   ```

3. Identify the smallest change within an existing module boundary. Preserve
   unrelated edits. Do not create new modules or add dependencies silently.
4. When changing structure, responsibility, data, or flows, update the overall
   design and affected module docs with the implementation. Mark unverified
   mechanisms explicitly; do not describe planned behavior as working code.
5. Use English for development material and comments. Keep the two user READMEs
   aligned without adding developer details. Update both locale catalogs for
   management-interface text changes.
6. Follow [validation and review](VALIDATION.md). State missing tools or tests;
   never report a build/test pass when no corresponding target exists.
7. Commit only when requested, using an English Conventional Commit. Publishing,
   pushing, or opening a pull request requires separate explicit authorization.

## Build prerequisites

- Windows with MSVC C++17 support and an installed Windows SDK.
- CMake 3.21 or newer with a generator compatible with the installed toolchain.
- An x86_64 target. The Visual Studio generator calls this architecture `x64`.

The verified local tools are CMake 3.28.3, Visual Studio 2017 Build Tools 15.9,
MSVC 19.16.27051 (v141), and Windows SDK 10.0.17763.0. No new tool installation
is required on this workstation. A successful build is not evidence that the
experimental Explorer integration works on every Windows version.

## Configure, build, and test

From the repository root in PowerShell:

```powershell
cmake -S . -B build/vs2017-x64 -G "Visual Studio 15 2017" -A x64 -T v141 "-DCMAKE_SYSTEM_VERSION=10.0.17763.0"
cmake --build build/vs2017-x64 --config Debug --parallel
ctest --test-dir build/vs2017-x64 -C Debug --output-on-failure

cmake --build build/vs2017-x64 --config Release --parallel
ctest --test-dir build/vs2017-x64 -C Release --output-on-failure
```

Stop if any command fails. The quoted SDK argument is intentional for PowerShell.
This generator locates the installed Visual Studio tools without adding `cl.exe`
or Microsoft's linker to the global PATH. The SDK version above describes the
local environment, not a requirement to replace a different compatible SDK.

Outputs are `build/vs2017-x64/Debug/QTabs.exe` and
`build/vs2017-x64/Release/QTabs.exe`. `build/` is ignored by Git.
`QuickkyExplorerHook.dll` is built beside each EXE and must remain beside it.
No installer is built. Version resources/About read root `VERSION`; build time
is regenerated in UTC on each build.

## Run the background controller

```powershell
Start-Process .\build\vs2017-x64\Debug\QTabs.exe
```

Normal launch stays in the notification area with no independent file-manager
window. Tabs are disabled unless previously enabled. Do not enable unscoped
capture on the daily desktop as part of development tests; use
`--capture-test-root` with dedicated folders under `build/live-test`.
To stop it, run:

```powershell
Start-Process .\build\vs2017-x64\Debug\QTabs.exe -ArgumentList '--stop' -Wait
```

These diagnostics only read Explorer metadata and never hide windows, inject a
DLL, or claim successful tab admission:

```powershell
Start-Process .\build\vs2017-x64\Debug\QTabs.exe -ArgumentList '--snapshot' -Wait -RedirectStandardOutput .\build\snapshot.json
Get-Content .\build\snapshot.json
Start-Process .\build\vs2017-x64\Debug\QTabs.exe -ArgumentList '--observe-ms 1000' -Wait
```

Snapshots contain identifiers, generations, and the managed count, not titles or
folder paths. Missing identity/desktop queries fail closed. Do not elevate the
controller: a different integrity level excludes ordinary Explorer candidates.
UAC is not needed for these operations.

CTest runs `core.model`, `capture.discovery`, `explorer.native_guard`,
`workspace.bridge`, `workspace.performance`, `hook.staging`, `app.instance`,
`app.refresh`, and `app.smoke`. Fixtures are hidden test-owned windows, not
real Explorer attachment. Bridge tests load the DLL only into the test process.
`app.smoke` skips discovery and closes its message-only window.
`app.refresh` checks scheduling with a synthetic clock, not live collection speed.
Invalid arguments return 160 (`ERROR_BAD_ARGUMENTS`). Use `"-DBUILD_TESTING=OFF"`
in a separate build directory only when intentionally omitting test executables.

For Explorer integration, follow the
[dedicated testing procedure](EXPLORER_TESTING.md) before any live attachment.

## Hosted Windows checks

The `Windows smoke` workflow runs on pushes and pull requests targeting
`develop` or `main`. It also supports manual validation of the selected branch:

```powershell
gh workflow run windows-smoke.yml --ref develop
gh run list --workflow windows-smoke.yml --branch develop --limit 5
gh run watch RUN_ID --exit-status
```

This checks release scripts, builds/packages x64, and runs discovery-free smoke
checks; it never publishes a release or changes branch versions. A green check
belongs to the run's exact source SHA, not automatically to later commits.
GitHub-token version-bump pushes do not trigger another push workflow.

## Distribution and release

Use [Release preview and publication](RELEASE.md) for the two release SOPs and
the `scripts/` build/package commands. Development uses `develop`; release
automation promotes the selected source to `main` before hosted smoke testing,
publishes only after the checks pass, then increments the development PATCH
version. Do not push or publish as part of routine local
validation without explicit authorization.
