# `stats`

Descriptive statistics, relationships, models, histograms, and probability helpers.

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

> Auto-generated from `stdlib/stats.dn` by `tools/gen_stdlib_docs.py`.

### `record FiveNumberSummary derive eq, copy, debug`

A compact five-number summary, using Type-7 linear quantiles.

**Fields:**

- `minimum: real64`
- `first_quartile: real64`
- `median: real64`
- `third_quartile: real64`
- `maximum: real64`

**Methods:**

- `fn iqr(): real64` — Interquartile range (Q3 - Q1).

### `record Summary derive eq, copy, debug`

A comprehensive summary for a sample containing at least two values.

**Fields:**

- `count: int`
- `sum: real64`
- `mean: real64`
- `minimum: real64`
- `maximum: real64`
- `range: real64`
- `population_variance: real64`
- `sample_variance: real64`
- `population_stddev: real64`
- `sample_stddev: real64`
- `standard_error: real64`
- `median: real64`
- `first_quartile: real64`
- `third_quartile: real64`
- `interquartile_range: real64`
- `skewness: real64`
- `excess_kurtosis: real64`

**Methods:**

- `fn to_text(): text` — A concise human-readable rendering suitable for notebooks.

### `record FrequencyTable derive copy`

Sorted distinct numeric values and their occurrence counts.

**Fields:**

- `values: [real64]`
- `counts: [int]`
- `relative_frequencies: [real64]`
- `total: int`

**Methods:**

- `fn len(): int` — Number of distinct values.

### `record LinearRegression derive eq, copy, debug`

Ordinary least-squares fit for y = slope*x + intercept, together with diagnostics that are commonly needed in analysis notebooks.

**Fields:**

- `slope: real64`
- `intercept: real64`
- `correlation: real64`
- `r_squared: real64`
- `residual_sum_squares: real64`
- `mean_squared_error: real64`
- `root_mean_squared_error: real64`
- `residual_standard_error: real64`
- `sample_count: int`

**Methods:**

- `fn predict(x: real64): real64` — Predict one response.
- `fn predict_all<T is numeric>(xs: [T]): [real64]` — Predict responses for several numeric inputs.

### `record Histogram`

Equal- or variable-width histogram. Intervals are left-closed and right-open, except the final interval also includes its right edge.

**Fields:**

- `edges: [real64]`
- `counts: [int]`
- `densities: [real64]`
- `sample_count: int`
- `included_count: int`
- `underflow: int`
- `overflow: int`

**Methods:**

- `fn len(): int` — Number of bins.
- `fn bin_centers(): [real64]` — Midpoint of each bin, useful as x coordinates for plot.bar.
- `fn counts_as_real64(): [real64]` — Counts converted to real64 for plotting and arithmetic.
- `fn relative_frequencies(): [real64]` — Included counts divided by the number of included observations.
- `fn to_text(): text` — Concise notebook rendering; full data remains available in public fields.

### `record ConfidenceInterval derive eq, copy, debug`

Symmetric confidence interval around an estimate.

**Fields:**

- `estimate: real64`
- `lower: real64`
- `upper: real64`
- `margin: real64`
- `confidence: real64`

### `fn empty_five_number_summary(): FiveNumberSummary`

Return a zero-valued FiveNumberSummary for use with Outcome.value_or.

### `fn empty_summary(): Summary`

Return a zero-valued Summary for use with Outcome.value_or.

### `fn empty_frequency_table(): FrequencyTable`

Return an empty FrequencyTable for use with Outcome.value_or.

### `fn count<T>(values: [T]): int`

Number of observations. Unlike reductions, this is defined for empty data.

**Example:**
```dune
stats.count([1.0, 2.0, 3.0])  // 3
```

### `fn sum<T is numeric>(values: [T]): real64`

Compensated sum in real64. The empty sum is 0.

**Example:**
```dune
stats.sum([1, 2, 3])  // 6
```

### `fn mean<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Arithmetic mean. Empty input returns Failed.

**Example:**
```dune
stats.mean([1.0, 2.0, 3.0]).value_or(0.0)  // 2
```

### `fn minimum<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Smallest value as real64. Empty input returns Failed.

