#include "globals.hpp"
#include "HotEdge.hpp"

#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/debug/log/Logger.hpp>

#include <lua.hpp>

namespace {
    // Lua-mode Hyprland has no way to reach a classic named dispatcher
    // (hotedge:toggle) from hl.bind / hl.dispatch, so the same handlers are
    // also exposed as hl.plugin.hyprhotedge.<fn>(arg). Each returns `true`,
    // or `false, error` on failure, mirroring SDispatchResult.
    int luaCall(lua_State* L, SDispatchResult (*fn)(std::string)) {
        const auto res = fn(luaL_optstring(L, 1, ""));
        lua_pushboolean(L, res.success);
        if (res.success)
            return 1;
        lua_pushstring(L, res.error.c_str());
        return 2;
    }

    int luaToggle(lua_State* L) {
        return luaCall(L, CHotEdge::dispatchToggle);
    }
    int luaShow(lua_State* L) {
        return luaCall(L, CHotEdge::dispatchShow);
    }
    int luaHide(lua_State* L) {
        return luaCall(L, CHotEdge::dispatchHide);
    }
    int luaEnable(lua_State* L) {
        return luaCall(L, CHotEdge::dispatchEnable);
    }
    int luaDisable(lua_State* L) {
        return luaCall(L, CHotEdge::dispatchDisable);
    }
    int luaToggleActive(lua_State* L) {
        return luaCall(L, CHotEdge::dispatchToggleActive);
    }
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    Log::logger->log(Log::INFO, "[HotEdge] Plugin loading...");

    // Register dispatchers
    // Args can be slot name (edge1, edge2, etc.) or empty for default behavior
    HyprlandAPI::addDispatcherV2(PHANDLE, "hotedge:toggle", CHotEdge::dispatchToggle);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hotedge:show", CHotEdge::dispatchShow);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hotedge:hide", CHotEdge::dispatchHide);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hotedge:enable", CHotEdge::dispatchEnable);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hotedge:disable", CHotEdge::dispatchDisable);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hotedge:toggle-active", CHotEdge::dispatchToggleActive);

    // Lua API: hl.plugin.hyprhotedge.toggle("right"), .show(...), .hide(...),
    // .enable(), .disable(), .toggle_active(). Namespace matches the config
    // path (plugin.hyprhotedge.*).
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprhotedge", "toggle", luaToggle);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprhotedge", "show", luaShow);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprhotedge", "hide", luaHide);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprhotedge", "enable", luaEnable);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprhotedge", "disable", luaDisable);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprhotedge", "toggle_active", luaToggleActive);

    // Create the HotEdge instance BEFORE registering callbacks
    g_pHotEdge = std::make_unique<CHotEdge>();

    // Config values must be registered inside pluginInit, and before init()
    // reads them. They are owned by the plugin now (addConfigValueV2), replacing
    // the deprecated addConfigValue/getConfigValue name-lookup pair.
    g_pHotEdge->registerConfigValues();
    g_pHotEdge->init();

    // Register event listeners as members on g_pHotEdge so they unregister
    // automatically in PLUGIN_EXIT when the instance is destroyed. This replaces
    // the previous static-local pattern, which prevented clean plugin reload.
    //
    // Note: previously listened on render.pre as well, but calling dispatchers
    // (togglespecialworkspace) from inside render preparation races with
    // screencopy frame teardown and crashes Hyprland in surface scale walks.
    // mouse.move alone is enough for edge detection.
    g_pHotEdge->registerCallbacks();

    Log::logger->log(Log::INFO, "[HotEdge] Registered callbacks");
    Log::logger->log(Log::INFO, "[HotEdge] Plugin loaded successfully!");

    return {"hypr-hot-edge", "Hot edge trigger for special workspace overlays (supports multiple edges per monitor)", "claychinasky", "0.5.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    Log::logger->log(Log::INFO, "[HotEdge] Plugin unloading...");
    g_pHotEdge.reset();
}
