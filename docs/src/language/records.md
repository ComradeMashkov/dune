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

## Visibility across modules

Record fields and methods are private across module boundaries unless the member
is marked `export`. See [Modules](modules.md).
