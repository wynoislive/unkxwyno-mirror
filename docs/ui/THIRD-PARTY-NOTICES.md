# Third-party components in 1.2.0

The authored WPF host and USB lifecycle helper reuse native components from the user-supplied DouWan 4.4.0.9 x64 package. Native files retain their original bytes, metadata, signatures, and vendor interface. This integration does not claim authorship of those components or affiliation with their vendors.

The supplied engine includes DouWan's receiver and recorder libraries, Qt, FFmpeg, SDL, libyuv, virtual-device components, and supporting libraries. Existing vendor files and notices are retained with the original payload under engine/. The package inventory records each file's size and SHA-256. A matching hash establishes identity with this package, not an independent security audit or a recovered source implementation.

The original engine's license, account, and activation requirements remain applicable. This local integration grants no additional redistribution rights or ownership of the supplied proprietary components. No license restriction branches are patched or bypassed.

The USB helper reuses an existing Windows driver package on the target PC; it does not redistribute or install a new driver package. The investigated libusb-win32 package identifies LGPL licensing in its INF, and its original signed binaries remain managed by Windows.

AirPlay-Windows and scrcpy from the superseded 1.1.0 build are not part of this package. Their historical notices are preserved in archived documentation and older release folders.

The independent QuickTime protocol reference examined during analysis remains reference material under refrence/quicktime-protocol-reference; its code and binaries are not included in this product. Its observations are not substituted for the shipped DouWan evidence.
