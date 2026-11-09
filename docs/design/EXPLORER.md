# Explorer Adapters

## Status

`NativeFrame` implements the matched Windows 11 native tab backend; scoped host
tests verify merge, current-folder creation, and native lifecycle preservation.
`NativeTabGuard` supplies the classic single-view creation policy; its clipping
fixture is diagnostic-only, not the Windows 11 backend or proof of Windows 10
presentation compatibility. See the
[overall design](PROJECT_DESIGN.md).

The dedicated [native ABI adaptation design](NATIVE_ABI.md) defines mode-based
recognition, offline reference fixtures, and the compatibility-extension procedure.

## Responsibility

Isolate Explorer-specific integration. Windows 10 uses Quickky-owned tabs;
Windows 11 uses Explorer-owned tabs and the inspected native drop semantics.

## Contract

- Separate Windows 10 custom presentation from Windows 11 native transfer.
  Share candidate identity, user/session/integrity, and test-scope validation.
- Validate capabilities against the actual Explorer build and observed layout;
  an OS version label or a hard-coded window-class path is not enough.
- Preserve Explorer navigation, commands, content interaction, and accessibility
  outside the replaced tab/chrome area.
- On Windows 11, leave the native tab bar, native input, and native lifecycle
  intact. Do not clip its host, reject native tab creation, or add custom chrome.
- Use the inspected native drop pipeline to merge tabs into the main frame.
  Preserve existing destination tabs and acknowledge native creation before
  removing a specific source tab. Do not promise intact browser-state migration:
  the tested native drag itself loses selection and navigation history.
- Confirm transfer before treating a source as consumed. Never close a source
  merely because a request was sent, timed out, or opened a matching path.
- Preserve every source tab in multi-tab roots; if a transfer cannot be performed
  safely, leave that root usable rather than discarding its remaining tabs.
- Do not assume a private method is stable across builds. Match the actual
  Explorer capability/build evidence; unsupported layouts remain ordinary.

## Boundary

`NativeFrame` belongs in this module. It binds on the owning UI thread and uses
the inspected tab-actions interface for native drop add-at-index and removal of
a specific GUID. Enumeration still requires private tab storage: the inspected
interface has no enumeration method, and registering an event handler does not
return an initial snapshot. Symbol availability is not an ABI contract.

Compatibility requires a uniquely recognized compiled code contract, not a
file-version or CodeView allowlist. Production scans read-only image sections for
candidate tables, resolves their actual method pointers, and validates a reviewed
complete-entry variant belonging to one of five accessor families. The initial
rule recognizes the inspected x64 implementations of `GetTabDataById` and
`GetBrowserTabFromId`: normalized whole-function fingerprints preserve instruction
flow, record size, GUID fields, and output layout while permitting relocation and
the three decoded object displacements and four reviewed diagnostic source-line
constants to change. Unknown function bodies reject
integration even when their names match. This is a deliberately narrow contract
family, not a general x64 decoder or a stable Microsoft API.
The reviewed 22621 family additionally checks its split lookup's complete cold
loop fragment and backedge. A public-symbol spelling or probe-parser failure
does not by itself prove that a transfer capability is missing.

Mode recognition runs on the owning Explorer UI thread. There is no downloader,
runtime PDB parser, symbol cache, resolver worker, or cross-process ABI descriptor.
Development-only PDB inspection supplies evidence for reference fixtures and
reviewed code variants. Versioned maps are linked into tests, not the product.
Images without an unambiguous mode match remain ordinary. Method RVAs come from
the inspected tables, never a nearby version or a common address delta. Locator records
contain identity and RVAs, not executable code.
Before accessing objects, the target validates image identity, section
bounds, the accessor contract, both vtables, and method entries. The native
object/record memory guard accepts only committed readable regions,
excluding execute-only and guarded pages. The existing
13-slot tab-actions interface and typed drop entry point remain explicit private
contracts. A new compiler or changed private behavior may require a new reviewed
contract; finding symbols alone must never authorize native calls.
Active GUID/PIDL snapshots use the matched native `GetTabDataById` action on the
owner thread; its title and PIDL outputs use task-allocator ownership. Controller
scope validation remains public Shell automation. Native snapshots do not make
cross-apartment Shell automation enumeration calls inside `WM_COPYDATA` handling.

Native `+` is adapted by an instance-local tab-actions vtable copy and a scoped
Ctrl+T handler, not global code patching or a second UI. The adapter keeps the
original interface alive and restores its table on detach/controller loss. Each
in-flight callback retains its attachment until forwarding has unwound.

