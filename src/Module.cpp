// wxl-maxzoom: unlock the World of Warcraft 3.3.5a camera -- zoom-out, view distance, and fog.
// Copyright (C) 2026 Michael Malura <michael@malura.de> -- https://malura.de
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// HOW IT WORKS
// ------------
// Two levers, together, take the 3.3.5a camera far past its stock limit:
//
// 1. The multiplier. The client's console variable `cameraDistanceMaxFactor` scales the base camera
//    pull-back distance (`cameraDistanceMax`, default 15.0). We raise it through the client's OWN
//    Lua/CVar path -- WarcraftXL exposes the engine's verified FrameScript executor, so the module
//    runs `SetCVar("cameraDistanceMaxFactor", ...)` in the client's script context.
//
// 2. The hard clamp. On its own, lever 1 does nothing past ~50 yards: the engine computes the
//    effective distance as `min(cameraDistanceMaxFactor * cameraDistanceMax, 50.0)` and that 50.0 is
//    a hard ceiling (a single float constant in the client's .rdata at 0x00A1E2FC, reverse-engineered
//    with Ghidra against build 12340 -- it is why factor 6 and factor 30 looked identical, both
//    clamped to the same wall). This module lifts that ceiling in process memory (VirtualProtect ->
//    write -> restore), so the multiplier above actually controls the distance: factor 30 -> ~450
//    yards. The patch is guarded to only fire when the stock value (50.0) is present, touches one
//    4-byte float, and is not persisted to disk -- a restart of the client fully reverts it.
//
// 3. View distance. The `farclip` CVar sets the render far plane. Stock it clamps to ~791 yards;
//    setting `farClipOverride` = 1 raises that to the engine's own high cap of ~1583 (both caps are
//    .rdata floats found with Ghidra). To go past 1583 we lift the high-cap float at 0x00A3E710 in
//    memory (same guarded technique as the camera clamp), then drive `farclip` from a slider.
//
// 4. Fog. WoW's world fog is not a CVar; it is produced per frame by the sky/light system, and the
//    engine's high-level fog override is capped by its own distance ceiling (so it cannot fully clear
//    fog). We instead hook the exact per-frame fog producer (Sky landmark kFogUpdate, build 12340) and,
//    while the "Disable fog" toggle is on, overwrite the fog near/far it just wrote with values far past
//    the horizon -- nothing in the world reaches them, so distance fog never blends in.
//
// Everything is re-asserted on every world enter (the client reloads state across loading screens),
// and all of it is adjustable live from the WarcraftXL overlay (F9).

#include "wxl/EventScript.hpp"   // wxl::ext::EventScript + the shared wxl::events::Event enum
#include "game/Script.hpp"        // pulls in the verified wxl::offsets::engine::lua landmarks

#include <windows.h>
#include <cstdio>

namespace wxl_maxzoom
{
    // The stock client's factor is 1.0. This build honours values far above the classic ~2.6 UI cap,
    // so we open the throttle wide: a bold default and a slider that runs all the way to 30x for a
    // genuinely absurd, map-scale pull-back. The client validates and clamps the value itself, so
    // asking for more than it will grant is harmless -- it simply settles at its own ceiling.
    constexpr float kDefaultFactor = 10.0f;
    constexpr float kSliderMin     = 1.0f;
    constexpr float kSliderMax     = 30.0f;

    // The FrameScript entry points, taken by value from the SDK's verified landmark table (reaching
    // them via game/Script.hpp keeps this file clear of any direct offsets/ include).
    namespace lua = wxl::offsets::engine::lua;
    using ExecFn = void(__cdecl*)(const char* source, void* state);
    using CtxFn  = void*(__cdecl*)();

    // The hard camera-distance ceiling. A float in the client's .rdata that the engine clamps the
    // effective camera distance against (min(factor*base, THIS)). Reverse-engineered with Ghidra on
    // build 12340; guarded below so the patch only fires against exactly this client.
    constexpr uintptr_t kCamClampAddr = 0x00A1E2FC;
    constexpr float     kStockClamp   = 50.0f;      // the value the stock client ships
    constexpr float     kLiftedClamp  = 100000.0f;  // effectively "no ceiling"; the factor now decides

    // View distance (farclip). Defaults: stock max ~791, or ~1583 with farClipOverride=1. We lift the
    // high-cap float (Ghidra: 0x00A3E710, stock ~1583.33) so the slider can drive it much further.
    constexpr float     kDefaultViewDist   = 2000.0f;
    constexpr float     kViewDistMin       = 500.0f;
    constexpr float     kViewDistMax       = 10000.0f;
    constexpr uintptr_t kFarclipCapAddr    = 0x00A3E710;
    constexpr float     kLiftedFarclipCap  = 100000.0f;

