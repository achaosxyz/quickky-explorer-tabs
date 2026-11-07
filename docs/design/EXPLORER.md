# Explorer Adapters

## Status

`NativeFrame` implements the matched Windows 11 native tab backend; scoped host
tests verify merge, current-folder creation, and native lifecycle preservation.
`NativeTabGuard` supplies the classic single-view creation policy; its clipping
fixture is diagnostic-only, not the Windows 11 backend or proof of Windows 10
presentation compatibility. See the
[overall design](PROJECT_DESIGN.md).

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

`NativeFrame` belongs in this module. It binds on the owning UI thread, validates
the loaded ExplorerFrame CodeView identity, and uses the tab-actions interface
for enumeration, native drop add-at-index, and removal of a specific GUID. The
initial profile validates window-data and tab-actions vtables before accessing
private fields. It must not scan unknown layouts or mutate unmatched profiles.
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
The initial ABI profile is restricted to the loaded ExplorerFrame CodeView GUID
`822A8C93-960A-C7B6-7F6A-87CF48E8CE6F`, age 1 (file version
`10.0.26100.9949`). Other profiles are rejected, not guessed from OS names.
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
Native snapshot reception, target-side native add, and current-folder `+` use the
same location policy. An excluded `+` location falls back to native default behavior.
Keep the final source-removal GUID/PIDL check free of additional naming callouts.

On Windows build 26300 with ExplorerFrame `10.0.26100.9949`, scoped native tests
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
