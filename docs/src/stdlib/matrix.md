# `matrix`

A small NumPy-style foundation: vectors and matrices.

> Auto-generated from `stdlib/matrix.dn` by `tools/gen_stdlib_docs.py`.

### `record Vector<T is numeric>`

A fixed-length numeric vector backed by a flat array `data`.

**Methods:**

- `fn new(data: [T]): Vector<T>` — Wrap an existing array as a Vector (no copy).
- `fn len(): int` — Number of elements.
- `fn shape(): [int]` — Shape as a one-element array [length], mirroring Matrix.shape().
- `fn is_empty(): bool` — True when the vector has no elements.
- `fn get(index: int): T` — Element at `index`.
- `fn set(index: int, value: T): unit` — Overwrite the element at `index` in place.
- `fn to_array(): [T]` — A plain array copy of the elements.
- `fn copy(): Vector<T>` — A deep copy of this vector (new backing array).
- `fn equals(other: Vector<T>): bool` — Element-wise equality with `other` (same length and same values).
- `fn same_shape(other: Vector<T>): bool` — True when `other` has the same length (shape) as this vector.
- `fn slice(start: int, end: int): Vector<T>` — A sub-vector over [start, end) as a new vector.
- `fn concat(other: Vector<T>): Vector<T>` — This vector followed by `other`, as a new vector.
- `fn fill(value: T): unit` — Overwrite every element with `value` in place.
- `fn add(other: Vector<T>): Vector<T>` — Element-wise addition of two equal-length vectors.
- `fn add(value: T): Vector<T>` — Add a scalar `value` to every element (broadcast).
- `fn sub(other: Vector<T>): Vector<T>` — Element-wise subtraction of two equal-length vectors.
- `fn sub(value: T): Vector<T>` — Subtract a scalar `value` from every element (broadcast).
- `fn rsub(value: T): Vector<T>` — Reverse-subtract: each element becomes `value` minus the element.
- `fn mul(other: Vector<T>): Vector<T>` — Element-wise (Hadamard) product of two equal-length vectors.
- `fn mul(value: T): Vector<T>` — Multiply every element by a scalar (delegates to scale).
- `fn div(other: Vector<T>): Vector<T>` — Element-wise division of two equal-length vectors.
- `fn div(value: T): Vector<T>` — Divide every element by a scalar (broadcast).
- `fn rdiv(value: T): Vector<T>` — Reverse-divide: each element becomes `value` divided by the element.
- `fn scale(factor: T): Vector<T>` — Multiply every element by scalar `factor` (the core of mul-by-scalar).
- `fn neg(): Vector<T>` — Negate every element.
- `fn abs(): Vector<T>` — Absolute value of every element.
- `fn clip(lower: T, upper: T): Vector<T>` — Clamp every element into the inclusive range [lower, upper].
- `fn dot(other: Vector<T>): T` — Dot product with `other`: sum of element-wise products.
- `fn norm_squared(): T` — Squared Euclidean length (dot product with itself).
- `fn norm(): real64` — Euclidean length: sqrt of the squared norm, as real64.
- `fn distance_squared(other: Vector<T>): T` — Squared distance to `other` (norm_squared of the difference).
- `fn distance(other: Vector<T>): real64` — Euclidean distance to `other`, as real64.
- `fn sum(): T` — Sum of all elements (starts from additive identity 0).
- `fn product(): T` — Product of all elements (starts from multiplicative identity 1).
- `fn mean(): real64` — Arithmetic mean as real64 (panics on an empty vector).
- `fn min(): T` — Minimum element (seeded with element 0; panics if empty).
- `fn max(): T` — Maximum element (seeded with element 0; panics if empty).
- `fn argmin(): int` — Index of the minimum element (argmin).
- `fn argmax(): int` — Index of the maximum element (argmax).
- `fn to_row_matrix(): Matrix<T>` — View this vector as a 1-by-n row matrix.
- `fn to_column_matrix(): Matrix<T>` — View this vector as an n-by-1 column matrix.
- `fn reshape(rows: int, cols: int): Matrix<T>` — Reshape the elements into a rows-by-cols matrix (sizes must match).
- `fn dot(other: Matrix<T>): Vector<T>` — Row-vector times matrix, spelled as `dot` (delegates to matmul).
- `fn matmul(other: Matrix<T>): Vector<T>` — Treat this vector as a row and multiply by matrix `other` (1xn * nxm).
- `fn outer(other: Vector<T>): Matrix<T>` — Outer product: an m-by-n matrix where entry (i,j) = this[i] * other[j].

