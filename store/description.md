# Max Zoom

Pull the camera **way** back. The stock 3.3.5a client caps how far you can zoom out, and the UI
slider stops even shorter. **Max Zoom** raises the client's own `cameraDistanceMaxFactor` so you can
frame a whole boss fight, a sweeping landscape, or your entire party at once.

- **Two-lever design** — raises the client's own `cameraDistanceMaxFactor` CVar, *and* lifts the
  engine's hard 50-yard camera ceiling (a single float, reverse-engineered with Ghidra) so the
  multiplier actually reaches its full range.
- **Guarded & reversible** — the one 4-byte memory write only fires against the known stock value on
  build 12340, and isn't persisted; a restart reverts it.
- **Live control** — open the WarcraftXL overlay (**F9**) and drag the slider to taste.
- **Sticks** — re-applied on every world enter, so it survives loading screens and logins.

Default multiplier is 10×, and the slider runs all the way to a map-scale **30×**. Built by
[Michael Malura](https://malura.de).
