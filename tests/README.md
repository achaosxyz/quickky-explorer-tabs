# Tests

CTest registers `app.smoke` when `BUILD_TESTING` is enabled (the default).
It starts `QuickkyExplorerTabs --smoke-test`, which creates a hidden application
window, posts its close message, runs the normal message loop, and exits with
status zero. CTest fails on a nonzero status or a ten-second timeout.

This test exercises actual Win32 application startup and teardown. It does not
inspect or modify Explorer, display a window, validate visible presentation, or
prove any tab-management functionality. There are no policy unit tests yet.

See [Development](../docs/guides/DEVELOPMENT.md) for configure/build/test commands.

The accepted validation boundary separates pure policy tests (capture lifecycle,
tab ordering/selection, identity handling, and locale resolution) from live
Explorer integration checks. Add tests alongside the corresponding source;
do not create placeholder passing tests for missing functionality.

Use [Validation and Review](../docs/guides/VALIDATION.md) for scaffold checks and
[Explorer Integration Testing](../docs/guides/EXPLORER_TESTING.md) for the manual
Windows validation procedure. Record exact tested builds and recovery results;
neither documentation nor a clean build proves Explorer compatibility.
