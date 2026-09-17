# hypr-hot-edge

A Hyprland plugin that triggers special workspace overlays when the mouse cursor reaches a screen edge or corner. Each of the 4 edges and 4 corners can drive its own special workspace.

## Features

- **Slot-based configuration**: Up to 16 independent triggers (edge1-edge16)
- **Edges and corners**: 4 edges plus 4 corners per monitor
- **Corner dead margin**: a configured corner carves its own width + `corner_margin` (default 10px) out of both neighbouring edges, so sliding into the corner never clips the edge first
- **Per-monitor support**: Assign edges to specific monitors or all monitors
- **Multi-monitor aware**: Each monitor can have its own edge panels with separate animations
- **Auto-hide**: Panels automatically close when cursor leaves the panel area, or opt out per slot with `hide_on_leave = 0` for fullscreen panels
- **Dwell time**: Optional delay before triggering (prevents accidental activation)
- **Instant slam trigger**: Hitting the absolute screen edge (or the exact corner pixel) triggers immediately, bypassing dwell
- **Keyboard toggle**: Hotkeys to show/hide panels with grace period to prevent immediate close
- **Smart dispatchers**: Toggle by side name (right/bottom) - automatically finds correct slot for current monitor

## Installation

### Building from source

```bash
# Clone the repository
git clone https://github.com/claychinasky/hypr-hot-edge.git
cd hypr-hot-edge

# Build
make

# The plugin will be at build/hypr-hot-edge.so
```

### Reloading during development

Use `make reload`, not `make unload load`.

`hyprctl plugin unload` reports `ok` but does not drop the mapping, so a
following `load` of the **same path** gets the cached image back from `dlopen`
and silently keeps running the old code -- reporting `ok` a second time. Every
symptom looks like your change did nothing. `make reload` loads a uniquely
named copy each time, which forces the linker to map the new build.

To confirm which build is live, query an option that only exists in the new one:

```bash
hyprctl getoption plugin:hyprhotedge:edge9:enabled   # "no such option" => stale image
```

### Requirements

- Hyprland (built from source or with headers available)
- Lua 5.5 headers (the version Hyprland links; needed for the `hl.plugin.hyprhotedge` bindings)
- CMake
- C++23 compatible compiler

## Configuration

Add to your `hyprland.conf`:

```conf
# Load the plugin
plugin = /path/to/hypr-hot-edge.so

# Configure slots (edge1 through edge16)
plugin {
    hyprhotedge {
        # Each slot can be any edge on any monitor
        # side = left/right/top/bottom/topleft/topright/bottomleft/bottomright
        # target_monitor = "*" for all, or specific name like "DP-1"

        # Gap in px between a corner zone and the edges either side of it.
        # Global - applies to every corner on every monitor.
        corner_margin = 10

        edge1 {
            enabled = 1
            side = right
            trigger_width = 15        # pixels from edge to trigger zone
            dwell_time = 150          # ms delay before triggering (0 = instant)
            special_workspace = hotedge-right-mon1
            target_monitor = HDMI-A-1
        }

        edge2 {
            enabled = 1
            side = right
            trigger_width = 15
            dwell_time = 150
            special_workspace = hotedge-right-mon2
            target_monitor = DP-3
        }

        edge3 {
            enabled = 1
            side = bottom
            trigger_width = 15
            dwell_time = 150
            special_workspace = hotedge-bottom-mon1
            target_monitor = HDMI-A-1
        }

        edge4 {
            enabled = 1
            side = bottom
            trigger_width = 15
            dwell_time = 150
            special_workspace = hotedge-bottom-mon2
            target_monitor = DP-3
        }

        # Corners work the same way; the trigger zone is a
        # trigger_width x trigger_width square in the corner.
        # Enabling a corner also shrinks the two edges next to it on the
        # same monitor by trigger_width + 10px, leaving a gap where
        # neither fires - that gap is what stops the edge from winning
        # the race to the corner.
        edge5 {
            enabled = 1
            side = topright
            trigger_width = 15
            dwell_time = 150
            special_workspace = hotedge-topright-mon1
            target_monitor = HDMI-A-1
        }

        # edge6 through edge16 available for more configurations
    }
}
```

