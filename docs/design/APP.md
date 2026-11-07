# Application

## Status

The C++17 application composes Explorer discovery, capture/group state, an
experimental workspace bridge, and per-user preferences. It has no independent
browsing window. The [overall design](PROJECT_DESIGN.md) defines the accepted
scope and the distinction between preview evidence and release compatibility.

## Lifetime and modes

The manifest declares Windows 10/11 compatibility so version-helper queries can
choose the Windows 11 backend instead of a compatibility-limited OS version.
Target-side ABI matching remains a separate capability check.

The message loop checks its stop event while draining messages as well as in
the outer wait. A continually busy reconciliation timer queue must not starve
shutdown.

The application target is `QuickkyExplorerTabs`; its distributed filename is
`QTabs.exe`. The adjacent `QuickkyExplorerHook.dll` retains its existing name.
Version and build-time resources come from the root `VERSION` file and the
per-build metadata generator. Packaging and publication are defined by the
overall design and [release SOP](../guides/RELEASE.md).

The endpoint stops accepting input before workspace teardown and is destroyed
before its stack context/COM apartment. While a tray popup or About dialog owns
a nested message loop, the controller timer observes `--stop` and dismisses only
those application-owned popups so orderly restoration can finish.

- Resident launch owns a session-local singleton and stop event, STA COM, and a
  message-only controller endpoint. Duplicate launch exits without replacing it.
- The notification icon has a hidden top-level broker window so it can receive
  `TaskbarCreated` broadcasts and own the popup menu. It is never shown as an
  application window. The icon is loaded from the EXE's `logo.ico` resource.
- Tabs are disabled by default. Explicitly saved enablement starts the
  experimental workspace; disabling or exiting restores its members.
- `--capture-test-root PATH` validates an existing directory, uses the resident
  singleton, and enables capture only within that root. It does not persist the
  global tabs setting.
- `--stop` signals the resident stop event; absence is a successful no-op.
  Inaccessible or colliding kernel objects are not reconfigured.
- `--snapshot` emits one read-only JSON candidate snapshot to redirected stdout;
  `--observe-ms N` observes for 1..600000 milliseconds without replacing the
  resident controller. Neither mode attaches or creates a tray icon.
- `--smoke-test` skips discovery and tray registration, creates a message-only
  window, and closes it through the message pump.
- Invalid arguments return `ERROR_BAD_ARGUMENTS` (160); setup failures return
  nonzero. The embedded manifest requests `asInvoker`, not automatic UAC.

## Composition and commands

`TrayIcon` owns notification registration, taskbar recreation, localized popup
presentation, the explicitly requested About dialog, and icon cleanup. The four
actions are startup, tabs enablement, About, and exit. About displays the author,
email, version, UTC build timestamp, and a clickable GitHub link. It opens a
browser only when that link is activated.

Main-loop policy composes the workspace and settings. The application retains
one staged hook library across enable/disable cycles. Its IPC callback validates
and queues bounded input; it never performs COM navigation or visibility changes
inside that callback. Group actions execute after callback return.

Discovery removal does not forget intentionally hidden managed windows.
Workspace reconciliation revalidates them, including lifetime, process/user/
session/integrity, desktop, native view, and the optional folder boundary.
Normal shutdown cancels all leases before requesting individual detachment,
while the controller endpoint is still alive. DLL watchdogs independently cover
controller loss and a failed or delayed IPC acknowledgment.

## Boundaries and evidence

The hidden tray broker is not a management page or file manager. Snapshot and
smoke results are not proof of real Explorer integration. Synthetic CTest checks
exercise protocol rejection, attach/detach, and lease recovery in the test
process; scoped host tests establish only their recorded Explorer paths.
General Windows 10/11 compatibility and complete native-tab replacement remain
unverified. See [Explorer](EXPLORER.md) and the integration SOP.
