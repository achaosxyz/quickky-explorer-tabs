# Shared Contracts

## Status

`window_identity.h` defines `NativeWindow`, `TabId`, `WindowCandidate`, and
`WindowIdentity` for discovery and models. `hook_protocol.h` defines the concrete
versioned bridge used by the application, DLL, and guard-only probe.
Native identity, descriptor and fingerprint types live in `explorer/native_abi.h`
and never cross this protocol boundary. Versioned reference types are isolated
in `tests/abi_reference/profile.h`, not production headers.
See the [overall design](PROJECT_DESIGN.md).

## Responsibility

Hold the minimal shared definitions needed by the application and Explorer DLL,
such as tab identity, attachment identity, and cross-process request/result data.

## Contract

- Add definitions only when an implemented interaction requires them. Do not
  create a general-purpose utilities framework or service locator.
- Do not put capture/group policy, rendering, or OS-build adaptation here.
- Never transmit process-local pointers or ownership-bearing C++ objects across
  process boundaries. A remote HWND is an identifier, not an authorization token.
- Document protocol versioning, sender/target validation, payload limits, request
  correlation, and attachment acknowledgment with the first concrete transport.
- Give an attachment identity enough lifetime information to reject stale commands
  after a window is destroyed, reused, or Explorer restarts.
- The current identity includes process creation time and a controller-local
  discovery generation. It is an in-process model identifier, not an authorized
  cross-process protocol or a guarantee that every native lifetime was observed.
- Reject incompatible protocol versions instead of attempting unsafe attachment.
- Do not log folder paths or other sensitive user data by default.

## Boundary

`hook_protocol.h` defines the version-9 `WM_COPYDATA` header shared
by controller, scoped workspace DLL, and manual guard probe. It carries target
HWND, controller PID/start time, attachment nonce, at most 32 fixed-size tab
labels, and explicit presentation/input commands. Payloads are value-only.
Native preparation carries no ABI descriptor, RVAs, or function pointers. The
target recognizes a built-in code family and derives the validated layout locally in
`explorer`, not in this shared-data header.
See [native ABI adaptation](NATIVE_ABI.md).
The nonce also derives a `Local\QTabs.Lease.` cancellation-event name; no handles
or C++ object pointers cross process boundaries. Ordinary commands are fixed-size. Only a New
input appends an absolute PIDL snapshot (at most 64 KiB, including its terminator);
the header size covers the whole packet. Validate every item boundary and clone
the bytes before acknowledging, without COM calls inside the receiver callback.

Protocol 9 uses a separate bounded `NativeExchange` mapping and completion event,
both derived from that nonce. It carries a monotonically correlated request,
native GUIDs (at most 128), HRESULT, a folder PIDL (at most 64 KiB), and a separate
selection snapshot (at most 64 KiB). Selection bytes contain a 32-bit count,
length-prefixed relative selected PIDLs (at most 256), then a length-prefixed
optional focused PIDL (zero length means no focus). Empty selection is an explicit
eight-byte snapshot. Invalid lengths, duplicate items and trailing bytes are
rejected before use. `NativeCapture` returns settled selection; `NativeRestore`
applies/confirms it for the requested GUID/folder. `NativeRemove` requires the
expected selection and revalidates it before deleting the source GUID. The copy-data
message only queues work; posted UI dispatch publishes the response before
signaling completion. Timeout cancels the lease and forbids storage reuse.
Unsupported versions/counts are rejected.
Source capture and destination add return a lease-bound input generation. Remove
and restore must echo it, so input during the controller/IPC gap is not silently
accepted as a new baseline. These tokens are independent of request correlation.
The hook advances them for keys, mouse buttons, wheels and button-held motion,
not ordinary hover. This input classification does not change the wire layout;
native selection, GUID, folder and view revalidation still apply independently.
For `NativeRestore`, `count` carries folder-ready/write-consumed flags rather
than a tab count. An acknowledged `E_PENDING` is a completed readiness poll, not
an uncertain mutation. The controller retains those flags with the same created
GUID/PIDL and input token for the next poll; writes cannot repeat after a consumed
write or successful confirmation. Unacknowledged exchanges remain uncertain.
Native attachments also share a nonce-named input-change event. Settled capture
and destination add arm it; actionable input, detach or cancellation signal it without
detaching ordinary workspaces. A remove request carries the destination nonce:
the source opens that event and checks it after its COM reads, immediately before
irreversible native removal. This closes the restore-to-remove input gap without
passing process-local handles across processes. It cannot roll back a native
call after that final decision boundary.
`NativeReveal` uses the existing active-GUID field without a PIDL.
It is a post-transfer presentation request, not another add/remove or
permission to close a frame. Older protocol versions are rejected.

No container library, logging framework, or generic error hierarchy is selected.
A shared directory does not justify unrelated abstractions.

## Acceptance evidence

Test stale identities, malformed requests, protocol incompatibility, bounded
payloads, and mismatched senders when a concrete implementation exists.
