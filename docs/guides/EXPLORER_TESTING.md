# Explorer Integration Testing

## Preconditions

No Explorer integration executable exists in the scaffold. This procedure does
not authorize injecting a DLL into the user's daily Explorer process. Execute
live integration checks only for an implemented change in an explicitly scoped,
disposable Windows test environment with a recovery path.

## Procedure

1. Record the OS edition/build, Explorer binary version, architecture, display
   scaling, monitor layout, theme, locale, and separate-process setting.
2. Use disposable folders/files and a restorable environment. Confirm how to
   regain access if folder windows or the Shell become unusable.
3. Establish ordinary Explorer behavior before attachment. Include an existing
   multi-tab Windows 11 window; do not close its tabs just to make the test pass.
4. Verify targeted UI-thread attachment and normal detach. Confirm unsupported
   windows stay visible and usable without partial Quickky changes.
5. Exercise Win+E, a desktop folder, Open in new window, third-party folder launch,
   `explorer.exe <folder>`, and `/select,<file>`. Check simultaneous launches and
   repeated discovery events. Record flashes separately from successful capture.
6. Verify Quickky tab selection/close and workspace close behavior, focus,
   renaming, navigation, selection, search state, file drag-and-drop, modal dialogs,
   taskbar/Alt+Tab, snapping, minimize/maximize, and monitor/DPI transitions.
7. On Windows 11, exercise native-tab buttons, shortcuts, menus, and mouse entry
   points. Verify no independent native-tab interaction remains. Obscured pixels
   or a blocked shortcut alone do not pass this check.
8. Test controller termination, IPC loss, and Explorer restart independently.
   Confirm inactive hidden windows become usable again, original state is
   restored, and stale attachments are not reused.
9. Verify the management page in `en-US` and `zh-CN`, language selection/fallback,
   long text, and accessible labels when that page exists.

## Evidence and completion

Record pass/fail/not-tested for each applicable case, the exact tested builds,
and observed limitations. Do not infer support for other builds. Report failure
of native-tab replacement or recovery as a blocking integration issue, not a
cosmetic defect. No cases in this procedure have been executed by initialization.
