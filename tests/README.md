# Tests

CTest registers five checks when `BUILD_TESTING` is enabled (the default).
All checks retain explicit failure handling in Release builds; no third-party
test framework is required. Timeouts are ten seconds, except the fifteen-second
workspace bridge check.

- `core.model`: thirteen groups of pure tests for tab ordering, stable IDs,
  selection/removal, duplicate events, candidate lifetime changes, acknowledgment
  tokens, failed attempts, stale completions, restoration, and foreground policy.
- `capture.discovery`: real WinEvent/message delivery on hidden, test-owned
  `CabinetWClass` fixtures through a test-local inspector; covers initial scan,
  duplicate scans, creation/destruction, changed identity, eligibility removal,
  stop/restart, callback exceptions, and production-probe rejection checks.
- `app.smoke`: creates a message-only window, posts its close request, dispatches
  destruction, and exits zero without inspecting or modifying Explorer.
- `explorer.native_guard`: rejects additional synthetic native-view children
  only beneath its selected hidden fixture, and restores creation on detach.
- `workspace.bridge`: loads the real DLL into the test process, uses hidden
  synthetic roots on a dedicated UI thread, and checks protocol rejection,
  configure/presentation bounds, creation interception, normal detach, lease
  cancellation recovery, prepare/commit rejection, authenticated close,
  destruction during initialization, root/child layout and label configuration,
  directory boundaries, and locale resolution. It never
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
