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

## Build And Test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
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
