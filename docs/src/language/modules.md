# Modules

Modules are loaded from `.dn` files. The standard library is a set of such
modules (see the [Standard library](../stdlib/index.md) section).

## Importing

There are three import forms, and they interoperate freely:

```dn
import math;                          // plain: use as `math.square`
import matrix as m;                   // alias: use as `m.Vector`
from matrix import Vector, Matrix;    // selective: use `Vector` unqualified
```

Selective imports are comma-separated and may span several lines. Importing an
unknown symbol, a private symbol, or reusing an alias that collides with another
module is a compile-time error that names the offending symbol.

## Module declaration

A file may open with a `module name;` declaration that names the unit for
documentation and diagnostics. It is optional and, in this first version, does
not bind the file to a directory layout — modules are located by file name on the
search path.

```dn
module geometry;

export record Point {
    export x: int,
    export y: int,
}

export fn manhattan(a: Point, b: Point): int { /* ... */ }
```

## Export visibility

If a module contains any explicit `export`, only exported functions, constants,
records, choices, and contracts are visible through `module.name`; everything
else stays private to the module. Record fields and methods are private across
module boundaries unless the member itself is marked `export`.

```dn
export const ANSWER: int = 42;

fn hidden(): int { return 7; }         // private to this module

export fn public(): int { return hidden(); }
```

Receiver methods declared by a module become available on values of the receiver
type after import — for example, `import array;` enables both `array.first(xs)`
and `xs.first()`.
