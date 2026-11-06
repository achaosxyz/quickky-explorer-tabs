# Tests

CTest registers three checks when `BUILD_TESTING` is enabled (the default).
All checks retain explicit failure handling in Release builds; no third-party
test framework is required. CTest enforces a ten-second timeout for each check.

- `core.model`: thirteen groups of pure tests for tab ordering, stable IDs,
  selection/removal, duplicate events, candidate lifetime changes, acknowledgment
  tokens, failed attempts, stale completions, restoration, and foreground policy.
- `capture.discovery`: real WinEvent/message delivery on hidden, test-owned
  `CabinetWClass` fixtures through a test-local inspector; covers initial scan,
  duplicate scans, creation/destruction, changed identity, eligibility removal,
  stop/restart, callback exceptions, and production-probe rejection checks.
- `app.smoke`: creates a message-only window, posts its close request, dispatches
  destruction, and exits zero without inspecting or modifying Explorer.

These tests do not prove native-tab replacement, visible UI, actual attachment,
or native window recovery. Read-only snapshots and bounded observation on the
development host are separate from these synthetic tests.

See [Development](../docs/guides/DEVELOPMENT.md) for configure/build/test commands.

The accepted validation boundary separates pure policy tests (capture lifecycle,
tab ordering/selection, identity handling, and locale resolution) from live
Explorer integration checks. Add tests alongside the corresponding source;
do not create placeholder passing tests for missing functionality.

Use [Validation and Review](../docs/guides/VALIDATION.md) for scaffold checks and
[Explorer Integration Testing](../docs/guides/EXPLORER_TESTING.md) for the manual
Windows validation procedure. Record exact tested builds and recovery results;
neither documentation nor a clean build proves Explorer compatibility.
