# Agent Instructions

This repository contains a small compiled programming language written in C++23.

## Project Goal

Build a tiny compiled language with this pipeline:

source code
→ lexer
→ parser
→ AST
→ bytecode compiler
→ virtual machine

Example:

```txt
let x = 40 + 2;
print(x);
```

Expected output:

```txt
42
```

---

## Rules

- Never push directly to `main`
- Always work through pull requests
- Keep PRs small and focused
- Add tests for every feature or bug fix
- Do not reformat unrelated files
- Do not add dependencies without approval
- Do not change project structure unless required
- Prefer simple implementations over abstractions

---

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Coding Style

- Use C++23
- Prefer `std::unique_ptr`
- Avoid raw owning pointers
- Prefer clear code over clever code
- Keep functions small
- Use descriptive names
- Avoid unnecessary inheritance

---

## Testing

Every behavior change should include tests.

Prefer:
- unit tests
- integration tests
- golden tests

---

## Pull Requests

Every PR should:

- build successfully
- pass all tests
- stay focused on one task
- include a clear summary
- avoid unrelated changes

---

## Current Priorities

Current implementation order:

1. lexer
2. parser
3. AST
4. bytecode compiler
5. VM
6. REPL
7. language features

Focus on correctness first.
Optimization comes later.
