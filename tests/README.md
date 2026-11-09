# Tests

CTest registers ten checks when `BUILD_TESTING` is enabled (the default).
All checks retain explicit failure handling in Release builds; no third-party
test framework is required. Timeouts are ten seconds, except the fifteen-second
workspace bridge check and twenty-second instance/native-ABI checks.

- `core.model`: thirteen groups of pure tests for tab ordering, stable IDs,
  selection/removal, duplicate events, candidate lifetime changes, acknowledgment
  tokens, failed attempts, stale completions, restoration, and foreground policy.
- `capture.discovery`: real WinEvent/message delivery on hidden, test-owned
  `CabinetWClass` fixtures through a test-local inspector; covers initial scan,
  duplicate scans, creation/destruction, changed identity, eligibility removal,
  stop/restart, callback exceptions, production-probe rejection, and unready-window
  retry notifications. A hidden fixture becomes eligible after its sole creation
  event; a deferred re-probe, not a new lifecycle event, discovers it.
  A separate hook-free phase feeds the actual event and timer callbacks directly:
  64 events share one pending timer/scan; events delivered after consumption may
  start another pass. It also checks unrelated foreground events, explicit refresh,
  late canceled callbacks, immediate destruction invalidation, and Stop. Counts
  have explicit delivery/consumption boundaries, not elapsed-time drains; real
  global WinEvent delivery is tested for eventual state separately. Parallel
  fixture processes can produce relevant global events for each other, so a fixed
  time window is not an isolated notification batch.
- `app.smoke`: creates a message-only window, posts its close request, dispatches
  destruction, and exits zero without inspecting or modifying Explorer.
- `app.instance`: exercises the resident lifetime guard with isolated kernel-object
  names and test-owned child processes, not a live QTabs controller. It checks
  concurrent launches from two executable directories, clean and crash restart,
  duplicate shutdown isolation, wrong-kind/pre-existing object rejection, and
  stop-request isolation from orphaned same-name events. A separate child holds
  the startup gate to freeze half-published states with a missing or colliding
  endpoint; stop must fail without signaling either one.
  A process-local HKCU override checks QTabs startup/preferences in a unique
  temporary registry subtree; real application settings are not changed.
- `app.refresh`: uses a synthetic clock to check prompt/coalesced scheduling,
  bounded readiness retries, reentrant notifications, acknowledged-transfer
  continuation, cancellation, and long-uptime timestamps. It does not measure
  real Explorer readiness or end-to-end collection latency.
- `explorer.native_guard`: rejects additional synthetic native-view children
  only beneath its selected hidden fixture, and restores creation on detach.
- `explorer.native_abi`: rejects malformed PE data, descriptor mismatches, unknown
  accessor code, and redirected lookup calls without executing native methods.
  Synthetic PE records check unique valid identities and empty outputs on failure
  on every host. Test-owned memory covers readable, execute-only, guarded,
  inaccessible, copy-on-write, reserved, and region-boundary cases.
  Each registered reference image is recognized by code mode before its exact
  addresses and layout are compared with the independent `abi_reference/` oracle.
  Unknown identities and missing version resources must not veto that match.
  The verified baseline additionally exercises relocated tables/accessors,
  ambiguous tables, every damaged typed entry, complete window-entry return,
  malformed unwind data, decoded field changes and diagnostic immediates with
  rejected instruction/call opcode changes. Legacy catalog/fallback tests remain
  in the test-only library; no such selector is linked into production.
  Other hosts report those positive cases as unavailable rather than claiming
  compatibility. No test downloads or parses PDBs, starts a resolver worker, or
  attaches to Explorer. Synthetic maps do not establish another OS's compatibility.
- `explorer.native_frame`: synthetic roots and typed fake actions check selection
  of the transferred GUID, normal/maximized restoration, visible-window no-op,
  missing GUID, selection failure, canceled lease, wrong thread, restore-time
  selection changes and reentrant destruction. It does not bind to Explorer or
  establish real transfer support.
- `workspace.performance`: counts custom publication decisions for unchanged
  passes, labels, selection, placement, external visibility, and new lifetimes.
  Hidden test-owned strips check visual-hover change detection, theme refresh,
  device-resource release and lazy recreation. This is not a frame-rate benchmark.
  The controller's reconciliation phase also uses unprepared sentinel clients
  and an uninitialized probe to check missing batch coverage, late attachments,
  generation replacement, stale removal nominations, and later fresh validation
  for both backends. This injects lifecycle changes at the automation boundary;
  it does not reproduce Shell COM pumping or Explorer attachment.
- `hook.staging`: injects a runtime copy failure after writing a partial file and
  checks that the per-attempt destination and staging directory are removed. It
  runs serially because its runtime source fixture shares the test binary directory.
- `workspace.bridge`: loads the real DLL into the test process, uses hidden
  synthetic roots on a dedicated UI thread, and checks protocol rejection,
  configure/presentation bounds, creation interception, normal detach, lease
  cancellation recovery, prepare/commit rejection, authenticated close,
  destruction during initialization, root/child layout and label configuration,
  directory boundaries, and locale resolution. Read-only Shell metadata cases
  exclude Control Panel aliases/descendants/applets and namespace junctions,
  while retaining normal virtual roots and an owned ordinary folder with a
  Control Panel-like name. No management window is opened. It never
  injects into real Explorer or modifies user settings.

These tests do not prove native-tab replacement, visible UI, or recovery on real
Explorer. Scoped host integration tests are separate evidence; a hidden fixture
does not establish OS-build compatibility or every native-tab entry point.

`QuickkyNativeTransferProbe` is a manual, mutating Windows 11 integration tool,
not a CTest. It validates an explicit directory boundary and two eligible HWNDs,
then confirms native destination GUID/PIDL before removing only the source's
unchanged active GUID. Never target daily Explorer windows. See the
[Explorer testing SOP](../docs/guides/EXPLORER_TESTING.md).

See [Development](../docs/guides/DEVELOPMENT.md) for configure/build/test commands.

Release-script checks live in `scripts/test-release.ps1`; they use isolated Git
repositories under `build/` and a mocked GitHub CLI. They are separate from
CTest and never publish a GitHub release. See the
[release SOP](../docs/guides/RELEASE.md) for packaging and hosted smoke checks.

The accepted validation boundary separates pure policy tests (capture lifecycle,
tab ordering/selection, identity handling, and locale resolution) from live
Explorer integration checks. Add tests alongside the corresponding source;
do not create placeholder passing tests for missing functionality.

Use [Validation and Review](../docs/guides/VALIDATION.md) for scaffold checks and
[Explorer Integration Testing](../docs/guides/EXPLORER_TESTING.md) for the manual
Windows validation procedure. Record exact tested builds and recovery results;
neither documentation nor a clean build proves Explorer compatibility.

`QuickkyWorkspaceBridgeTests --classic-caption` opts into a visible synthetic
caption fixture on an authorized desktop. It never uses Explorer: it checks
non-activating ownership, client geometry, background z-order, movement,
hide/restore, and surface-loss recovery. Default CTest fixtures remain hidden;
these generic Win32 checks are not Windows 10 Explorer compatibility evidence.
