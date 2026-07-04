# Compile-time evaluation (`foreknown`)

A declaration marked `foreknown` is evaluated **at compile time**. The value it
produces is folded into the bytecode before the program starts running, so there
is no runtime cost for the computation — only for using the result.

`foreknown` can be applied to both constants and functions.

## Foreknown constants

A `foreknown const` is computed once, during compilation:

```dn
foreknown const KB: int = 1024;
foreknown const PAGE: int = KB * 4;   // 4096, folded at compile time
```

Foreknown constants may only appear at the top level of a file.

## Foreknown functions

A `foreknown fn` is an ordinary function that is *also* allowed to run during
compilation. It can be called to initialise a `foreknown const`:

```dn
foreknown fn factorial(n: int): int {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}

foreknown const FACT5: int = factorial(5);   // 120, computed by the compiler
```

Foreknown functions support the usual control flow — `if`/`else`, `while`,
C-style `for`, `break`, `continue`, `return` — and local bindings:

```dn
foreknown fn sum_to(n: int): int {
    total: int = 0;
    for i: int = 0; i <= n; i = i + 1 {
        total = total + i;
    }
    return total;
}

foreknown const TRIANGLE: int = sum_to(10);   // 55
```

A foreknown function remains a normal function too: it can still be called at
runtime like any other.

## Rules and limitations

Because foreknown code runs inside the compiler, it must be pure and
self-contained. The type checker rejects a foreknown declaration that:

- calls a function that is **not** itself `foreknown`;
- performs I/O (`io.print`, `io.println`, …) or string formatting (`fmt.format`);
- uses the `?` try operator or the `in` membership operator;
- reads module members other than foreknown constants;
- builds or indexes aggregates (arrays, tuples, records, comprehensions);
- mutates aggregate values, or assigns to anything but a local variable.

In addition, a foreknown function may not be `foreign`, and generic foreknown
functions are not supported yet.

These restrictions guarantee that the compiler can fully evaluate the
declaration and bake the result into the program.
