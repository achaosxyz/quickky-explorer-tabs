# Shared Contracts

## Status

`window_identity.h` defines `NativeWindow`, `TabId`, `WindowCandidate`, and
`WindowIdentity` for the implemented discovery and models. No IPC protocol exists.
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

No wire format, container library, logging framework, generic error hierarchy,
or IPC transport is defined. A shared directory does not justify new abstractions.

## Acceptance evidence

Test stale identities, malformed requests, protocol incompatibility, bounded
payloads, and mismatched senders when a concrete implementation exists.
