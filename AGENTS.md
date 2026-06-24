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
var x = 40 + 2;
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

## Backends

Dune has two execution backends that share the same front end (lexer, parser,
type checker, module loader):

- The **bytecode VM** (`src/compiler`, `src/vm`) is the **canonical** backend.
  It needs no external toolchain, powers `dune <file>` and `dune check`, and is
  the basis for the planned REPL.
- The **native LLVM backend** (`src/codegen`) emits textual LLVM IR that
  `clang++` turns into a native binary for `dune build`. It is **best-effort**.

Policy:

- Every language feature must work in the **VM and type checker first** — that
  is what gates a pull request.
- The native backend may lag. It is fine to land a feature in the VM and follow
  up on native support later; prefer a clear "not supported in the native
  backend yet" error over silently diverging from the VM.
- CI reflects this: the `build-test` job (VM / core tests, all platforms) is the
  required gate; the `native-backend` job runs the native backend tests
  best-effort and never blocks a merge.
- Name native backend tests `cli_build_native_<case>` so CI can separate them
  with `ctest -R cli_build_native` / `-E cli_build_native`.

The native backend is gated by the `DUNE_ENABLE_NATIVE` CMake option (default
`ON`). With `-D DUNE_ENABLE_NATIVE=OFF` the build needs no `clang++` and skips
the native toolchain entirely — the VM, type checker, and LSP still build and
run, while `dune build` reports a clear error. The required CI gate uses this to
build VM-only on every platform.

---

## Build

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

VM-only build (no LLVM toolchain required, native `dune build` disabled):

```bash
cmake -S . -B build -D DUNE_ENABLE_NATIVE=OFF -D DUNE_ENABLE_LINT=OFF
cmake --build build -j
ctest --test-dir build -E cli_build_native --output-on-failure
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

Commits should follow Conventional Commits, for example:

- `feat: add parser expression support`
- `fix: handle unexpected lexer input`
- `docs: document build workflow`

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
