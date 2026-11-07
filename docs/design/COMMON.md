# Shared Contracts

## Status

`window_identity.h` defines `NativeWindow`, `TabId`, `WindowCandidate`, and
`WindowIdentity` for discovery and models. `hook_protocol.h` defines the concrete
versioned bridge used by the application, DLL, and guard-only probe.
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

`hook_protocol.h` defines the version-6 `WM_COPYDATA` header shared
by controller, scoped workspace DLL, and manual guard probe. It carries target
HWND, controller PID/start time, attachment nonce, at most 32 fixed-size tab
labels, and explicit presentation/input commands. Payloads are value-only.
The nonce also derives a `Local\QTabs.Lease.` cancellation-event name; no handles
or C++ object pointers cross process boundaries. Ordinary commands are fixed-size. Only a New
input appends an absolute PIDL snapshot (at most 64 KiB, including its terminator);
the header size covers the whole packet. Validate every item boundary and clone
the bytes before acknowledging, without COM calls inside the receiver callback.

Windows 11 adds a separate bounded `NativeExchange` mapping and completion event,
both derived from that nonce. It carries a monotonically correlated request,
native GUIDs (at most 128), HRESULT, and a PIDL (at most 64 KiB). The copy-data
message only queues work; posted UI dispatch publishes the response before
signaling completion. Timeout cancels the lease and forbids storage reuse.
Unsupported versions/counts are rejected.

No container library, logging framework, or generic error hierarchy is selected.
A shared directory does not justify unrelated abstractions.

## Acceptance evidence

Test stale identities, malformed requests, protocol incompatibility, bounded
payloads, and mismatched senders when a concrete implementation exists.
