# `csv`

CSV parsing and numeric-matrix I/O.

Minimal CSV support, layered purely on top of `fs` and `text` — no new VM
primitives. The first iteration handles comma-separated cells and
newline-separated rows (LF or CRLF); quoting and escaping are a follow-up.

`csv` reads and writes simple comma-separated data using only the Dune standard library. It handles comma-separated cells, LF or CRLF row endings, and trailing newlines; quoted fields and escaped commas are not part of this first implementation.

Use `parse_rows` when you already have CSV text, `read_rows` and `write_rows` for text-cell files, and the matrix helpers when a rectangular numeric file should become a `matrix.Matrix<int>` or `matrix.Matrix<real64>`. File and parse failures are returned as `Outcome` values rather than hidden exceptions.

```dn
import csv;

rows = csv.parse_rows("name,score\nada,42\ngrace,37\n");

print(rows.len());
print(rows[1][0]);
print(rows[1][1]);
```

> Auto-generated from `stdlib/csv.dn` by `tools/gen_stdlib_docs.py`.

### `fn parse_rows(content: text): [[text]]`

Parse CSV `content` into rows of text cells. A trailing newline is ignored.

### `fn read_rows(path: text): outcome.Outcome<[[text]], text>`

Read a CSV file into rows of text cells.
On success returns `Done(rows)`, otherwise `Failed(message)`.

### `fn read_matrix_real64(path: text): outcome.Outcome<matrix.Matrix<real64>, text>`

Convert parsed text rows into a rectangular real64 matrix. Trims each cell,
enforces a consistent row width, and reports the first parse or shape error.
Returns Done(Matrix) or Failed(message).

### `fn read_matrix_int(path: text): outcome.Outcome<matrix.Matrix<int>, text>`

Same as `read_matrix_real64` but parses cells as integers.

### `fn write_rows(path: text, rows: [[text]]): outcome.Outcome<text, text>`

Write rows of text cells back out as CSV (comma-separated, newline-terminated).
Returns Done(path-like text) or Failed(message) from the underlying write.

### `fn write_matrix_real64(path: text, data: matrix.Matrix<real64>): outcome.Outcome<text, text>`

Serialize a real64 matrix to CSV and write it to `path`.
