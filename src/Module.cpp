// wxl-maxzoom: lift the World of Warcraft 3.3.5a camera zoom-out limit far past the stock ceiling.
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
// The client's own console variable `cameraDistanceMaxFactor` is the multiplier on the maximum
// camera pull-back distance. The default UI slider only reaches a small fraction of what the CVar
// itself accepts. This module asks the client -- through its OWN Lua/CVar path -- to raise that
// factor. Nothing is patched in memory and no address is written: we call the engine's FrameScript
// executor (a WarcraftXL-verified landmark) to run `SetCVar("cameraDistanceMaxFactor", ...)`, which
// the client then validates and clamps itself. That makes this one of the safest possible mods --
// the worst case is the client refusing a value, never a crash.
//
// The value is re-asserted on every world enter (login / loading screen), and is adjustable live
// from the WarcraftXL overlay via a slider.

#include "wxl/EventScript.hpp"   // wxl::ext::EventScript + the shared wxl::events::Event enum
#include "game/Script.hpp"        // pulls in the verified wxl::offsets::engine::lua landmarks

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
            // Re-assert the zoom ceiling every time a map finishes loading: the client reloads CVars
            // across loading screens, so a single apply at startup would not stick.
            on<&MaxZoom::OnWorldEnter>(wxl::events::Event::OnWorldEnter);
        }

        float* Factor() { return &factor_; }

        /// Push the current factor into the client's CVar. Safe to call at any time; a no-op until
        /// scripting exists.
        void Apply()
        {
            char lua[96];
            std::snprintf(lua, sizeof(lua),
                          "SetCVar(\"cameraDistanceMaxFactor\", \"%.2f\")", factor_);
            RunLua(lua);
        }

    private:
        void OnWorldEnter(const wxl::events::WorldEnterArgs&) { Apply(); }

        float factor_ = kDefaultFactor;
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
        g_api->UiText("Camera zoom-out multiplier.");
        g_api->UiText("Stock client = 1.0. Higher pulls the camera further back.");
        g_api->UiSeparator();

        float* f = g_maxzoom->Factor();
        if (g_api->UiSliderFloat("cameraDistanceMaxFactor", f, kSliderMin, kSliderMax))
            g_maxzoom->Apply(); // live: apply the instant the slider moves

        if (g_api->UiButton("Apply now"))
            g_maxzoom->Apply();
    }
}

/// First entry point: describe ourselves. No side effects.
const WXL_PluginInfo* __cdecl WXL_Query(void)
{
    static const WXL_PluginInfo info = {
        sizeof(WXL_PluginInfo),
        WXL_API_VERSION,
        "wxl-maxzoom",
        2,
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

    api->UiAddPanel("Max Zoom", &wxl_maxzoom::PanelBody, nullptr);
    api->Log(WXL_LOG_INFO, "wxl-maxzoom", "max-zoom ready (default factor %.1f)",
             wxl_maxzoom::kDefaultFactor);
    return 1;
}
