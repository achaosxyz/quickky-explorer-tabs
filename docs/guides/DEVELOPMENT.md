# Development Workflow

## Current scope

The repository contains documentation, reserved source directories, and initial
locale catalogs. There are no build targets or runnable tests. C++20, MSVC, and
CMake are the selected native-development baseline, not an installed toolchain
or a working build configuration.

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

## Introducing the first source target

Add the smallest actual CMake target with the source it builds, and record exact
configure/build/test commands here in the same change. Do not add fake targets
or placeholder implementations just to make a scaffold appear buildable.

For Explorer integration, follow the
[dedicated testing procedure](EXPLORER_TESTING.md) before any live attachment.
