`csv` reads and writes simple comma-separated data using only the Dune standard library. It handles comma-separated cells, LF or CRLF row endings, and trailing newlines; quoted fields and escaped commas are not part of this first implementation.

Use `parse_rows` when you already have CSV text, `read_rows` and `write_rows` for text-cell files, and the matrix helpers when a rectangular numeric file should become a `matrix.Matrix<int>` or `matrix.Matrix<real64>`. File and parse failures are returned as `Outcome` values rather than hidden exceptions.

```dn
import csv;

rows = csv.parse_rows("name,score\nada,42\ngrace,37\n");

print(rows.len());
print(rows[1][0]);
print(rows[1][1]);
```
