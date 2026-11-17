# Independent desktop design target

## Authority

This target belongs to the independent runtime. It replaces the legacy wrapper
as the visual direction but does not prove that capture or streaming works.
Product capability remains controlled by the independent capability/session
contracts and `docs/FEATURE_INVENTORY.md`.

## Target

- File: `independent-target.png`
- SHA-256: `409cda61873784ebd3d12b743d35c0995afef1136a8b4a787dbfe9cee351753a`
- Actual dimensions: 1586 × 992 pixels
- State: Windows desktop, no device connected, local discovery idle
- Source: generated with the authorized image tool, then inspected and corrected
  so setup refers only to the UNKXWYNO companion.

The original generated image incorrectly suggested using a generic casting
feature. That copy was rejected because the product does not implement or claim
AirPlay, Cast, or Miracast. The retained target contains the corrected companion
instruction.

## Direction contract

- Thesis: a quiet native workbench where the mirror canvas and next connection
  action lead; metrics and quality claims appear only after measurement.
- Palette: cool near-black and charcoal surfaces, mint-teal primary action,
  one cool-grey family, separate warning/error/success roles.
- Shape: 8-pixel control radii, 12-pixel workspace radii, 4/8 spacing rhythm.
- Type: Segoe UI Variable/Segoe UI with no downloaded font or web dependency.
- States: disconnected, discovering, empty, error, setup-required, connected,
  recovering, and stopped. Only states backed by current runtime data may be
  presented as live.
- Accessibility: native keyboard order, visible focus, labelled controls,
  scalable text, high contrast, and no color-only status meaning.

## Verification requirement

The compiled WinUI result must be captured at the target size and a compact
desktop size after packages load. A build alone is not visual verification.
Until a real render is compared with this image, target fidelity remains
`UNVERIFIED`.

## Runtime inspection - 2026-11-14

The published WinUI app was captured using DPI-aware `PrintWindow` and inspected:

- `winui-runtime.png`: 1280 by 832, SHA-256
  `0452b43512fb552074ab59db92ad0c8d5814d33347d688875aa29ca58216ced9`.
- `winui-runtime-compact.png`: 960 by 640, SHA-256
  `fc319e7662dc5e6abb2e0c0ea44ee61e9865029bc1f8c6326614e32c405db022`.
- `winui-diagnostics-runtime.png`: 1280 by 832, SHA-256
  `6113f8419b3dadd65abb7d45b9baca867127a0cf239fa53f985c6d0b104e4365`.

These captures precede the later core-DLL integration, so their setup/core text
is historical. UI Automation subsequently confirmed the published integrated
app reports `Independent core 0.1.0`.

Runtime inspection caught and corrected clipped compact actions, overlapping
diagnostics, and inaccessible connection-rail content. The compact connection
guide was invoked through UI Automation and opened responsively. Visual
direction is implemented and inspected at these sizes; exact same-size target
fidelity and all accessibility modes remain unverified.

