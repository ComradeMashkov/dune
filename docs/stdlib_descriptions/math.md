`math` is a pure-Dune numeric module. It exposes real constants such as `PI`, `TAU`, and `E`, generic helpers such as `square`, `cube`, `abs`, `min`, `max`, and `clamp`, and elementary real functions implemented with series expansion, Newton iteration, and range reduction.

Use it when you need portable numeric behavior in both the VM and native backend. The functions are intentionally small and deterministic; they do not call a native math library.

```dn
import math;

print(math.square(7));
print(math.clamp(15, 0, 10));
print(math.sqrt(81.0));
print(math.round(math.PI));
```
