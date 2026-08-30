#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopTimer.hpp>
#include <hyprland/src/config/values/ConfigValues.hpp>

#include <string>
#include <chrono>
#include <array>

enum class EdgeSide {
    LEFT = 0,
    RIGHT = 1,
    TOP = 2,
    BOTTOM = 3,
    TOP_LEFT = 4,
    TOP_RIGHT = 5,
    BOTTOM_LEFT = 6,
    BOTTOM_RIGHT = 7
};

// Support up to 16 configurations (8 zones — 4 edges + 4 corners — × 2 monitors)
constexpr int MAX_EDGE_SLOTS = 16;
constexpr const char* EDGE_SLOT_NAMES[] = {"edge1", "edge2", "edge3", "edge4", "edge5", "edge6", "edge7", "edge8",
                                           "edge9", "edge10", "edge11", "edge12", "edge13", "edge14", "edge15", "edge16"};
constexpr const char* SIDE_NAMES[] = {"left", "right", "top", "bottom",
                                      "topleft", "topright", "bottomleft", "bottomright"};

inline bool isCornerSide(EdgeSide s) {
    return s >= EdgeSide::TOP_LEFT;
}

// A corner zone would otherwise sit inside both of its composing edge zones, so
// sliding toward the corner clips the edge first and opens the wrong panel.
// Each configured corner instead carves its own width plus this margin out of
// both neighbouring edges, leaving a gap the edges never trigger in. The gap is
// exactly this value: the edge is inset by trigger_width + margin, and the
// corner occupies the trigger_width part of it.
// Overridable with the single global plugin:hyprhotedge:corner_margin.
constexpr int DEFAULT_CORNER_MARGIN = 10;

struct EdgeConfig {
    EdgeSide side = EdgeSide::RIGHT;
    int triggerWidth = 15;          // pixels from edge to trigger
    int dwellTime = 150;            // ms to wait before triggering
    std::string specialWorkspace;   // name of special workspace to show
    std::string targetMonitor;      // monitor name ("*" = all, "DP-1" = specific)
    bool enabled = false;
    // Auto-hide once the cursor leaves the panel rectangle. Turn off for a
    // panel whose real size does not match getPanelArea()'s 1/3 assumption --
    // a fullscreen workspace, most obviously, which would otherwise close as
    // soon as the cursor left the corner quadrant it never occupied.
    bool hideOnLeave = true;
};

// Handles returned by addConfigValueV2. Hyprland 0.56 deprecated the
// name-lookup API (getConfigValue), which returns nullptr under the Lua config
// manager — dereferencing that is what SIGSEGV'd the compositor. Holding the
// value objects instead removes the lookup, so there is nothing to be null.
struct EdgeConfigValues {
    SP<Config::Values::Int>    enabled;
    SP<Config::Values::String> side;
    SP<Config::Values::Int>    triggerWidth;
    SP<Config::Values::Int>    dwellTime;
    SP<Config::Values::String> specialWorkspace;
    SP<Config::Values::String> targetMonitor;
    SP<Config::Values::Int>    hideOnLeave;
};

struct EdgeState {
    bool bCursorInZone = false;
    bool bDwelling = false;
    std::chrono::steady_clock::time_point dwellStart;
    bool bHideDelaying = false;
    std::chrono::steady_clock::time_point hideDelayStart;

    // Track which monitor the overlay was opened on
    std::string activeMonitorName;

    // Grace period after keyboard toggle (prevents immediate auto-hide)
    bool bJustShown = false;
    std::chrono::steady_clock::time_point showTime;
};

class CHotEdge {
public:
    CHotEdge();
    ~CHotEdge();

    void init();
    void registerConfigValues();   // must run inside pluginInit, before init()
    void registerCallbacks();
    void onTick();
    void onActiveWindowChange(PHLWINDOW pWindow);
    void onMonitorRemoved(PHLMONITOR pMonitor);

    // Dispatchers - args can be empty (all edges), edge slot ("edge1"), or side name ("right")
    static SDispatchResult dispatchToggle(std::string args);
    static SDispatchResult dispatchShow(std::string args);
    static SDispatchResult dispatchHide(std::string args);

    // Runtime activation - gates cursor-driven automation only.
    // Explicit dispatchers (toggle/show/hide) and existing visible panels are untouched.
    static SDispatchResult dispatchEnable(std::string args);
    static SDispatchResult dispatchDisable(std::string args);
    static SDispatchResult dispatchToggleActive(std::string args);

    // Configuration
    void reloadConfig();

    // State
    bool isOverlayVisible(int slotIndex) const;
    bool isOverlayVisibleOnMonitor(int slotIndex, PHLMONITOR monitor) const;
    bool isAnyOverlayVisible() const;

private:
    void processEdge(int slotIndex);
    bool isCursorInEdgeZone(int slotIndex, PHLMONITOR monitor);
    bool isCursorInPanelArea(int slotIndex, PHLMONITOR monitor);
    bool isCursorAtScreenEdge(int slotIndex, PHLMONITOR monitor);
    bool isEdgeEnabledForMonitor(int slotIndex, PHLMONITOR monitor);
    void showOverlay(int slotIndex);
    void hideOverlay(int slotIndex);
    PHLMONITOR getCurrentMonitor();
    PHLMONITOR getMonitorByName(const std::string& name);
    CBox getEdgeZone(PHLMONITOR monitor, EdgeSide side, int triggerWidth);
    CBox getPanelArea(PHLMONITOR monitor, EdgeSide side);
    // How much an edge must give up at each end to the corners configured on
    // this monitor. .first is the low end (top for vertical edges, left for
    // horizontal), .second the high end. Always {0,0} for a corner side.
    std::pair<double, double> cornerInsets(PHLMONITOR monitor, EdgeSide side);

    static int parseEdgeArg(const std::string& arg);
    static EdgeSide parseSide(const std::string& str);

    bool hasPendingState() const;
    void armPendingTimer();

    std::array<EdgeConfig, MAX_EDGE_SLOTS> m_edges;
    std::array<EdgeState, MAX_EDGE_SLOTS> m_states;
    std::array<EdgeConfigValues, MAX_EDGE_SLOTS> m_configValues;

    // Single global, not per-slot: it is a feel setting for how far edges keep
    // clear of corners, and a different value per edge would just make the
    // layout inconsistent.
    SP<Config::Values::Int> m_cornerMarginValue;
    int                     m_cornerMargin = DEFAULT_CORNER_MARGIN;

    bool m_active = true;

    // mouse.move is the only tick source, so a dwell or hide delay armed by the
    // final motion event would never elapse while the cursor sits perfectly
    // still — the panel simply never appeared until you jiggled the mouse. This
    // re-ticks while any slot has something pending, and stays disarmed otherwise.
    SP<CEventLoopTimer> m_pendingTimer;

    // Event-bus listener handles. Stored as members so they unregister when
    // CHotEdge is destroyed (in PLUGIN_EXIT) — replaces the previous static-local
    // pattern that prevented clean plugin unload/reload.
    CHyprSignalListener m_mouseMoveCb;
    CHyprSignalListener m_activeWindowCb;
    CHyprSignalListener m_configReloadedCb;
    CHyprSignalListener m_monitorRemovedCb;
};

inline std::unique_ptr<CHotEdge> g_pHotEdge;
