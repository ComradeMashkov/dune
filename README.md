# Dune

Dune is a programming language which aims to keep systems-style code small,
readable, and predictable while still compiling through a real compiler
pipeline. It has a lexer, parser, AST, static type checker, bytecode VM, and an
LLVM-based native backend.

The language is intentionally compact: explicit types when they matter,
overloads, generics, choices with `when` expressions, modules loaded from `.dn`
files, and a standard library that is increasingly written in Dune itself.
Runtime checks catch common mistakes such as invalid indexes and slices, while
the type checker rejects mismatched assignments, calls, returns, and binary
operations before execution.

Good fits for Dune today:

- experimenting with compiled language implementation
- writing small typed programs with straightforward syntax
- trying VM execution and native LLVM output from the same source
- building standard library code in the language itself

## Example

Example `hello.dn` source:

```dn
// Dune supports single-line comments.
x = 40 + 2;
print(x);
```

Expected output:

```txt
42
```

Control flow example:

```dn
x = 3;

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
log(message: text): unit {
  print(message);
}

count: usize = 5;
precise: real64 = 2.25;

log("ready");
print(count);
print(precise);
```

Arrays and modules:

```dn
import math;
import array;

values: [int] = [1, math.square(2), 5];
values.push(math.square(values[2]));

print(values.len());
print(values[0]);
print(values[3]);
print(values.first());
```

Modules are loaded from `.dn` files. The standard library currently includes
`stdlib/math.dn`, `stdlib/array.dn`, `stdlib/text.dn`, `stdlib/maybe.dn`,
`stdlib/outcome.dn`, `stdlib/assert.dn`, `stdlib/collections.dn`, and
`stdlib/autograd.dn`. Low-level
array and text operations such as `len`, `push`, indexing, and slicing remain
runtime primitives; higher-level helpers are ordinary Dune functions in the
standard library.

The standard library can expose receiver methods with `method` declarations. For
example, importing `array` makes both `array.first(values)` and `values.first()`
available. Importing `text` similarly enables helpers such as
`message.trim().ends_with("x")`.

Module files can mark their public API with `export`. If a module contains any
explicit exports, only exported functions, constants, records, choices, and
contracts can be accessed through `module.name`; private helpers remain callable
inside the module. Record fields and record methods are private across module
boundaries unless that member is marked `export`.

```dn
export const ANSWER: int = 42;

hidden(): int {
  return 7;
}

export public(): int {
  return hidden();
}
```

Operators and explicit casts:

```dn
value = 17;
exact: real64 = value to real64;

print(-value);
print(value % 5);
print(!false && true);
```

Array and text methods:

```dn
values: [int] = [1, 2];
values.push(3);
print(values.pop());
print(values.is_empty());

message: text = "dune language";
print(message.len());
print(message.contains("lang"));
print(message.starts_with("dune"));
```

Indexing, slices, loops, and foreign functions:

```dn
foreign c_sqrt(value: real64): real64 = "sqrt";

message: text = "dune language";
print(message[0]);
print(message[5:13]);

values: [int] = [1, 2, 3, 4];
middle: [int] = values[1:3];

for i = 0; i < middle.len(); i = i + 1 {
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

The `autograd` module provides scalar reverse-mode automatic differentiation.
It is implemented in Dune itself with records, arrays, methods, and mutation;
the VM does not contain autograd-specific primitives.

```dn
import autograd;

x = autograd.variable(2.0);
y = autograd.variable(3.0);
loss = x.mul(y).add(x.pow(2.0)).add(1.0);
loss.backward();