### `record Matrix<T is numeric>`

A dense 2-D matrix stored row-major in a flat array of length rows*cols.

**Methods:**

- `fn new(rows: int, cols: int, data: [T]): Matrix<T>` — Construct a matrix, validating dimensions and data length.
- `fn rows(): int` — Number of rows.
- `fn cols(): int` — Number of columns.
- `fn shape(): [int]` — Shape as [rows, cols].
- `fn is_empty(): bool` — True when the matrix holds no elements.
- `fn is_square(): bool` — True when the matrix is square (rows == cols).
- `fn len(): int` — Total number of elements (rows*cols).
- `fn get(row: int, col: int): T` — Element at (row, col).
- `fn set(row: int, col: int, value: T): unit` — Overwrite the element at (row, col) in place.
- `fn to_array(): [T]` — A plain row-major array copy of all elements.
- `fn copy(): Matrix<T>` — A deep copy of this matrix.
- `fn equals(other: Matrix<T>): bool` — Element-wise equality with `other` (same shape and same values).
- `fn same_shape(other: Matrix<T>): bool` — True when `other` has the same rows and cols.
- `fn can_matmul(other: Matrix<T>): bool` — True when this matrix can be multiplied by `other` (cols == other.rows).
- `fn fill(value: T): unit` — Overwrite every element with `value` in place.
- `fn row(row: int): Vector<T>` — Extract row `row` as a Vector.
- `fn column(col: int): Vector<T>` — Extract column `col` as a Vector.
- `fn flatten(): Vector<T>` — Flatten all elements into a single Vector (row-major order).
- `fn reshape(rows: int, cols: int): Matrix<T>` — Reshape into new dimensions preserving the element order (sizes must match).
- `fn diagonal(): Vector<T>` — The main diagonal (i,i) as a Vector, truncated to the shorter dimension.
- `fn diag(): Vector<T>` — Alias for `diagonal`.
- `fn trace(): T` — Trace: the sum of the diagonal entries.
- `fn add(other: Matrix<T>): Matrix<T>` — Element-wise matrix addition (same shape required).
- `fn add(value: T): Matrix<T>` — Add a scalar to every element (broadcast).
- `fn sub(other: Matrix<T>): Matrix<T>` — Element-wise matrix subtraction (same shape required).
- `fn sub(value: T): Matrix<T>` — Subtract a scalar from every element (broadcast).
- `fn rsub(value: T): Matrix<T>` — Reverse-subtract: each element becomes `value` minus the element.
- `fn mul(other: Matrix<T>): Matrix<T>` — Element-wise (Hadamard) product of two same-shape matrices.
- `fn mul(value: T): Matrix<T>` — Multiply every element by a scalar (delegates to scale).
- `fn hadamard(other: Matrix<T>): Matrix<T>` — Named alias for element-wise multiplication.
- `fn div(other: Matrix<T>): Matrix<T>` — Element-wise matrix division (same shape required).
- `fn div(value: T): Matrix<T>` — Divide every element by a scalar (broadcast).
- `fn rdiv(value: T): Matrix<T>` — Reverse-divide: each element becomes `value` divided by the element.
- `fn scale(factor: T): Matrix<T>` — Multiply every element by scalar `factor` (core of scalar multiply).
- `fn neg(): Matrix<T>` — Negate every element.
- `fn abs(): Matrix<T>` — Absolute value of every element.
- `fn clip(lower: T, upper: T): Matrix<T>` — Clamp every element into the inclusive range [lower, upper].
- `fn transpose(): Matrix<T>` — Transpose: swap rows and columns into a new cols-by-rows matrix.
- `fn matmul(other: Matrix<T>): Matrix<T>` — Matrix product: (rows x cols) * (cols x other.cols) -> (rows x other.cols).
- `fn dot(other: Matrix<T>): Matrix<T>` — Matrix-times-matrix spelled as `dot`.
- `fn dot(vector: Vector<T>): Vector<T>` — Matrix-times-vector spelled as `dot`.
- `fn mul_vector(vector: Vector<T>): Vector<T>` — Multiply this matrix by a column vector, producing a vector.
- `fn sum_rows(): Vector<T>` — Vector of per-row sums (one entry per row).
- `fn sum_columns(): Vector<T>` — Vector of per-column sums (one entry per column).
- `fn mean_rows(): Vector<real64>` — Vector of per-row means as real64 (panics if there are no columns).
- `fn mean_columns(): Vector<real64>` — Vector of per-column means as real64 (panics if there are no rows).
- `fn sum(): T` — Sum of every element in the matrix.
- `fn product(): T` — Product of every element in the matrix.
- `fn mean(): real64` — Mean of every element as real64 (panics if empty).
- `fn norm_squared(): T` — Sum of squares of all elements (the squared Frobenius norm).
- `fn norm(): real64` — Frobenius norm: sqrt of the sum of squares, as real64.
- `fn min(): T` — Minimum element over the whole matrix (via flatten; panics if empty).
- `fn max(): T` — Maximum element over the whole matrix (panics if empty).
- `fn argmin(): int` — Flat index of the minimum element (row-major).
- `fn argmax(): int` — Flat index of the maximum element (row-major).
- `fn det2(): T` — Determinant of a 2x2 matrix: ad - bc.
- `fn det3(): T` — Determinant of a 3x3 matrix via cofactor expansion along the first row.