The target bridge carries bounded pointer-free requests/replies in a nonce-named
shared mapping. After timeout, cancel the lease rather than reuse the mapping.
Destination creation and a subsequent selected-GUID/PIDL confirmation precede
source removal, which also revalidates the source GUID and PIDL. Native work runs
from a posted UI message, not the synchronous copy-data input stack. Unsupported
profiles leave source tabs and ordinary windows unchanged. Cancellation prevents
work that has not started; an in-flight synchronous native call can still finish
after timeout. An unconfirmed result is not rollback or proof that the source
survives. Quarantine that source identity without retrying the add or attempting
rollback; source removal is only attempted after destination confirmation.
The reference-image catalog and accessor families are listed in the
[native ABI adaptation design](NATIVE_ABI.md).
Only `10.0.26100.9549` has scoped live evidence;
the other entries have exact-image/PDB and offline contract checks. Unknown
identities use the same version-independent recognition path, including all
thirteen complete entries and accessor validation before binding.
Unknown rule IDs, unfamiliar bodies or invalid locators fail closed. Selection
uses the loaded module, not a newer on-disk DLL. See
[native ABI adaptation](NATIVE_ABI.md) for the full recognition boundary.
The existing clipping experiment is not the Windows 11 product backend. Windows
10 layout reservation requires its own validation, not an assumed XAML host.
The diagnostic clipping experiment defers reentrant region restoration until
the outer `SetWindowRgn` returns, so that call cannot reapply a stale cutout.

The guard-only probe does not clip. It tests the narrower hypothesis that a
target-thread `WH_CBT` hook can reject creation of
additional `ShellTabWindowClass` children of a selected single-tab Explorer root.
The class name is a build-sensitive observation, not a supported Explorer API.
Windows with zero/multiple such children are rejected without closing tabs.
Clipping and rejecting a new child are not proof of every native command path:
test native buttons, shortcuts, menu/mouse routes, rollback, orphaned UI, and
behavior after detachment before claiming production readiness. No tab UI is installed
by the probe. Private XAML tree
mutation, private commands, registry/feature-flag changes, and binary patching
are not approved merely by listing an adapter directory. A concrete mechanism
requires documented evidence and agreement on its compatibility boundary.

## Acceptance evidence

### Scoped platform workflows

- Windows 10 build 19045, 96 DPI: custom capture; maximized tab switching;
  minimize/restore and normal restore; current-folder `+` after navigation;
  inactive/active close; tray disable/re-enable, About, Exit; and controller-loss
  restoration passed on newly created test folders. Baseline windows remained
  unchanged. The runtime DLLs were app-local, with no runtime system installation.
- Windows 11 build 26300 with the matched profile above: separate-process,
  multi-tab source merge into a maximized main frame, no overlay/clipping,
  current-folder `+`, Ctrl+T, and exit preservation passed.

These cases do not establish all launch routes, DPI/theme variants, modal
behavior, multiple monitors, or compatibility with other Explorer images.

### Current-folder bridge

`folder_location` resolves an exact Explorer root through `IShellWindows`,
`IServiceProvider`, `IShellBrowser`, `IFolderView`, and `IPersistFolder2`.
Ambiguous roots are rejected instead of choosing one native tab. It retains an
absolute PIDL snapshot before input leaves the source UI thread. The receiver
validates serialized item boundaries before cloning; naming and launch remain
outside the IPC callback. The folder location includes an
absolute PIDL, a parsing name for scoped filesystem admission, and a Shell display
name for the Quickky label. `opennewwindow` with the click-time PIDL requests a
new Explorer window; it never drives native tabs or substitutes a launch path
for the current folder. A failed resolution or launch leaves existing views intact.

Admission requires a Shell folder and rejects Control Panel category/classic/all-task
roots, Connections, Printers, and their descendants by parsing-namespace identity.
Namespace components with a `System.ControlPanel.Category` class registration
are also excluded, including directly addressed applets. Localized display names
and window titles are not policy inputs. Filesystem folders and ordinary virtual
browsing roots such as This PC, Home, Libraries, and Recycle Bin are not excluded
merely for lacking a filesystem path. The all-view resolver rejects an entire
frame if any view is excluded; both pre-commit and pre-transfer scope checks reuse it.
Checks for multiple roots share one automation enumeration within a validation
phase, rejecting roots with an unreadable view. Fresh checks after preparation
and immediately before native mutation are not replaced with earlier snapshots.
Unresolved high-risk limitation: these synchronous Shell COM/name calls have no
hard deadline. A blocked call can stall the controller STA; bounded bridge IPC
does not bound Shell automation. A live but blocked controller does not signal
process/lease loss, so the existing watchdog alone cannot restore hidden members
merely because the STA is unresponsive.
Native snapshot reception, target-side native add, and current-folder `+` use the
same location policy. An excluded `+` location falls back to native default behavior.
Keep the final source-removal GUID/PIDL check free of additional naming callouts.

On Windows build 26300 with ExplorerFrame `10.0.26100.9549`, scoped native tests
verify automatic two-window merge, a pre-existing two-tab source transferred one
GUID at a time, current-folder native `+` and Ctrl+T, and preservation of the
merged native group on orderly exit. Baseline Explorer roots remain present.
These tests do not prove intact history/selection, every launch route, or other
ABI profiles. `QuickkyNativeTransferProbe` exercises the same mapping/event
transport and requires two explicitly named, fully directory-scoped frames.

The separate Explorer process may remain cached after its last window closes,
so a pinned DLL image can remain locked. This is not dynamic-unload evidence.
Live DLL clients must load a per-run copy rather than locking the build output.

Record exact OS/Explorer builds, every tested native-tab entry point, existing
multi-tab handling, and restoration behavior. Do not claim all Windows 10/11
builds are supported from a test on one machine.
