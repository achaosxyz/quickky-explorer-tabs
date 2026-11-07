<p align="center">
  <img src="resources/logo.png" width="96" height="96" alt="QTabs logo">
</p>

<h1 align="center">QTabs</h1>

<p align="center">Your Explorer. One workspace. Your own tabs.</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT license"></a>
  <a href="VERSION"><img src="https://img.shields.io/badge/preview-experimental-orange" alt="Experimental preview"></a>
  <img src="https://img.shields.io/badge/platform-Windows_x86__64-0078D4" alt="Windows x86_64">
</p>

<p align="center"><strong>English</strong> · <a href="README.zh-CN.md">Simplified Chinese</a></p>

QTabs brings newly opened File Explorer folder windows together as tabs,
right inside Explorer. Keep the file manager you already know, without another
browsing window or a second application to manage your files.

## Features

- Windows 11 keeps its native tab bar, without a second bar drawn over it.
- Windows 10 rectangular tabs, with maximize, minimize, restore, and window recovery.
- Automatic collection of eligible new folder windows into one workspace.
- Control Panel, Printers, and connection-management pages stay outside the workspace.
- Switch folders by selecting a tab; close a tab with its close button.
- Open another tab at the active folder's **current** location with **+**.
- Windows 11 groups stay in Explorer when QTabs is disabled or exits.
- Run in the notification area, with no independent browsing window.
- Single-instance background controller; repeated launches exit quietly.
- English and Simplified Chinese tray menus, following the Windows UI language.

## Getting started

Extract all files from `QTabs_vX.X.X.zip`. The required Visual C++ runtime DLLs
are included; no separate runtime installation is needed. Keep all extracted
DLLs beside `QTabs.exe`. Launch the
EXE normally; it does not request administrator privileges. Tabs start disabled
unless you previously enabled them. Look for the QTabs icon in the notification
area, including its overflow panel if Windows hides the icon.

### Notification-area menu

Right-click the notification-area icon:

- **Start with Windows** — enable or disable startup for your Windows account.
- **Enable Tabs** — collect eligible windows, or stop collecting when unchecked.
- **About** — see the author, version, build time, and clickable full GitHub URL.
- **Exit** — remove QTabs integration and close the tray application.

The checkmarks reflect startup registration and whether tabs are currently
enabled. Startup applies to your account only. QTabs does not install a system
service or change Explorer's global settings.

### Try it in isolation

For an isolated trial, create a dedicated folder and launch
`QTabs.exe --capture-test-root "C:\QuickkyTabs"`. Only folder
windows inside that directory are collected; this mode does not save the tabs
setting. Open folders beneath that directory, select tabs to switch, and use
**+** to open another view of the active folder. Disable tabs or exit to return
to ordinary Explorer behavior. On Windows 11, already merged tabs stay together;
they are not split into separate windows. Do not use the preview to manage important or
unrelated windows.

## Compatibility and limitations

| Area | Current preview |
| --- | --- |
| Architecture | x86_64 |
| Windows 11 | Native merge, current-folder + / Ctrl+T and exit tested on build 26300 |
| Windows 10 | Capture, current-folder +, maximize/restore and recovery tested on build 19045 |
| Existing native-tab groups | Eligible sources merge one tab at a time; unrelated tabs are not closed |
| Native workspace capacity | Up to 128 tabs; additional windows stay separate |
| Windows 10 workspace capacity | Up to 32 tabs; additional windows stay separate |
| Native transfer state | Follows native drop behavior; selection/history may reset |
| Accessibility, themes, multi-monitor and modal-window behavior | Not fully verified |

Windows 10/11 on x86_64 are the project targets. QTabs does not collect the
desktop, taskbar, Open/Save dialogs, or windows on other virtual desktops.
Native collection currently requires the tested Explorer component version
`10.0.26100.9949`; Windows updates may leave windows unmanaged until compatibility
is verified. Unsupported classic caption layouts stay separate rather than cover
Explorer's file area. Uncertain merges are not automatically retried. A timeout is
not a rollback: an already-started native operation may still complete.
Administrator privileges are not required.

Collection is not instantaneous: Explorer must finish initializing its folder
view, and a busy shell can still delay merging. A separate window may appear briefly.

## Feedback

Found a problem or have an idea? [Open an issue](https://github.com/achaosxyz/quickky-explorer-tabs/issues).
Include the version and build time from **About**, your Windows version, steps to
reproduce, and what you expected to happen. Redact personal file names and paths
before sharing screenshots or diagnostic information.

Feature ideas, bug reports, and translation corrections are welcome. Please
discuss changes that affect Explorer behavior before sending a large change.

## Author

**aCHAOS** · [GitHub](https://github.com/achaosxyz) · [achaosxyz@gmail.com](mailto:achaosxyz@gmail.com)

## License

Project source: [MIT](LICENSE). Bundled Microsoft Visual C++ runtime binaries
retain their original licensing terms.
