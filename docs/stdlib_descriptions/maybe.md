`maybe` defines `Maybe<T>`, the standard optional value type. A value is either `Present(T)` or `Absent`, which makes missing data explicit without choosing a sentinel value such as `0` or an empty string.

Use `Maybe<T>` for lookups and operations that may not return a value. Constructors create present or absent values, while `has_value`, `is_absent`, and `value_or` cover the common checks and fallback path.

```dn
import maybe;

found: maybe.Maybe<int> = maybe.present(42);
missing: maybe.Maybe<int> = maybe.absent(0);

print(found.has_value());
print(found.value_or(0));
print(missing.is_absent());
print(missing.value_or(7));
```
