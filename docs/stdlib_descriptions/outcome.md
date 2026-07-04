`outcome` defines `Outcome<T, E>`, the standard success-or-failure type. `Done(T)` carries a successful value and `Failed(E)` carries an error value; the `?` operator understands this type and short-circuits failures from functions that also return an `Outcome`.

Use it for recoverable errors such as parsing, file I/O, and validation. The specialized helpers for `int` and `text` reduce boilerplate in common cases, and `value_or` and `failure_or` make simple fallback handling concise.

```dn
import outcome;

fn checked(value: int): outcome.Outcome<int, text> {
    if value > 0 {
        return outcome.done_int(value);
    }

    return outcome.failed_int("not positive");
}

fn doubled(value: int): outcome.Outcome<int, text> {
    number: int = checked(value)?;
    return outcome.done_int(number * 2);
}

print(doubled(21).value_or(0));
print(doubled(0).failure_or("ok"));
```
