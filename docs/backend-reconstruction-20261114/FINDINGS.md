# Backend reconstruction findings - 2026-11-14

Status: partial investigation and independently implemented USB lower layers.
Wired and wireless screen mirroring are not complete. No 100% stability claim.

## Request and identities

The current request explicitly rejects restoring the old receiver and asks for
source implementation informed by analysis of DouWan's wired/wireless backend.
The earlier question about restoring native components is superseded.

Original installers remain read-only and retain their SHA-256:

- Regular: `81a43b30558e34af6cb9c3056b07e8bc93d88ecb23e68257bb246da07f552f02`.
- x64: `9d012d7452a4b2d65bf5046e3e97ad747e38110036c7aa86666ea23553b98476`.
- Regular `Apple/usbscreen.dll`:
  `6519aa84bdc4fd2f05ef4f3a04b9d2d830da99f5632490c6ab339cb8b1aae0a2`.
- Regular `Apple/airmediaserve.dll`:
  `e2665fdb0a58f429f4a666083b1a745020c9f620a20330a2e038d8d5c6fa58bb`.

No native runtime was launched or restored during this investigation. Static
analysis and prior runtime captures have different evidence boundaries.

## Wired observations

| Finding | Source/evidence | Implementation or gap |
| --- | --- | --- |
| Dedicated capture interface class/subclass/protocol FF/2A/FF; two opposite bulk endpoints | Prior x64 runtime constructor instruction window `runtime-usb-constructor.local.json`, addresses 0x7ff97dc576e8 onward; ordinary local disassembly, not REA Evidence | Independent WinUSB adapter validates Apple VID and exact interface, endpoint directions, bulk type and packet sizes |
| LE32 total frame length at 0, numeric family at 4; fragmented/coalesced reads | `ev_2912d020b1521f55e7356dea3460adf6b1ff89860d109b0f5aa46490b4a373b6`, 0x1003a960 | Bounded incremental packet reader implemented |
| Ping output is LE32 words 16, 0x70696e67, 1, 1 | `ev_d5aed5aedd67e0bf5e41927d0b26f78ae7a7c6708d72c39091747d048d34a4b3`, 0x1003be70 | Exact byte-vector test and encoder |
| Async subtype at 16; need output retains two opaque words | `ev_d8c5def4db68a2bc5d469b45064fae75d6362e97944dfd75e2aac9a9727b2b00` | Need encoder; opaque semantics not invented |
| feed/eat! contain nested length and sbuf tag | `ev_1069522871b6e80502ea144a241c1d7d659413f6f86f471f897d9b11b852c5cd` and `ev_4e3f456aa73908ea2ee5c2aebadd711a713388c4d424b8be4a9acdedf889ac17` | Bounds-checked sample view; audio/video identity and format decoding unresolved |
| Serialized writes, 1000 ms timeout, ZLP on packet-size multiples; 16 KiB reads with 200 ms timeout | `ev_6a72b8d01516c386cbbfac5cdba5ff3c45376a2b6dbe84d933657d10df74fe0d`, `ev_26a2397e9a191703a1a72694d5f86aa3364f93e011a3d706def40d3cd395812a` | WinUSB timeouts and SHORT_PACKET_TERMINATE policy; physical I/O untested |

The original reader has weak malformed-length handling. The independent reader
rejects oversized/invalid input and requires reset after a framing error. The
default 4 MiB parser ceiling is an engineering safety bound, not a recovered
universal maximum frame size. Incoming ping classification requires only the
8-byte outer envelope; emitted ping is exactly 16 bytes. Sync envelopes need
28 bytes for correlation words; subtype-specific additional fields still need
their own validation before a responding state machine is implemented.

Fresh REA/Ghidra 12.1.4 findings:

- `ev_c413999200380b6dd68accf938dc97e5243508169bc11988e1c8e6f643fb1e99`,
  0x1003ae50, calls device lookup, a lockdown client constructor, pairing-record
  checks, HostID lookup, and StartSession.
- `ev_f5ede042439a310091362d26b9da374c7ce4a740d7e27c5357e2a1fd73494362`,
  0x10032590, duplicates `iOSScreenCaptureAssistant` as the client label. It is
  not evidence of a StartService name.
- `ev_db35206e8042122fb5db8e367718a7e1e2649e045739752ee6fee83ececa9b53`
  reads the initialized 16-bit port value at 0x101761ec: little-endian 62078.
- `ev_5bd3639d0e70c80718a7c8b4226947cee83e3b5c48833dea57bfe543e2128e92`,
  0x10033390, composes StartSession with HostID/SystemBUID and enables session
  SSL when the response requests it. No private pairing-record values were read.