### Workspace Rules

Configure how panels appear on screen using `gapsout` (format: top right bottom left):

```conf
# Right panels (~1/3 width from right edge, 1700px gap on left for 2560px wide monitor)
workspace = special:hotedge-right-mon1, gapsout:0 0 0 1700
workspace = special:hotedge-right-mon2, gapsout:0 0 0 1700

# Bottom panels (~1/3 height from bottom, 960px gap on top for 1440px tall monitor)
workspace = special:hotedge-bottom-mon1, gapsout:960 0 0 0
workspace = special:hotedge-bottom-mon2, gapsout:960 0 0 0

# Left panels (gap on right)
workspace = special:hotedge-left, gapsout:0 1700 0 0

# Corner panels (a quadrant: gap on the two opposite sides)
workspace = special:hotedge-topright-mon1, gapsout:0 0 960 1700

# Top panels (gap on bottom)
workspace = special:hotedge-top, gapsout:0 0 960 0
```

### Fullscreen panels

The plugin never sizes a panel -- that is entirely your `workspace` rule. But it
does assume, for auto-hide, that the panel is the 1/3-of-screen rectangle
`getPanelArea()` computes. Make a panel fullscreen without telling it and the
panel closes the instant the cursor leaves that rectangle, while still visibly
covering the screen.

So a fullscreen panel needs both halves:

```conf
plugin {
    hyprhotedge {
        edge5 {
            enabled = 1
            side = bottomleft
            special_workspace = hotedge-fullscreen
            target_monitor = DP-3
            hide_on_leave = 0          # <- without this it closes immediately
        }
    }
}

workspace = special:hotedge-fullscreen, gapsout:0 0 0 0
```

Give yourself a way to close it, since the cursor no longer does:

```conf
bind = SUPER CTRL, B, hotedge:toggle, bottomleft
```

It also closes when focus leaves its workspace, when the cursor moves to another
monitor, or when **another panel takes over** -- Hyprland allows one active
special workspace per monitor, so flicking to a configured edge displaces the
fullscreen panel, and that edge then auto-hides itself. That makes an ordinary
edge a usable dismiss gesture. Note the edge must lie outside the corner's dead
band (`trigger_width + corner_margin`) for the flick to register.

### Animation

```conf
# Fade animation (works well for all edge directions)
animation = specialWorkspace, 1, 3, almostLinear, fade
animation = specialWorkspaceIn, 1, 2, almostLinear, fade
animation = specialWorkspaceOut, 1, 3, almostLinear, fade

# Or slide animation (horizontal only)
animation = specialWorkspace, 1, 4, easeOutQuint, slide

# Disable blur/dim for cleaner look
decoration {
    dim_special = 0.0
    blur {
        special = false
    }
}
```

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | int | 0 | Enable this edge slot (0 or 1) |
| `side` | string | "right" | Zone: `left`, `right`, `top`, `bottom`, `topleft`, `topright`, `bottomleft`, `bottomright` |
| `trigger_width` | int | 15 | Pixel width of the trigger zone; for a corner this is the side of its square, and it also sets how much of the neighbouring edges the corner reserves |
| `dwell_time` | int | 150 | Milliseconds to wait in the zone before triggering. `0` = fire the moment the zone is entered. Ignored when you hit the absolute edge/corner pixel, which is always instant |
| `special_workspace` | string | "" | Name of the special workspace to toggle |
| `target_monitor` | string | "*" | Monitor name or "*" for all monitors |
| `hide_on_leave` | int | 1 | Auto-hide once the cursor leaves the panel area. Set `0` for a panel whose real size does not match the 1/3-of-screen rectangle the plugin assumes -- a fullscreen workspace above all. It then closes only on focus loss, monitor change, another panel taking over, or a dispatcher |

