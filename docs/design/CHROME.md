# Tab Chrome

## Status

`src/chrome/` is reserved; no tab strip or renderer exists.
See the [overall design](PROJECT_DESIGN.md).

## Responsibility

Present Quickky-owned tabs with a Windows 11-inspired native appearance on both
Windows 10 and Windows 11, and translate presentation input into group actions.

## Contract

- Use the Quickky group model as the source of labels, ordering, and selection;
  do not inspect a native TabView to build a second independent tab model.
- Use Win32, Direct2D, and DirectWrite for the tab strip. No web UI or additional
  UI toolkit is part of the selected baseline.
- Define selected, inactive, hovered, focused, and close-button states consistently
  with system themes, high contrast, and per-monitor DPI.
- Preserve title-bar drag, double-click, system buttons, and resizing behavior.
- Route Quickky tab actions through the group policy rather than directly mutating
  Explorer or synthesizing global keyboard input.
- Attach the strip to real Explorer chrome, not an independent Quickky browsing
  window. The `+` button resolves the active tab's current location, opens a new
  Explorer folder window there, and lets normal capture admit it. The button and
  folder-launch bridge are not implemented by the read-only controller.
- Make keyboard navigation and accessible names part of the interaction contract;
  a visual replica alone is not an acceptable functional tab strip.
- Windows 11 native-tab suppression is the adapter's responsibility. Rendering
  must not be used as evidence that native-tab behavior has been replaced.

## Boundary

No pixel geometry, animation system, icon package, or accessibility implementation
has been selected. Do not add a full UI framework just for tab-strip primitives.
Management-page localization belongs to [Settings](SETTINGS.md).

## Acceptance evidence

Test themes, scaling, monitor transitions, input hit regions, title-bar behavior,
keyboard operation, and accessibility alongside the actual Explorer adapter.
