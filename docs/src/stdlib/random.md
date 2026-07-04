# `random`

A small deterministic pseudo-random generator.

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

> Auto-generated from `stdlib/random.dn` by `tools/gen_stdlib_docs.py`.

### `record Random`

Deterministic, seedable pseudo-random numbers.

`Random` uses the Park-Miller "minimal standard" generator
(state = 16807 * state mod 2147483647). The multiplication never
overflows a 64-bit integer, so the sequence is identical on every
backend for a given seed.

Method names avoid the reserved type names `int`/`real64`, so the
raw-uniform helpers are `next_int`/`next_real` and the bounded
integer helper is `between`.

**Methods:**

- `fn new(seed: int): Random` — Build a generator from `seed`, normalising it into the valid range.
- `fn next_int(): int` — Advance the generator and return the raw state in [1, 2147483646].
- `fn next_real(): real64` — Uniform real64 in [0.0, 1.0). Never returns exactly 0.0 or 1.0.
- `fn between(lo: int, hi: int): int` — Uniform integer in [lo, hi); `hi` is exclusive.
- `fn real_between(lo: real64, hi: real64): real64` — Uniform real64 in [lo, hi).
- `fn normal(mean: real64, stddev: real64): real64` — One sample from a normal distribution via the Box-Muller transform.

### `fn seed(value: int): Random`

Convenience constructor matching `random.seed(42)`.

### `fn uniform(rng: Random, count: int, lo: real64, hi: real64): [real64]`

`count` uniform real64 values in [lo, hi).

### `fn normal(rng: Random, count: int, mean: real64, stddev: real64): [real64]`

`count` samples from a normal distribution.