    // Fog. WarcraftXL's higher-level SetOverrideFog is capped by the engine's own distance ceiling, so
    // it cannot fully clear fog. Instead we hook the exact per-frame fog producer (Sky landmark
    // kFogUpdate) and, while the toggle is on, overwrite the fog near/far it just wrote with values far
    // past the horizon -- nothing in the world reaches them, so distance fog vanishes. Addresses are
    // spelled out here (build 12340) so this file stays free of any direct offsets/ include.
    constexpr uintptr_t kFogUpdate = 0x007F16F0; // __cdecl void(void): produces the frame's fog near/far
    constexpr uintptr_t kFogNear   = 0x00D38B90; // f32 fog start distance (terrain/sky consume it)
    constexpr uintptr_t kFogFar    = 0x00D38B94; // f32 fog end distance
    using FogUpdateFn = void(__cdecl*)();

    /// Raise the engine's hard camera-distance ceiling in process memory. Idempotent and guarded:
    /// it writes only when the address currently holds the known stock value (or our lifted one), so
    /// a wrong image is left untouched. Returns true if the ceiling is now lifted.
    static bool LiftCameraClamp()
    {
        float* p = reinterpret_cast<float*>(kCamClampAddr);
        DWORD old = 0;
        if (!VirtualProtect(p, sizeof(float), PAGE_READWRITE, &old))
            return false;
        const bool known = (*p == kStockClamp || *p == kLiftedClamp);
        if (known)
            *p = kLiftedClamp;
        DWORD tmp = 0;
        VirtualProtect(p, sizeof(float), old, &tmp);
        return known;
    }

    /// Raise the engine's high farclip cap so `farclip` can be driven past its ~1583 ceiling. Guarded
    /// to the known stock value (range-checked, since 1583.33 has no exact float literal) or our lifted
    /// one. Returns true if the cap is now lifted.
    static bool LiftFarclipCap()
    {
        float* p = reinterpret_cast<float*>(kFarclipCapAddr);
        DWORD old = 0;
        if (!VirtualProtect(p, sizeof(float), PAGE_READWRITE, &old))
            return false;
        const bool known = (*p > 1583.0f && *p < 1584.0f) || *p == kLiftedFarclipCap;
        if (known)
            *p = kLiftedFarclipCap;
        DWORD tmp = 0;
        VirtualProtect(p, sizeof(float), old, &tmp);
        return known;
    }

    // Set by the fog toggle; read every frame by the fog-update detour below.
    bool        g_fogDisabled   = false;
    FogUpdateFn g_origFogUpdate = nullptr;

    /// Runs right after the engine recomputes the frame's fog. While disabled, shove the fog band past
    /// the horizon so nothing drawn in the world reaches it -- the fog is simply never blended in.
    static void __cdecl FogUpdateDetour()
    {
        if (g_origFogUpdate)
            g_origFogUpdate();
        if (g_fogDisabled)
        {
            *reinterpret_cast<float*>(kFogNear) = 90000.0f;
            *reinterpret_cast<float*>(kFogFar)  = 100000.0f;
        }
    }

    /// Runs a line of Lua in the client's active FrameScript context, or does nothing if scripting is
    /// not up yet (e.g. still on the glue/login screen).
    static void RunLua(const char* source)
    {
        auto getContext = reinterpret_cast<CtxFn>(lua::kFrameScriptGetContext);
        auto execute    = reinterpret_cast<ExecFn>(lua::kFrameScriptExecute);
        void* ctx = getContext();
        if (ctx)
            execute(source, ctx);
    }

    class MaxZoom final : public wxl::ext::EventScript
    {
    public:
        MaxZoom()
        {
            // Re-assert the CVar-driven levers every time a map finishes loading: the client reloads
            // CVars across loading screens, so a single apply would not stick. (Fog rides a per-frame
            // hook instead, so it needs no re-assert.)
            on<&MaxZoom::OnWorldEnter>(wxl::events::Event::OnWorldEnter);
        }

        float* Factor()       { return &factor_; }
        float* ViewDistance() { return &viewDistance_; }
        int*   FogDisabled()  { return &fogDisabled_; }

        /// Lift the hard camera ceiling, raise the max-distance CVar, and immediately pull the camera
        /// out to that new maximum. The engine also snaps the camera IN when the factor drops below the
        /// current distance, so the slider drives the camera both ways on the spot. The move/smooth
        /// speed CVars are cranked so that motion is near-instant instead of a slow glide.
        /// CameraZoomOut clamps to the max, so the margin just guarantees we hit it.
        void ApplyZoom()
        {
            LiftCameraClamp();
            char lua[256];
            std::snprintf(lua, sizeof(lua),
                          "SetCVar(\"cameraDistanceMoveSpeed\", \"50\");"
                          "SetCVar(\"cameraDistanceSmoothSpeed\", \"50\");"
                          "SetCVar(\"cameraDistanceMaxFactor\", \"%.2f\");"
                          "CameraZoomOut(%.0f)",
                          factor_, factor_ * 15.0f + 10.0f);
            RunLua(lua);
        }