### `fn maximum<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Largest value as real64. Empty input returns Failed.

### `fn data_range<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Difference between the largest and smallest observations.

### `fn midrange<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Midpoint between the largest and smallest observations.

### `fn min<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Conventional alias for minimum.

### `fn max<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Conventional alias for maximum.

### `fn range<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Conventional alias for data_range.

### `fn pvariance<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Population variance (divide by n), computed with Welford's stable one-pass recurrence. A single observation has population variance 0.

**Example:**
```dune
stats.pvariance([1.0, 2.0, 3.0]).value_or(0.0)  // 0.666667
```

### `fn variance<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Sample variance (divide by n-1), matching Python statistics.variance and Julia var(corrected=true). At least two observations are required.

**Example:**
```dune
stats.variance([1.0, 2.0, 3.0]).value_or(0.0)  // 1
```

### `fn population_variance<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Explicitly named alias for pvariance.

### `fn sample_variance<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Explicitly named alias for variance.

### `fn pstdev<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Population standard deviation.

### `fn stddev<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Sample standard deviation. At least two observations are required.

**Example:**
```dune
stats.stddev([1.0, 2.0, 3.0]).value_or(0.0)  // 1
```

### `fn population_stddev<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Explicitly named alias for pstdev.

### `fn sample_stddev<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Explicitly named alias for stddev.

### `fn standard_error<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Standard error of the arithmetic mean, using the sample standard deviation.

### `fn root_mean_square<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Root mean square, useful for signal magnitude.

### `fn geometric_mean<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Geometric mean. All observations must be strictly positive.

### `fn harmonic_mean<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Harmonic mean. All observations must be strictly positive.

### `fn median<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Median with linear averaging for an even-sized sample.

**Example:**
```dune
stats.median([4.0, 1.0, 3.0, 2.0]).value_or(0.0)  // 2.5
```

### `fn median_low<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Lower middle observation (never averages for even-sized input).

### `fn median_high<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Upper middle observation (never averages for even-sized input).

### `fn quantile<T is numeric>(values: [T], probability: real64): outcome.Outcome<real64, text>`

Type-7 linear quantile for probability in [0, 1]. This is the default used by R, NumPy, and Julia Statistics.

**Example:**
```dune
stats.quantile([0.0, 10.0, 20.0], 0.25).value_or(0.0)  // 5
```

### `fn quantile_with<T is numeric>(values: [T], probability: real64, interpolation: text): outcome.Outcome<real64, text>`

Quantile with one of: linear, lower, higher, nearest, midpoint.

### `fn quantiles<T is numeric>(values: [T], probabilities: [real64]): outcome.Outcome<[real64], text>`

Several Type-7 quantiles in one call. Probabilities retain caller order.

### `fn percentile<T is numeric>(values: [T], percent: real64): outcome.Outcome<real64, text>`

Conventional percentile for p in [0, 100], matching NumPy. Use quantile for probabilities in [0, 1].

**Example:**
```dune
stats.percentile([0.0, 10.0, 20.0], 25.0).value_or(0.0)  // 5
```

### `fn five_number_summary<T is numeric>(values: [T]): outcome.Outcome<FiveNumberSummary, text>`

The minimum, quartiles, and maximum. Input is copied before sorting.

### `fn interquartile_range<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Interquartile range (Q3 - Q1).

### `fn median_absolute_deviation<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Median absolute deviation from the sample median.

### `fn mean_absolute_deviation<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Mean absolute deviation from the arithmetic mean.

### `fn trimmed_mean<T is numeric>(values: [T], fraction: real64): outcome.Outcome<real64, text>`

Mean after removing `fraction` from each tail. The fraction must be in [0, 0.5), and at least one observation must remain.

### `fn winsorized_mean<T is numeric>(values: [T], fraction: real64): outcome.Outcome<real64, text>`

Winsorized mean: values in each trimmed tail are replaced with the nearest retained boundary instead of removed.

### `fn frequencies<T is numeric>(values: [T]): outcome.Outcome<FrequencyTable, text>`

Sorted frequency table for numeric data.

### `fn mode<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Smallest mode when several values share the maximum frequency.

