# Quickky Explorer Tabs

[English](README.md) | [Simplified Chinese](README.zh-CN.md)

A tabbed workspace for Windows File Explorer, designed to collect newly opened
folder windows into one place.

> **Not ready for daily use.** The application runs in the background and only
> observes Explorer windows. It does not display tabs, collect windows, replace
> native tabs, or provide a management interface yet. There is no installable
> release. The features below describe the intended experience.

## The experience

- **Keep folders together.** Newly opened File Explorer folder windows join the
  same tabbed workspace instead of remaining separate windows.
- **Use the Explorer you know.** Keep File Explorer's folder browsing experience
  rather than switch to a different file manager or a separate Quickky window.
- **Open beside your current folder.** The tab-strip `+` action opens another
  Explorer window at the active tab's current folder and collects it as a tab.
- **Get consistent tabs on Windows 10 and Windows 11.** Quickky provides its own
  tabs with a Windows 11-inspired appearance. On Windows 11, they replace the
  built-in tab controls rather than add a second row of tabs.
- **Choose your language.** The management interface is designed for English and
  Simplified Chinese.

## Windows support

The target is Windows 10 and Windows 11 on x86_64 PCs. Specific Windows versions
have not yet been verified.

The workspace is scoped to Explorer folder windows on the current virtual
desktop. It does not include the desktop itself, the taskbar, or Open/Save dialogs.

## Getting started

There is no installer or usable tabbed workspace yet. If you have the current
`QuickkyExplorerTabs.exe`, launching it starts a background observer with no
independent window. Run `QuickkyExplorerTabs.exe --stop` to exit. It does not
change File Explorer or its configuration and does not need administrator rights.
Installation and full tab usage instructions require a usable release.

## License

[MIT](LICENSE).
