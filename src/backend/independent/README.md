# Independent backend reconstruction

This source is compiled into `unkxwyno_core.dll`. It does not load the original
receiver or install/change device drivers. A separate, explicit administrator
trial script can temporarily use the already installed capture driver.

- `usb_capture_protocol.*`: incremental native USB capture framing, exact ping
  and need envelopes, nested sample bounds, endpoint selection.
- `winusb_capture_transport.*`: Windows bulk transfer adapter for an already
  exposed Apple FF/2A/FF capture interface. Single serialized reader/writer,
  cancellation, explicit error propagation and short-packet termination.
- `apple_usb_devices.*`: registered Apple/WinUSB interface discovery and the
  documented capture-mode request over a compatible existing driver.
- `libusb_capture_transport.*`: dynamically linked upstream libusb adapter.
- `libusb_win32_capture_transport.*`: direct system libusb-win32 API adapter;
  reads live descriptors, activates the hidden configuration, selects its actual
  FF/2A/FF interface, and claims its discovered bulk endpoints.
- `usb_capture_session.*`: typed CoreMedia dictionaries, bounded format/sample
  parsing, PING/SYNC replies, video/audio announcements, NEED backpressure,
  AVC configuration and AVCC-to-Annex-B conversion.
- `usb_mirror.*`: C ABI session ownership, Windows H.264 decoder, bounded PCM
  playback, aspect-preserving viewer, stop/cleanup and actual displayed-FPS count.

The desktop's Mirror iPhone USB action now calls this capture pipeline instead
of counting devices through usbmux. A recorded MIT upstream USB fixture decodes
to one actual 1136x2448 image through the production DLL. On the connected phone,
direct libusb-win32 activation reveals configuration 5 and claims interface 2.
Live media acceptance is still incomplete: initial tests either repeat PING or
time out waiting for handshake data. Do not call this a working release.

The original recorder has a separate vendor startup control at VA 0x1003dc90;
its observed request is implemented without copying code or opaque response
blobs. This did not by itself resolve the live handshake. Exact lockdown/session
initialization and fresh-session recovery remain investigation targets.

Frame-rate requests are not yet negotiated over USB. Display dimensions are
negotiation hints, and delivered dimensions/FPS come from decoded/presented
frames. Rendering is currently software-based. See root THIRD_PARTY_NOTICES.md.

Evidence and reconstruction boundaries:
`docs/backend-reconstruction-20261114/FINDINGS.md`.
