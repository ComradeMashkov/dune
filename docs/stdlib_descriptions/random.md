`random` provides deterministic pseudo-random numbers from a seedable `Random` record. It uses the Park-Miller minimal standard generator, so a given seed produces the same sequence across backends.

Use `next_int` and `next_real` for raw draws, `between` and `real_between` for bounded values, and `uniform` or `normal` when you want arrays of samples. The generator is mutable, so repeated method calls advance its state.

```dn
import random;

rng: random.Random = random.seed(42);

print(rng.next_int());
print(rng.between(1, 7));

samples = random.uniform(random.seed(7), 3, 0.0, 1.0);
print(samples.len());
```
