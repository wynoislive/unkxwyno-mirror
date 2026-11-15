# Agent Rules & Instructions

## Core Directive: Mandatory & Proactive Skill Utilization

> [!IMPORTANT]
> **RULE:** Always identify, consult, and actively utilize whichever installed skill is helpful and relevant to formulating the best response or executing the user's task.

### Skill Routing & Quick Reference

| Domain | Recommended Skill | When to Use |
| :--- | :--- | :--- |
| **Reverse Engineering & Protocols** | `reverse-engineer-anything`, `reverse-engineering-tools` | Binary reversing, x86/x64 disassembly, PE/Mach-O analysis, USB video streaming protocol dissection. |
| **Android & APK Analysis** | `android-apk-reverse-engineering`, `apk-reverse`, `re-android-native` | APK decompilation, Smali analysis, JNI native library (.so) reversing, Android USB accessory (AOA) protocols. |
| **ADB & Device Automation** | `adb`, `adb-debug`, `understanding-adb-architecture`, `android-device-automation` | Low-level ADB commands, device transport inspection, screen capture & touch input injection. |
| **High-Performance Native Engineering** | `cpp-pro`, `cpp-coding-standards`, `rust-backend` | Writing high-throughput video decoder pipelines, low-latency screen mirroring logic, and hardware-accelerated rendering. |
| **Bug Diagnosis & Diagnostics** | `diagnosing-bugs`, `concurrency-debugging` | Resolving frame drops, stutter, USB transmission timeouts, buffer desyncs, and audio/video latency. |
| **Clean Architecture & Code Craft** | `clean-architecture`, `clean-code`, `code-craft` | Structuring modular, maintainable, decoupled core protocols and interface adapters. |
| **Planning & Verification** | `gstack` (`plan-eng-review`, `qa`, `verify`) | Architecture roadmap, idempotency testing, protocol verification. |

### Operational Rules
1. **Scan available skills in `.agents/skills/`** before executing any task.
2. **Consult `SKILL.md`**: When a skill matches the problem space, read its instructions and follow its workflow.
3. **Reproducible & Clean Artifacts**: Keep research findings in `docs/` and `manual research/`, decompilation outputs and built binaries in `output/`, and maintain `source_sample_examples/` as the baseline.
