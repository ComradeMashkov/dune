# Dune

Dune is a programming language which aims to keep systems-style code small,
readable, and predictable while still compiling through a real compiler
pipeline. It has a lexer, parser, AST, static type checker, bytecode VM, and an
LLVM-based native backend.

The language is intentionally compact: explicit types when they matter,
overloads and generics for reusable code, modules loaded from `.dn` files, and a
standard library that is increasingly written in Dune itself. Runtime checks
catch common mistakes such as invalid indexes and slices, while the type checker
rejects mismatched assignments, calls, returns, and binary operations before
execution.

Good fits for Dune today:

- experimenting with compiled language implementation
- writing small typed programs with straightforward syntax
- trying VM execution and native LLVM output from the same source
- building standard library code in the language itself

## Example

Example `hello.dn` source:

```dn
// Dune supports single-line comments.
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
import array;

let values: [int] = [1, math.square(2), 5];
values.push(math.square(values[2]));

print(values.len());
print(values[0]);
print(values[3]);
print(values.first());
```

Modules are loaded from `.dn` files. The standard library currently includes
`stdlib/math.dn`, `stdlib/array.dn`, and `stdlib/text.dn`. Low-level array and
text operations such as `len`, `push`, indexing, and slicing remain runtime
primitives; higher-level helpers are ordinary Dune functions in the standard
library.

The standard library can expose receiver methods with `impl` blocks. For
example, importing `array` makes both `array.first(values)` and `values.first()`
available. Importing `text` similarly enables helpers such as
`message.trim().ends_with("x")`.

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

The `math` module currently provides constants and generic numeric functions:

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

The `array` module provides generic helpers for common element types:

- `copy(values)` or `values.copy()`
- `reverse(values)` or `values.reverse()`
- `contains(values, needle)` or `values.contains(needle)`
- `index_of(values, needle)` or `values.index_of(needle)`
- `first(values)` or `values.first()`
- `last(values)` or `values.last()`
- `append(values, value)` or `values.append(value)`
- `range(start, end)`
- `sum(values)`

The `text` module provides text and glyph helpers:

- `len(value)` or `value.len()`, `is_empty(value)` or `value.is_empty()`
- `contains(value, needle)` or `value.contains(needle)`
- `starts_with(value, prefix)` or `value.starts_with(prefix)`
- `ends_with(value, suffix)` or `value.ends_with(suffix)`
- `char_at(value, index)` or `value.char_at(index)`
- `slice(value, start, end)` or `value.slice(start, end)`
- `prefix(value, end)` or `value.prefix(end)`, `suffix(value, start)` or `value.suffix(start)`
- `index_of(value, glyph)` or `value.index_of(glyph)`, `count(value, glyph)` or `value.count(glyph)`
- `is_space(glyph)`, `is_digit(glyph)`, `is_alpha(glyph)`
- `trim_start(value)` or `value.trim_start()`
- `trim_end(value)` or `value.trim_end()`
- `trim(value)` or `value.trim()`

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

Functions can also be generic. Generic functions are instantiated from actual
call sites instead of eagerly expanding every supported type. A generic
parameter can be unbounded, or it can use one of the current built-in bounds:
`integer`, `numeric`, or `real`.

```dn
fn identity<T>(value: T) -> T {
  return value;
}

fn square<T: numeric>(value: T) -> T {
  return value * value;
}
```

Receiver methods can be written with `impl`. The receiver is available inside
the method body as `self`; exported impl methods can also be called as module
functions after import.

```dn
export impl<T> [T] {
  fn first() -> T {
    return self[0];
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

- arrays are zero-based: `values[0]` is the first element
- arrays: `values[index]`, `values[start:end]`, `values[:end]`, `values[start:]`
- text is zero-based: `message[0]` returns the first `glyph`
- text slices return `text`
- native output checks array/text indexes, slices, and empty `pop()` calls at runtime

Comments:

- `//` starts a single-line comment

Built-in receiver methods:

- arrays: `len()`, `push(value)`, `pop()`, `clear()`, `is_empty()`
- text: `len()`, `is_empty()`, `contains(needle)`, `starts_with(prefix)`

Standard library receiver methods are enabled by importing their module:

- `import array;` enables helpers such as `values.first()` and `values.reverse()`
- `import text;` enables helpers such as `message.trim()` and `message.ends_with("x")`

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
- single-line comments
- unary operators
- logical operators
- modulo
- explicit casts with `as`
- typed functions
- overloaded functions
- generic functions with basic bounds
- call-site generic instantiation
- impl blocks
- static scalar types
- booleans
- signed and unsigned integer widths
- floating point values
- glyph and text values
- unit-returning functions
- dynamic arrays
- array methods
- text methods
- standard library extension methods
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
- runtime bounds checks
- CLI
- tests
- CI
