# Max Zoom

Pull the camera **way** back. The stock 3.3.5a client caps how far you can zoom out, and the UI
slider stops even shorter. **Max Zoom** raises the client's own `cameraDistanceMaxFactor` so you can
frame a whole boss fight, a sweeping landscape, or your entire party at once.

- **Safe by design** — no memory patching. It changes the client's own console variable through the
  client's own scripting path, and the client validates the value itself.
- **Live control** — open the WarcraftXL overlay (**F9**) and drag the slider to taste.
- **Sticks** — re-applied on every world enter, so it survives loading screens and logins.

Default multiplier is 10×, and the slider runs all the way to a map-scale **30×**. Built by
[Michael Malura](https://malura.de).
