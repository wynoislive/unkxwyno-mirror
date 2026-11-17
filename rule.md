# Project Rules

These rules apply to all work in this project.

## Directory ownership

- `docs/` is the only directory for project documentation.
- `ready-products/` is reserved for replication targets and reference products. Do not place source code, documentation, tests, or unrelated files there.
- `refrence/` contains all reference files supplied for analysis or comparison. Use this directory for reference material instead of scattering it through the project.
- `test/` contains all test files, test fixtures, and test-specific support files.
- `src/` contains the complete code written for the project.
- `samples/` contains sample files and examples intended for the user to inspect.

## File placement rules

- Always create documentation inside `docs/`.
- Always create tests inside `test/`.
- Always write project implementation code inside `src/`.
- Always put reference files inside `refrence/`.
- Always put user-facing samples inside `samples/`.
- Keep `ready-products/` limited to replication products and their required source artifacts.
- Do not create new project files in the repository root unless they are project configuration files or explicitly requested.

## Workflow

- Before creating a file, check that its destination matches the directory ownership rules above.
- Preserve existing user files and avoid moving or overwriting them without explicit instruction.
- If a requested file does not clearly belong in one directory, ask before creating it.