### Global Options

Set directly under `hyprhotedge`, not inside a slot:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `corner_margin` | int | 10 | Gap in px between a corner zone and the edges either side of it. An edge is inset by that corner's `trigger_width + corner_margin`, and the corner occupies the `trigger_width` part, so the untriggerable gap is exactly `corner_margin`. Clamped at 0 |

The two corner knobs do different jobs, and it is easy to reach for the wrong one:

- `trigger_width` on the corner slot sizes **the corner target itself** (a
  `trigger_width` x `trigger_width` square). Raise it to make the corner easier
  to hit.
- `corner_margin` only pushes **the neighbouring edges further away**. It does
  not enlarge the corner.

With `trigger_width = 15` and `corner_margin = 100`, the corner is still a 15x15
square at 0-15; 15-115 triggers nothing at all, and the edge resumes at 115.

## Dispatchers

The plugin provides dispatchers for both per-panel control and a global on/off switch for cursor-driven edge detection:

```conf
# Toggle by side name (recommended - auto-selects slot for current monitor)
bind = SUPER CTRL, H, hotedge:toggle, right
bind = SUPER CTRL, B, hotedge:toggle, bottom
bind = SUPER CTRL, L, hotedge:toggle, left
bind = SUPER CTRL, T, hotedge:toggle, top
bind = SUPER CTRL, Q, hotedge:toggle, topleft
bind = SUPER CTRL, E, hotedge:toggle, topright

# Toggle specific slot
bind = SUPER CTRL, 1, hotedge:toggle, edge1
bind = SUPER CTRL, 2, hotedge:toggle, edge2

# Show/hide specific edges
bind = SUPER CTRL, S, hotedge:show, right
bind = SUPER CTRL, X, hotedge:hide, right

# Pause/resume cursor-driven edge detection globally
bind = SUPER CTRL, P, hotedge:toggle-active
```

### Dispatcher Arguments

| Argument | Description |
|----------|-------------|
| `right`, `left`, `top`, `bottom` | Toggle edge by side for current monitor |
| `topleft`, `topright`, `bottomleft`, `bottomright` | Toggle corner for current monitor (aliases: `tl`, `tr`, `bl`, `br`) |
| `edge1` - `edge16` | Toggle specific slot |
| `1` - `16` | Toggle slot by number |
| *(empty)* | Toggle first visible or first enabled slot |

### Runtime Activation

The `hotedge:enable`, `hotedge:disable`, and `hotedge:toggle-active` dispatchers gate **only cursor-driven automation** — useful when you want to temporarily stop edges from triggering on mouse movement (e.g. while gaming, drawing, or doing precise edge work) without losing your panel layout.

| Dispatcher | Effect |
|------------|--------|
| `hotedge:enable` | Resume cursor-edge detection and focus-loss auto-hide |
| `hotedge:disable` | Pause cursor-edge detection and focus-loss auto-hide |
| `hotedge:toggle-active` | Flip the current state |

What disable does **not** touch:
- **Visible panels stay visible.** Open them with the cursor first, then disable, and they remain until you hide them.
- **Windows inside the special workspaces are untouched.** They're owned by Hyprland, not the plugin.
- **Explicit dispatchers still work.** `hotedge:toggle`, `hotedge:show`, and `hotedge:hide` always honor user intent regardless of active state. You can also still toggle the workspaces directly via `hyprctl dispatch togglespecialworkspace <name>`.

State is runtime-only (resets to enabled on plugin load / Hyprland restart).

## Lua configuration (`hyprland.lua`)

Everything above is written for the classic `hyprland.conf`. If Hyprland is
running your config through Lua (Omarchy does, for one), three things differ.

### Options live under `plugin.hyprhotedge`

The config namespace is `hyprhotedge` (one word, no hyphen) on purpose:
Hyprland's Lua bridge rewrites hyphens in option paths to underscores, so a
hyphenated name is spelled one way in `hyprland.conf` and another in Lua.

