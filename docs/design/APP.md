# Application

## Status

`src/app/` is reserved; no application entry point or runtime exists.
The [overall design](PROJECT_DESIGN.md) defines the product boundary.

## Responsibility

Own application lifetime, compose the capture/group/settings modules, and own
the controller side of communication with Explorer integration instances.

## Contract

- Scope one controller to the current user session; do not interfere with other
  users or require elevation for ordinary operation.
- Keep policy in capture/group modules, not in IPC handlers or the entry point.
- Track integration instances by validated identity, not an unverified PID alone.
- Treat attachment requests as incomplete until acknowledged by the target.
- Normal shutdown requests restoration and detachment before releasing control.
- Controller loss must also be observable from the DLL; cleanup cannot depend
  exclusively on the controller staying alive.

## Boundary

No tray UI, startup registration, installer, IPC transport, or controller
singleton mechanism is implemented or selected. Do not invent these as part of
directory initialization. Management-page behavior belongs to [Settings](SETTINGS.md).

## Acceptance evidence

Show deterministic startup/shutdown, attachment acknowledgment, duplicate
controller handling, and restored Explorer windows after controller termination.
None of these checks has been executed.
