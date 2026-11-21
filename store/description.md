# Max Zoom

Take the camera's three stock limits off at once:

- **Zoom** — pull the camera far back (up to 30× the stock distance), near-instantly. The engine's
  hard ~50-yard ceiling (reverse-engineered with Ghidra) is lifted so the multiplier really reaches.
- **View distance** — push the render far plane from the stock ~791 yards out to the horizon
  (slider to 10000), unlocking the engine's own high cap and then lifting it further.
- **Fog** — a one-click toggle that switches the distance fog off by hooking the per-frame fog
  producer, so the landscape stays crisp all the way out.

**Safe & reversible** — the memory writes are guarded to the known stock values on build 12340, are
tiny, and last for the current process. Change zoom, view distance, or fog in the WarcraftXL
overlay (**F9**). Max Zoom saves each change beside the DLL and restores it on the next launch.

Built by [Michael Malura](https://malura.de).
