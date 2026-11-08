<p align="center">
  <img src="resources/logo.png" width="96" height="96" alt="QTabs logo">
</p>

<h1 align="center">QTabs</h1>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT license"></a>
  <img src="https://img.shields.io/badge/platform-Windows_x86__64-0078D4" alt="Windows x86_64">
  <a href="https://github.com/achaosxyz/quickky-explorer-tabs/actions/workflows/windows-smoke.yml?query=branch%3Adevelop" title="Latest develop smoke-test status and run history">
    <img src="https://github.com/achaosxyz/quickky-explorer-tabs/actions/workflows/windows-smoke.yml/badge.svg?branch=develop" alt="Windows smoke (develop)">
  </a>
</p>

<p align="center"><strong>English</strong> · <a href="README.zh-CN.md">Simplified Chinese</a></p>

QTabs collects newly opened File Explorer folder windows into one tabbed
workspace, without replacing Explorer or adding a separate browsing window.

## Features

- Windows 11 uses Explorer's native tabs; Windows 10 uses rectangular tabs.
- Automatically collect new folder windows, excluding Control Panel and other
  system-management pages.
- Select tabs to switch folders and use the close button to close a tab.
- Use **+** to open another tab at the active folder's **current** location.
- Support maximize, minimize, and restore on Windows 10.
- Run from the system tray with a single background instance.

## Getting started

1. Download `QTabs_vX.X.X.zip` from [Releases](https://github.com/achaosxyz/quickky-explorer-tabs/releases/latest).
2. Extract the complete archive and run `QTabs.exe`.
3. Right-click the QTabs tray icon and check **Enable Tabs**.

Visual C++ runtime DLLs are included; no separate installation or administrator
privileges are required. Keep all extracted files together.

### Notification-area menu

Right-click the notification-area icon:

- **Start with Windows** — start QTabs when you sign in.
- **Enable Tabs** — turn automatic window collection on or off.
- **Compatibility check** — view the Explorer DLL version and support status.
- **About** — view the author, version, and build information.
- **Exit** — stop QTabs.

## Compatibility and limitations

- Windows x86_64; tested on Windows 10 build 19045 and Windows 11 build 26300.
  Compatibility varies with the Windows and Explorer version.
- Windows 11 uses built-in compatibility checks. Unsupported Explorer windows
  remain separate.
- Up to 128 tabs on Windows 11 or 32 on Windows 10; extra windows remain separate.
- A new window may appear briefly before collection. Native merging may reset
  file selection and navigation history.
- When QTabs is disabled or exits, Windows 10 windows return to separate windows;
  Windows 11 native tabs stay grouped in Explorer.

## Feedback

[Open an issue](https://github.com/achaosxyz/quickky-explorer-tabs/issues) with the
QTabs version, Windows version, and steps to reproduce. Hide personal file names
and paths in screenshots or logs.

## Author

**aCHAOS** · [GitHub](https://github.com/achaosxyz) · [achaosxyz@gmail.com](mailto:achaosxyz@gmail.com)

## License

Project source: [MIT](LICENSE). Bundled Microsoft Visual C++ runtime binaries
retain their original licensing terms.
