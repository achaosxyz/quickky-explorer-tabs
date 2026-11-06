# Project Design

## Status and authority

This repository contains a background C++17 Win32 controller, read-only Explorer
discovery, a pure capture lifecycle and tab model, CMake/CTest targets, design
contracts, and initial language catalogs. The controller has no independent
visible window. It observes candidates but never requests attachment or changes
Explorer windows. Actual tab presentation, native-tab replacement, the current-
folder `+` action, recovery, and management settings remain unimplemented.

`docs/design/` is the single source of truth for project structure and design.
This document owns the overall architecture; the linked module documents own
their local contracts. Module documents must remain consistent with this file.
`AGENTS.md` is a context summary and contributor rule index, not a substitute
for this design. Direct user and host instructions take precedence.

## Product scope

Quickky Explorer Tabs is a Windows File Explorer enhancement for people who
want folder windows collected into a single tabbed workspace while retaining
the real Explorer browsing experience. The project is maintained as open source
under the MIT license in the root `LICENSE` file.

The accepted requirements are:

- Support Windows 10 and Windows 11 on x86_64.
- Use Quickky-owned tabs on both systems, with a Windows 11-inspired appearance.
- Replace Windows 11's native tab behavior rather than use native tabs as the
  backend or merely cover their pixels. Native tabs must not remain independently
  usable inside a managed window.
- Automatically collect newly opened Explorer folder windows into the workspace.
- Run the controller in the background, with tabs attached to real Explorer
  windows; do not provide an independent Quickky file-manager window.
- The tab-strip `+` action opens a new Explorer folder window at the active
  Quickky tab's current location and admits that window through normal capture.
  Resolve the location at click time, not from the tab's original launch path.
- Provide an English and Simplified Chinese management interface.

Window uniqueness means one visible managed Explorer workspace in the active
virtual desktop of the current user session. It does not mean one Explorer
process or one underlying HWND. Unsupported windows must remain usable outside
the group instead of being hidden to enforce uniqueness.

Desktop and taskbar windows, file picker dialogs, other applications, cross-user
or cross-integrity capture, and cross-virtual-desktop merging are outside the
boundary. Exact supported OS builds are not yet verified.

## Technology baseline

- C++17, Win32, and a single-threaded COM apartment for the controller and probes.
- Direct2D and DirectWrite for the Quickky tab strip.
- CMake 3.21 or newer and MSVC with C++17 support for Windows x86_64 builds.
  The local build path uses Visual Studio 2017 Build Tools and its v141 toolset.
- An application process and an Explorer-only integration DLL.
- UTF-8 JSON catalogs for management-interface localization.

No third-party dependencies, package manager, UI framework, installer, or native
Explorer private API has been selected. CMake builds `QuickkyExplorerTabs.exe`,
the `quickky_core` and `quickky_capture` static libraries, and model/discovery
test executables when testing is enabled. No integration DLL target exists yet.

## Module boundaries

| Directory | Responsibility | Design |
| --- | --- | --- |
| `src/app/` | Application lifetime, composition, and the application-side IPC endpoint. | [Application](APP.md) |
| `src/capture/` | Folder-window discovery, identity checks, and capture coordination. | [Capture](CAPTURE.md) |
| `src/group/` | Quickky tab ownership, active-window selection, and group state. | [Group](GROUP.md) |
| `src/hook/` | Target-thread bootstrap, message bridging, recovery, and the DLL-side IPC endpoint. | [Hook](HOOK.md) |
| `src/explorer/` | Build-specific Explorer integration and native-tab suppression. | [Explorer](EXPLORER.md) |
| `src/chrome/` | Quickky tab rendering, hit testing, and presentation input. | [Chrome](CHROME.md) |
| `src/settings/` | Management-page settings and locale selection. | [Settings](SETTINGS.md) |
| `src/common/` | Shared identifiers and narrowly scoped cross-process contracts. | [Common](COMMON.md) |

`app`, `capture`, `group`, and `common` contain code; `hook`, `explorer`, `chrome`,
and `settings` remain reserved. The accepted composition assigns capture, group,
and settings to the application and Explorer
adapters and chrome to the DLL. Shared definitions belong in `common`; product
policy does not. IPC transports and integration interfaces remain unspecified.

## Repository layout

```text
AGENTS.md
README.md
README.zh-CN.md
LICENSE
CMakeLists.txt
src/app/main.cpp
src/capture/{capture_coordinator,explorer_discovery,explorer_window_probe}.{h,cpp}
src/group/group_model.{h,cpp}
src/common/window_identity.h
src/{app,capture,group,hook,explorer,chrome,settings,common}/
resources/locales/{en-US,zh-CN}.json
tests/{core_tests,discovery_tests}.cpp
tests/README.md
docs/design/PROJECT_DESIGN.md
docs/design/{APP,CAPTURE,GROUP,HOOK,EXPLORER,CHROME,SETTINGS,COMMON}.md
docs/guides/{DEVELOPMENT,VALIDATION,EXPLORER_TESTING}.md
```

The root also contains `.editorconfig` and `.gitignore`. `resources/` contains
non-code assets; `tests/` contains executable checks and documents their boundaries.
`docs/guides/` contains operating procedures rather than architecture decisions.

## Data model

The concrete core contracts are:

- `WindowCandidate`: native handle, process/thread IDs, and process creation time.
- `WindowIdentity`: a candidate plus a monotonically allocated discovery
  generation. A removed/reobserved window receives a new generation.
- `GroupModel`: ordered tab IDs and window identities, with optional active tab.
  It performs no Win32 operations and does not own or close native windows.
