# Capture

## Status

The module contains read-only Explorer discovery and a pure capture lifecycle
coordinator. Explicitly enabled `WorkspaceController` instances request real
attachment through the separate hook client; discovery itself has no mutations.
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
- Every attachment attempt has its own request token. Late results from an old
  attempt or window generation cannot admit a tab. Failure/timeout returns the
  record to discovered without affecting any native window.
- Restore keeps group membership until acknowledgment. Failed restore returns
  to managed; successful restore or window loss removes the corresponding tab.
- Initial enumeration plus WinEvent notifications and periodic reconciliation
  maintain the observed eligible set. All callbacks run on the owning message
  thread. Probe and change-handler exceptions fail discovery instead of unwinding
  through a Win32 callback. An injected inspector is a test seam, not a production
  bypass option for Explorer ownership checks.

## Boundary

The explicit scoped workspace composes discovery with acknowledged attachment.
Removal from the visible discovery set is not a destruction acknowledgment:
managed hidden identities are checked against their actual HWND/process lifetime
before release. Normal discovery/observation remains read-only.

Capture does not render tabs, terminate Explorer, or reopen only a folder path
and assume selection/search state was preserved. Group membership is owned by
[Group](GROUP.md); thread attachment is owned by [Hook](HOOK.md).

## Acceptance evidence

Exercise startup with existing windows, repeated events, simultaneous launches,
window destruction during attachment, failed adapters, and each documented
launch source. Record visible flashes separately from collection correctness.
