# Application

## Status

`src/app/main.cpp` composes read-only discovery, capture state, and group state in
a background C++17 controller. It never creates an independent visible window
or initiates attachment to Explorer.
The [overall design](PROJECT_DESIGN.md) defines the remaining product boundary.

## Implemented behavior

- The CMake target `QuickkyExplorerTabs` builds a Windows x86_64 GUI executable.
- Normal launch creates a message-only window and a session-local singleton,
  initializes STA COM, and maintains eligible candidates through WinEvent
  notifications and one-second reconciliation. No candidate becomes managed.
- A duplicate normal launch exits without stopping the existing controller.
- `--stop` signals the session-local stop event; absence of a controller is a
  successful no-op. Inaccessible/colliding objects are not reconfigured.
- `--snapshot` writes one read-only JSON snapshot to redirected standard output
  and exits. It emits numeric identifiers/generations, not folder paths/titles.
- `--observe-ms N` runs a separate read-only observer for 1..600000 milliseconds.
  Snapshot and bounded modes do not stop or replace the resident controller.
- `--smoke-test` skips discovery, creates a message-only window, posts `WM_CLOSE`,
  and uses the same message pump to destroy it and exit with status zero.
- Invalid arguments exit with `ERROR_BAD_ARGUMENTS` (160); setup/discovery failures
  return nonzero. Observer callbacks are confined to the message thread.
- The controller does not inject DLLs, subclass Explorer, change visibility,
  create real tabs, load language catalogs, or persist settings. Normal operation
  is unelevated; elevating it can exclude medium-integrity Explorer candidates.

## Responsibility

Own application lifetime, compose the capture/group/settings modules, and own
the controller side of communication with Explorer integration instances.

## Integration contract (not implemented)

- Scope one controller to the current user session; do not interfere with other
  users or require elevation for ordinary operation.
- Keep policy in capture/group modules, not in IPC handlers or the entry point.
- Track integration instances by validated identity, not an unverified PID alone.
- Treat attachment requests as incomplete until acknowledged by the target.
- Normal shutdown requests restoration and detachment before releasing control.
- Controller loss must also be observable from the DLL; cleanup cannot depend
  exclusively on the controller staying alive.

## Boundary

No tray UI, startup registration, installer, or integration IPC transport exists.
The named mutex/event are local lifetime controls, not an Explorer command
protocol. Management-page behavior belongs to [Settings](SETTINGS.md); this
background controller is not a management page or a replacement file manager.

## Acceptance evidence

The CTest `app.smoke` check covers message-only window creation, close-message dispatch,
destruction, and successful process exit, with a ten-second timeout.
It does not verify visible presentation or Explorer compatibility.

Model tests cover acknowledgment and stale identity/token handling. Fixture-only
Win32 tests cover discovery events and failure containment. Bounded read-only
snapshot/observation, duplicate normal launch, and `--stop` have been exercised on
the development host. No native-tab replacement or window restoration has been
validated on real Explorer.