- `CaptureCoordinator`: tracks discovered, attaching, managed, and restoring
  records. Successful attachment acknowledgment is the only admission path;
  each attachment attempt has a unique token to reject late acknowledgments.

No wire format, process injection, or native-window visibility policy is
implemented. These model types do not constitute successful Explorer capture.
The remaining conceptual entities are:

- **Explorer window identity:** HWND, owning process/thread, and an attachment
  generation sufficient to distinguish a destroyed window from a reused handle.
- **Quickky tab:** a stable application-owned ID associated with a managed
  Explorer folder window. A native Explorer tab is not a Quickky tab.
- **Group:** ordered Quickky tabs, an active tab, and the common window placement.
- **Attachment:** the capture lifecycle and the original state required to undo
  Quickky changes without closing the user's Explorer window.
- **Settings:** management-interface locale (`en-US` or `zh-CN`). Initial catalogs
  exist, but runtime selection and persistence do not.

Concepts are not permission to introduce generic frameworks or speculative
serialization. Concrete types and IPC payloads must be documented with their
first implementation.

## Read-only discovery boundary

`ExplorerWindowProbe` accepts visible, unowned, uncloaked `CabinetWClass` top-level
windows whose full executable path is the system Explorer path. It checks the
current user, session, integrity level, and virtual desktop, and fails closed if
the required identity or desktop queries fail. It records no paths or captions.

`ExplorerDiscovery` installs out-of-context WinEvent observations before the
initial enumeration, reconciles candidates on relevant events and a bounded
refresh timer, and coalesces duplicate identities. Removing a candidate means
it is no longer eligible, not necessarily that its native window was destroyed.
Observed destroy events force a new identity if the handle has already been
reused. Generations protect model commands; they are not a Windows guarantee
that no lifecycle event can ever be missed.

The controller remains message-only. `--snapshot` writes a read-only JSON
snapshot to redirected standard output; `--observe-ms N` runs a bounded observer;
`--stop` signals the current session's controller. Snapshot and bounded-observer
modes do not replace or stop the resident controller. No normal code path begins
attachment until an Explorer adapter can confirm native-tab replacement and
recovery. Consequently no window is hidden, regrouped, closed, or injected into.

## Key flows

The implemented executable flow is:

1. Validate arguments. Normal launch acquires the session-local controller
   singleton and stop event, using the current user's default object ACLs.
2. Initialize COM and create a message-only window, never a file-manager window.
3. Start read-only discovery. Added candidates enter discovered state; removed
   candidates invalidate that identity. No attachment request is issued.
4. Reconcile on events and a one-second timer. `--stop` signals the resident;
   `--observe-ms N` closes its own bounded observer after 1..600000 milliseconds.
5. `--snapshot` emits candidate identifiers, generation, and the managed count to
   redirected standard output and exits. `--smoke-test` skips Explorer discovery
   and closes its own message-only window through the message pump.

Invalid arguments return `ERROR_BAD_ARGUMENTS` (160); setup/discovery failures
return nonzero. Stop is idempotent when no controller event exists. Normal
duplicate launch exits without stopping the original controller. Snapshot and
bounded modes do not acquire or signal the resident singleton. Object-name
collisions or inaccessible objects fail rather than changing permissions.

The unimplemented Explorer behavior contracts are:

1. Discover an eligible Explorer folder window and validate its identity.
2. Request attachment on its UI thread; verify the Explorer adapter can safely
   replace native-tab behavior before admitting the window to the group.
3. Commit the Quickky tab only after attachment succeeds; keep or restore the
   ordinary Explorer window if attachment fails.
4. Route Quickky tab actions through group policy and the target-window bridge.
5. On detach, application loss, or unsupported integration, restore affected
   windows and remove Quickky behavior without terminating Explorer.

The candidate grouping mechanism retains each Explorer top-level window and
shows the active member at the group placement while hiding inactive members.
This mechanism is unverified. Cross-process `SetParent` is not the baseline.
Reliable automatic collection is required; universally flash-free creation is
not claimed. Path-only reopening must not discard file selection or view state.

## Integration constraints and validation gates

Windows 10 and Windows 11 adapters isolate Explorer differences. Version names
alone are not capability checks. Unknown or failed integrations leave Explorer
unmanaged, usable, and visible. No native-tab disabling mechanism is established.

Before a usable integration can be claimed, evidence must establish:

- Safe UI-thread attachment and detachment on explicitly recorded OS builds.
- Complete native-tab replacement, including non-keyboard entry points and
  windows that already contain native tabs; visual masking alone does not pass.
- Correct tab switching, focus, taskbar/Alt+Tab behavior, placement, modal windows,
  drag-and-drop, and multi-monitor DPI handling.
- Collection of supported launch sources without losing navigation or selection.
- Restoration of hidden windows when the controller disconnects or exits.

These gates are unverified design constraints, not a release roadmap or a record
of successful tests. If a gate requires a new private mechanism or a change of
architecture, document the evidence and obtain agreement on the changed boundary
before implementing it. Do not silently substitute the native-tab backend.

## Documentation and procedures

- [Development workflow](../guides/DEVELOPMENT.md)
- [Validation and review](../guides/VALIDATION.md)
- [Explorer integration testing](../guides/EXPLORER_TESTING.md)

All development material, design documents, comments, and commit messages use
English. `README.zh-CN.md` is the Chinese user-documentation exception. Chinese
translation values in the `zh-CN` catalog are required localization data, not
an exception permitting Chinese development documentation.