### `fn vector<T is numeric>(data: [T]): Vector<T>`

Free-function constructor: build a Vector from an array.

### `fn from_flat<T is numeric>(rows: int, cols: int, data: [T]): Matrix<T>`

Build a Matrix from flat row-major data plus explicit dimensions.

### `fn from_rows<T is numeric>(rows: [[T]]): Matrix<T>`

Build a Matrix from an array of row arrays (all rows must be equal length).

### `fn zeros<T is numeric>(size: int): Vector<T>`

A zero vector of the given size (the literal 0 takes on type T).

### `fn zeros<T is numeric>(rows: int, cols: int): Matrix<T>`

A zero matrix of the given dimensions.

### `fn ones<T is numeric>(size: int): Vector<T>`

A ones vector of the given size.

### `fn ones<T is numeric>(rows: int, cols: int): Matrix<T>`

A ones matrix of the given dimensions.

### `fn full<T is numeric>(size: int, value: T): Vector<T>`

A vector of `size` copies of `value`.

### `fn full<T is numeric>(rows: int, cols: int, value: T): Matrix<T>`

A matrix of the given dimensions filled with `value`.

### `fn arange<T is numeric>(end: T): Vector<T>`

arange overload: [0, end) with step 1.

### `fn arange<T is numeric>(start: T, end: T): Vector<T>`

arange overload: [start, end) with step 1.

### `fn arange<T is numeric>(start: T, end: T, step: T): Vector<T>`

A vector of evenly spaced values over [start, end) advancing by `step`.

### `fn identity<T is numeric>(size: int): Matrix<T>`

The size-by-size identity matrix (1 on the diagonal, 0 elsewhere).

### `fn eye<T is numeric>(size: int): Matrix<T>`

Alias for `identity`.

### `fn diagonal<T is numeric>(values: Vector<T>): Matrix<T>`

A square matrix with `values` on the diagonal and zeros elsewhere.

### `fn diag<T is numeric>(values: Vector<T>): Matrix<T>`

Alias for `diagonal`.

### `fn dot<T is numeric>(left: Vector<T>, right: Vector<T>): T`

Free-function dot product of two vectors.

### `fn dot<T is numeric>(left: Matrix<T>, right: Matrix<T>): Matrix<T>`

Free-function matrix product of two matrices.

### `fn dot<T is numeric>(left: Matrix<T>, right: Vector<T>): Vector<T>`

Free-function matrix-times-vector product.

### `fn matmul<T is numeric>(left: Matrix<T>, right: Matrix<T>): Matrix<T>`

Free-function matrix multiplication (alias of matmul method).

### `fn outer<T is numeric>(left: Vector<T>, right: Vector<T>): Matrix<T>`

Free-function outer product of two vectors.
