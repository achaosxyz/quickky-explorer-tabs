# Agent Development Rules

## Project overview

Quickky Explorer Tabs is a Windows File Explorer enhancement for users who want
folder windows collected into a single tabbed workspace. Its accepted scope
uses Quickky-owned rectangular tabs on Windows 10 and preserves native Explorer
tabs on Windows 11, collecting new windows through native tab-drop
transfer without an additional strip, plus an English/Simplified Chinese
management interface. This
repository contains a C++17 tray controller, Explorer discovery, capture/group
models, a target-thread hook bridge, experimental custom tabs and current-folder
`+`, per-user preferences, CMake/CTest checks, and bilingual language catalogs.
The integration is an early preview, not verified general Windows 10/11 support
or complete native-tab replacement.

## Core logic

1. Validate arguments and run a message-only background controller, without an
   independent Quickky browsing window. Normal duplicate launch exits harmlessly.
2. Discover eligible Explorer windows by initial enumeration, WinEvent observation,
   and periodic reconciliation. Verify process, user, session, integrity, and
   virtual-desktop identity. Admission excludes Control Panel and related
   management namespaces even when Explorer owns them. Discovery itself never
   mutates candidates.
3. Keep tabs disabled by default. Windows 10 custom groups require single-view
   windows and restore hidden members on disable/exit/controller loss. Windows 11
   uses an explicitly matched native ABI: confirm destination GUID/PIDL before
   deleting the revalidated source GUID, never a whole source frame. Native
   groups remain native on exit; uncertain transfers are quarantined without
   automatic retry. Timeout cannot roll back an already-started native call.
4. Support `--stop`, read-only `--snapshot`, bounded `--observe-ms N`, and a
   discovery-free `--smoke-test`, and directory-scoped `--capture-test-root PATH`.
   Diagnostic and smoke modes do not attach or create a tray icon. Invalid
   arguments return status 160.

Do not present observation or model tests as working native-tab replacement,
visible grouping, recovery, or locale selection. The `+` requirement is to open
the active tab's current location. Windows 10 captures the new window; Windows 11
creates a native tab without an additional strip. Do not promise intact history
or selection: the observed native drop semantics do not preserve them.

## Modules

All listed modules contain code; a separate management page is not implemented:

- `src/app/`: Background controller, tray, About dialog, and diagnostics.
- `src/capture/`: Explorer folder-window discovery and capture coordination.
- `src/group/`: Quickky tab ownership and active-workspace state.
- `src/hook/`: Target-thread bootstrap, message bridge, and local recovery.
- `src/explorer/`: Folder snapshots, matched native tab actions, and diagnostic guard.
- `src/chrome/`: Quickky tab presentation and input boundary.
- `src/settings/`: Management preferences and localization boundary.
- `src/common/`: Window/tab identities and a bounded, versioned hook protocol.

## Repository directories

- `src/`: Native application source and reserved integration modules.
- `resources/locales/`: Initial `en-US` and `zh-CN` management-interface catalogs.
- `resources/`: Icon/version resources and the `asInvoker` application manifest.
- `VERSION`: Single plain-text product version source.
- `cmake/`: Per-build version and UTC timestamp generation.
- `scripts/`: PowerShell build, ZIP packaging, release preview/publication, and
  release-script checks.
- `.github/workflows/`: Minimal Windows smoke checks and manually dispatched
  preview/release automation from `develop`.
- `tests/`: Pure model tests, synthetic Win32 discovery tests, and test boundaries.
- `docs/design/`: Overall architecture and individual module contracts.
- `docs/guides/`: Development and validation standard operating procedures.
- `README.md`: English user-facing features, availability, and usage.
- `README.zh-CN.md`: Equivalent Simplified Chinese user documentation.
- `LICENSE`: MIT license terms for this open-source project.
- `CMakeLists.txt`: Windows x86_64 controller/core targets and CTest checks.

## Single source of truth

@docs/design/PROJECT_DESIGN.md

