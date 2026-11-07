# Group

## Status

`GroupModel` implements ordered tab identity, selection, removal, and reordering
without Win32 side effects. `WorkspaceController` composes it with the hook
client for experimental real-window admission and visibility switching.
See the [overall design](PROJECT_DESIGN.md).

## Responsibility

The native backend retains a main frame and admits eligible source frames only
after validating every folder against the optional test root. It snapshots the
active source GUID/PIDL, acknowledges native destination creation, and removes
only that same source GUID after revalidation. Remaining source tabs are handled
on later reconciliations. A mutation with an unsuccessful or uncertain outcome
quarantines that source identity until disable/re-enable; automatic retries must
not duplicate tabs. Disable/exit removes integration without splitting native
tabs back into windows.

Own workspace collection policy. On Windows 10, own Quickky tab identity,
ordering, selection, and placement; on Windows 11, select the main native frame
and coordinate native-drop transfer without duplicating native tab state or
promising preserved browsing history/selection.

## Contract

- The Windows 10 model represents managed Explorer folder windows. Windows 11
  native tabs remain owned by Explorer and are not mirrored as custom tab labels.
- On Windows 11, select a surviving eligible main frame and merge subsequent
  eligible source frames through verified native transfers. Reconcile uncertain
  outcomes instead of repeating a transfer or closing a source blindly.
- Scope grouping to the current user session and active virtual desktop.
- Admit a window only after the integration confirms safe attachment.
- After batch admission, retain a visible managed foreground window as active;
  candidate handle ordering must not hide the user's current foreground root.
- Duplicate admission is idempotent and does not change selection. A conflicting
  lifetime for the same handle must be removed before admitting its replacement.
- Removing the active tab selects its right neighbor, or its left neighbor when
  no right neighbor exists. Removing the last tab clears the active selection.
- Reordering preserves tab IDs and active selection; invalid requests do nothing.
- The `+` command targets the active tab's current folder and uses normal capture
  for the newly opened Explorer window; labels are not a navigation backend.
- Queue New input with its source-thread PIDL snapshot; later navigation must not
  change the destination. Clone bounded validated PIDL bytes without Shell COM
  inside the synchronous IPC receiver, then resolve names/launch after return.
- The candidate presentation strategy shows the active Explorer window and hides
  inactive members, without reparenting them into a foreign-process container.
- Tab switching must preserve usable focus and common window placement. Taskbar,
  Alt+Tab, minimize/maximize, snapping, and modal dialogs require explicit tests.
- Closing a tab closes only its associated folder window. There is no close-all
  action; application exit restores members and never terminates Explorer.
- Recovery and explicit detach restore windows instead of applying close actions.
  The integration retains the state needed to undo all Quickky mutations.

## Boundary

`WorkspaceController` composes the existing pure group and capture models with
the scoped hook client. It owns admission, shared placement, selection, close,
and click-time folder launch. Original HWNDs are never reparented. A hidden
managed window remains managed until destruction or identity change; new visible
candidates obey the optional explicit test-root boundary. At most 32 windows
are admitted by the current bounded bridge; additional windows stay ordinary
Explorer windows. Failed attachment
does not hide anything. A failed presentation stops the scoped workspace and
cancels its attachment leases instead of leaving a partially hidden group.
Release cancels the kernel lease before requesting detach. If the target cannot
acknowledge, controller membership is retired and restoration is delegated to the
already-signaled target watchdog; it is not reported as confirmed restoration.
This terminal handoff differs from a retryable pure-model restore transaction.

No session persistence, tab drag-out, multi-group mode, or path-based state
reconstruction is included in the scaffold. Do not turn window grouping into a
general-purpose application container. Presentation belongs to [Chrome](CHROME.md).

## Acceptance evidence

`core.model` validates ordering/selection and capture coordination without Win32
side effects. Scoped Windows 10 build 19045 checks cover capture, selection,
current-folder opening, tab close, maximize/minimize/restore, tray disable/re-enable,
normal exit, and controller-loss restoration. Scoped Windows 11 checks cover
native merge into a maximized destination, multi-tab sources, current-folder
`+`/Ctrl+T, and preserving merged tabs on exit; the matched profile and limitations
are recorded in [Explorer](EXPLORER.md). Focus, modal dialogs, snapping, and other
release gates still need independent evidence.
