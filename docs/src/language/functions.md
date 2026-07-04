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

## Function values and method chaining

A named function can be passed by name as a first-class value wherever a function
type `fn(T): U` is expected. This powers the higher-order array pipeline.

```dn
import array;

fn is_positive(value: int): bool { return value > 0; }
fn square(value: int): int { return value * value; }

values: [int] = [-2, 3, -1, 4];
result = values.filter(is_positive).map(square).sum();
print(result);   // 25
```

`import array;` supplies `map(fn(T): U)`, `filter(fn(T): bool)`,
`reduce`/`fold`, `any`, `all`, and `count_where`. Function values run on the
bytecode VM.

## Foreign functions

`foreign fn` binds a Dune signature to a native C symbol. These are used sparingly
(the standard library restricts them to a single sanctioned primitive).

```dn
foreign fn c_sqrt(value: real64): real64 = "sqrt";

print(c_sqrt(81.0));   // 9
```
