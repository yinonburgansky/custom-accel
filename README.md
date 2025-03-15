<p align="center">
  <img alt="icon" width="128" height="128" src="./data/icons/hicolor/scalable/apps/io.github.yinonburgansky.CustomAccel.png">
</p>

# Custom Accel

Create custom acceleration curves for your mouse and touchpad.
Accelerate pointer motion and scrolling.

Visualize your device speed on the plot and fine-tune the acceleration curve to your preference.

![screenshot](./screenshot.png)

## Usage Instructions

1. Select the device for which you want to create a custom acceleration curve.
2. Move the mouse or touchpad within your normal speed range. The plot's x-axis will automatically update to match your highest speed.
3. Adjust the top speed multiplier to modify the pointer's top speed relative to your mouse's top speed. Drag the bezier curve handles to modify the curve.
4. Click the "Apply Settings" button and move your mouse around. A dialog will appear asking if you want to keep or restore the settings. It will automatically restore the settings after 10 seconds.
5. Repeat until you are satisfied with the results, then click the "Keep settings" button.
6. Due to [issue](https://github.com/yinonburgansky/custom-accel/issues/2): To ensure your acceleration settings persist after a restart, manually copy the settings using `xinput list-props "device name"` and write the settings to `/usr/share/X11/xorg.conf.d/40-libinput.conf`.

## Recommendations

- Avoid excessive speeds that don't represent your typical usage. Fine-tuning the curve is most effective at low speeds; you won't notice much difference at high speeds. The curve will be linearly extrapolated for speeds outside your normal range. Very high speeds outside your normal range mean less precision for the lower speeds where it really matters.
- Use the "Top speed multiplier" to find your desired top speed first, as it affects both bezier handles. Use the bezier handles to define how fast the pointer accelerates to your top speed.
- Pay attention to your pointer movement at low and high speeds:
    - High speeds: Use the "Top speed multiplier" slider to adjust the pointer speed.
    - Low speeds: Use the left bezier handle to speed up or slow down the pointer.
    - It is recommended to keep the right bezier handle on the imaginary diagonal line connecting the bottom-left and top-right corners of the plot.

## Build

```bash
flatpak-builder --force-clean build io.github.yinonburgansky.CustomAccel.json
./build/files/bin/custom-accel
```

## FAQ

### Why is Wayland not supported?

On `X11`, all desktop environments use the common `libinput` configuration store provided by `xf86-input-libinput`. However, on `Wayland`, `libinput` does not provide a common configuration store, and compositors have not established a unified base configuration store. Each compositor exposes the same libinput settings using different APIs, and some do not provide a programmable API at all, making it incompatible with the app. Additionally, libinput and the compositors do not intend for third-party applications to modify acceleration settings. Without an official common interface, it is impractical for the app to support each compositor individually.  
See [issue](https://github.com/yinonburgansky/custom-accel/issues/1) for more details.

### Why does my mouse speed jump up and down and is not smooth?

The speed values are derived from your mouse's raw data: `speed = hypot(dx, dy) / dt`. This minimal processing is performed by libinput to calculate the speed. Attempting to artificially smooth the speed would result in sluggish, less responsive, and delayed movement.

## Contributions Welcome!

Contributions for bug fixes and existing approved issues are always appreciated! For new feature ideas, please start a discussion in an issue first. This helps ensure that efforts align with the project's goals and avoids unnecessary work. Feature requests and issues will be prioritized based on the number of upvotes (👍) they receive. Thanks for being part of the project!