- `ev_b1d27553b07c11cf5657dd31d8b029972466d1fa953e4ff964e692547b600216`
  confirms 0x1003a1a0 is a destructor. An earlier suggested startup label was
  withdrawn rather than implemented as initialization.

The exact capture-mode enable request, complete property/sync schemas, codec
sample structures and physical reconnect lifecycle remain unresolved. Regular
x86 and protected x64 implementations are not proven byte-for-byte equivalent.

## Wireless observations and inference

The original regular receiver contains `_airplay._tcp` at file offset 5086348,
`_raop._tcp` at 5086460, `/pair-verify` at 5088280, `/pair-setup` at 5088296,
`RTSP/1.0` at 5092308, streamConnectionID at 5160240, RTP/UDP transport format
at 5162880, and `/fp-setup` at 5163724. These are ordinary read-only artifact
observations, not new REA Evidence or proof of an executed handshake.

Captured x64 runtime data independently contains the same discovery, RTSP and
RTP families. The saved original runtime API observation reports iOS wireless
cast_type 2. Together these strongly support AirPlay/RAOP as the wireless
iPhone architecture. A custom companion protocol cannot truthfully be called
the same wireless mechanism.

Discovery registration TXT fields/ports, active authentication, SETUP schemas,
encrypted media framing, clock synchronization and reconnect behavior still
need caller/runtime evidence. Existing `docs/receiver-source-assessment.md`
records protected-data constraints for previously considered alternatives.
No replayed responses, private keys, opaque decryption tables or licensing bypass
were added. Native iPhone wireless compatibility is unverified.

## Validation

- Native Debug/Release builds and two CTest executables pass.
- Packet tests cover exact golden bytes, every ping split, 4096 coalesced frames,
  malformed/oversized lengths, unsupported tags, explicit session reset, nested
  buffer bounds and opposite-order endpoint selection.
- A copied production parser with a deliberately changed ping word failed the
  golden-wire assertion with exit 1. Mutation artifacts are outside the source
  tree in a task-specific temporary directory; production bytes stayed intact.
- Test orchestration now requires both native executables to be discovered once.
  A controlled missing-test inventory was rejected before execution.
- WinUSB open/transfer code compiles; only invalid-path rejection was executed.
  No device capture read, write, mode transition or driver change was performed.

Microsoft API contract used for the independent Windows adapter:
[WinUSB initialization](https://learn.microsoft.com/en-us/windows/win32/api/winusb/nf-winusb-winusb_initialize),
[pipe policies](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/winusb-functions-for-pipe-policy-modification).

Raw REA results and exported evidence are preserved under the ignored `raw/`
subdirectory. Static pseudocode remains analysis material, not shipped code.

### Wireless analysis limitation

Fresh REA opened the original wireless DLL with its verified hash, but the
`/pair-verify` string/caller investigation could not pass Ghidra's 330000 ms
startup deadline. The producer reported incomplete analysis and no confirmed
out-of-memory error. Its complete error result is preserved in
`raw/017-search_strings-1794631994847.json`. No wireless function execution or
handshake completeness is claimed. The final post-switch export contained zero
records; it is not a substitute for the earlier ten-record USB export or the
saved wireless artifact observations. Both opened targets were closed and the
owned REA helper exited normally. This installed tool catalog has no advertised
reset_investigation operation.

### Public protocol cross-check

The independent author's [QuickTime USB protocol notes](https://github.com/danielpaulus/quicktime_video_hack/blob/main/doc/technical_documentation.md)
under an [MIT license](https://github.com/danielpaulus/quicktime_video_hack/blob/main/LICENSE)
corroborate the dedicated bulk interface, synchronization, feed/eat sample
envelopes and need-driven delivery. They describe activation using vendor request
0x40/0x52, value 0, index 2. That activation request has not yet been corroborated
at the supplied binary's call site and was not sent to the phone. Their example
ping and some replies differ from this binary; the implementation keeps the
verified binary's emitted ping. Public notes are reference evidence, not proof
of matching behavior on the connected device. No upstream software was executed
or substituted for this backend.

## Remaining checklist

| Question | State |
| --- | --- |
| Original USB framing and transfer rules | Answered for selected paths |
| Lockdown session construction | Partially answered |
| Capture-mode activation | Unresolved |
| Media format, decode, render, audio sync | Unresolved |
| Wireless protocol family | Strong inference; active handshake unresolved |
| Wireless native iPhone interoperability | Unresolved |
| End-to-end wired/wireless stability | Untested |
