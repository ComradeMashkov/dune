# Records, methods, and contracts

## Records

A record groups named fields. It can also declare methods, which receive the
instance as `this`.

```dn
record Point {
    x: int,
    y: int,

    fn magnitude_squared(): int {
        return this.x * this.x + this.y * this.y;
    }
}

p: Point = Point { x: 3, y: 4 };
print(p.magnitude_squared());   // 25
```

A `static fn` belongs to the record rather than an instance and is often used as
a constructor:

```dn
record Counter {
    value: int,

    static fn zero(): Counter { return Counter { value: 0 }; }
}
```

## Derive

`derive` asks the compiler to generate common methods from the fields:

- `eq` generates `equals` (structural equality).
- `copy` generates `copy` (a deep copy).
- `debug` generates `to_text` (a debug rendering).

```dn
record Vec2 derive eq, copy {
    x: int,
    y: int,
}
```

## The Display contract

A record is printable when it provides a `to_text(): text` method.
`print(record)` and `format("{}", record)` call it.

```dn
record Point {
    x: int,
    y: int,

    fn to_text(): text {
        return format("({}, {})", this.x, this.y);
    }
}

print(Point { x: 1, y: 2 });   // (1, 2)
```

`import display;` provides a matching `Display` contract (so a record can declare
`with display.Display`) and a `show(value)` helper.

## Contracts

A `contract` names a set of method signatures a record can promise to implement
with the `with` clause. This gives generic code a way to require behavior.

```dn
contract Display {
    fn to_text(): text;
}

record Tag with Display {
    name: text,
    fn to_text(): text { return this.name; }
}
```

## Operator overloading

When the left operand of `+`, `-`, `*`, or `/` is a record, the operator
dispatches to a conventionally-named method on that record:

| Operator | Method  |
| -------- | ------- |
| `a + b`  | `a.add(b)` |
| `a - b`  | `a.sub(b)` |
| `a * b`  | `a.mul(b)` |
| `a / b`  | `a.div(b)` |

The method is resolved with the normal overload rules, so the right operand can
be another record or a scalar, and the result type is whatever the method
returns:

```dn
record Vec2 {
    x: int,
    y: int,
    fn add(other: Vec2): Vec2 { return Vec2 { x: this.x + other.x, y: this.y + other.y }; }
    fn mul(factor: int): Vec2 { return Vec2 { x: this.x * factor, y: this.y * factor }; }
}

a: Vec2 = Vec2 { x: 1, y: 2 };
b: Vec2 = Vec2 { x: 3, y: 4 };

sum: Vec2 = a + b;      // Vec2 { x: 4, y: 6 }
scaled: Vec2 = a * 10;  // Vec2 { x: 10, y: 20 }
```

This is how the [`matrix`](../stdlib/matrix.md) module's vectors and matrices get
natural arithmetic — `v + w`, `v * scalar`, and element-wise `v * w` all map to
the corresponding `Vector`/`Matrix` methods. Applying an operator to a record
that lacks the matching method is a compile-time error
(`operator '+' is not defined for type 'Point'`). Matrix multiplication stays
explicit via `.matmul(...)` / `.dot(...)` to avoid ambiguity with element-wise
`*`.

## Visibility across modules

Record fields and methods are private across module boundaries unless the member
is marked `export`. See [Modules](modules.md).
