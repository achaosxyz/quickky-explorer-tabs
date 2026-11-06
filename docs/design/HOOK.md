# Hook Integration

## Status

`src/hook/` is reserved. No injection, subclassing, IPC, or recovery code exists.
See the [overall design](PROJECT_DESIGN.md).

## Responsibility

Bootstrap Explorer-only integration on the target window's UI thread, bridge
window messages and commands, and restore Explorer when control is lost.

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

No bootstrap hook type, transport, liveness protocol, or unload implementation
has been selected. Native-tab behavior belongs to [Explorer](EXPLORER.md), not a
collection of undocumented hooks scattered through this module.

## Acceptance evidence

Use disposable Windows environments for attach/detach races, controller loss,
Explorer restart, and safe callback teardown. A controller-only happy path is
not sufficient recovery evidence.
