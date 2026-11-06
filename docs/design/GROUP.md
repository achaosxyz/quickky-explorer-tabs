# Group

## Status

`GroupModel` implements ordered tab identity, selection, removal, and reordering
without Win32 side effects. No native window-switching implementation exists.
See the [overall design](PROJECT_DESIGN.md).

## Responsibility

Own Quickky tab identity, ordering, active-tab selection, and the shared placement
of the visible managed workspace.

## Contract

- A Quickky tab refers to a managed Explorer folder window, not a Windows 11
  native tab. Preserve the underlying Explorer browsing state.
- Scope grouping to the current user session and active virtual desktop.
- Admit a window only after the integration confirms safe attachment.
- Duplicate admission is idempotent and does not change selection. A conflicting
  lifetime for the same handle must be removed before admitting its replacement.
- Removing the active tab selects its right neighbor, or its left neighbor when
  no right neighbor exists. Removing the last tab clears the active selection.
- Reordering preserves tab IDs and active selection; invalid requests do nothing.
- The `+` command targets the active tab's current folder and uses normal capture
  for the newly opened Explorer window. No folder launcher or `+` UI exists yet.
- The candidate presentation strategy shows the active Explorer window and hides
  inactive members, without reparenting them into a foreign-process container.
- Tab switching must preserve usable focus and common window placement. Taskbar,
  Alt+Tab, minimize/maximize, snapping, and modal dialogs require explicit tests.
- Closing a tab closes only its associated folder window. Closing the workspace
  requests closure of its managed folder windows, never process termination.
- Recovery and explicit detach restore windows instead of applying close actions.
  The integration retains the state needed to undo all Quickky mutations.

## Boundary

No session persistence, tab drag-out, multi-group mode, or path-based state
reconstruction is included in the scaffold. Do not turn window grouping into a
general-purpose application container. Presentation belongs to [Chrome](CHROME.md).

## Acceptance evidence

`core.model` validates ordering/selection policy and capture coordination without
Win32 side effects. Validate the candidate presentation strategy on real Explorer windows
before claiming a single-window user experience.
