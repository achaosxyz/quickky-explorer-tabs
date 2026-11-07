# Tab Chrome

## Status

`TabStrip` implements experimental Win32 caption/diagnostic chrome with Direct2D/DirectWrite.
Capture, switching, close buttons, and click-time `+` have scoped host evidence;
full accessibility and general theme/layout compatibility are not established.
The classic adapter uses an Explorer-owned, non-activating caption tool window
with rectangular tabs. Scoped Windows 10 build 19045 tests validate capture,
current-folder creation, maximize/minimize/restore, switching, and recovery at
96 DPI. Synthetic tests alone are not Windows 10 Explorer evidence.
The separate child/clipping fixture is diagnostic-only. Neither custom surface
is initialized by the Windows 11 native backend.
See the [overall design](PROJECT_DESIGN.md).

## Responsibility

Present Quickky-owned rectangular tabs on Windows 10 and translate their input
into group actions. Windows 11 presentation belongs entirely to Explorer.

## Contract

- Use the Quickky group model as the source of labels, ordering, and selection;
  do not inspect a native TabView to build a second independent tab model.
- Implement an independent rectangular tab bar on Windows 10. Never initialize
  it for the Windows 11 native backend or overlay an existing native tab row.
- Keep the containing attachment alive across reentrant root/child callbacks;
  destruction during layout/configuration must never invalidate the presenter
  while a method is still on the stack.
- Use Win32, Direct2D, and DirectWrite for the tab strip. No web UI or additional
  UI toolkit is part of the selected baseline.
- Define selected, inactive, hovered, focused, and close-button states consistently
  with system themes, high contrast, and per-monitor DPI.
- Preserve title-bar drag, double-click, system buttons, and resizing behavior.
- Route Quickky tab actions through the group policy rather than directly mutating
  Explorer or synthesizing global keyboard input.
- Attach the strip to real Explorer chrome, not an independent Quickky browsing
  window. The `+` button resolves the active tab's current location, opens a new
  Explorer folder window there, and lets normal capture admit it. These actions
  run only after explicit workspace enablement.
- Classic caption bounds come from `DWMWA_CAPTION_BUTTON_BOUNDS`, in window
  coordinates. Never query undefined minimized/hidden caption geometry: hide the
  surface until the owner is restored. The strip stays left of system caption
  buttons, has no taskbar/Alt+Tab entry, never activates itself, and does not move
  Explorer navigation or file content. Root visibility changes explicitly hide
  or restore it. A recursion guard bounds nested window-position callbacks.
  A traditional caption ends above the native client origin. For the observed
  Windows 10 client-extended Ribbon layout, require a visible top Ribbon dock
  below the caption. Seed the resize frame with per-DPI system metrics, trim
  additional native resize rows by hit testing, then
  scan native `WM_NCHITTEST` results at the top, middle, and bottom of each column
  for a contiguous `HTCAPTION` span left of system buttons, rejecting columns
  occupied by visible child windows rather than assuming root hit tests route
  to those children. Exclude Quick Access
  controls and reject insufficient/unknown layouts. Recompute on maximize and
  restore; the maximized frame's negative outer coordinates are not screen bounds.
  Entirely off-monitor owners hide the surface, including the restore transition
  where the minimized style clears before its offscreen placement changes.
  Caption loss makes the binding unhealthy and invokes local recovery.
  Joining/reflowing a background owner must preserve its z-order band rather
  than raise the caption over another application; disabled owners hide it.
  Root layout notifications coalesce a posted reflow and hide the old surface
  until Explorer's outer placement callbacks finish. Present reflows after the
  placement API returns; callbacks do not test unsettled native hit regions.
- Make keyboard navigation and accessible names part of the interaction contract;
  a visual replica alone is not an acceptable functional tab strip.
- Windows 11 tab transfer belongs to the Explorer adapter, not this renderer.
  Custom rendering is not evidence that a native tab was transferred.

## Boundary

The Windows 10 presenter uses an Explorer-owned `Quickky.TabStrip` caption popup
on the attached Explorer UI thread. Direct2D/DirectWrite paint rectangular tabs,
labels, close targets, and `+`; hit testing uses the same DPI-scaled layout as
painting. It never clips Explorer content. The separate diagnostic child fixture
can paint rounded tabs over a region-masked native host; that fixture is not the
resident backend or compatibility evidence. The classic creation guard rejects
additional native Shell views. Empty strip space forwards caption drag and
double-click; system caption-button space is excluded. At high tab counts the
visible range follows selection and the mouse wheel scrolls it. Input is sent to
group policy, never implemented by driving the hidden native tab strip.

This is an experimental renderer: full UI Automation accessibility and verified
theme/DPI/title-bar behavior remain release gates, not implied by visible tabs.

The HWND accessible name includes the tab count; individual accessible tab/button
providers are not implemented. Do not add a full UI framework for these primitives.
Management-page localization belongs to [Settings](SETTINGS.md).

Inactive, minimized, and hidden-owner surfaces discard their HWND render target
and brush; factories and text format remain for lazy repaint on show. Transient
caption-reflow hiding retains them to avoid allocation churn. Hover invalidation
occurs only when the painted tab changes, not for every coordinate change within
it. Theme/high-contrast
state is cached between theme, settings, and system-color notifications; root
notifications also invalidate the child fixture's cache. Paint does not query
the registry or high-contrast settings.

## Acceptance evidence

Test themes, scaling, monitor transitions, input hit regions, title-bar behavior,
keyboard operation, and accessibility alongside the actual Explorer adapter.
