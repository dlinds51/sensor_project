# True slider: custom widget source (developer preview)

OpenC3's built-in Telemetry Viewer screens do not provide a native slider widget.
`servo_control.txt` is an install-ready fallback: it has -90 / ZERO / +90
buttons, an angle entry field, and live telemetry.

`src/ServoSliderWidget.vue` is a **source-only prototype**, NOT packaged into
the install-ready gem. It requires COSMOS-compatible Vue widget build tooling,
and the `OpenC3Api` import/method must be checked against the installed OpenC3
version before enabling. It is **not yet validated or tested**.

Follow https://docs.openc3.com/docs/guides/custom-widgets to generate the widget,
build `tools/widgets/ServoSliderWidget/ServoSliderWidget.umd.min.js`, and
verify command API compatibility. Only after the bundle is built and tested:
1. Add `WIDGET SERVOSLIDER` to `plugin.txt`.
2. Add `SERVOSLIDER` to a screen.
3. Include `tools/widgets/ServoSliderWidget/*` in the gemspec `s.files`.
4. Rebuild and upgrade the plugin.

DO NOT mark this custom widget install-ready without compiling/testing.