### `fn modes<T is numeric>(values: [T]): outcome.Outcome<[real64], text>`

Every mode, sorted ascending.

### `fn percentile_rank<T is numeric>(values: [T], value: real64): outcome.Outcome<real64, text>`

Fraction of observations less than or equal to `value`, in [0, 1].

### `fn population_skewness<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Population skewness (third standardized central moment). Constant data

**Returns:** 0; at least one observation is required.

### `fn skewness<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Bias-corrected sample skewness. At least three observations are required.

### `fn population_excess_kurtosis<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Population excess kurtosis (fourth standardized moment minus 3). Constant data returns 0.

### `fn excess_kurtosis<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Bias-corrected Fisher sample excess kurtosis. At least four observations are required.

### `fn coefficient_of_variation<T is numeric>(values: [T]): outcome.Outcome<real64, text>`

Coefficient of variation: sample standard deviation divided by |mean|.

### `fn describe<T is numeric>(values: [T]): outcome.Outcome<Summary, text>`

Comprehensive descriptive summary. At least two observations are required because the record includes sample variance and standard error. Skewness and kurtosis are reported as 0 when the sample is too small or constant.

**Example:**
```dune
stats.describe([1.0, 2.0, 3.0]).value_or(stats.empty_summary()).mean  // 2
```

### `fn weighted_mean<T is numeric, W is numeric>(values: [T], weights: [W]): outcome.Outcome<real64, text>`

Arithmetic mean with non-negative reliability/frequency weights.

**Example:**
```dune
stats.weighted_mean([10.0, 20.0], [1.0, 3.0]).value_or(0.0)  // 17.5
```

### `fn weighted_pvariance<T is numeric, W is numeric>(values: [T], weights: [W]): outcome.Outcome<real64, text>`

Weighted population variance, dividing by the total weight.

### `fn weighted_variance<T is numeric, W is numeric>(values: [T], weights: [W]): outcome.Outcome<real64, text>`

Unbiased weighted sample variance for reliability weights. The denominator is sum(w) - sum(w^2)/sum(w); it must be positive.

### `fn weighted_pstdev<T is numeric, W is numeric>(values: [T], weights: [W]): outcome.Outcome<real64, text>`

Weighted population standard deviation.

### `fn weighted_stddev<T is numeric, W is numeric>(values: [T], weights: [W]): outcome.Outcome<real64, text>`

Unbiased weighted sample standard deviation.

### `fn weighted_quantile<T is numeric, W is numeric>(values: [T], weights: [W], probability: real64): outcome.Outcome<real64, text>`

Weighted inverse-empirical-CDF quantile. Zero-weight observations do not affect the threshold; q must be in [0, 1].

### `fn pcovariance<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<real64, text>`

Population covariance (divide by n), using a stable online co-moment.

### `fn covariance<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<real64, text>`

Sample covariance (divide by n-1), matching Python statistics.covariance.

**Example:**
```dune
stats.covariance([1.0, 2.0, 3.0], [2.0, 4.0, 6.0]).value_or(0.0)  // 2
```

### `fn population_covariance<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<real64, text>`

Explicitly named alias for population covariance.

### `fn sample_covariance<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<real64, text>`

Explicitly named alias for sample covariance.

### `fn correlation<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<real64, text>`

Pearson product-moment correlation. Constant input is reported explicitly.

**Example:**
```dune
stats.correlation([1.0, 2.0, 3.0], [2.0, 4.0, 6.0]).value_or(0.0)  // 1
```

### `fn pearson_correlation<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<real64, text>`

Explicit alias for Pearson product-moment correlation.

### `fn spearman_correlation<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<real64, text>`

Spearman rank correlation with average ranks for ties.

### `fn kendall_tau<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<real64, text>`

Kendall tau-b rank correlation, correcting the denominator for ties in each input. O(n^2), deterministic, and suitable for notebook-sized data.

### `fn autocorrelation<T is numeric>(values: [T], lag: int): outcome.Outcome<real64, text>`

Correlation between a series and itself shifted by `lag` observations.

### `fn mean_absolute_error<Y is numeric, P is numeric>(observed: [Y], predicted: [P]): outcome.Outcome<real64, text>`

