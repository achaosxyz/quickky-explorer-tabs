# Project Design

## Status and authority

The repository contains a C++17 tray application, read-only Explorer discovery,
capture/group models, a target-thread bridge, Windows 11 native tab actions,
an experimental Windows 10 custom strip, per-user settings, and CMake/CTest
checks. Explicit enablement selects the platform backend. There is no independent
browsing window.

The accepted platform split is Windows 10 custom tabs and Windows 11 native
tabs/transfer. The existing custom-tab bridge is not evidence of native transfer;
its Windows 11 clipping and creation-guard path is not the accepted backend.
Native merge follows Explorer's tab-drop semantics, not an assertion that a
browser object moves intact. A scoped cross-process native drag on the tested
host merged the tab but lost selection and navigation history. Those limitations
must remain explicit; native appearance is not evidence of state preservation.

This is an early preview, not complete native-tab replacement or general
Windows 10/11 compatibility. Windows 11 uses five built-in accessor families and
complete-entry code variants described in [native ABI adaptation](NATIVE_ABI.md).
Only the original image has live acceptance; images without an unambiguous mode
match leave windows ordinary. Runtime symbol loading and downloads
are not used.
Windows 10 build 19045 has scoped live acceptance for custom capture, current-folder
creation, maximized switching, minimize/restore, tray actions, and recovery.
Other layouts/builds remain unverified.
The remaining release gates below are not satisfied by rendering over a row.

`docs/design/` is the single source of truth for project structure and design.
This document owns the overall architecture; the linked module documents own
their local contracts. Module documents must remain consistent with this file.
`AGENTS.md` is a context summary and contributor rule index, not a substitute
for this design. Direct user and host instructions take precedence.

## Product scope

QTabs is a Windows File Explorer enhancement for people who
want folder windows collected into a single tabbed workspace while retaining
the real Explorer browsing experience. The project is maintained as open source
under the MIT license in the root `LICENSE` file.

The accepted requirements are:

- Support Windows 10 and Windows 11 on x86_64.
- On Windows 10, use Quickky-owned rectangular tabs and manage the visibility of
  their Explorer folder windows.
- On Windows 11, retain Explorer's native tab bar and native tab lifecycle. Collect
  newly opened windows into the main Explorer frame through native tab transfer;
  do not draw an additional strip, clip the native host, or block native tab use.
- Use the verified native drop primitives and acknowledge destination creation
  before removing the source native tab. Do not claim complete state preservation
  or substitute an unacknowledged path launch followed by closing a whole frame.
  Unsupported operations leave source windows usable. Unconfirmed mutations
  quarantine the source identity: an in-flight native call may finish after
  timeout, so failure is not proof of rollback or source preservation.
- Automatically collect newly opened Explorer folder windows into the workspace.
  Exclude Control Panel and related management namespaces even when Explorer
  owns their windows. Preserve ordinary filesystem and virtual browsing roots.
- Run the controller in the background, with tabs attached to real Explorer
  windows; do not provide an independent Quickky file-manager window.
- On Windows 10, `+` opens a new Explorer folder window at the active Quickky
  tab's current location and admits that window through normal capture. On
  Windows 11, `+` creates a native tab at the active native tab's current location.
  Snapshot the absolute PIDL on the source UI thread at click time, not from the
  original launch path or the source's later location when a queued action runs.
- Provide an English and Simplified Chinese management interface.

Window uniqueness means one visible managed Explorer workspace in the active
virtual desktop of the current user session. It does not mean one Explorer
process or one underlying HWND. Unsupported windows must remain usable outside
the group instead of being hidden to enforce uniqueness.

Desktop and taskbar windows, file picker dialogs, other applications, cross-user
or cross-integrity capture, and cross-virtual-desktop merging are outside the
boundary. Exact supported OS builds are not yet verified.

## Technology baseline

### Tray-controlled integration

The application owns a notification-area icon using the repository's `logo.ico`,
not an independent visible window. Its localized context menu contains exactly
Start at sign-in, Enable Tabs, Compatibility check, About, and Exit. The explicitly
requested compatibility dialog reports the checked ExplorerFrame fixed binary
version and a read-only built-in ABI check; it does not exercise a live transfer.
Windows 10 reports the custom-tab backend separately from native ABI support.
Normal launch starts with tabs disabled
unless the user previously enabled them. Enabling tabs composes the integration
controller; disabling tabs or exiting restores managed windows first. The
explicit test-root mode always restricts capture to its root and never changes
the persisted enable-tabs preference. Automated live tests must still use this
scoped mode; a tray action is not authorization to test on unrelated windows.

