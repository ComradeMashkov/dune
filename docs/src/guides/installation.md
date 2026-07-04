# Installation and building

Dune is built from source with CMake. It targets C++23.

## Build

```sh
git clone https://github.com/ComradeMashkov/dune.git
cd dune
cmake -S . -B build -D DUNE_ENABLE_NATIVE=OFF
cmake --build build -j
```

This produces the `dune` binary at `build/dune`. The native LLVM backend is
optional and gated behind `-D DUNE_ENABLE_NATIVE=ON` (it requires an LLVM
toolchain); with it off, the virtual machine still runs every program.

## Run a program

```sh
./build/dune examples/matrix_basics.dn
```

## Run the tests

```sh
ctest --test-dir build
```

The standard library is discovered relative to the binary (and via the
`DUNE_STDLIB_PATH` environment variable), so `import math;` and friends work out
of the box.

Next: the [command-line tool](cli.md).
