# unkxwyno-protocol

> **Shared Wire Protocol & C ABI Contracts for unkXwyno Mirror**  
> **Official Web**: [https://unkxwyno-mirror.genzhost.best](https://unkxwyno-mirror.genzhost.best)

`unkxwyno-protocol` provides the unified binary packet definitions, wire contracts, and C ABI interfaces for the unkXwyno Mirror ecosystem across Android, iOS, Windows, macOS, and Linux.

---

## 📦 Header Inventory

| Header | Description |
| :--- | :--- |
| [`include/unkxwyno_protocol.h`](include/unkxwyno_protocol.h) | 8-byte binary framing header, channel IDs, packet types, AOA2 USB accessory strings, and serialization helpers. |
| [`include/unkxwyno_abi.h`](include/unkxwyno_abi.h) | Stable C ABI functions, callback definitions, engine handles, and P/Invoke exports for C# (.NET 9 / Avalonia) and Rust. |

---

## 📜 Specifications

- [**Binary Packet Framing Specification**](spec/packet_framing.md)
- [**Android Open Accessory 2.0 (AOA2) Handshake Specification**](spec/aoa2_handshake.md)
- [**iOS usbmuxd & AirPlay Mirroring Specification**](spec/airplay_usbmuxd.md)

---

## 🧪 Verification

Run the automated Python test suite:
```bash
python tests/test_protocol.py
```

Build and run C++20 static assertion tests:
```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
