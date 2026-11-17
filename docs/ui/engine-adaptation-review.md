# Static adaptation review

Result: **no material error found in the reviewed adaptation**. Runtime behavior remains unverified. No backend was executed, attached, or loaded; no profiles or implementation files were changed by this review.

Reviewed `src/build/prepare-branded-engine.cjs`, `docs/ui/engine-adaptation.json`, prepared input, and actual output `src/app/obj/engine/unkxwyno-receiver.exe`. Input SHA-256 is `c631ab24c8704683750afa23ffe16b4386ed1f886b1be203e92808770cd9f368`; output SHA-256 is `431af2143fc7ca5b844474f0f0fb94fc90447a4a90ebf5dba4cfef225de6041e`, matching the adaptation record.

Independent binary parsing and instruction decoding confirmed:

- `.umcore` is a non-overlapping read/execute section atVA `0x141544000`, file offset19616256, virtual size26, raw size512. File/RVA alignment, section count, header space, SizeOfImage, SizeOfCode, and file backing are consistent. Existing section headers are unchanged. The PE security directory is empty.
- API CALL `0x1400d26b9` reaches the new stub. Stub `LEA R8,[0x141544010]` selects NUL-terminated `127.0.0.1`; its tail JMP reaches original helper `0x1400de2d0`. It changes only the host argument, leaves stack/nonvolatile registers untouched, and preserves the original caller return address and stack arguments. Relative references remain valid under normal image rebasing. A leaf stub that does not modify RSP/nonvolatile registers does not require a new unwind frame.
- Failed-start alert jump reaches common cleanup `0x14002f7db`; preceding successful-start conditional/timer path and main-object pointer stores remain unchanged. Startup show/activation calls are removed exactly as documented.
- Receiver literal is exactly `Mirror\0` (six characters), and singleton key is exactly17 characters `UNKXWYNO_MIRROR_1` followed by the original NUL. Both fit their original slots and explicit lengths.
- Independent dword-based checksum computation matches stored checksum19655136. Comparison against the prepared input found no undeclared changes within the original file; only the six documented edits, section header/count, image/code sizes, and checksum differ.

Original call semantics are supported by REA Evidence `ev_43a5b24a68f1bb4860e6a38de1feb9a85a7122cb315b5aed067dfb91e9f3365d` and resolver Evidence `ev_d7c1fb7891fbcdf058ea3a8138934a7fe14f03b7099905e5e1273d8836728bed`. Startup semantics are supported by `ev_70004dbdd808bf8fa49c557cbed0bd0302e94c82b97b2420bb0f8fa1fbf7e0fc`. These describe the original artifact; output validation is independent local static parsing/iced-x86 decoding, not new REA runtime evidence.

Scope limits: the shared receiver literal also changes its other existing brand references. Later vendor windows/tray surfaces, actual loopback listener reachability, renamed-profile resolution, receiver discovery, drivers, and phone operation were not observed. This review does not validate the earlier prepared-input changes. It confirms the new adaptation's file structure and encoded control flow, not operational completeness.

Reproducible review: `node src/analysis/review-branded-engine.cjs` (read-only artifact inspection). Detailed results: `engine-adaptation-review.json`. Decoded output: `engine-adaptation-decoded.json` and `.asm`. All29 static assertions passed.