Mean absolute error between observed and predicted values.

### `fn mean_squared_error<Y is numeric, P is numeric>(observed: [Y], predicted: [P]): outcome.Outcome<real64, text>`

Mean squared error between observed and predicted values.

### `fn root_mean_squared_error<Y is numeric, P is numeric>(observed: [Y], predicted: [P]): outcome.Outcome<real64, text>`

Root mean squared error.

### `fn coefficient_of_determination<Y is numeric, P is numeric>(observed: [Y], predicted: [P]): outcome.Outcome<real64, text>`

Coefficient of determination R^2. Constant observed data is rejected.

### `fn linear_regression<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<LinearRegression, text>`

Ordinary least-squares regression with an intercept and diagnostics.

**Example:**
```dune
stats.linear_regression([1.0, 2.0, 3.0], [3.0, 5.0, 7.0]).value_or(stats.empty_regression()).slope  // 2
```

### `fn simple_linear_regression<X is numeric, Y is numeric>(xs: [X], ys: [Y]): outcome.Outcome<LinearRegression, text>`

Explicit alias emphasizing that the model has one predictor.

### `fn empty_regression(): LinearRegression`

Zero-valued regression model for Outcome.value_or.

### `fn empty_histogram(): Histogram`

Empty histogram for Outcome.value_or.

### `fn histogram_with_edges<T is numeric>(values: [T], edges: [real64]): outcome.Outcome<Histogram, text>`

Histogram with caller-supplied, strictly increasing edges. Values below the first edge and above the last edge are counted separately.

### `fn histogram_range<T is numeric>(values: [T], bins: int, lower: real64, upper: real64): outcome.Outcome<Histogram, text>`

Equal-width histogram over an explicit [lower, upper] range.

### `fn histogram<T is numeric>(values: [T], bins: int): outcome.Outcome<Histogram, text>`

Equal-width histogram whose range is inferred from the data. Constant data is centered in a synthetic unit-width range so all bins remain meaningful.

**Example:**
```dune
stats.histogram([1.0, 2.0, 3.0, 4.0], 2).value_or(stats.empty_histogram()).counts  // [2, 2]
```

### `fn digitize<T is numeric>(values: [T], edges: [real64]): outcome.Outcome<[int], text>`

Bin index for each value using histogram edge semantics. Underflow is -1; overflow is bin_count. Exact equality with the final edge lands in the final bin.

### `fn bincount(values: [int], minimum_length: int): outcome.Outcome<[int], text>`

Counts of non-negative integer values, like NumPy bincount. The result has at least `minimum_length` entries.

### `fn cumulative_sum<T is numeric>(values: [T]): [real64]`

Cumulative compensated sum in real64. Empty input returns an empty array.

### `fn cumulative_mean<T is numeric>(values: [T]): [real64]`

Cumulative arithmetic mean after each observation.

### `fn moving_sum<T is numeric>(values: [T], window: int): outcome.Outcome<[real64], text>`

Sliding-window sums, one value for each complete window.

### `fn moving_mean<T is numeric>(values: [T], window: int): outcome.Outcome<[real64], text>`

Sliding-window arithmetic means.

**Example:**
```dune
stats.moving_mean([1.0, 2.0, 3.0, 4.0], 2).value_or([])  // [1.5, 2.5, 3.5]
```

### `fn moving_pvariance<T is numeric>(values: [T], window: int): outcome.Outcome<[real64], text>`

Sliding population variance for each complete window.

### `fn moving_variance<T is numeric>(values: [T], window: int): outcome.Outcome<[real64], text>`

Sliding sample variance for each complete window (window >= 2).

### `fn moving_stddev<T is numeric>(values: [T], window: int): outcome.Outcome<[real64], text>`

Sliding sample standard deviation for each complete window.

### `fn exponential_moving_average<T is numeric>(values: [T], alpha: real64): outcome.Outcome<[real64], text>`

Exponentially weighted moving average. `alpha` is in (0, 1]; the first output equals the first input, and subsequent outputs use alpha*x + (1-alpha)*prev.

### `fn z_scores<T is numeric>(values: [T]): outcome.Outcome<[real64], text>`

Sample z-scores (center by mean, scale by sample standard deviation).

