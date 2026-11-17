# Third-party notices

The independent USB capture implementation uses the published protocol research
in [quicktime_video_hack](https://github.com/danielpaulus/quicktime_video_hack).
The session property schema was adapted from its MIT-licensed implementation.
No upstream executable or proprietary receiver component is shipped.

Reference revision: `d81396e2e7758d98c2a594853b64f98b54a8a871`.
The recorded `tests/usb_capture/reference-feed.bin` fixture is also from that
MIT-licensed repository (SHA-256
`fa246a5cf1a264ae6e745b41faf60a391b2ceef6fdd7c65a22e2edcb3296f566`).
It is test data, not a live-device validation or a shipped user recording.

MIT License

Copyright (c) 2019 danielpaulus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## libusb

The Windows runtime dynamically loads the unmodified x64 MinGW build of
[libusb 1.0.30](https://github.com/libusb/libusb/releases/tag/v1.0.30), licensed
under LGPL 2.1 or later. Its complete license is distributed as
`libusb-LICENSE.txt`. Users may replace this DLL with an ABI-compatible build.
No static linking or library modifications are used.

Corresponding source: https://github.com/libusb/libusb/releases/download/v1.0.30/libusb-1.0.30.tar.bz2

Pinned upstream archive SHA-256:
`7fb1dfec805b97983763d7d0ae244320da12add1003d4249c96cc4d586398c79`.
Corresponding source archive SHA-256:
`fea36f34f9156400209595e300840767ab1a385ede1dc7ee893015aea9c6dbaf`.
Unmodified DLL SHA-256:
`5bd409849825009b6fe25861a6147f76d256aab248f07049d78387e3bff12d94`.
Capture drivers are separately installed Windows components; this app neither
bundles nor silently installs a device driver.

The direct libusb-win32 adapter uses the public API declarations in
`lusb0_usb.h` from revision `2e8aaf2f1fa0b98a115015419c0689c16be3ec55`
of https://github.com/mcuee/libusb-win32 (LGPL 2 or later). The build downloads
the unmodified header, digest
`e76f3ee6f230f7e4d28baaf989bb44eda17d93877a30147ca508651201f89c6c`.
It dynamically calls the user's separately installed system driver runtime;
that runtime is not redistributed in the app.

## OpenSSL 3.5.9

The USB trusted-session implementation dynamically loads unmodified OpenSSL
3.5.9 x64 libraries, licensed under Apache 2.0. The complete license is shipped
as `OpenSSL-LICENSE.txt`. These are independent crypto libraries from the official
portable Git for Windows distribution, not components of the reference mirroring product.
The preparation/package scripts pin their SHA-256 identities:

- `libssl-3-x64.dll`: `6af55a4d522e2cc29ff0833cad4f6f7560f8ba2d107b97990d9415c590a4224a`
- `libcrypto-3-x64.dll`: `f8839fd7d5b5e7b1763c88705538c5762a2f97df17f37092e37de3c663f30b1e`

Reproducible binary distribution:
https://github.com/git-for-windows/git/releases/download/v2.56.1.windows.1/MinGit-2.56.1-64-bit.zip
Archive SHA-256: `4a92850b6e917da4fd010669b614b55dccf5b0ec333a534cc4be4700dfefdafd`.
The build extracts only the two libraries and their license, verifies their
individual hashes, and does not execute or install Git from this archive.

Corresponding upstream source: https://github.com/openssl/openssl/tree/openssl-3.5.9

The application generates its own pairing credentials. No private Apple/vendor
key or saved iTunes pairing record is read or redistributed. No root certificate
is installed into Windows trust stores. TLS peer identity is pinned to the
connected device's public pairing key before application data is accepted.
