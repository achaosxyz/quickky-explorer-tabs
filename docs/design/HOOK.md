# Hook Integration

## Status

`QTabsHook.dll` implements target-thread bootstrap, bounded IPC,
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
Protocol version 10 carries bounded file-selection snapshots and executes
`NativeCapture`/`NativeRestore` on the same posted-command path. Capture readiness
uses a 25-ms UI timer with an 800-ms limit, below the exchange's two-second deadline;
there are no sleeps. Source capture requires 50 ms unchanged observations and a
200-ms grace for empty selection. Restore performs one readiness poll per command
and acknowledges `E_PENDING` for later controller reconciliation. Its progress
flags survive those commands, so it writes once and then only confirms. The
existing message hook records target keyboard/mouse
input; capture/add return generation tokens that restore/final removal must echo,
rejecting actionable input across the whole operation including IPC gaps.
`native_input.h` defines that filter: keys, mouse buttons, wheels and button-held
client motion advance the generation. Unpressed hover motion does not: Explorer
can synthesize it during layout/activation without any selection or navigation
command. Independent selection, folder and view checks still protect both ends.
The destination's nonce-named input-change event stays armed from add to final
source removal. Source-side validation checks that peer event without pumping
after its COM reads, so target input/cancellation during those reads cannot pass
the last pre-mutation guard. The event does not cancel an ordinary attachment.
The remove request also carries the destination PID and process creation time.
Before source removal, the source UI thread opens and validates that process and
provides a non-pumping foreground-delegation callback to `NativeFrame::Remove`.
The adapter calls it only after successful final source COM validation, immediately
before native removal, then rechecks availability and input fences. The callback
calls `AllowSetForegroundWindow` for that PID only, while source-side activation
authority can still exist. No public-view COM reads follow delegation. The process handle remains open through removal to
prevent PID reuse. This is permission delegation, not early window activation;
the destination still reveals only after confirmed transfer. Foreground denial
does not authorize bypasses, repeating removal or discarding confirmed progress.
Destination restore retains an initial-folder-ready flag across controller polls;
only the initial missing/mismatched folder may wait. A folder change after it
was observed, an active-tab change or a canceled input token aborts. Reentrant
native posts cannot execute an occupied request twice. Add and an already-invoked
native remove are never retried; acknowledged pre-removal readiness can continue
on a later controller pass. Detach cancels the capture timer; the callback/mapping
keepalive survives teardown.
`NativeReveal` uses the same authenticated posted-command
path. Only a confirmed transfer requests this optional presentation operation;
failure never retries native add/remove. It reselects the created GUID in normal,
maximized and minimized destinations, restoring only minimized frames. It requests
foreground activation and keyboard focus in the resolved public Shell-view window,
without changing file selection or bypassing foreground policy. The destination's
input-change event also guards presentation across reentrant calls; later genuine
input cancels activation without undoing the confirmed transfer.
Preparation carries no ABI descriptor. Commit recognizes a built-in code family
against the loaded image and validates its accessor contract on the target thread
before any private COM call.
The client exposes the last acknowledged native HRESULT to the scoped transfer
probe. An unconfirmed exchange reports `E_PENDING`, not a fabricated native
outcome; diagnostics do not include folder paths or selection contents.
There is no controller resolver/readiness worker, DbgHelp session, runtime PDB
parser, symbol cache, download, or object-layout scan.
See [native ABI adaptation](NATIVE_ABI.md).

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

The synthetic workspace bridge also exercises the real client exchange against
an owned message-only reply fixture, without bootstrap or native calls. It checks
acknowledged pending/successful restore progress, timeout, invalid reply version,
request correlation and completion state, plus canceled-lease reuse rejection.
This transport check does not establish Explorer transfer compatibility.

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