Preferences use the current user's `Software\QTabs` registry key.
Startup uses only `QTabs` in the current-user `Run` key. Settings and internal
object names use QTabs identities, without reading or migrating older names. Menu
text comes from the aligned language catalogs, embedded at build time; locale
resolution uses an explicit preference when present, then Windows UI language,
then English. The application manifest requests `asInvoker`, not automatic UAC:
the controller and Explorer must have matching integrity levels.

The root `VERSION` text file is the single version source. CMake
reads it during configuration and regenerates native build metadata on each
build through `cmake/build_metadata.cmake`. The executable icon/version resource
and About dialog use this metadata. About displays author aCHAOS, contact
`achaosxyz@gmail.com`, the full clickable project GitHub URL, version, and UTC build time.

### Build, package, and release contract

Development uses `develop`; `main` receives the selected release candidate before
hosted smoke testing. Release tags identify published source. The CMake project
and application target are `QTabs`, with output filename `QTabs.exe`. Its hook
DLL stays `QuickkyExplorerHook.dll`. PowerShell scripts in `scripts/` configure an
x64 Release build, run the discovery-free `app.smoke` check, and package the EXE,
DLL, version, license, and user documentation as `QTabs_vMAJOR.MINOR.PATCH.zip`.
The ZIP includes the active MSVC toolchain's redistributable runtime DLLs beside
the application. CMake records their source paths for packaging and their names
for the hook staging code. The hook library stages those app-local DLLs with its
per-run image, because Explorer has a different DLL-search context. Build-tree
runs may use the installed runtime. No runtime installer or system-level setup
is part of extraction or startup.

The Windows smoke workflow accepts pushes and pull requests for `develop` and
`main`, plus manual dispatch for an explicitly selected ref. It checks release
scripts and the packaged discovery-free smoke test without publishing or
changing branches. The release workflow independently gates publication on
those same checks.

Maintainer preview presents English release notes in the conversation for
confirmation; no standalone preview file is required. It does not build, move
refs, or create a GitHub prerelease. Automation stages notes and a pinned plan
as intermediate artifacts. The dispatch can supply the approved English body
through `release_notes`; otherwise it uses commit-based notes. The formal
workflow accepts only `develop`, first fast-forwards `main`
to that exact commit, then builds/smoke-tests it on Windows, publishes the tested
ZIP as `vMAJOR.MINOR.PATCH`, and finally increments the development version.

Only after successful publication does automation advance `develop` with a
version-only commit. An atomic push keeps `main` pinned to the tested commit
while advancing `develop`, rejecting concurrent branch changes without force.
The next version increments SemVer PATCH, retaining MAJOR and MINOR: `0.0.1`
becomes `0.0.2`. Resource-version overflow stops the release rather than carrying
into MINOR. No branch is force-pushed. A
diverged `main`, concurrent `develop` change, or repository protection failure
stops finalization; existing published assets are not overwritten. A failed smoke
test leaves `main` at the selected candidate, without a release or version bump;
automation never rewinds it. The release SOP documents inspection and recovery
of partial publication. These checks do
not establish Explorer compatibility or satisfy native-integration gates.

### Scoped integration experiment

An explicit `--capture-test-root PATH` mode may attach only Explorer windows
whose folder locations are all inside the selected test directory. The Windows
10 custom backend additionally requires a single view.
Normal launch remains read-only until tabs are explicitly enabled; observation
and smoke modes are always read-only. This experiment does not
relax the general native-tab replacement release gate or authorize modifying
pre-existing windows outside the selected test scope.

The controller composes `WorkspaceController`, `HookClient`, and the Explorer
adapters. Windows 11 retains a main native frame, snapshots source GUID/PIDL,
uses the matched native drop-add action, confirms destination GUID/PIDL, then
removes only the unchanged source GUID and verifies its absence. Multiple source
tabs are transferred over successive reconciliations. Uncertain mutation results
quarantine the source identity rather than retry an add that may have succeeded.
Disable/exit removes integration but does not split native groups.