### `fn population_z_scores<T is numeric>(values: [T]): outcome.Outcome<[real64], text>`

Population z-scores (scale by population standard deviation).

### `fn min_max_scale<T is numeric>(values: [T]): outcome.Outcome<[real64], text>`

Scale data linearly into [0, 1]. Constant input is rejected explicitly.

### `fn standard_normal_pdf(value: real64): real64`

Standard-normal probability density.

### `fn standard_normal_cdf(value: real64): real64`

Standard-normal cumulative distribution, approximated to about 1e-7.

### `fn standard_normal_quantile(probability: real64): outcome.Outcome<real64, text>`

Inverse standard-normal CDF using Peter Acklam's rational approximation.

### `fn normal_pdf(value: real64, location: real64, scale: real64): outcome.Outcome<real64, text>`

Normal-distribution density with explicit location and positive scale.

### `fn normal_cdf(value: real64, location: real64, scale: real64): outcome.Outcome<real64, text>`

Normal-distribution cumulative probability.

### `fn normal_quantile(probability: real64, location: real64, scale: real64): outcome.Outcome<real64, text>`

Normal-distribution quantile.

### `fn mean_confidence_interval<T is numeric>(values: [T], confidence: real64): outcome.Outcome<ConfidenceInterval, text>`

Two-sided normal-approximation confidence interval for a sample mean. `confidence` must be strictly between 0 and 1.

### `fn uniform_pdf(value: real64, lower: real64, upper: real64): outcome.Outcome<real64, text>`

Continuous uniform density over [lower, upper].

### `fn uniform_cdf(value: real64, lower: real64, upper: real64): outcome.Outcome<real64, text>`

Continuous uniform cumulative probability.

### `fn exponential_pdf(value: real64, rate: real64): outcome.Outcome<real64, text>`

Exponential density for non-negative values and a positive rate.

### `fn exponential_cdf(value: real64, rate: real64): outcome.Outcome<real64, text>`

Exponential cumulative probability.

### `fn bernoulli_pmf(outcome_value: int, probability: real64): outcome.Outcome<real64, text>`

Bernoulli probability mass for outcome 0 or 1.

### `fn binomial_pmf(k: int, n: int, probability: real64): outcome.Outcome<real64, text>`

Binomial probability mass for k successes in n independent trials.

### `fn poisson_pmf(k: int, rate: real64): outcome.Outcome<real64, text>`

Poisson probability mass for a non-negative count and positive rate.

### `fn poisson_cdf(k: int, rate: real64): outcome.Outcome<real64, text>`

Poisson cumulative probability P(X <= k), evaluated by a stable recurrence.

### `fn empty_vector(): matrix.Vector<real64>`

Empty real vector for Outcome.value_or in vector-oriented workflows.

### `fn empty_matrix(): matrix.Matrix<real64>`

Empty 0x0 real matrix for Outcome.value_or.

### `fn count<T is numeric>(values: matrix.Vector<T>): int`

Vector overloads keep the same names as array functions, so code can move between raw arrays and matrix.Vector without changing its statistical API.

### `fn sum<T is numeric>(values: matrix.Vector<T>): real64`

### `fn mean<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn minimum<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn maximum<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn data_range<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn min<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn max<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn range<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn pvariance<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn variance<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn population_variance<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn sample_variance<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn pstdev<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn stddev<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn population_stddev<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn sample_stddev<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn median<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn quantile<T is numeric>(values: matrix.Vector<T>, probability: real64): outcome.Outcome<real64, text>`

### `fn quantile_with<T is numeric>(values: matrix.Vector<T>, probability: real64, interpolation: text): outcome.Outcome<real64, text>`

### `fn percentile<T is numeric>(values: matrix.Vector<T>, percent: real64): outcome.Outcome<real64, text>`

### `fn five_number_summary<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<FiveNumberSummary, text>`

### `fn interquartile_range<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn median_absolute_deviation<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<real64, text>`

### `fn describe<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<Summary, text>`

### `fn frequencies<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<FrequencyTable, text>`

### `fn histogram<T is numeric>(values: matrix.Vector<T>, bins: int): outcome.Outcome<Histogram, text>`

