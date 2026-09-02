# wxl-maxzoom

**Lift the World of Warcraft 3.3.5a (build 12340) camera zoom-out limit far past the stock ceiling — adjustable live.**

A [WarcraftXL](https://github.com/WarcraftXL) module. The default client only lets the camera pull
back a short distance; the UI slider stops even shorter. `wxl-maxzoom` raises the client's own
`cameraDistanceMaxFactor` console variable so you can zoom way out — great for getting the whole
fight, a landscape, or a raid on screen at once.

> Built by [Michael Malura](https://malura.de).

## How it works (and why it's safe)

This module **patches nothing in memory and writes no game address.** It asks the client to change
its *own* setting, through the client's *own* Lua/CVar path:

```lua
SetCVar("cameraDistanceMaxFactor", "3.9")
```

WarcraftXL exposes the engine's verified FrameScript executor, so the module runs that one line in
the client's script context on every world enter, and again whenever you move the overlay slider.
The client then validates and clamps the value itself — the worst case is the client refusing a
number, never a crash. That makes this one of the least invasive mods you can run.

The value is re-asserted on every loading screen (the client reloads CVars across them), so it
sticks across logins and zone changes.

## Install

**Via the WarcraftXL hub (recommended):** open the store in [wxl-hub](https://github.com/WarcraftXL/wxl-hub),
find **Max Zoom**, and hit install.

**Manually:** drop `wxl-maxzoom.dll` into your patched client at
`Extensions/wxl-maxzoom/wxl-maxzoom.dll`. You need a client already set up with the
[WarcraftXL](https://github.com/WarcraftXL/wxl-core) framework (`WarcraftXL.dll` + a patched
`Wow.exe`).

## Use

- Just play — the camera max-zoom is raised automatically as soon as you enter the world. Scroll out.
- Press **F9** to open the WarcraftXL overlay, then use the **Max Zoom** panel's slider to tune the
  multiplier live (1.0 = stock, higher pulls further back).

The default multiplier is `3.9`. The client clamps it to whatever its own registered maximum is, so
asking for more than it allows is harmless.

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