print(loss.data);
print(x.grad);
print(y.grad);
```

Expected output:

```txt
11
7
2
```

Core helpers:

- `variable(data)`, `value(data)`, `constant(data)`
- `data(value)` and `grad(value)`, with exported `value.data`, `value.grad`,
  and `value.requires_grad` fields
- `add(left, right)` or `left.add(right)`
- `sub(left, right)` or `left.sub(right)`
- `mul(left, right)` or `left.mul(right)`
- `div(left, right)` or `left.div(right)`
- `neg(value)`, `pow(value, exponent)`, `relu(value)`
- `tanh(value)`, `exp(value)`, `ln(value)`, `sqrt(value)`
- `backward(value)` or `value.backward()`
- `zero_grad(value)` or `value.zero_grad()`

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

Records group named fields and can have receiver methods:

```dn
record Point {
  x: real64,
  y: real64,

  sum(): real64 {
    return this.x + this.y;
  }
}

point: Point = Point { x: 1.5, y: 2.5 };
print(point.sum());
```

Functions can be overloaded by parameter types:

```dn
show(value: int): int {
  return value + 1;
}

show(value: bool): int {
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
`integer`, `numeric`, `real`, `comparable`, or `ordered`.

```dn
identity<T>(value: T): T {
  return value;
}

square<T is numeric>(value: T): T {
  return value * value;
}
```

Records can be generic too. A record literal uses the expected type from the
left-hand side or function return type when filling in type arguments.

```dn
record Box<T> {
  value: T,
}

boxed<T>(value: T): Box<T> {
  return Box { value: value };
}

answer: Box<int> = boxed(42);
```

Records can declare lightweight constructors and explicitly implement contracts.
Constructors are statically checked functions associated with the record type;
they must return the enclosing record type and are called with `Record.new(...)`.
Contracts declare method requirements only. A record satisfies a contract bound
only when its declaration lists that contract in a `with` clause.

```dn
contract Shape {
  area(): real64;
}

record Circle with Shape {
  radius: real64,

  new(radius: real64): Circle {
    return Circle { radius: radius };
  }

  area(): real64 {
    return 3.0 * this.radius * this.radius;
  }
}

area_of<T is Shape>(shape: T): real64 {
  return shape.area();
}

circle: Circle = Circle.new(2.0);
print(area_of(circle));
```

When records come from modules, member visibility is explicit:

```dn
export record Counter {
  value: int,

  export new(): Counter {
    return Counter { value: 0 };
  }

  export inc(): unit {
    this.value = this.value + 1;
  }

  export current(): int {
    return this.value;
  }
}
```

After `import counters;`, callers can use `counters.Counter.new()`,
`counter.inc()`, and `counter.current()`, but `counter.value` is rejected unless
the field is exported. Contracts can also be exported and used as qualified
generic bounds, for example `T is geometry.Shape`. Contract-bound calls are still
statically dispatched through generic instantiation; there is no `dyn`, vtable,
inheritance, or heterogeneous contract-object array support yet.

Blocks, loop initializers, and `when` payloads have lexical scopes. A typed
binding such as `x: int = 2` creates a new binding in the current scope and can
shadow a mutable outer binding. Plain `x = 2` updates the nearest visible `x`, or
creates one in the current scope if none exists. Constants cannot be reassigned
or accidentally shadowed. Assignments to scalar values copy the value.

```dn
x = 1;

{
  x: int = 2;
  print(x);
}

print(x);
```

Arrays and records are reference values. Assigning or passing one copies a handle
to the same storage, so mutation through one mutable alias is visible through
another alias. `const` is a binding-level promise: it prevents rebinding a name
and prevents mutation through that constant name, such as `values[0] = 2`, but it
does not make the underlying array or record deeply immutable through other
aliases. Use `array.copy(values)` when you need a separate array.

```dn
import array;

values: [int] = [1];
alias = values;
alias[0] = 2;
print(values[0]);

separate = array.copy(values);
separate[0] = 9;
print(values[0]);
```

Choices model values that can be one of several variants. Variants can either be
empty or carry one payload value, and generic choice payloads are substituted from
the expected choice type.

```dn
choice Maybe<T> {
  Present(T),
  Absent,
}

value: Maybe<int> = Present(42);

answer = when value {
  is Present(x) { x }
  is Absent { 0 }
};
```

Receiver methods can be written with `method`. The receiver is available inside
the method body as `this`; exported methods can also be called as module
functions after import.

```dn
export method<T> [T].first(): T {
  return this[0];
}
```

`when` expressions compare a subject against literal patterns or choice variant
patterns. Literal matches require a `_` fallback arm. Choice matches must cover
every variant, or include `_` as a fallback. A payload variant pattern binds the
payload only inside that arm.

```dn
label = when answer.value {
  is 42 { "answer" }
  is _ { "other" }
};

unwrapped = when value {
  is Present(x) { x }
  is Absent { 0 }
};
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
- `record` records with named fields, for example `Point { x: 1, y: 2 }`
- generic records, for example `Box<int>`
- contracts, for example `Shape` as a generic bound
- generic choices, for example `Maybe<int>` or `Outcome<int, text>`
- choice variants, for example `Present(42)` or `Absent`

Indexing and slicing:

- arrays are zero-based: `values[0]` is the first element
- arrays: `values[index]`, `values[start:end]`, `values[:end]`, `values[start:]`
- text is zero-based: `message[0]` returns the first `glyph`
- text slices return `text`
- array slots and record fields can be assigned directly: `values[0] = 9`, `point.x = 7`, `points[0].x = 5`
- nested assignment targets are supported for arrays of arrays and arrays of records
- native output checks array/text indexes, slices, and empty `pop()` calls at runtime
- arrays and records alias on assignment; scalars copy on assignment

Comments:

- `//` starts a single-line comment

Built-in receiver methods:

- arrays: `len()`, `push(value)`, `pop()`, `clear()`, `is_empty()`
- text: `len()`, `is_empty()`, `contains(needle)`, `starts_with(prefix)`

Standard library receiver methods are enabled by importing their module:

- `import array;` enables helpers such as `values.first()` and `values.reverse()`
- `import text;` enables helpers such as `message.trim()` and `message.ends_with("x")`
- `import maybe;` exposes choice `Maybe<T>`, `present(value)`, `absent(default)`, and `value_or()`
- `import outcome;` exposes choice `Outcome<T, E>`, `done(value, error_default)`, `failed(value_default, error)`, and `failure_or()`
- `import collections;` exposes small array builders such as `pair_int()` and `repeat_int()`

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

Check a file without running it:

```bash
./build/dune check hello.dn
```

Start the editor language server:

```bash
./build/dune lsp
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

## Zed

Dune has a Zed extension in `editors/zed`. It provides Tree-sitter syntax
highlighting for `.dn` files and compiler diagnostics through `dune lsp`.

To try it locally:

1. Build Dune:

```bash
cmake -S . -B build
cmake --build build -j
```

2. In Zed, use `Extensions: Install Dev Extension` and select `editors/zed`.

The extension uses `dune` from `PATH` when available. Otherwise it falls back to
`build/dune` inside the opened worktree and sets `DUNE_STDLIB_PATH` to the
worktree `stdlib` directory, so diagnostics can resolve standard modules.

## Current Features

The current release implements a small compiled language with:

- lexer
- parser
- AST
- arithmetic
- inferred bindings through first assignment with `=`
- lexical block, loop, and `when` payload scopes
- constants
- single-line comments
- unary operators
- logical operators
- modulo
- explicit casts with `to`
- typed functions
- overloaded functions
- generic functions with basic bounds
- generic functions with explicit contract bounds
- call-site generic instantiation
- generic records
- record constructors
- record member visibility
- contracts and explicit record `with` declarations
- choices
- receiver methods
- static scalar types
- booleans
- signed and unsigned integer widths
- floating point values
- glyph and text values
- unit-returning functions
- dynamic arrays
- records with fields and methods
- mutable array indexes and record fields
- `when` expressions with literal and choice variant patterns
- array methods
- text methods
- standard library receiver methods
- text indexing
- slices
- imports
- export visibility
- foreign functions
- standard library modules
- `dune check`
- `dune lsp`
- Zed syntax highlighting
- native heap cleanup on normal program exit and runtime panic paths
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
