# dune lang

A small compiled programming language written in C++23.

## Example

Example `hello.dn` source:

```dn
let x = 40 + 2;
print(x);
```

Expected output:

```txt
42
```

## Run

```bash
cmake -S . -B build
cmake --build build
./build/dune hello.dn
```

Compile to a native output file through generated assembly:

```bash
./build/dune build hello.dn -o hello
./hello
```

Emit only assembly:

```bash
./build/dune asm hello.dn -o hello.s
```

## Build And Test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The default build runs `clang-format` and `clang-tidy` checks through CMake before
compiling targets. If those tools are not installed locally yet, configure with:

```bash
cmake -S . -B build -D DUNE_ENABLE_LINT=OFF
```

## v0.1

The `0.1.0` release implements a small calculator language with:

- lexer
- parser
- AST
- arithmetic
- variables
- print
- bytecode
- VM
- CLI
- tests
- CI
