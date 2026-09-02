# wxl-maxzoom

**Unlock the World of Warcraft 3.3.5a (build 12340) camera — zoom-out, view distance, and fog — far past the stock limits, adjustable live.**

A [WarcraftXL](https://github.com/WarcraftXL) module. Stock, the client only lets the camera pull back
a short way, draws terrain out to ~791 yards, and drowns the distance in fog. `wxl-maxzoom` takes all
three limits off: pull the camera way out, push the view distance to the horizon, and switch the
distance fog off — for a whole-fight, whole-landscape, or map-scale view.

> Built by [Michael Malura](https://malura.de).

![Fog off, view distance far, zoom 30 — a whole valley to the horizon](store/screenshot.png)

## How it works

Each limit was found by reverse-engineering the client with [Ghidra](https://ghidra-sre.org/) (build
12340), then lifted the least invasive way that works.

**1. Zoom — the multiplier.** The `cameraDistanceMaxFactor` CVar scales the base camera distance
(`cameraDistanceMax`, default 15). The mod raises it through the client's *own* Lua/CVar path —
WarcraftXL exposes the engine's verified FrameScript executor — then calls `CameraZoomOut` so the
camera snaps to the new distance on the spot (with the move/smooth-speed CVars cranked so it's near
instant).

**2. Zoom — the hard clamp.** On its own, lever 1 stops at ~50 yards: the engine computes the
effective distance as `min(cameraDistanceMaxFactor * cameraDistanceMax, 50.0)`, and that `50.0` is a
hard ceiling — a single float in `.rdata` at `0x00A1E2FC`. (It's *why* factor 6 and factor 30 looked
identical — both hit the same wall.) The mod lifts it in process memory so the multiplier really
controls the distance: factor 30 → ~450 yards.

**3. View distance (farclip).** The `farclip` CVar sets the render far plane. Stock it clamps to
~791 yards; setting `farClipOverride` = 1 raises the engine's own cap to ~1583 (both caps are `.rdata`
floats found with Ghidra). To go past 1583 the mod lifts the high-cap float at `0x00A3E710`, then
drives `farclip` from a slider up to 10000.

**4. Fog.** WoW's world fog is not a CVar; it is produced every frame by the sky/light system, and the
engine's own fog override is capped by its distance ceiling (so it can't clear fog). The mod instead
hooks the exact per-frame fog producer (`0x007F16F0`) and, while the toggle is on, overwrites the fog
near/far it just wrote (`0x00D38B90` / `0x00D38B94`) with values far past the horizon — nothing in the
world reaches them, so distance fog never blends in.

The CVar-driven levers are re-asserted on every world enter (the client reloads CVars across loading
screens); the fog toggle rides a persistent per-frame hook.

### Is it safe?

- The memory writes are **guarded** (they fire only against the known stock values on client build
  12340), tiny (single floats), and **not persisted to disk** — restarting the client fully reverts
  everything.
- WarcraftXL refuses to load a module built against a different client build than 12340, so the
  hardcoded addresses can never be applied to an image where they mean something else.

## Install

**Via the WarcraftXL hub (recommended):** open the store in [wxl-hub](https://github.com/WarcraftXL/wxl-hub),
find **Max Zoom**, and hit install.

**Manually:** drop `wxl-maxzoom.dll` into your patched client at
`Extensions/wxl-maxzoom/wxl-maxzoom.dll`. You need a client already set up with the
[WarcraftXL](https://github.com/WarcraftXL/wxl-core) framework (`WarcraftXL.dll` + a patched
`Wow.exe`).

## Use

Press **F9** to open the WarcraftXL overlay, then use the **Max Zoom** panel:

- **`cameraDistanceMaxFactor`** slider (1–30): drives the camera in/out live, near-instantly. Default 10.
- **`farclip (yards)`** slider (500–10000): the render view distance. Default 2000 (stock max is ~791).
- **`Disable fog`** checkbox: switches the distance fog off.

All of it is applied automatically on world enter too, so it sticks across logins and zone changes.

## Build from source

Requires CMake ≥ 3.20 and a Win32 C++ toolchain (Visual Studio 2022). The module builds against the
WarcraftXL core, which acts as the build shell:

```sh
git clone --branch v1.1 --recurse-submodules https://github.com/WarcraftXL/wxl-core.git
git clone https://github.com/maluramichael/wxl-maxzoom.git wxl-core/extensions/wxl-maxzoom
cmake -S wxl-core -B wxl-core/build -A Win32
cmake --build wxl-core/build --config Release --target wxl-maxzoom
# -> wxl-core/build/Release/wxl-maxzoom.dll
```

The included GitHub Actions workflow does exactly this and publishes the DLL as a release on every
push.

## Credits

- Built on [WarcraftXL](https://github.com/WarcraftXL) by iThorgrim & contributors — the modding
  framework that makes client-side 3.3.5a mods like this possible.
- Module by [Michael Malura](https://malura.de).

## License

[GNU General Public License v3.0 or later](LICENSE) — the same license as WarcraftXL.
