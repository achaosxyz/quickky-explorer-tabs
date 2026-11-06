# Settings and Localization

## Status

`src/settings/` is reserved. Initial English and Simplified Chinese JSON catalogs
exist under `resources/locales/`; there is no management page, locale loader, or
settings persistence. See the [overall design](PROJECT_DESIGN.md).

## Responsibility

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

No management-page framework, persistence location, translation library, or
in-process locale switching mechanism is selected. Add concrete choices with
the first implementation, not as scaffold dependencies.

## Acceptance evidence

Check JSON validity, duplicate keys, exact key parity, nonempty values, and
placeholder parity. Runtime checks must cover both supported languages,
explicit selection, system-language resolution, fallback, and text clipping.
