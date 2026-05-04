# Phase 5C: Floating input-method window — investigation & proposal

Investigation date: 2026-05-03. Subject: making plasma-keyboard (or any
KWin Wayland input method) behave as a small, draggable, decorated
toplevel window — Windows-touch-keyboard / iPadOS-floating-keyboard
style — instead of the bottom-anchored panel KWin currently enforces.

This document is intended as a starting point for a KDE Discuss /
Matrix conversation with KWin maintainers before any patches land.

## Current behavior

When KWin needs an input method, it:

1. Reads `kwinrc [Wayland] InputMethod[$e]=` for the IM binary path.
2. Creates a private Wayland socket connection
   (`WaylandServer::createInputMethodConnection()` →
   `m_inputMethodServerConnection`).
3. Spawns the IM binary with `WAYLAND_SOCKET=<fd>` so the IM client
   connects on that private connection.
4. Restricts `zwp_input_method_v1` and `zwp_input_panel_v1` so they
   can ONLY be bound by that connection
   (`src/wayland_server.cpp:145, 173`).

When the IM client binds `zwp_input_panel_v1` and creates a panel
surface, KWin wraps it as `InputPanelV1Window`
(`src/inputpanelv1integration.cpp:27`), which hardcodes:

```cpp
bool isPlaceable() const override   { return false; }
bool isCloseable() const override   { return false; }
bool isResizable() const override   { return false; }
bool isMovable() const override     { return false; }
bool isMovableAcrossScreens() const override { return false; }
bool acceptsFocus() const override  { return false; }
bool isInputMethod() const override { return true; }
```

The window is positioned by `InputPanelV1Window::showTopLevel(...)`
based on the `position` argument the client passed to
`zwp_input_panel_surface_v1.set_toplevel(...)`.

**Net effect:** the panel is undecorated, immovable, unresizable, and
positioned by KWin. Bottom-anchored full-width is the only viable
layout. This makes architectural sense for compact candidate selectors
on phones; it is wrong for full-featured desktop input methods that
want to behave like ordinary tool windows.

## Why two simpler workarounds don't fly

**Skip `zwp_input_panel_v1`, use xdg-shell from the IM client.**
The IM client can in principle just create a regular xdg-shell
toplevel without ever binding `zwp_input_panel_v1`. We tested this in
a local plasma-keyboard branch
(`phase5c-floating-experiment`):

- ✅ Text input (input-method-v1 commits) continued to work.
- ❌ But when KWin spawned plasma-keyboard with this branch, the
  resulting window still appeared as bottom-anchored, undecorated,
  full-width — same as a normal panel.

We were not able to definitively distinguish whether KWin applies
panel-style treatment to *all* windows from the IM connection, or
whether something in QtVirtualKeyboard / Qt's QPA rebinds
`zwp_input_panel_v1` implicitly. (Test was inconclusive partly
because we were initially testing against a stale spawned process,
and partly because a clean reproduction with hard-coded window
geometry produced "nothing appears.") Worth re-running with KWin
debug logging.

**Two-process architecture: a tiny "shim" IM + a normal-app keyboard
window with IPC between them.** Killed by:
> `if (client != waylandServer()->inputMethodConnection() && inputmethodInterfaces.contains(interfaceName))` (`wayland_server.cpp:173`)

The keyboard window process can't bind `zwp_input_method_v1` because
it isn't the IM-connection client. It would have to round-trip every
keystroke through the shim over a private socket. Architecturally
ugly (two processes pretending to be one) and KWin still owns whether
to allow it. Not worth pursuing.

## Proposal: extend KWin + plasma-keyboard with a floating-window role

### plasma-keyboard

`InputPanelRole` already enumerates Keyboard / OverlayPanel. Add a
third value:

```cpp
enum class Role {
    Keyboard,        // zwp_input_panel_v1 bottom-anchored panel
    OverlayPanel,    // zwp_input_panel_v1 compositor-positioned overlay
    FloatingWindow,  // ordinary xdg-shell toplevel (NEW)
};
```

When `FloatingWindow`, `initInputPanelIntegration` skips the
`QWaylandInputPanelShellIntegration` swap entirely; the window comes
up as Qt's default xdg-shell toplevel. Position is persisted via
`Qt.labs.settings`. A small chrome bar (drag area + close button) can
be optional since standard KWin decorations suffice on the desktop.

User-selectable from the existing kcm_plasmakeyboard config UI.

### KWin

The narrow change: a window property (or, cleaner, a new Wayland
protocol) the IM client sets to opt out of `InputPanelV1Window`
treatment. Three implementation sketches in roughly increasing
cleanness:

**A. Window property + xdg-shell.** IM client creates an xdg-shell
toplevel, sets a window property `_kde_kwin_floating_input_method=1`
(some namespaced form). KWin sees the property when wrapping the
window and:
- Does NOT instantiate `InputPanelV1Window` for it.
- Does NOT apply input-method positioning policy.
- Treats it as an ordinary `XdgToplevelWindow` (decorations, drag,
  close, etc.).
- The input-method-v1 protocol still routes through this client for
  text-commits, so input continues to flow.

**B. New `mode` parameter on `zwp_input_panel_surface_v1.set_toplevel`.**
Add a `position_floating` value alongside the existing
`position_center_bottom`. KWin treats floating panels as
xdg-shell-equivalent in window management while keeping the panel
binding for protocol consistency. Larger protocol surface but stays
within input-panel-v1.

**C. Input-method-v2 alignment.** The wider Wayland ecosystem is
moving toward input-method-v2 / text-input-v3. If KDE plans to
migrate, the floating-window concern should be raised at the
protocol design level, not bolted onto the deprecated v1 path. This
is the maintainer-buy-in question.

**Backward compat:** Maliit and other existing input methods don't
set the new property / don't request the new mode → no change in
behavior for them.

### Open questions for KDE maintainers

1. Is `zwp_input_method_v1` / `zwp_input_panel_v1` slated for
   replacement by input-method-v2? Patching v1 risks rapid bitrot
   if so.
2. Is there appetite for a "tool window input method" concept, or
   does the project consider input methods inherently
   compositor-positioned panels?
3. Would the property approach (sketch A) be acceptable as a
   minimal first step, or is a protocol extension preferred?
4. Is there precedent for IM clients wanting to be normal toplevels
   that we should align with (Hyprland, Sway, GNOME)?

## Reference points

- `src/wayland_server.cpp:145, 173` — IM-connection interface
  authorization (the wall that makes Path A impossible).
- `src/inputpanelv1integration.cpp:27` — where panel windows are
  wrapped.
- `src/inputpanelv1window.h:36-70` — the hardcoded immobility.
- `src/wayland/inputmethod_v1.cpp:368-442` — panel-surface binding.
- `~/Projects/plasma-keyboard` branch `phase5c-floating-experiment`
  — local exploration; both commits worth keeping for reference.

## What ships meanwhile

Nothing. The bottom-anchored swipe keyboard is functional but is a
dealbreaker for the personal use case driving this work, so we
won't ship it as the final form. Hold release until the floating
question is resolved.
