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

Typed functions and scalar values:

```dn
fn log(message: text) -> unit {
  print(message);
}

let count: usize = 5;
let precise: real64 = 2.25;

log("ready");
print(count);
print(precise);
```

Arrays and modules:

```dn
import math;

let values: [int] = [1, math.square(2), 5];
values.push(math.square(values[2]));

print(values.len());
print(values[3]);
```

Modules are loaded from `.dn` files. The standard library currently includes
`stdlib/math.dn`; array operations such as `len` and `push` are methods on array
values.

The `math` module currently provides constants and overloaded numeric functions:

- `PI`, `TAU`, `E`, `INVERSE_E`
- `PI32`, `TAU32`, `E32`, `INVERSE_E32`

- `square(value)`
- `cube(value)`
- `abs(value)`
- `min(left, right)`
- `max(left, right)`
- `clamp(value, lower, upper)`
- `sqrt(value)`
- `sin(value)`
- `cos(value)`
- `tan(value)`
- `exp(value)`
- `ln(value)`
- `pow(base, exponent)`
- `floor(value)`
- `ceil(value)`
- `round(value)`

Functions can be overloaded by parameter types:

```dn
fn show(value: int) -> int {
  return value + 1;
}

fn show(value: bool) -> int {
  if value {
    return 10;
  } else {
    return 20;
  }
}
```

Supported scalar types:

- `int`, `bool`
- `i8`, `i16`, `i32`, `i64`, `isize`
- `u8`, `u16`, `u32`, `u64`, `usize`
- `uint8`, `uint16`, `uint32`, `uint64`
- `real`, `real32`, `real64`
- `glyph`, `text`, `unit`

Supported compound types:

- `[T]` dynamic arrays, for example `[int]` or `[text]`

## Run

```bash
cmake -S . -B build
cmake --build build -j
./build/dune hello.dn
```

Compile to a native output file through generated LLVM IR:

```bash
./build/dune build hello.dn -o hello
./hello
```

Emit only LLVM IR:

```bash
./build/dune llvm hello.dn -o hello.ll
```

## Build And Test

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The default build runs `clang-format` and `clang-tidy` checks through CMake before
compiling targets. If those tools are not installed locally yet, configure with:

```bash
cmake -S . -B build -D DUNE_ENABLE_LINT=OFF
```

## Current Features

The current release implements a small compiled language with:

- lexer
- parser
- AST
- arithmetic
- variables
- constants
- typed functions
- overloaded functions
- static scalar types
- booleans
- signed and unsigned integer widths
- floating point values
- glyph and text values
- unit-returning functions
- dynamic arrays
- array methods
- imports
- standard library modules
- comparison operators
- print
- assignment
- if/else
- while
- bytecode
- VM
- LLVM native backend
- CLI
- tests
- CI