The Windows 10 custom experiment composes `TabStrip`, shows an active Explorer
root at shared placement, and hides inactive roots with local lease recovery.
Its `+` snapshots the click-time PIDL and requests `opennewwindow` for capture.
It is limited to 32 tabs and one attachment per UI thread. The existing clipping
fixture is diagnostic, not evidence of a usable Windows 10 layout.
The classic presenter uses a non-activating, Explorer-owned caption tool window,
bounded by DWM's caption-button rectangle. It never resizes Explorer content or
clips a XAML host. It is a tab surface, not a separate browsing window. Root
movement, visibility, DPI, and destruction control its lifetime. Unsupported
caption geometry rejects admission. The child/clipping fixture has a separate
diagnostic protocol mode, never selected by the resident controller.
Classic admission accepts the observed Windows 10 Ribbon layout only when its
top dock stays below the caption and native caption hit tests identify a clear
contiguous span. Quick Access controls, resize borders, and system buttons remain
outside that span. DPI-aware frame metrics keep maximized captions on screen;
minimize hides the surface and restore recomputes its bounds. Unknown layouts
remain ordinary windows. Hidden/disabled owners hide the surface; reflow
preserves its owner's z-order band. Classic window notifications coalesce a
posted UI-thread reflow so Explorer's outer placement stack finishes before
native hit testing; the surface hides while that work is pending. Explicit
presentation reflows after `SetWindowPlacement` returns. Synthetic checks do not establish
Windows 10 Explorer compatibility.
The native bridge bounds snapshots and destination capacity to 128 tabs and
admits at most 32 frame attachments at once. Other ABI profiles, other Windows 10
layouts, accessibility, and general DPI/theme compatibility remain unverified.
Root and strip callbacks hold the attachment alive across reentrant Win32 calls.
Detach stops interception/restores the root immediately, but object destruction
waits until outstanding callback/method scopes have unwound.

Custom publication tracks acknowledged labels/selection and presentation per
attachment. Unchanged views and already-correct visibility/placement do not send
Configure/Present again; identity and folder admission are still reconciled.
Inactive and hidden-owner strips release device-dependent rendering resources;
transient caption-reflow hiding retains them. Pointer movement repaints only when
its visual hover target changes. Theme state is refreshed on system notifications
rather than read during every paint.

- C++17, Win32, and a single-threaded COM apartment for the controller and probes.
- Direct2D and DirectWrite for the Quickky tab strip.
- CMake 3.21 or newer and MSVC with C++17 support for Windows x86_64 builds.
  The local build path uses Visual Studio 2017 Build Tools and its v141 toolset.
- An application process and an Explorer-only integration DLL.
- UTF-8 JSON catalogs for management-interface localization.

