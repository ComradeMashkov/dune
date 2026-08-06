# Functions and generics

## Functions

Functions are declared with `fn`, take typed parameters, and declare a return
type after the parameter list.

```dn
fn add(a: int, b: int): int {
    return a + b;
}

total: int = add(10, 20);
```

Arguments and return values follow Dune's uniform value semantics: scalars are
independent values, while arrays and records are shared handles. Parameter
reassignment stays local, but aggregate mutation is visible to the caller. See
[Values, copying, and mutation](value-semantics.md#function-arguments).

Functions may be **overloaded** by the number and types of their parameters; the
type checker selects the matching definition at each call site.

## Generics and bounds

Type parameters go in angle brackets. A parameter may carry a bound that
constrains which types satisfy it.

```dn
fn square<T is numeric>(value: T): T {
    return value * value;
}
```

Available bounds:

- `integer` — the integer types.
- `numeric` — integers and reals.
- `comparable` — supports `==` / `!=`.
- `ordered` — supports `<`, `<=`, `>`, `>=`.

A contract name is also a valid bound: `T is Display` requires `T` to implement
the [`Display`](../stdlib/display.md) contract.

### Multiple bounds

A single type parameter can carry several bounds. Group them with `+` under one
`is`, or repeat `is` for the same parameter — both mean "every listed
constraint must hold":

```dn
// Grouped: T must be both ordered (for `<`) and comparable (for `==`).
fn spans<T is ordered + comparable>(low: T, value: T, high: T): bool {
    return (low < value) == (value == high);
}

// Repeated form is equivalent.
fn spans2<T is ordered, T is comparable>(low: T, value: T, high: T): bool {
    return (low < value) == (value == high);
}
```

When an argument fails a bound the diagnostic names the specific unmet
constraint, e.g. `type 'text' does not satisfy bound 'numeric' on 'T'`.

### Const generics and static shapes

A generic argument can also be a **positive integer literal** instead of a type.
The [`matrix`](../stdlib/matrix.md) module uses this to carry the *shape* of a
`Matrix` or `Vector` in its type, so shape mistakes become compile-time errors
instead of runtime panics:

```dn
import matrix;

// A 3x3 matrix and a length-3 vector, spelled in the type.
a: matrix.Matrix<real64, 3, 3> = matrix.identity(3);
v: matrix.Vector<real64, 3> = matrix.vector([1.0, 2.0, 3.0]);

// The product's shape is checked and flows into the result type.
r: matrix.Vector<real64, 3> = a.mul_vector(v);
```

`Matrix<T, Rows, Cols>` takes two dimensions; `Vector<T, Len>` takes one. The
type checker verifies shapes for the core operations — vector `dot`, matrix
`add`/`sub`/`mul`/`div` (element-wise, same shape), matrix `matmul`/`dot`, and
matrix–vector `mul_vector`/`dot` — and rejects incompatible ones:

```dn
m: matrix.Matrix<real64, 3, 3> = matrix.identity(3);
w: matrix.Vector<real64, 4> = matrix.vector([1.0, 2.0, 3.0, 4.0]);
bad = m.mul_vector(w);
// error: matrix-vector shape mismatch: matrix.Matrix<real, 3, 3> has 3 column(s)
//        but matrix.Vector<real, 4> has length 4
```

Static shapes are **optional and coexist with the dynamic API**: a plain
`Matrix<real64>` (no dimensions) matches any shape, so existing code keeps
working and a dynamic value is assignable to and from a statically-shaped
binding. Shapes are a type-check-time concern only — the runtime representation
is unchanged.

Phase 1 accepts integer *literals* only. Named const parameters, const
expressions (`N + 1`), and shape inference from array literals are planned
follow-ups (see issue #43).

## Function values

Function types use `fn(P1, P2): R`. Named functions and lambdas are ordinary
values: they can be stored in bindings and aggregates, passed, returned, copied,
and invoked through any function-valued expression.

```dn
import array;

fn is_positive(value: int): bool { return value > 0; }
fn square(value: int): int { return value * value; }

values: [int] = [-2, 3, -1, 4];
result = values.filter(is_positive).map(square).sum();
```

An overloaded named function needs an expected function type so the checker can
select one signature. Generic named functions are monomorphized at call sites;
they cannot be stored by bare name without concrete type arguments.

## Lambdas

A lambda starts with `fn` but has no name:

```dn
square = fn(value: int): int {
    value * value
};

result: int = square(6); // 36
```

The body is a full function body. It supports local bindings, loops,
conditionals, `when`, early `return`, and a final tail expression. A `unit`
lambda can use `return;`.

When the target has a function type, omitted annotations are inferred from that
context:

```dn
increment: fn(int): int = fn(value) { value + 1 };
```

Without a contextual function type, omitted parameter and result annotations use
the same `int` defaults as named functions. Explicit annotations are recommended
at public or non-obvious boundaries because diagnostics then show the intended
signature directly.

Lambdas do not declare their own generic parameter list. They may, however,
appear inside a generic named function; monomorphization substitutes the
concrete types through the lambda and its capture environment:

```dn
fn remember<T>(value: T): fn(): T {
    fn(): T { value }
}

answer = remember(42);
label = remember("Dune");
```

## Closures and captures

A lambda becomes a closure when it references a binding outside its own body.
Captures follow Dune's ordinary value semantics and are evaluated once when the
closure is created:

- scalars, text, and callable bindings are snapshots;
- arrays and records copy their shared handles, so aggregate mutation remains
  visible through every alias;
- rebinding the original variable later does not change an existing closure;
- a captured name cannot be reassigned inside the closure;
- nested closures forward any outer values needed by their own children.

```dn
factor: int = 10;
scale = fn(value: int): int { value * factor };
factor = 20;
scale(4); // 40: the closure captured 10

items = [1];
append = fn(value: int): unit {
    items.push(value); // aggregate contents may change
    return;
};
append(2); // the outer array is now [1, 2]
```

Factory calls create independent environments, so closures can safely outlive
the function invocation that created them:

```dn
fn make_adder(base: int): fn(int): int {
    return fn(value: int): int { base + value };
}

add_two = make_adder(2);
add_ten = make_adder(10);
```

## Calling function-producing expressions

Any expression with a function type is callable. Parenthesized lambdas can be
invoked immediately, and calls can be chained when a function returns another
function:

```dn
answer = (fn(value: int): int { value + 1 })(41);
same = make_adder(40)(2);

callbacks: [fn(): int] = [fn(): int { 40 }, fn(): int { 2 }];
also = callbacks[0]() + callbacks[1]();
```

The checker reports the complete expected and actual `fn(...)` signatures for
argument, arity, and return mismatches. Calling a non-function value is rejected
before bytecode generation.

## Standard-library integration

`import array;` supplies `map(fn(T): U)`, `filter(fn(T): bool)`,
`reduce`/`fold`, `any`, `all`, and `count_where`. Named functions and capturing
lambdas use the same callback path:

```dn
offset = 3;
shifted = [1, 2, 3].map(fn(value: int): int { value + offset });
```

Function values and closures run on Dune's canonical bytecode VM; there is no
separate backend or native-only closure behavior.

## Foreign functions

`foreign fn` binds a Dune signature to a native C symbol. These are used sparingly
(the standard library restricts them to a single sanctioned primitive).

```dn
foreign fn c_sqrt(value: real64): real64 = "sqrt";

print(c_sqrt(81.0));   // 9
```
