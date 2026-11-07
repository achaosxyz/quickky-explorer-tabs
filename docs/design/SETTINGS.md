# Settings and Localization

## Status

`preferences` implements per-user startup, the saved tabs setting, and locale
resolution. English/Simplified Chinese catalogs supply the tray and About dialog
through build-generated native strings. A separate management page and a runtime
language picker are not implemented. See the [overall design](PROJECT_DESIGN.md).

## Responsibility

`preferences` owns the per-user startup registration, persisted tabs-enabled
choice, and locale selection. The tray uses four catalog keys under `tray`:
`startup`, `enableTabs`, `about`, and `exit`; About field labels are under `about`.
CMake embeds these UTF-8 catalog values into
a generated native header; no JSON parser or third-party dependency is required.
Startup changes touch only `QuickkyExplorerTabs` in the current user's `Run` key.
Enabling/disabling startup takes effect only after a successful registry write;
the menu reads actual registration rather than a cached checkbox. No management
page or runtime language-selection UI is implied by localized tray text.

Own management-interface preferences, locale resolution, and translated text
lookup without coupling the page to Explorer internals.

## Contract

- Supported locale identifiers are `en-US` and `zh-CN`.
- An explicit user choice takes priority over the system UI language. Without
  a choice, resolve Chinese system UI languages to `zh-CN`, English to `en-US`,
  and unsupported languages to `en-US`.
- Resolve missing translated keys through `en-US`. Validation must reject
  missing English keys and mismatched catalog key sets before release.
- Keep all management-interface text in catalogs, including labels, actions,
  descriptions, errors, and accessible names. Do not concatenate translated
  sentence fragments or put user-facing translations in source comments.
- Use matching named placeholders across locales if parameterized messages are
  introduced. User paths and Explorer-provided folder names are not translated.
- Store catalog files as UTF-8. Translation values may contain Chinese; catalog
  keys, code identifiers, developer documentation, and comments remain English.

## Initial catalog

Both catalogs contain `settings.title`, `settings.language.label`,
`settings.language.options.en-US`, `settings.language.options.zh-CN`,
`settings.actions.apply`, and `settings.actions.cancel`. These are resource
definitions only, not evidence that the corresponding UI is implemented.

## Boundary

Preferences use `HKCU\Software\Quickky\ExplorerTabs`; startup uses the single
`QuickkyExplorerTabs` value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
The startup command is the quoted executable path, without scope-test arguments.
No service, machine-wide registration, scheduler task, or administrator rights
are needed. Scoped integration does not write the global tabs preference.
The version comes from root `VERSION`; build metadata is generated separately.

## Acceptance evidence

Check JSON validity, duplicate keys, exact key parity, nonempty values, and
placeholder parity. Runtime checks must cover both supported languages,
explicit selection, system-language resolution, fallback, and text clipping.
