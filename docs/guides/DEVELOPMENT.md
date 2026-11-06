# Development Workflow

## Current scope

The repository builds a minimal C++17 Windows x86_64 GUI executable and registers
a hidden-window CTest smoke test. Other source modules remain reserved. The
executable has no Explorer integration or management page.

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
unimplemented Explorer integration works on any Windows version.

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

Outputs are `build/vs2017-x64/Debug/QuickkyExplorerTabs.exe` and
`build/vs2017-x64/Release/QuickkyExplorerTabs.exe`. `build/` is ignored by Git.
No integration DLL or installer is built.

## Run the minimal application

```powershell
Start-Process .\build\vs2017-x64\Debug\QuickkyExplorerTabs.exe
```

Normal launch shows a standard empty window; close it with the window's close
button. CTest instead passes `--smoke-test` to create and close the window without
showing it. Both paths leave Explorer untouched. Other arguments return exit
status 160 (`ERROR_BAD_ARGUMENTS`). Pass `-DBUILD_TESTING=OFF` at configuration
time only when intentionally building without the smoke test.

For Explorer integration, follow the
[dedicated testing procedure](EXPLORER_TESTING.md) before any live attachment.
