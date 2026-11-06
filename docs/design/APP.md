# Application

## Status

`src/app/main.cpp` implements a minimal C++17 Unicode Win32 entry point. It opens
an empty window with the product name, runs a message loop, and exits on close.
It does not implement controller composition or Explorer integration.
The [overall design](PROJECT_DESIGN.md) defines the remaining product boundary.

## Implemented behavior

- The CMake target `QuickkyExplorerTabs` builds a Windows x86_64 GUI executable.
- Normal launch creates and shows a standard empty application window.
- `--smoke-test` creates the same window without showing it, posts `WM_CLOSE`,
  and uses the normal message loop to destroy it and exit with status zero.
- Other command lines exit with `ERROR_BAD_ARGUMENTS` (160). Win32 setup or
  message-loop failures return a nonzero status.
- The executable neither enumerates Explorer windows nor initializes COM,
  installs hooks, creates tabs, loads language catalogs, or persists settings.

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

No tray UI, startup registration, installer, IPC transport, or controller
singleton mechanism is implemented or selected. Management-page behavior belongs
to [Settings](SETTINGS.md); the empty bootstrap window is not a management page.

## Acceptance evidence

The CTest `app.smoke` check covers hidden-window creation, close-message dispatch,
destruction, and successful process exit, with a ten-second timeout.
It does not verify visible presentation or Explorer compatibility.

Integration acceptance still requires attachment acknowledgment, duplicate
controller handling, and restored Explorer windows after controller termination.
Those integration checks have not been executed.
