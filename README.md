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
`stdlib/math.dn`, `stdlib/array.dn`, and `stdlib/text.dn`. Low-level array and
text operations such as `len`, `push`, indexing, and slicing remain runtime
primitives; higher-level helpers are ordinary Dune functions in the standard
library.

Module files can mark their public API with `export`. If a module contains any
explicit exports, only exported functions and constants can be accessed through
`module.name`; private helpers remain callable inside the module.

```dn
export const ANSWER: int = 42;

fn hidden() -> int {
  return 7;
}

export fn public() -> int {
  return hidden();
}
```

Operators and explicit casts:

```dn
let value = 17;
let exact: real64 = value as real64;

print(-value);
print(value % 5);
print(!false && true);
```

Array and text methods:

```dn
let values: [int] = [1, 2];
values.push(3);
print(values.pop());
print(values.is_empty());

let message: text = "dune language";
print(message.len());
print(message.contains("lang"));
print(message.starts_with("dune"));
```

Indexing, slices, loops, and extern functions:

```dn
extern fn c_sqrt(value: real64) -> real64 = "sqrt";

let message: text = "dune language";
print(message[0]);
print(message[5:13]);

let values: [int] = [1, 2, 3, 4];
let middle: [int] = values[1:3];

for let i = 0; i < middle.len(); i = i + 1 {
  if i == 1 {
    continue;
  }

  print(middle[i]);
}

print(c_sqrt(81.0));
```

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

The `array` module provides overloaded helpers for common element types:

- `copy(values)`
- `reverse(values)`
- `contains(values, needle)`
- `index_of(values, needle)`
- `first(values)`
- `last(values)`
- `append(values, value)`
- `range(start, end)`
- `sum(values)`

The `text` module provides text and glyph helpers:

- `len(value)`, `is_empty(value)`
- `contains(value, needle)`, `starts_with(value, prefix)`, `ends_with(value, suffix)`
- `char_at(value, index)`, `slice(value, start, end)`, `prefix(value, end)`, `suffix(value, start)`
- `index_of(value, glyph)`, `count(value, glyph)`
- `is_space(glyph)`, `is_digit(glyph)`, `is_alpha(glyph)`
- `trim_start(value)`, `trim_end(value)`, `trim(value)`

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

Indexing and slicing:

- arrays: `values[index]`, `values[start:end]`, `values[:end]`, `values[start:]`
- text: `message[index]` returns `glyph`; text slices return `text`

Built-in receiver methods:

- arrays: `len()`, `push(value)`, `pop()`, `clear()`, `is_empty()`
- text: `len()`, `is_empty()`, `contains(needle)`, `starts_with(prefix)`

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
- unary operators
- logical operators
- modulo
- explicit casts with `as`
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
- text methods
- text indexing
- slices
- imports
- export visibility
- extern functions
- standard library modules
- comparison operators
- print
- assignment
- if/else
- while
- for
- break/continue
- bytecode
- VM
- LLVM native backend
- CLI
- tests
- CI
