`collections` provides a few small constructors for common array shapes. It is intentionally narrow: one- and two-element arrays for `int` and `text`, plus a simple `repeat_int` helper.

Use these helpers in tests, examples, and small programs when named construction is clearer than spelling out an array literal or a loop.

```dn
import collections;

pair = collections.pair_int(2, 3);
words = collections.singleton_text("dune");
repeated = collections.repeat_int(4, 3);

print(pair[0] + pair[1]);
print(words[0]);
print(repeated.len());
```
