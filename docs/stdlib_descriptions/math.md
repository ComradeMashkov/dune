`math` is a pure-Dune numeric module. It exposes real constants such as `PI`, `TAU`, and `E`, generic helpers such as `square`, `cube`, `abs`, `min`, `max`, and `clamp`, and elementary real functions implemented with series expansion, Newton iteration, and range reduction.

Use it when you need portable, deterministic numeric behavior. The functions are intentionally small and self-contained; they do not call a native math library.

```dn
import io;
import math;

io.println(math.square(7));
io.println(math.clamp(15, 0, 10));
io.println(math.sqrt(81.0));
io.println(math.round(math.PI));
```