### `fn histogram_range<T is numeric>(values: matrix.Vector<T>, bins: int, lower: real64, upper: real64): outcome.Outcome<Histogram, text>`

### `fn z_scores<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<matrix.Vector<real64>, text>`

### `fn population_z_scores<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<matrix.Vector<real64>, text>`

### `fn min_max_scale<T is numeric>(values: matrix.Vector<T>): outcome.Outcome<matrix.Vector<real64>, text>`

### `fn moving_mean<T is numeric>(values: matrix.Vector<T>, window: int): outcome.Outcome<matrix.Vector<real64>, text>`

### `fn moving_stddev<T is numeric>(values: matrix.Vector<T>, window: int): outcome.Outcome<matrix.Vector<real64>, text>`

### `fn exponential_moving_average<T is numeric>(values: matrix.Vector<T>, alpha: real64): outcome.Outcome<matrix.Vector<real64>, text>`

### `fn weighted_mean<T is numeric, W is numeric>(values: matrix.Vector<T>, weights: matrix.Vector<W>): outcome.Outcome<real64, text>`

### `fn weighted_variance<T is numeric, W is numeric>(values: matrix.Vector<T>, weights: matrix.Vector<W>): outcome.Outcome<real64, text>`

### `fn weighted_quantile<T is numeric, W is numeric>(values: matrix.Vector<T>, weights: matrix.Vector<W>, probability: real64): outcome.Outcome<real64, text>`

### `fn covariance<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<real64, text>`

### `fn sample_covariance<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<real64, text>`

### `fn pcovariance<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<real64, text>`

### `fn population_covariance<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<real64, text>`

### `fn correlation<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<real64, text>`

### `fn pearson_correlation<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<real64, text>`

### `fn spearman_correlation<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<real64, text>`

### `fn kendall_tau<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<real64, text>`

### `fn linear_regression<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<LinearRegression, text>`

### `fn simple_linear_regression<X is numeric, Y is numeric>(xs: matrix.Vector<X>, ys: matrix.Vector<Y>): outcome.Outcome<LinearRegression, text>`

### `fn describe<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<Summary, text>`

Descriptive statistics over all matrix elements in row-major order.

### `fn histogram<T is numeric>(values: matrix.Matrix<T>, bins: int): outcome.Outcome<Histogram, text>`

Histogram over all matrix elements in row-major order.

### `fn describe_columns<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<[Summary], text>`

One comprehensive Summary per matrix column. Rows are observations and columns are variables, matching NumPy/SciPy's conventional data layout.

### `fn describe_rows<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<[Summary], text>`

One comprehensive Summary per matrix row.

### `fn column_means<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<matrix.Vector<real64>, text>`

Column means as a Vector. Empty matrix axes return Failed, not a VM panic.

### `fn row_means<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<matrix.Vector<real64>, text>`

Row means as a Vector.

### `fn column_variances<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<matrix.Vector<real64>, text>`

Sample variance of each matrix column.

### `fn column_stddevs<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<matrix.Vector<real64>, text>`

Sample standard deviation of each matrix column.

### `fn column_medians<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<matrix.Vector<real64>, text>`

Median of each matrix column.

### `fn column_histograms<T is numeric>(values: matrix.Matrix<T>, bins: int): outcome.Outcome<[Histogram], text>`

One histogram per matrix column, useful immediately after csv.read_matrix.

### `fn covariance_matrix<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<matrix.Matrix<real64>, text>`

Sample covariance matrix. Rows are observations, columns are variables.

### `fn correlation_matrix<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<matrix.Matrix<real64>, text>`

Pearson correlation matrix. Any constant column yields a clear Failed value.

### `fn standardize_columns<T is numeric>(values: matrix.Matrix<T>): outcome.Outcome<matrix.Matrix<real64>, text>`

Standardize every matrix column to sample mean 0 and sample standard deviation 1. Constant columns are rejected explicitly.

### `fn linear_regression_columns<T is numeric>(values: matrix.Matrix<T>, x_column: int, y_column: int): outcome.Outcome<LinearRegression, text>`

Fit y on x using two columns from one observation matrix. This is convenient for matrices loaded by the csv module.
