# Choices, `when`, and `?`

## Choices

A `choice` is a tagged union: a value is exactly one of its variants, and a
variant may carry a payload.

```dn
choice Shape {
    Circle(real64),
    Rectangle,
}

s: Shape = Circle(2.0);
```

## `when` expressions

`when` matches a value against patterns and produces a result. It has two forms:
a literal/wildcard form and a variant-binding form.

```dn
// literal / wildcard
label = when value {
    is 1 { "one" }
    is _ { "many" }
};

// bind a variant payload
area = when s {
    Circle(radius) => 3.14159 * radius * radius;
    Rectangle => 0.0;
};
```

## Optional and result: `maybe` and `outcome`

The standard library builds two common choices on top of this machinery:

- [`maybe`](../stdlib/maybe.md) — `Maybe<T>` with `present(value)`,
  `absent(default)`, and `value_or()`.
- [`outcome`](../stdlib/outcome.md) — `Outcome<T, E>` with `done`, `failed`, and
  `failure_or()`.

## The `?` operator

The postfix `?` operator propagates the "empty" or "error" case of a `Maybe` or
`Outcome` out of the enclosing function, returning early, and otherwise unwraps
the contained value. It binds tighter than binary operators.

```dn
import outcome;

fn total(): outcome.Outcome<int, text> {
    a = read_number()?;   // returns early on failure
    b = read_number()?;
    return outcome.done(a + b, "");
}
```

## Printing choices

Choices print by default — no boilerplate. `io.println(value)` and
`fmt.format("{}", value)` render a choice as its variant name, plus the payload
in parentheses when the variant carries one:

```dn
import io;

choice Shape { Circle(int), Named(text), Empty }

a: Shape = Circle(5);
io.println(a);      // Circle(5)
io.println(Empty);  // Empty
```

This works when every variant payload is a scalar or `text` (including generic
choices instantiated with such types, like `Maybe<int>`). A choice whose variant
carries a record, array, or tuple is not printable by default — give the payload
type its own `to_text` rendering or format the fields explicitly. See
[Display](../stdlib/display.md).
