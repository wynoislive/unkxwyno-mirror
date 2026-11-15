# Workspace Agent Rules & Guidelines

## Core Directive: Mandatory & Proactive Skill Utilization

> [!IMPORTANT]
> **MANDATORY RULE FOR ALL TASKS & RESPONSES:**
> Always identify, consult, and proactively utilize whichever installed skill is helpful, relevant, or specialized for the task at hand before responding or executing actions.

---

### 1. Skill Discovery & Selection Protocol
Before formulating a response or executing code changes, check available skills in `.agents/skills/` and workspace lockfile:

- **Reverse Engineering & Protocol Analysis**:
  - `reverse-engineer-anything` / `reverse-engineering-tools`: Binary analysis, offset finding, disassembly, PE header examination, and network protocol reverse engineering.
  - `android-apk-reverse-engineering` / `apk-reverse` / `re-android-native`: APK decompilation, Smali analysis, JNI `.so` native library reversing, DouWan Android protocol reverse engineering.
  - `diagnosing-bugs` / `concurrency-debugging`: Hard bugs, latency bottlenecks, audio/video sync regressions, and USB frame loss.

- **ADB, Android & Mirroring Toolchain**:
  - `adb` / `adb-debug` / `understanding-adb-architecture`: Direct device control, shell commands, socket forwarding, and ADB daemon protocol interaction.
  - `android-device-automation`: Input event injection and automated testing.

- **High-Performance Development**:
  - `cpp-pro` / `modern-cpp` / `cpp-coding-standards`: High-performance C++ video pipelines, DirectX/OpenGL renderers, and USB driver interfacing.
  - `rust-backend` / `code-craft`: Memory-safe high-throughput streaming server/client implementations.

- **Workflow, Planning & Quality Assurance**:
  - `gstack` (`plan-eng-review`, `qa`, `verify`, `test-audit`): Systematic planning and regression verification.
  - `clean-code` / `clean-architecture`: Clean modular design separating transport, decoding, and rendering.

---

### 2. Execution Guidelines
1. **Always Read `SKILL.md` First**: When a relevant skill is identified, read its `SKILL.md` (and relevant references) to follow its structured procedure.
2. **Adopt Skill Best Practices**: Incorporate domain heuristics, safety checks, and verification patterns defined by the skill.
3. **Transparent Execution**: Inform the user when a specialized skill or tool is utilized to ensure clarity and reproducibility.
