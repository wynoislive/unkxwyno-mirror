# unkxwyno mirror desktop interface

Authority: the current user brief explicitly requires removing all DouWan runtime dependencies. This document records a UI redesign only; the receiver replacement remains unfinished. The desktop app must not claim independent mirroring until a replacement receiver implements and passes real device tests.

The incumbent screenshot shows a dark orange interface, account/purchase navigation, lengthy promotional copy and a blocking network-error dialog. The new primary task is connecting a phone and seeing its mirrored display. A network failure must be recoverable inside the workspace rather than replacing it with a vendor dialog.

## Direction

Use a quiet dark Windows workbench with near-black green-grey ground, layered charcoal panels, soft borders, bright mint-teal for the primary action and clear state colors. Segoe UI keeps controls native and available offline. Do not include account, premium or purchase controls. The mirror canvas leads; no decorative metrics or fabricated connected phones.

- Accent: `#68D8C9`; accent foreground `#062521`; selected background `#1C3936`.
- Application ground: `#0D1417`; navigation ground: `#10181C`; panels: `#172126`; raised surface: `#202B30`; text: `#EDF3F1`; secondary text: `#A2B2B4`; borders: `#2A393D`.
- Use a 4/8 spacing rhythm. Navigation is 208 pixels at the baseline desktop size. Buttons/inputs use 8-pixel radii and workspace surfaces 12.
- Default window: 1280 x 832 logical pixels, minimum 960 x 640. At narrow desktop widths, collapse the supplementary connection panel and keep its guide reachable by button. Preserve the mirror canvas and primary action.
- Use one family of authored vector outline icons, no emoji and no web fonts/CDNs.
- Native labeled buttons, visible keyboard focus, Escape to dismiss overlays/fullscreen, Ctrl+N to connect and Ctrl+S to save a snapshot. Network/file work stays asynchronous.

## Target and references

`target.png` is a historical light-theme capture and is not the target for the current redesign. No image-generation tool is available in this session, so the current brief and these tokens define the target. Build semantic native WPF text, controls and vector geometry; do not claim a generated reference or pixel match.

Intentional correction to the target: Android connection instructions must not imply that generic Miracast/Smart View is necessarily supported. Use the engine-compatible connection guidance, and display the real configured receiver identity rather than assuming a discovery name. No phone or receiver is reported connected until an actual API response supports it.

The existing native workspace remains the structural reference for the phone display, navigation and connection flow. The prior light screenshot is retained only as a baseline. No paid Uizze reference service was connected.

All three requested `npx skills use` commands completed; their complete output is retained beside this file. The UI/UX Pro Max search script could not run because Python and `py -3` are unavailable. Its general desktop guidance is a fallback, not a verified database match. `ui-styling` guidance is adapted to native WPF rather than adding a web stack.

UI/UX Pro Max's two design-system searches returned marketing-oriented layouts rather than a verified native device-workspace system. Those results were not retained as the authoritative design system. This native direction is a documented fallback based on the brief and native conventions. Its WPF stack search did return applicable keyboard-navigation, command-binding and asynchronous-work guidance. No optional paid Uizze connection was available or required.

## Required states and verification

Capture the compiled WPF interface at 1536 x 1024 and the compact 1112 x 652 viewport, plus connection overlay, captures empty state, settings, selector popup, and service-unavailable recovery. Inspect full images and correct clipping/hierarchy. Do not present fixture devices as live data. `visual-evidence-dark-review.json` records hashes for this dark-theme, review-only pass; the older `visual-evidence.json` describes a previous interface. Distinguish a source-render preview from a live receiver run.

Package-only/static checks do not establish phone compatibility, live API behavior, capture performance or engine startup. A source-render preview is distinct from executing the final compiled application. Existing Windows Application Control restrictions must not be bypassed to obtain a claimed runtime pass.
