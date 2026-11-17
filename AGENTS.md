# Repository agent guidance

## Scope

This repository currently contains two DouWan installer binaries:

- `ready-products/DouWan-Video-Setup-En-4.4.0.9.exe`
- `ready-products/DouWan-Video-Setup-En-4.4.0.9-x64.exe`

Treat these files as the primary investigation targets. Do not execute them unless
the user explicitly requests runtime observation and the required safety checks
have been completed.

## Installed skills

The following Codex skills are configured for this repository through the global
skill installation:

- `gstack` for routing reviews, QA, verification, and documentation workflows.
- `reverse-engineer-anything` for static analysis of the installer artifacts.
- `rea-tool-design` for REA tool and Evidence-contract work.
- `rea-analysis-optimization` for REA analysis performance work.
- `rea-ci-tests-audit` for REA test and CI audits.
- `rea-changelog-update` for REA release-note work.

## Analysis rules

- Identify the exact target file and preserve its SHA-256 before analysis.
- Keep observations, inferences, and unknowns separate.
- Prefer static inspection and format-aware inventory before runtime execution.
- Record evidence identifiers and analysis limitations in any report.
- Do not modify or overwrite files under `ready-products/`.