        /// Lift the farclip cap, unlock the override, and set the render far plane from the slider.
        void ApplyViewDistance()
        {
            LiftFarclipCap();
            char lua[128];
            std::snprintf(lua, sizeof(lua),
                          "SetCVar(\"farClipOverride\", \"1\"); SetCVar(\"farclip\", \"%.0f\")",
                          viewDistance_);
            RunLua(lua);
        }

        /// Publish the toggle to the per-frame fog detour. Cheap; safe anywhere.
        void ApplyFog() { g_fogDisabled = (fogDisabled_ != 0); }

        void ApplyAll() { ApplyZoom(); ApplyViewDistance(); ApplyFog(); }

    private:
        void OnWorldEnter(const wxl::events::WorldEnterArgs&) { ApplyAll(); }

        float factor_       = kDefaultFactor;
        float viewDistance_ = kDefaultViewDist;
        int   fogDisabled_  = 0;      // int, not bool: UiCheckbox writes through an int*
    };

    // Constructed in WXL_Load, never at static-init: wxl::ext::EventScript binds through the service
    // table, which does not exist until the core hands it over, so a subclass built earlier would bind
    // to nothing. The extension is never unloaded, so it simply lives for the process lifetime.
    const WXL_Api* g_api     = nullptr;
    MaxZoom*       g_maxzoom = nullptr;

    // --- overlay panel -------------------------------------------------------------------------
    static void PanelBody(void* /*user*/)
    {
        if (!g_api || !g_maxzoom) return;

        g_api->UiText("Camera zoom-out multiplier. Stock = 1.0; higher pulls the camera back.");
        if (g_api->UiSliderFloat("cameraDistanceMaxFactor", g_maxzoom->Factor(), kSliderMin, kSliderMax))
            g_maxzoom->ApplyZoom(); // live: apply the instant the slider moves
        if (g_api->UiButton("Apply zoom now"))
            g_maxzoom->ApplyZoom();

        g_api->UiSeparator();
        g_api->UiText("View distance (farclip). Stock max ~791 yards.");
        if (g_api->UiSliderFloat("farclip (yards)", g_maxzoom->ViewDistance(), kViewDistMin, kViewDistMax))
            g_maxzoom->ApplyViewDistance();

        g_api->UiSeparator();
        g_api->UiText("Distance fog.");
        if (g_api->UiCheckbox("Disable fog", g_maxzoom->FogDisabled()))
            g_maxzoom->ApplyFog();
    }
}

/// First entry point: describe ourselves. No side effects.
const WXL_PluginInfo* __cdecl WXL_Query(void)
{
    static const WXL_PluginInfo info = {
        sizeof(WXL_PluginInfo),
        WXL_API_VERSION,
        "wxl-maxzoom",
        4,
        WXL_CLIENT_BUILD,
    };
    return &info;
}

/// Second entry point: wire up the event binding and the overlay panel.
int __cdecl WXL_Load(const WXL_Api* api)
{
    if (!api || api->apiVersion != WXL_API_VERSION)
        return 0;

    wxl_maxzoom::g_api = api;
    wxl::ext::EventScript::Bind(api);              // hand the event base its service table
    wxl_maxzoom::g_maxzoom = new wxl_maxzoom::MaxZoom(); // ctor binds OnWorldEnter, now that Bind ran

    // Lift both hard ceilings now, at load, so they are gone before the first world render.
    const bool camLifted = wxl_maxzoom::LiftCameraClamp();
    const bool fcLifted  = wxl_maxzoom::LiftFarclipCap();

    // Hook the per-frame fog producer so the "Disable fog" toggle can push fog past the horizon.
    const bool fogHooked = api->HookAttach(
        "wxl-maxzoom.fog", wxl_maxzoom::kFogUpdate,
        reinterpret_cast<void*>(&wxl_maxzoom::FogUpdateDetour),
        reinterpret_cast<void**>(&wxl_maxzoom::g_origFogUpdate),
        WXL_HOOK_DEFAULT_PRIORITY) != 0;

    api->UiAddPanel("Max Zoom", &wxl_maxzoom::PanelBody, nullptr);
    api->Log(WXL_LOG_INFO, "wxl-maxzoom",
             "ready (factor %.1f; camera ceiling %s; farclip cap %s; view %.0f; fog hook %s)",
             wxl_maxzoom::kDefaultFactor,
             camLifted ? "lifted" : "NOT lifted",
             fcLifted  ? "lifted" : "NOT lifted",
             wxl_maxzoom::kDefaultViewDist,
             fogHooked ? "attached" : "FAILED");
    return 1;
}