No third-party dependencies, package manager, UI framework, or installer has been
selected. The Windows 11 adapter uses Explorer's private tab-actions interface;
five compiled accessor families and reviewed complete-entry variants recognize
the actual image without version ordering or fixed locator RVAs. Versioned maps
are offline regression fixtures under `tests/abi_reference/`, not a production
allowlist or part of production headers. CodeView identifies
the inspected image, not permission to bind. The image must have one valid,
unambiguous CodeView record. The reference-image catalog and its boundaries are maintained in
[native ABI adaptation](NATIVE_ABI.md). Reusable accessor rules validate
implementation and derive object displacements;
split lookup implementations must also validate their out-of-line loop fragment
and its backedge before their storage layout can be used;
only explicitly reviewed relocation, displacement, and diagnostic metadata
operands are normalized. Native storage must be in committed readable memory,
not execute-only or guarded pages. There is no download, runtime PDB parser, symbol
cache, or resolver worker. Read-only section scans discover candidate actions
tables through their data-accessor slot, validate all thirteen complete entries,
and independently locate a unique window table. Accessor/lookup relocation and
derived fields follow the reviewed family rules; other entry bodies remain
conservative complete-byte contracts. No general rel32 wildcarding is allowed.
Ambiguous tables, unmatched bodies or invalid storage reject mutation. The dedicated
[native ABI adaptation design](NATIVE_ABI.md) defines the reference catalog, recognition
family, compatibility limits, and extension procedure. Its
[source provenance](NATIVE_ABI.md#source-provenance) owns the checked-in DLL/PDB
source register, accepted image checksums, installed baseline and acquisition
rules; ignored research output is supporting evidence, not the only record.
CMake builds `QTabs.exe`,
`QuickkyExplorerHook.dll`, module static libraries, and synthetic/model test
executables when testing is enabled. Windows 11 retains the original tab host and
uses native add/remove actions, with current-folder `+` adapted on the managed
frame. No region clipping or second strip belongs to that backend.

## Module boundaries

| Directory | Responsibility | Design |
| --- | --- | --- |
| `src/app/` | Application lifetime, composition, and the application-side IPC endpoint. | [Application](APP.md) |
| `src/capture/` | Folder-window discovery, identity checks, and capture coordination. | [Capture](CAPTURE.md) |
| `src/group/` | Quickky tab ownership, active-window selection, and group state. | [Group](GROUP.md) |
| `src/hook/` | Target-thread bootstrap, message bridging, recovery, and the DLL-side IPC endpoint. | [Hook](HOOK.md) |
| `src/explorer/` | Native Windows 11 frame actions, folder adaptation, and Windows 10 layout boundary. | [Explorer](EXPLORER.md) |
| `src/chrome/` | Windows 10 Quickky rendering, hit testing, and presentation input. | [Chrome](CHROME.md) |
| `src/settings/` | Management-page settings and locale selection. | [Settings](SETTINGS.md) |
| `src/common/` | Shared identifiers and narrowly scoped cross-process contracts. | [Common](COMMON.md) |

All modules contain code. Capture, group, preferences, and `HookClient` run in
the application; the native-tab guard and chrome run in the target Explorer UI
thread. The application owns one staged hook library for its lifetime, including
tabs enable/disable cycles. Failed DLL staging/loading cleans up before retry.
Shared bounded wire values belong in `common`;
product policy does not. A separate management page is not implemented.

## Repository layout

```text
AGENTS.md
README.md
README.zh-CN.md
LICENSE
CMakeLists.txt
VERSION
cmake/build_metadata.cmake
scripts/{common,build,preview-release,publish-release,test-release}.ps1
scripts/Test-NativeAbiSamples.ps1
.github/workflows/{windows-smoke,release}.yml
logo.ico
src/app/{main,tray_icon}.cpp
src/app/workspace_refresh.h
src/app/single_instance.h
src/capture/{capture_coordinator,explorer_discovery,explorer_window_probe}.{h,cpp}
src/group/{group_model,workspace_controller}.{h,cpp}
src/hook/{explorer_hook.cpp,hook_client.h,hook_client.cpp}
src/explorer/{native_tab_guard,folder_location,native_frame,native_abi}.{h,cpp}
src/explorer/native_abi_patterns.cpp
src/explorer/native_abi_internal.h
src/explorer/abi_patterns/pattern.h
src/explorer/abi_patterns/family_{1,2,3,4,5}.cpp
src/chrome/tab_strip.{h,cpp}
src/settings/preferences.{h,cpp}
src/common/{window_identity,hook_protocol}.h
resources/{app.manifest,app.rc.in,resource.h,runtime_files.h.in,logo.png,tab-preview.png}
resources/locales/{en-US,zh-CN}.json
resources/locales/tray_strings.h.in
tests/{core_tests,discovery_tests,native_guard_tests,workspace_bridge_tests,workspace_refresh_tests}.cpp
tests/{workspace_performance_tests,hook_staging_tests}.cpp
tests/native_guard_probe.cpp
tests/native_transfer_probe.cpp
tests/app_instance_tests.cpp
tests/native_abi_tests.cpp
tests/native_abi_reference_selection.cpp
tests/abi_reference/{profile.h,registry.cpp,rejections.json,<VERSION>.cpp}
tests/README.md
docs/design/PROJECT_DESIGN.md
docs/design/{APP,CAPTURE,GROUP,HOOK,EXPLORER,NATIVE_ABI,CHROME,SETTINGS,COMMON}.md
docs/guides/{DEVELOPMENT,VALIDATION,EXPLORER_TESTING,RELEASE}.md
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

The pure model types do not themselves perform Explorer capture. The integration
adds these concrete contracts:

- **Explorer window identity:** HWND, owning process/thread, and an attachment
  generation sufficient to distinguish a destroyed window from a reused handle.
- **Quickky tab:** a stable application-owned ID associated with a managed
  Explorer folder window. A native Explorer tab is not a Quickky tab.
- **Group:** ordered Quickky tabs, an active tab, and the common window placement.
- **WorkspacePublicationState:** acknowledged custom view and visibility/placement
  per attachment; state is never reused across a new attachment lifetime.
- **Workspace refresh schedule:** application-thread-only pending work and a
  one-second readiness deadline. New candidates and unadmitted root-window
  readiness hints coalesce into a 25 ms timer;
  readiness retries and confirmed native-transfer continuations use 100 ms.
  This is scheduling policy, not attachment permission or a latency guarantee.
- **Attachment:** the capture lifecycle and the original state required to undo
  Quickky changes without closing the user's Explorer window. A version-6 bounded
  `GuardRequest` binds commands to a sender HWND, controller lifetime and nonce;
  a per-window lifetime property and cancellation event protect local recovery.
  New input appends a bounded, validated PIDL snapshot; no native pointers cross
  the bridge. Native commands use a nonce-named bounded mapping and completion
  event, correlated by request number. Copy-data only queues work; posted UI
  dispatch executes COM-capable native actions and signals the published reply.
  Timeout cancels the lease rather than reusing potentially in-flight storage.
- **Settings:** per-user tabs/startup preferences and management-interface locale
  (`en-US` or `zh-CN`). Runtime locale resolution reads the language preference
  or Windows UI language; no language-picker page is implemented.
- **Native ABI family/variant:** an accessor-rule ID, reviewed function extents
  and complete-entry/window-entry fingerprints, with no version or fixed RVA.
  The target discovers tables and derives layout locally. Exact-image profiles
  are offline regression oracles only. Neither data type travels over IPC.

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

Relevant lifecycle/foreground bursts queue one discovery-thread timer instead
of enumerating inside each WinEvent callback. Events delivered while that timer
is pending share one pass; events delivered after consumption may start another.
This is not a guarantee of one scan per wall-clock notification burst across
asynchronous/global event delivery. Unrelated foreground windows do
not trigger enumeration. Known destruction retires the identity immediately,
before deferred reconciliation can observe a reused handle. Read-only discovery
continues while tabs are disabled; only attachment work requires enablement.
Folder checks batch the requested roots into one ShellWindows enumeration per
validation phase. Reconciliation binds the batch to the attachment identities
captured before automation: a missing result or changed generation defers that
entry to a later pass, while an empty result still rejects a covered attachment.
Deferred removals likewise recheck attachment identity. Fresh pre-commit and
pre-transfer checks remain mandatory. Shell
automation is synchronous and has no hard timeout: process/lease watchdogs detect
loss, not a live but blocked controller thread.

The controller remains message-only. `--snapshot` writes a read-only JSON
snapshot to redirected standard output; `--observe-ms N` runs a bounded observer;
`--stop` signals the current session's controller. Snapshot and bounded-observer
modes do not replace or stop the resident controller. Only explicit tabs
enablement or the directory-scoped integration mode invokes attachment. Hidden
managed members are revalidated separately; discovery removal alone must not
forget them or discard their leases.

## Key flows

The implemented executable flow is:

1. Validate arguments. Normal launch acquires the session-local controller
   singleton and stop event before COM, discovery, or tray creation, using the
   current user's default object ACLs. `SingleInstance` retains both handles until
   controller teardown finishes. Duplicate launches exit successfully, including
   copies launched from another directory. The private kernel-object names are
   `Local\QTabs.Controller` and `Local\QTabs.Stop`.
   A short-lived `<singleton>.Startup` mutex makes singleton/stop-event
   construction atomic with respect to stop requests. Startup waits at most one
   second for the gate; stop fails immediately while it is held, or when a
   singleton exists without a stop endpoint. Failure drops the singleton before
   releasing the gate. No initializing pair is treated as a ready resident.
2. Initialize COM and create a message-only window, never a file-manager window.
3. Start discovery and, in resident modes, the localized notification icon.
   Added candidates enter discovered state; unmanaged removals invalidate that
   identity. Previously enabled preferences or the explicit test root activate
   the selected platform workspace, subject to identity, scope, and capability checks.
   Folder admission rejects Control Panel roots, their descendants, and namespace
   components registered as Control Panel applets. A native frame containing any
   excluded view is left unmanaged as a whole; snapshots and native add/current-folder
   input use the same policy, not localized window-title matching.
4. Retain one-second discovery/recovery reconciliation. While a workspace is
   enabled, newly discovered candidates and create/show/foreground hints for
   unadmitted root `CabinetWClass` windows queue a coalesced 25 ms workspace pass,
   outside WinEvent/IPC callbacks. Retry readiness at 100 ms intervals for up to
   one second after the latest candidate. A confirmed native transfer queues a
   100 ms continuation even after that deadline, still one mutation per pass;
   failure or an uncertain outcome does not count as progress. Fast passes
   re-probe discovery before workspace reconciliation, retaining every normal
   identity/scope check. A readiness hint is not an admission or attachment.
   Disabling cancels fast work; timer creation failure leaves periodic recovery.
   `--stop` pins the existing singleton before opening/signaling its stop event;
   an absent singleton is a no-op and never signals an orphaned same-name event.
   `--observe-ms N` closes its own bounded observer after 1..600000 milliseconds.
5. `--snapshot` emits candidate identifiers, generation, and the managed count to
   redirected standard output and exits. `--smoke-test` skips Explorer discovery
   and closes its own message-only window through the message pump.

Invalid arguments return `ERROR_BAD_ARGUMENTS` (160); setup/discovery failures
return nonzero. Stop is idempotent when no controller event exists. Normal
duplicate launch exits without stopping the original controller. Snapshot and
bounded modes do not acquire or signal the resident singleton. Object-name
collisions or inaccessible objects fail rather than changing permissions or
resetting an existing stop event. Exiting or losing the controller releases its
handles; a subsequent launch can become the sole resident.

The attachment flow is:

1. Discover an eligible Explorer folder window and validate its identity.
2. Choose the platform backend. Windows 10 prepares the nonce-bound custom-tab
   attachment and revalidates before committing presentation. Windows 11 selects
   and validates a built-in native ABI map on the target UI thread, then binds
   a native-only bridge, validates the ABI capability and folder boundary, and
   retains native presentation. Neither path admits a destroyed or reused HWND.
3. Commit the Quickky tab only after attachment succeeds; keep or restore the
   ordinary Explorer window if attachment fails.
4. Route Windows 10 Quickky actions through group policy and the target bridge.
   Windows 11 reads the active native tab, creates it through the destination's
   tab-drop action, confirms the destination GUID/PIDL, and removes only that
   source tab after source revalidation, verifying its absence. Native merges are
   not split back into separate windows
   on disable/exit: the OS owns the resulting tabs.
   Close is an authenticated target-thread command, never an unqualified
   `WM_CLOSE` posted to a cached HWND. Retire a closed or failed target before
   publishing the remaining group.
5. On detach, application loss, or unsupported integration, restore affected
   windows and remove Quickky behavior without terminating Explorer.

The Windows 10 grouping mechanism retains each Explorer top-level window and
shows the active member at the group placement while hiding inactive members.
Basic capture, switching, current-folder opening, maximize/minimize/restore, and
recovery have scoped Windows 10 build 19045 evidence. Cross-process `SetParent`
is not the baseline.
Reliable automatic collection is required; universally flash-free creation is
not claimed. Path-only reopening must not discard file selection or view state.

## Integration constraints and validation gates

Windows 10 and Windows 11 adapters isolate Explorer differences. Version names
alone are not capability checks. Unknown or failed integrations leave Explorer
unmanaged, usable, and visible. A private ABI profile is a compatibility boundary,
not proof of support for other Explorer images.

`QuickkyNativeGuardProbe` and `QuickkyExplorerHook.dll` provide an explicit,
manual single-window mechanism test. The application uses its own staged hook
client, not the probe executable, for explicit automatic admission. See
[Hook](HOOK.md) and [Explorer](EXPLORER.md).
The user authorized local testing. Test-created folder windows/processes are
in scope; pre-existing folder windows/tabs are preserved. The observed host is
Windows build 26300 with Explorer file version 10.0.26100.1710; this observation
does not establish OS-build support.

Before a usable integration can be claimed, evidence must establish:

- Safe UI-thread attachment and detachment on explicitly recorded OS builds.
- Native Windows 11 merging without a second strip or disabled native controls,
  and Windows 10 custom-tab layout without covering Explorer content.
- Correct tab switching, focus, taskbar/Alt+Tab behavior, placement, modal windows,
  drag-and-drop, and multi-monitor DPI handling.
- Collection of supported launch sources without unrelated tab loss, with the
  native-drop navigation-history and selection limitations stated explicitly.
- Restoration of hidden windows when the controller disconnects or exits.

These gates are not fully verified; fixture tests and scoped host evidence
establish only the paths they exercise. If a gate requires a new private mechanism or a change of
architecture, document the evidence and obtain agreement on the changed boundary
before implementing it. Do not claim intact browser-state migration from a
successful native merge.

## Documentation and procedures

- [Development workflow](../guides/DEVELOPMENT.md)
- [Validation and review](../guides/VALIDATION.md)
- [Explorer integration testing](../guides/EXPLORER_TESTING.md)
- [Release preview and publication](../guides/RELEASE.md)

All development material, design documents, comments, and commit messages use
English. `README.zh-CN.md` is the Chinese user-documentation exception. Chinese
translation values in the `zh-CN` catalog are required localization data, not
an exception permitting Chinese development documentation.
