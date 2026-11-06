# Agent Development Rules

## Project overview

Quickky Explorer Tabs is a Windows File Explorer enhancement for users who want
folder windows collected into a single tabbed workspace. Its accepted scope
includes Quickky-owned tabs on Windows 10/11 and replacement of Windows 11 native
tab behavior, with an English/Simplified Chinese management interface. This
repository currently contains only the scaffold, design documents, and initial
language catalogs; no runtime application exists.

## Core logic

No runtime core logic is implemented. Do not present capture, native-tab
replacement, grouping, recovery, or locale selection as working features.
The accepted contracts and unverified mechanisms are documented in the overall
design, not duplicated here as an implemented execution flow.

## Modules

All listed directories exist but contain no runtime code:

- `src/app/`: Application composition and controller-side communication boundary.
- `src/capture/`: Explorer folder-window discovery and capture coordination.
- `src/group/`: Quickky tab ownership and active-workspace state.
- `src/hook/`: Target-thread bootstrap, message bridge, and local recovery.
- `src/explorer/`: Explorer adaptation and native-tab replacement boundary.
- `src/chrome/`: Quickky tab presentation and input boundary.
- `src/settings/`: Management preferences and localization boundary.
- `src/common/`: Minimal cross-module identifiers and shared IPC contracts.

## Repository directories

- `src/`: Reserved native application and DLL source modules.
- `resources/locales/`: Initial `en-US` and `zh-CN` management-interface catalogs.
- `tests/`: Validation scope documentation; no automated tests exist yet.
- `docs/design/`: Overall architecture and individual module contracts.
- `docs/guides/`: Development and validation standard operating procedures.
- `README.md`: English user-facing features, availability, and usage.
- `README.zh-CN.md`: Equivalent Simplified Chinese user documentation.
- `LICENSE`: MIT license terms for this open-source project.

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
- Do not silently replace Quickky-owned tabs with a native Explorer tab backend.
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
- Claiming native-tab replacement from visual masking alone.
- Live Explorer integration tests on an unscoped daily desktop.
- Changing the MIT license without explicit maintainer authorization.

## SOP index

- [Development workflow](docs/guides/DEVELOPMENT.md)
- [Validation and review](docs/guides/VALIDATION.md)
- [Explorer integration testing](docs/guides/EXPLORER_TESTING.md)
