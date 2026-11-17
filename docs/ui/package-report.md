# Separate native desktop package

Status: **DONE_WITH_CONCERNS** for the new interface and portable packaging. Native receiver integration remains unverified at runtime; complete feature parity and freedom from every premium-dependent path are not established.

Open [release/unkxwyno-mirror.exe](../../release/unkxwyno-mirror.exe), or the executable inside [the portable folder](../../release/unkxwyno-mirror). Transfer [unkxwyno-mirror.zip](../../release/unkxwyno-mirror.zip) as the complete package and extract it before opening the app. The small release-root executable is the same new desktop program and locates the engine inside the adjacent package folder. It is not standalone. The previous 71.9 MB launcher is preserved under `release/legacy-launcher`; the familiar release entry now opens the new interface.

The app uses authored native WPF navigation, light surfaces, teal actions, a new vector identity, device workspace, connection overlay, captures, settings and inline recovery. It contains no account, buy or premium controls. All three requested remote UI skills ran successfully; their complete outputs are retained in this directory. [Design decisions](DESIGN.md) describe their native Windows adaptation. [Visual evidence](visual-evidence.json) records actual source and screenshot hashes.

## Implemented behavior and limits

The desktop uses the existing engine's documented local API for device selection, periodically refreshed image previews, fullscreen previews, PNG snapshots, and home/back/mouse/keyboard commands. Controls depend on the connected phone's capabilities. Preview uses snapshot polling, with a default refresh target around 6 FPS, rather than the original native renderer. Recording, virtual-camera, multi-device broadcast and stream-quality controls are **not yet exposed**. Audio, 4K/full-rate rendering and phone compatibility were not verified. See [Quick start](QUICKSTART.md) for package layout, connection steps and these limits.

The internal executable is renamed `unkxwyno-receiver.exe`. Six additional exact-byte adaptations change the shared receiver label and singleton key, suppress startup window activation and the initial failure dialog, and route only the control API listener through a new loopback stub. The four previously documented local restriction changes remain in the input runtime. No account, payment or server response is fabricated. The original 198 dependencies and license material are preserved.

The receiver starts only on the user's Start receiver action, or a deliberately saved automatic-start preference. Its original administrator requirement is retained. The host validates its main digest and manages only the process it owns. It prepares the native `DouWan` profile used by the retained Qt application, preserving unrelated settings and creating a sibling backup before changes. Later vendor windows may still appear if suppression fails; this has not been observed at runtime.

## Verification

- Final C#5/.NET Framework desktop compilation passed. Both packaged entry points executed `--verify-package` successfully and checked all 199 engine file digests without starting the engine.
- The compiled application executed native UI review successfully: eight captured states and eleven interaction checks, including settings invalid-input correction and revisit. Compact captures are 1112x652. The wide runtime window is 1536x882 because of the desktop work area; a separate XAML source preview covers the 1536x1024 composition target. All named states were visually inspected. This is UI review, not a phone test.
- Fifteen local HTTP API/settings fixture checks passed against the final settings source. Ten isolated host/profile/digest checks passed without StartAsync or a native process. Results are retained under `test/api-client-run-*`, `test/engine-host-final-*` and `test/desktop-final-verify-*`.
- [Independent engine review](engine-adaptation-review.md) passed 29 static assertions: exact changes, PE layout, checksum, CALL/LEA/tail-JMP encoding and string lengths. [Adaptation record](engine-adaptation.json) and original [27-record REA evidence export](engine-investigation/engine-static.evidence.json) preserve observations separately from applied changes and runtime unknowns.
- [Release integrity check](../../test/desktop-release-verification.json) verified all 205 package/ZIP files, decompressed ZIP member digests, both release entry points, 198 unchanged dependencies and both original installer hashes. No vendor executable ran.

The UI skill's supplied integrity helper accepts only web/iOS/Android. The receipt explicitly declares Windows and retains the blocked live-receiver check; its result must not be presented as a full helper pass. A separate local hash check verifies the receipt's actual files. This distinction is recorded in the helper output beside the receipt.

Original targets remain unchanged: regular installer SHA-256 `81a43b30558e34af6cb9c3056b07e8bc93d88ecb23e68257bb246da07f552f02`; x64 installer `9d012d7452a4b2d65bf5046e3e97ad747e38110036c7aa86666ea23553b98476`. The adapted native engine is `431af2143fc7ca5b844474f0f0fb94fc90447a4a90ebf5dba4cfef225de6041e`. Current executable and ZIP digests are in [release/build-status.json](../../release/build-status.json).

The earlier launcher was blocked by Windows Application Control. This new frontend ran normally in its non-backend review modes; that does not establish that Windows will allow the modified receiver. On first receiver start, the host now provisions named inbound and outbound Windows Firewall rules for the packaged receiver; it reports an actionable error if Windows denies that operation.

## Skills and reproducibility

Build with `powershell -NoProfile -File src/build/build-desktop.ps1`. Verify the archive with `powershell -NoProfile -File test/verify-desktop-release.ps1`. The build performs static adaptation only. Regenerate the receipt after a final app review with `node src/build/write-visual-evidence.cjs`.

The requested gstack router remains in its documented degraded mode because its runner helpers are absent; its routing and self-learning notes are retained in the [original investigation report](../douwan-static-pass/report.md). Run `./setup` or `/gstack-upgrade` to restore its helpers. The System Design DOCX workflow remains unavailable: its [SKILL.md](C:/Users/dranz/.codex/plugins/cache/openai-curated-remote/openai-templates/0.1.1/skills/artifact-template-system-design/SKILL.md) says, “If no such capability can be identified and read, say it is unavailable and stop.” Its template configuration was read, but no connected document capability was available. That document workflow stopped without changing its reference DOCX; this report is a separate Markdown deliverable.

Durable learnings: Qt AppLocalDataLocation isolation depends on the executable application name, not the working directory; the original API's published localhost URL did not imply a loopback bind; a tail jump preserves the original listener's stack arguments; Framework ZIP creation on Windows needs explicit slash-normalized entry names for portable archives. No helper telemetry or consent was inferred.
