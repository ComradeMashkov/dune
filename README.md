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

Control flow example:

```dn
let x = 3;

while x > 0 {
  x = x - 1;
}

if x == 0 {
  print(42);
} else {
  print(0);
}
```

Boolean values are represented as `1` for `true` and `0` for `false` when printed.

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

## v0.2

The `0.2.0` release implements a small calculator language with:

- lexer
- parser
- AST
- arithmetic
- variables
- booleans
- comparison operators
- print
- assignment
- if/else
- while
- bytecode
- VM
- native assembly output
- CLI
- tests
- CI
