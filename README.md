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

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The compiler CLI is not implemented yet. For now, the build command compiles
the C++ project and the tests cover the lexer/parser behavior for `.dn` source.
