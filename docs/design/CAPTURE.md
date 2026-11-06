# Capture

## Status

`src/capture/` is reserved; no hooks or discovery logic exist.
See the [overall design](PROJECT_DESIGN.md).

## Responsibility

Discover eligible Explorer folder windows and coordinate their admission to a
Quickky group without losing the original window on failure.

## Contract

- Use window identity, not new-process creation, as the capture unit.
- The candidate discovery path is an initial `EnumWindows` pass plus WinEvent
  notifications. Notifications are observations, not pre-creation interception.
- Treat `CabinetWClass` as a candidate filter, not sufficient proof of eligibility;
  verify Explorer ownership, session/desktop scope, and adapter capability.
- Ignore desktop/taskbar windows, dialogs, foreign applications, and unsupported
  integrity or virtual-desktop boundaries.
- Coalesce duplicate events, revalidate identity before side effects, and never
  treat a reused HWND as an existing attachment.
- Model discovery, attaching, managed, restoring, and released states explicitly
  when code is introduced. Only confirmed attachment can become managed.
- Preserve the original window on rejection, timeout, or failed attachment.

## Boundary

Capture does not render tabs, terminate Explorer, or reopen only a folder path
and assume selection/search state was preserved. Group membership is owned by
[Group](GROUP.md); thread attachment is owned by [Hook](HOOK.md).

## Acceptance evidence

Exercise startup with existing windows, repeated events, simultaneous launches,
window destruction during attachment, failed adapters, and each documented
launch source. Record visible flashes separately from collection correctness.