```lua
hl.plugin.load(os.getenv("HOME") .. "/hypr-hot-edge/build/hypr-hot-edge.so")

hl.config({
    plugin = {
        hyprhotedge = {
            corner_margin = 100,
            edge1 = {
                enabled = 1,
                side = "right",
                trigger_width = 15,
                dwell_time = 150,
                special_workspace = "hotedge-right-dp3",
                target_monitor = "DP-3",
            },
            edge2 = {
                enabled = 1,
                side = "bottomleft",
                special_workspace = "hotedge-fullscreen",
                target_monitor = "DP-3",
                hide_on_leave = 0,
            },
        },
    },
    decoration = { dim_special = 0.0, blur = { special = false } },
})

-- gaps_out takes a per-side table, not the "top right bottom left" string.
hl.workspace_rule({ workspace = "special:hotedge-right-dp3", gaps_out = { left = 1700 } })
hl.workspace_rule({ workspace = "special:hotedge-fullscreen", gaps_out = 0 })

hl.animation({ leaf = "specialWorkspace", enabled = true, speed = 3, bezier = "almostLinear", style = "fade" })
```

### Dispatchers are called through `hl.plugin.hyprhotedge`

Lua-mode Hyprland has no way to invoke a named dispatcher such as
`hotedge:toggle` -- not from `hl.bind`, not from `hl.dispatch`, and not from
`hyprctl dispatch` either (which is just `hl.dispatch(...)` in that mode). The
plugin therefore also registers every dispatcher as a Lua function:

| Lua | Equivalent dispatcher |
|-----|-----------------------|
| `hl.plugin.hyprhotedge.toggle(arg)` | `hotedge:toggle` |
| `hl.plugin.hyprhotedge.show(arg)` | `hotedge:show` |
| `hl.plugin.hyprhotedge.hide(arg)` | `hotedge:hide` |
| `hl.plugin.hyprhotedge.enable()` | `hotedge:enable` |
| `hl.plugin.hyprhotedge.disable()` | `hotedge:disable` |
| `hl.plugin.hyprhotedge.toggle_active()` | `hotedge:toggle-active` |

`arg` is the same string the dispatcher takes (`right`, `bottomleft`, `edge3`,
...). Each returns `true`, or `false, "error"`.

The `hl.plugin.hyprhotedge` table does not exist on the very first config pass --
Lua runs before the plugin has loaded -- so a bind must look it up at press
time, not at config time:

```lua
local function hotedge(fn, arg)
    return function()
        local p = hl.plugin.hyprhotedge
        if p and p[fn] then p[fn](arg) end
    end
end

hl.bind("SUPER + CTRL + H", hotedge("toggle", "right"))
hl.bind("SUPER + CTRL + B", hotedge("toggle", "bottomleft"))
hl.bind("SUPER + D", hotedge("toggle_active"))
```

Test from a shell with `hyprctl repl 'hl.plugin.hyprhotedge.toggle("right")'`.

### First-pass config errors are expected

