# Explorer Adapters

## Status

`src/explorer/` is reserved. No adapter, private API binding, or verified OS-build
support exists. See the [overall design](PROJECT_DESIGN.md).

## Responsibility

Isolate Explorer-specific window integration and replace native-tab behavior
where necessary, exposing a consistent Quickky-owned interaction boundary.

## Contract

- Separate Windows 10 and Windows 11 integration responsibilities without
  duplicating capture, grouping, or rendering policy.
- Validate capabilities against the actual Explorer build and observed layout;
  an OS version label or a hard-coded window-class path is not enough.
- Preserve Explorer navigation, commands, content interaction, and accessibility
  outside the replaced tab/chrome area.
- On Windows 11, disable or intercept native-tab creation and interaction rather
  than drive native tabs as Quickky's backend.
- Covering the tab strip or intercepting `Ctrl+T` alone is not replacement. Account
  for native buttons, menus, mouse actions, shortcuts, and existing native tabs.
- Do not silently close existing native tabs to satisfy the one-tab invariant.
  If an existing multi-tab window cannot be preserved safely, leave it unmanaged
  and report the unsupported condition.
- If native-tab replacement cannot be established or layout changes unexpectedly,
  reject/detach the window and restore ordinary Explorer behavior.

## Boundary

No supported replacement mechanism has been established. Private XAML tree
mutation, private commands, registry/feature-flag changes, and binary patching
are not approved merely by listing an adapter directory. A concrete mechanism
requires documented evidence and agreement on its compatibility boundary.

## Acceptance evidence

Record exact OS/Explorer builds, every tested native-tab entry point, existing
multi-tab handling, and restoration behavior. Do not claim all Windows 10/11
builds are supported from a test on one machine.
