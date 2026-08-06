# Syntax basics

## Bindings

A binding introduces a variable. The type can be inferred from the initializer
or written explicitly after a colon. Bindings are mutable — assign to them again
with `=`.

```dn
x = 40 + 2;            // inferred as int
name: text = "Dune";   // explicit type
x = x - 1;             // reassignment
```

Use `const` when the name itself must not be reassigned or shadowed. `const`
does not recursively freeze an array or record; see
[Values, copying, and mutation](value-semantics.md#const-freezes-a-binding-not-an-object).

```dn
const answer: int = 42;
const values: [int] = [1];
values.push(2); // valid: values still names the same mutable array
```

Statements end with a semicolon. Blocks are delimited with `{ }`.

## Numeric literals

Integer literals support `_` separators, `0x` hex, `0b` binary, and explicit
integer suffixes such as `i32`, `i64`, `u8`, `u64`, and `usize`.

```dn
size = 1_000_000;
mask = 0xffu64;
bits = 0b1010_0101u8;
wide = 123i64;
rough: real64 = 1_000.5_25;
```

## Printing and formatting

`print(expression)` prints a value. `print` also accepts a string literal with
positional `{}` placeholders followed by arguments; the same formatting is
available as the `format(...)` expression, which returns `text`.

```dn
name: text = "Dune";
version: int = 1;

print(name);
print("{} v{}", name, version);
message: text = format("{} v{}", name, version);
```

The format string must be a literal, placeholders are plain `{}`, and the number
of placeholders must match the number of arguments. Printable values are the
scalar types (integers, `real32`/`real64`, `bool`, `glyph`, `text`) and any
record that provides a `to_text(): text` method — see the
[Display contract](records.md#the-display-contract).

## Text and glyph literals

Normal `text` literals decode `\n`, `\t`, `\r`, `\\`, `\"`, and `\0`; `glyph`
literals decode `\n`, `\t`, `\r`, `\\`, `\'`, and `\0`. Unknown escapes are
compile-time errors. Raw single-line text literals use `r"..."` and keep
backslashes literally.

```dn
path: text = r"C:\Users\name\data.csv";
line: text = "hello\n";
tab: glyph = '\t';
```
