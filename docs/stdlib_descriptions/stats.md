The module is designed for end-to-end data and notebook workflows rather than
only scalar reductions. All implementation code is ordinary Dune and runs
through the canonical bytecode VM; it adds no native primitive or dependency.

## API conventions

- Functions accept numeric arrays (`[int]`, `[real32]`, `[real64]`, and other
  numeric element types) and accumulate in `real64`. Core operations have
  matching `matrix.Vector<T>` overloads.
- Partial operations return `outcome.Outcome<value, text>`. Empty data, invalid
  probabilities, mismatched shapes, constant correlation/regression inputs,
  negative weights, and invalid distribution parameters are explicit failures.
- `variance` and `stddev` are sample estimators (denominator `n-1`), while
  `pvariance` and `pstdev` are population estimators (denominator `n`). This
  mirrors Python `statistics` and Julia's corrected estimator terminology.
- `quantile(data, q)` uses `q` in `[0, 1]`; `percentile(data, p)` uses `p` in
  `[0, 100]`. The default is Hyndman-Fan Type 7 linear interpolation, the common
  default in R, NumPy, and Julia. `quantile_with` also exposes `lower`, `higher`,
  `nearest`, and `midpoint` interpolation.
- Inputs are copied before sorting. A caller's array or vector is never reordered
  as a side effect.

These choices are informed by the public APIs of
[Python `statistics`](https://docs.python.org/3/library/statistics.html),
[NumPy statistics](https://numpy.org/doc/stable/reference/routines.statistics.html),
[SciPy `stats`](https://docs.scipy.org/doc/scipy/reference/stats.html), and
[Julia Statistics](https://docs.julialang.org/en/v1/stdlib/Statistics/).

## Feature groups

- Descriptive: compensated sum, mean, extrema/range, population/sample
  variance and standard deviation, standard error, RMS, geometric/harmonic
  means, five-number summary, Type-7 quantiles, modes/frequencies, percentile
  rank, MAD, trimmed/winsorized means, skewness, kurtosis, and a comprehensive
  `Summary` record.
- Weighted: mean, population/sample variance and standard deviation, and
  inverse-empirical-CDF weighted quantiles with non-negative weights.
- Relationships and models: population/sample covariance, Pearson, Spearman,
  Kendall tau-b, autocorrelation, MAE/MSE/RMSE/R², and ordinary least squares
  with predictions and regression diagnostics.
- Binning and series: equal/custom-edge histograms with density and
  under/overflow accounting, `digitize`, `bincount`, cumulative values, moving
  mean/variance/stddev, exponential moving average, z-scores, and min-max
  scaling.
- Probability: normal PDF/CDF/quantiles, normal-approximation mean confidence
  intervals, uniform and exponential PDF/CDF, and Bernoulli/binomial/Poisson
  mass functions.

## Dune module integration

`matrix.Vector<T>` works with the same names as arrays. For observation
matrices (rows = observations, columns = variables), use `describe_columns`,
`column_means`, `column_variances`, `covariance_matrix`, `correlation_matrix`,
`standardize_columns`, or `linear_regression_columns`. This layout connects
directly to the value returned by `csv.read_matrix_real64`.

`stats.histogram` returns a reusable `Histogram`; pass it to
`plot.histogram(histogram)` to chart the validated counts without recomputing
bins. See `examples/statistical_analysis.dn` and
`examples/notebooks/statistical_analysis.dnb` for complete script and notebook
workflows using `matrix`, `random`, `stats`, and `plot` together.

## Numerical behavior and limits

Sums use Neumaier compensation. Variance and covariance use stable online
recurrences. Quantile sorting is a deterministic insertion sort, currently
`O(n²)` because Dune has no native sorting primitive; rank and Kendall
operations are also `O(n²)`. These are appropriate for current notebook-sized
data but should be replaced by a pure-Dune `O(n log n)` sorting implementation
before treating very large arrays as a primary use case.

Probability functions use deterministic pure-Dune approximations. The normal
CDF is accurate to roughly `1e-7`; the inverse normal CDF uses Acklam's rational
approximation. Mean confidence intervals use a normal critical value, not a
Student-t correction. Inputs are expected to be finite `real64` values because
the language does not yet expose a standard NaN/missing-data policy.
