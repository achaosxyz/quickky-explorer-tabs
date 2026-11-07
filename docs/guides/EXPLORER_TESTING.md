# Explorer Integration Testing

## Preconditions

Snapshot/bounded diagnostics remain read-only; explicitly enabled workspaces
attach and change window visibility. Run at the same integrity level as Explorer,
without elevation. Hidden test-owned fixtures are separate from this mutating
SOP. This procedure does not authorize injecting into the user's daily Explorer.
Use an explicitly scoped, disposable Windows test environment with a recovery
path. On the authorized local host, create fresh folders under `build/live-test`
and run only `--capture-test-root` for that root; preserve all baseline windows.
First exclude an existing Quickky controller so a stop request cannot affect
someone else's session. Never terminate Explorer to clean up a test.

## Procedure

### Windows 11 native backend

1. Record the OS build and `ExplorerFrame.dll` file version. Native mutation is
   permitted only for a matched ABI; an unmatched frame must remain unchanged.
2. Snapshot existing Explorer roots/tabs. Create a unique directory beneath
   `build/live-test`, with `Alpha`, `Beta`, and `Gamma` subdirectories. All
   browser windows used below must be newly created for this run.
3. Open `Alpha`, then run `QTabs.exe --capture-test-root "<absolute test root>"`.
   Open `Beta` in a new window. Require one Explorer root with both real native
   tabs; check that no `Quickky.TabStrip` child exists and the native XAML host's
   Win32 region has not been clipped.
4. Invoke the native `+` button and press Ctrl+T. Each must add exactly one native
   tab at the current location. Confirm the resulting Shell view location, not
   only its label or a native Add HRESULT.
5. Repeat with a separate, pre-existing two-tab test source at `Beta` and
   `Gamma`, focusing `Alpha` before enablement. Require all three folders to
   survive in one native frame. Transfer removes only an unchanged source GUID
   after destination GUID/PIDL confirmation, never the whole multi-tab source.
6. Stop the controller immediately after creation and while reconciliation is
   active. Require timely exit, restored native behavior, and preservation of
   the merged native tabs. Baseline roots/tabs must remain unchanged.
7. If a merge times out or the source changes, preserve it. A failed/uncertain
   frame is not automatically retried during the current enablement session.
8. Close only the test-owned windows whose contents are still within the test
   scope. Record any unexpected window instead of guessing its ownership.

For a minimal manual transport/ABI check, stop the controller first and use:

```powershell
.\build\vs2017-tabs\Debug\QuickkyNativeTransferProbe.exe `
  "<absolute test root>" <destination-HWND-decimal> <source-HWND-decimal>
```

This probe mutates the explicitly named frames. It rejects out-of-scope roots,
confirms the destination's selected GUID and PIDL, and removes one unchanged
active source GUID. It is not a discovery tool or a general compatibility test.
Do not substitute any pre-existing daily Explorer HWNDs.

### Windows 10 custom workspace

1. Confirm the controller and Explorer have the same user/session/integrity.
   Record the Windows build and DPI. Use a fresh, authorized `build/live-test`
   subtree and snapshot baseline windows before opening test folders.
2. Launch the extracted ZIP's `QTabs.exe --capture-test-root "<test root>"`.
   Open two new folders below that root. Require one visible managed frame and
   a caption surface owned by each managed frame, not a clipped file/Ribbon view.
3. Maximize the active frame and switch tabs. Require the newly active frame
   to remain maximized. Minimize/restore, then restore to normal; check caption
   bounds after each transition, including the top resize border and system buttons.
4. Navigate the active folder, invoke `+`, and verify the new view's actual path.
   Close an inactive tab and then the active tab; surviving views must remain managed.
5. Disable/re-enable from the tray; inspect About metadata and Exit restoration.
   Test controller termination separately and require hidden views to recover.
6. Close only test-owned, still-in-scope views and confirm baseline frames/tabs
   remain unchanged. Do not install a system runtime just to make the ZIP pass:
   its bundled runtime DLLs must support controller and target-hook loading.

The scoped acceptance build is Windows 10 19045 at 96 DPI. Native hit testing
from inside nested placement notifications is not stable; classic reflow is
posted after that stack unwinds. Inspect underlying caption controls without
mistaking the Quickky-owned popup itself for a native control.

### Custom presentation diagnostic

Run the optional interactive Win32 caption fixture on an authorized interactive
development desktop (it creates only its own synthetic windows):

```powershell
.\build\vs2017-tabs\Debug\QuickkyWorkspaceBridgeTests.exe --classic-caption
```

It checks a caption without XAML, unchanged client geometry, initial/background
z-order, movement, disable/enable, hide/restore, and recovery after losing the
tab surface. It is intentionally outside CTest for headless CI. This is not a
Windows 10 Explorer test. Client-extended Ribbon admission additionally requires
observed dock geometry and safe native-caption hit tests in a scoped OS test.

Run `workspace.bridge` for synthetic custom rendering, guard, clipping, and
reentrant restoration checks. These fixtures are not Windows 10 layout evidence.
Windows 10 live acceptance requires the verified classic Explorer layout adapter;
do not substitute a Windows 11 XAML-host clipping test. Windows 11 product tests
must retain native controls, not reject their entry points.

### Additional acceptance checks

1. Record scaling, monitor layout, theme, locale, and separate-process settings.
2. Confirm unsupported windows remain usable without partial Quickky changes.
3. Exercise Win+E, a desktop folder, Open in new window, third-party folder launch,
   `explorer.exe <folder>`, and `/select,<file>`. Check simultaneous launches and
   repeated discovery events. Record flashes separately from successful capture.
4. Verify selection/close and workspace close behavior, focus,
   renaming, navigation, selection, search state, file drag-and-drop, modal dialogs,
   taskbar/Alt+Tab, snapping, minimize/maximize, and monitor/DPI transitions.
5. Test controller termination, IPC loss, and Explorer restart independently.
   Windows 10 hidden managed windows must recover; Windows 11 native groups must
   remain intact. Stale attachments must not be reused.
6. Verify tray startup/tabs checkmarks, About author/version/build time/link,
   disable/re-enable, and Exit. Scoped mode must not save the global tabs setting.
   Check `en-US`/`zh-CN` strings and locale fallback; a language-picker page is not
   implemented. Test `--stop` while an owned tray/About popup is open.

## Evidence and completion

Record pass/fail/not-tested for each applicable case, the exact tested builds,
and observed limitations. Do not infer support for other builds. A failed native
transfer or recovery is a blocking integration issue, not a cosmetic defect.
The verified native profile is Windows build 26300 / ExplorerFrame
`10.0.26100.9949`; acceptance evidence is limited to explicitly executed scoped
cases in the [Explorer module](../design/EXPLORER.md). Windows 10 build 19045 has
the scoped custom-workspace acceptance above; all launch routes, accessibility,
modal behavior, and multi-monitor compatibility are not established.
Re-run affected scoped cases after runtime changes; earlier results
are not proof of a final binary.
