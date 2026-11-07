# Hook Integration

## Status

`QuickkyExplorerHook.dll` implements target-thread bootstrap, bounded IPC,
classic caption/diagnostic child chrome, scoped keyboard handling, and local recovery. `HookClient`
connects explicitly enabled workspaces; the manual guard-only probe remains a
separate diagnostic. This is experimental integration. See the
[overall design](PROJECT_DESIGN.md).

## Responsibility

Workspace modes separate guard-only, classic caption, native, and diagnostic
child/clipping attachments. The resident platform selector never chooses the
diagnostic clipping mode; it remains an explicit synthetic-test fixture.

Bootstrap Explorer-only integration on the target window's UI thread, bridge
window messages and commands, and restore Explorer when control is lost.

Hook staging uses a per-run `QTabs-` directory. Registered native/layout messages
and the per-window attachment property use the `QTabs` prefix; older product
identities are not supported.

## Contract

- Target eligible Explorer UI threads only; do not inject arbitrary applications
  or use a desktop-wide hook as the default implementation.
- Thread-specific `SetWindowsHookEx` is a candidate bootstrap, not a completed
  integration. Validate bitness and target identity before attachment.
- Install/remove subclass helpers on the owning UI thread. Entering the target
  process alone does not satisfy the thread requirement.
- Keep `DllMain` minimal; defer COM, rendering, IPC, and other complex setup.
- The controller owns policy. The DLL executes validated, bounded requests and
  reports attachment, destruction, and recovery outcomes.
- Retain original window state locally so controller loss can restore windows
  without requiring a functioning controller process.
- Remove callbacks and stop using DLL code before any unload. Never unload a DLL
  while a subclass callback or other integration code can still execute.
- An Explorer restart invalidates attachments; never reuse their identities.

## Boundary

Windows 11 workspaces use mode 2, distinct from Windows 10 custom mode 1 and
the guard-only diagnostic mode 0. Prepare opens a nonce-named bounded shared
mapping; authenticated native commands carry a monotonically increasing request
number. `WM_COPYDATA` only copies and queues the command. A registered posted
message executes it outside input-synchronous dispatch, publishes its result,
and signals a nonce-named completion event. The controller waits with bounded
sent-message servicing but does not dispatch posted controller commands/timers.
The target checks the cancellation lease before mutation. Received native
snapshots and target-side native add also apply the shared folder-admission
policy; Control Panel and management namespace PIDLs cannot become transfer inputs.
A timeout cancels the attachment: neither side may reuse that exchange. It cannot
interrupt a synchronous native call already in progress. A failed native exchange
therefore means unconfirmed completion, not rollback or a preserved source; the
controller quarantines the source identity instead of retrying the mutation. Native
commit binds a matched `NativeFrame` and current-folder input only, without a
guard, region clipping, Quickky strip, or window visibility changes.

The scoped native-guard probe uses thread-specific `WH_CALLWNDPROC` to bootstrap
on a selected Explorer UI thread. Versioned fixed-size `WM_COPYDATA` requests
bind the target, controller PID/start time, sender window, and a random session
nonce. The guard-only probe is not automatic capture or a tab-admission path.
A target-thread timer watches an open controller-process handle and removes the
subclass/guard on controller loss. The DLL is pinned until the test Explorer
process exits: restoration is supported, dynamic unloading is not claimed.
Native-tab creation policy belongs to [Explorer](EXPLORER.md).

The probe is manual, outside CTest, and must target explicitly selected test
windows. The current local scope is a new `/separate` test Explorer process;
the pre-existing Explorer window and its native tabs are excluded. No global
hook, registry/feature-flag edit, binary patch, or private symbol detour is used.

## Acceptance evidence

### Scoped workspace bridge

The scoped integration uses a per-run DLL copy; failed copy/load attempts try to
remove only their own staging files before returning. Locked files are reported,
not promised immediate deletion. Loaded images may remain pinned by Explorer.
The bridge uses a versioned command header (New input appends a bounded PIDL)
with at most 32 tab labels, and the existing target/controller identity binding.
Rendering/input requests never contain process-local pointers. New resolves its
PIDL on the source UI thread before acknowledging the click; queued execution
must not reread a later source location. Each attachment
opens a nonce-derived local cancellation event as well as its controller process
handle. A 500 ms target-thread watchdog restores original window state if either
is signaled; failure to open the cancellation event rejects workspace attachment.
Thus a late successful attach after an IPC timeout cannot strand a window merely
because the controller is still running. Ordinary guard probes do not hide roots.

The DLL creates the classic owned surface or diagnostic child on the Explorer UI thread. Input is queued back to
the controller before changing membership or visibility, avoiding synchronous
cross-process command reentrancy. UI-thread keyboard interception translates
Ctrl+T, Ctrl+W, and Ctrl+Tab into Quickky actions for the attached root only.
Message type, supported shortcut, root, and modifier checks precede kernel-handle
liveness polling. The process/lease watchdog does not detect a controller STA
blocked in a synchronous Shell call while its process and lease remain live.

Workspace attachment is two-phase. Prepare binds the subclass, a kernel-cleared
per-window lifetime property, nonce, and
watchdog but does not create chrome, intercept native tabs, or change visibility.
After preparation, the controller re-probes identity and its optional folder
scope. Only an authenticated commit initializes the selected backend: custom
rendering/guard for the diagnostic custom path, native actions for Windows 11.
Destroying the prepared root invalidates the binding before a reused
HWND can accept commit. The guard-only probe retains immediate activation.
Recovery restores placement only while that lifetime property still belongs to
the attachment. Teardown requested reentrantly during chrome initialization is
deferred until initialization unwinds, without keeping native interception active.
Root callbacks retain a shared attachment lifetime; strip callbacks lock a weak
lifetime reference. Reentrant detach clears thread ownership immediately but
cannot free a presenter while its layout/configuration/callback is still running.
Classic root notifications hide and queue one pointer-free layout message,
instead of hit testing inside Explorer's nested placement callbacks. The queued
handler revalidates the current committed attachment/controller before reflow;
failed reflow still detaches safely. Explicit Present reflows after the native
placement API returns. Hidden/minimized notifications cancel pending presentation.
The controller stages its hook DLL and any bundled toolchain runtime DLLs in one
unique per-run directory. Runtime names come from the build, not a wildcard DLL
scan. Missing app-local files permit build-tree runs with an installed runtime;
packaging requires the complete toolchain list. Failed staging removes only
files created by that attempt. Explorer may pin loaded copies until it exits.
Runtime destinations are registered for cleanup before copying, including a
partially written destination when CopyFile fails. A copy-function test seam
exercises that failure without loading a hook or touching Explorer.
Cleanup is best-effort and emits a debug diagnostic when a staging directory
remains. It never unloads another process's module or deletes unrelated folders.
Retained files may persist after Explorer exits until temporary storage is cleaned;
automatic deferred deletion is not implemented.
Close is also authenticated and runs `WM_CLOSE` on the validated target UI
thread, rather than posting a raw close from a stale controller-side handle.

Use disposable Windows environments for attach/detach races, controller loss,
Explorer restart, and safe callback teardown. A controller-only happy path is
not sufficient recovery evidence.