- `docs/design/` is the SSOT for project structure and design.
- [PROJECT_DESIGN.md](docs/design/PROJECT_DESIGN.md) is the overall design.
- This file contains only project context, indexes, and agent behavior rules.
- When this summary conflicts with the overall design, follow the overall design,
  subject to higher-priority user and host instructions, and update this summary.
- Before modifying code, read the overall design, the relevant module documents,
  and this file's overview, core logic, module, and directory sections.
- Do not invent modules, directories, technologies, or abstraction layers absent
  from the accepted design. Document an in-scope design change before introducing
  the corresponding structure or behavior.
- Keep implementation consistent with design. Distinguish accepted contracts,
  implemented behavior, and verified behavior explicitly.

## Design index

- [Overall design](docs/design/PROJECT_DESIGN.md)
- [Application](docs/design/APP.md)
- [Capture](docs/design/CAPTURE.md)
- [Group](docs/design/GROUP.md)
- [Hook integration](docs/design/HOOK.md)
- [Explorer adapters](docs/design/EXPLORER.md)
- [Tab chrome](docs/design/CHROME.md)
- [Settings and localization](docs/design/SETTINGS.md)
- [Shared contracts](docs/design/COMMON.md)

## Development rules

- Use C++17 for native code; do not require C++20 or a newer toolchain without
  a concrete, agreed requirement. The local build uses VS 2017/v141.
- Use English for development/design documents, code comments, identifiers,
  issue/PR text, and commit messages. Use Conventional Commits, for example
  `feat: add capture policy tests` or `docs: clarify adapter boundaries`.
- `README.zh-CN.md` is the Chinese documentation exception; Chinese locale values
  are required resource data. Do not write Chinese development documentation.
- Keep both READMEs user-facing. Put architecture, builds, and development
  procedures in design documents and guides, not in the READMEs.
- Management-interface text must support `en-US` and `zh-CN` through catalogs;
  keep keys and placeholders aligned instead of hard-coding text in UI code.
- Make minimal, cohesive changes. Preserve unrelated work; do not refactor it.
- Do not add unrequested dependencies, speculative extension points, generic
  frameworks, or abstractions without a concrete requirement.
- Respect Explorer-only scope. Never enforce a single workspace by terminating
  Explorer processes or discarding the user's tabs/windows.
- Keep the controller background-only; do not add an independent Quickky file
  manager. Do not run ordinary Explorer discovery elevated or request UAC without
  a concrete privileged operation. Read-only observation does not authorize
  injection or visibility changes on the development desktop.
- Keep the platform split explicit: Windows 10 custom tabs, Windows 11 native
  tabs. Do not substitute path reopening/closing for state-preserving transfer.
- Perform validation proportional to risk. Report actual results and limitations;
  documentation checks do not prove native builds or Explorer compatibility.
- Do not commit, push, publish, or deploy without explicit authorization.

## Documentation update rules

- Update the overall design for every structural or core-logic change. Update
  affected module documents in the same change.
- Module additions/removals require updating the module boundaries and layout.
  Data changes require updating the data model; flow changes require updating
  key flows. Update these summaries when their content is affected.
- Keep documentation about the current repository and accepted design baseline;
  do not add historical logs, speculative roadmaps, or TODO inventories.
- Correct design drift before changing code, without treating accidental code
  behavior as an approved product decision. Do not leave known design/code drift
  as an indefinite follow-up.
- Update both user READMEs for user-visible changes, without claiming unavailable
  features are implemented.

## Prohibited actions

- Unrequested refactors or unrelated fixes.
- Undocumented module, structure, or core-flow changes.
- Unconfirmed dependencies or future-proofing abstractions.
- Treating an undefined or unverified design as an established fact.
- Claiming native-tab migration from visual masking or path reopening alone.
- Live Explorer integration tests on an unscoped daily desktop. The application's
  hidden-window smoke test does not attach to Explorer and is outside that scope.
- Changing the MIT license without explicit maintainer authorization.

## SOP index

- [Development workflow](docs/guides/DEVELOPMENT.md)
- [Validation and review](docs/guides/VALIDATION.md)
- [Explorer integration testing](docs/guides/EXPLORER_TESTING.md)
- [Release preview and publication](docs/guides/RELEASE.md)