On a cold start `hyprctl configerrors` may briefly list every
`plugin.hyprhotedge.*` key as unknown: the plugin registers them after the first
pass, and Hyprland re-runs the config once it has loaded. If they persist after
that, the plugin did not load -- see [Troubleshooting](#troubleshooting).

## Multi-Monitor Setup

For proper multi-monitor support, use **unique workspace names per monitor**. This prevents animation glitches where panels animate from the wrong position.

```conf
plugin {
    hyprhotedge {
        # Monitor 1 - right edge
        edge1 {
            enabled = 1
            side = right
            special_workspace = hotedge-right-hdmi
            target_monitor = HDMI-A-1
        }

        # Monitor 2 - right edge (different workspace name!)
        edge2 {
            enabled = 1
            side = right
            special_workspace = hotedge-right-dp3
            target_monitor = DP-3
        }
    }
}

# Separate workspace rules for each
workspace = special:hotedge-right-hdmi, gapsout:0 0 0 1700
workspace = special:hotedge-right-dp3, gapsout:0 0 0 1700
```

Find your monitor names with:
```bash
hyprctl monitors | grep Monitor
```

## Behavior Notes

- **Edge trigger**: Moving cursor to the absolute screen edge (last 2-3 pixels) triggers immediately, bypassing dwell time
- **Corner trigger**: A corner needs *both* of its boundaries, so only the exact corner pixel is instant. Anywhere else inside the corner square waits out `dwell_time` -- if corners feel sluggish, set `dwell_time = 0` on that slot
- **Corner dead margin**: each configured corner reserves `trigger_width + 10px` at the neighbouring ends of both its edges. Neither the edge nor the corner fires in that gap; it is what stops the edge from opening on your way into the corner
- **Zone trigger**: Moving cursor into the trigger zone (configurable width) starts the dwell timer
- **Auto-hide**: Panel closes when cursor leaves the panel area (with 150ms delay to prevent flicker)
- **Timers do not need mouse movement**: dwell and auto-hide run on a compositor timer that is armed only while something is pending, so they still fire with the cursor sitting perfectly still
- **Monitor-aware**: Moving cursor to another monitor automatically closes the panel on the previous monitor
- **Keyboard grace period**: When toggling via keyboard, there's an 800ms grace period before auto-hide kicks in, giving you time to move your cursor to the panel

## Troubleshooting

### Plugin not loading
- Check Hyprland log: `tail -f /tmp/hypr/$HYPRLAND_INSTANCE_SIGNATURE/hyprland.log | grep HotEdge`
- Ensure the plugin path is correct
- Rebuild after Hyprland updates

### Edge not triggering
- Verify `enabled = 1` in config
- Check `target_monitor` matches your monitor name (`hyprctl monitors`)
- Ensure `special_workspace` is set
- If you just rebuilt, make sure the new build is actually loaded -- see
  [Reloading during development](#reloading-during-development)

### Edge dead near a corner
Expected: a configured corner reserves `trigger_width + corner_margin` of both
adjacent edges. Lower `corner_margin` (or the corner's `trigger_width`) to
shrink the gap; raise `corner_margin` if you keep catching the edge on your way
into the corner.

### Config change had no effect
`hyprctl keyword plugin:hyprhotedge:...` updates Hyprland's registry but does **not**
reach the plugin -- it reads its values on the config-reloaded event, which
`keyword` does not raise. Edit `hyprland.conf` and run `hyprctl reload` instead.
`hyprctl getoption` will happily show the new value either way, so it is not a
reliable check that the setting is live.

In Lua mode, `unknown config key 'plugin.hot_edge...'` (or `hot-edge`) means
the config still uses the pre-0.5 namespace; it is `hyprhotedge` now. See
[Lua configuration](#lua-configuration-hyprlandlua).

### Corner feels slow
Only the exact corner pixel bypasses `dwell_time`. Set `dwell_time = 0` on the
corner slot to fire as soon as the square is entered, and/or raise the corner's
`trigger_width` to make that square a bigger target. Raising `corner_margin`
will not help -- it moves the edges away, it does not grow the corner.

### Panel closes immediately
- Check if cursor is in the panel area when it opens
- For keyboard toggles, you have 800ms to move cursor to panel
- Verify no other window is stealing focus

### Animation issues on multi-monitor
- Use unique workspace names per monitor
- Don't share the same `special_workspace` across different `target_monitor` values

### Reserved keyword error
- Use `target_monitor` not `monitor` (monitor is a reserved Hyprland keyword)

## Example: Minimal Single-Monitor Setup

```conf
plugin = /path/to/hypr-hot-edge.so

plugin {
    hyprhotedge {
        edge1 {
            enabled = 1
            side = right
            special_workspace = sidebar
            target_monitor = *
        }
    }
}

workspace = special:sidebar, gapsout:0 0 0 1700
animation = specialWorkspace, 1, 3, almostLinear, fade

bind = SUPER, H, hotedge:toggle, right
```

## License

MIT
