`cli` is a declarative command-line argument parser written in pure Dune. You
describe a command once — its options, flags, and positionals — and `cli` parses
a raw `[text]` argument list into a typed `ParseResult`, generates `--help` and
`--version` output, and reports argument errors as an `Outcome`.

## The pieces

- **`cli.command(name)`** starts a builder. Chain `.about(...)` and
  `.version(...)` for help/version text.
- **Options** take a value: `.option(name, short, description, default)` (uses
  the default when omitted) or `.required_option(name, short, description)`
  (fails to parse when missing). Read them back with `result.text(name)`,
  `.as_int(name)`, `.as_real64(name)`, or `.as_bool(name)` — each returns a
  `Maybe`.
- **Flags** are boolean switches: `.flag(name, short, description)`, read with
  `result.flag(name)` → `bool`.
- **Positionals** are order-based: `.positional(name, description)` (required)
  or `.optional_positional(...)`, read with `result.positional(name)` or
  `result.positional(index)` → `Maybe<text>`.
- **`.parse(args)`** returns `Outcome<ParseResult, text>`: `Done` with the
  parsed result, or `Failed` with a message like
  `unexpected positional argument 'extra'`. `--help`/`--version` short-circuit;
  check `result.is_help()` / `result.is_version()` and print
  `result.output_text()`.

## Example

```dn
import io;
import cli;

parser = cli.command("greet")
    .about("Greet someone by name")
    .version("1.0.0")
    .option("name", "n", "Who to greet", "world")
    .flag("shout", "s", "Upper-case the greeting")
    .positional("count", "How many times to greet");

result = parser.parse(["--name", "Ada", "-s", "3"]).value_or(cli.empty_result());

io.println(result.text("name").value_or("?"));       // Ada
io.println(result.flag("shout"));                     // 1  (true)
io.println(result.positional("count").value_or("?")); // 3
```

A missing required option or a stray positional is reported instead of parsed:

```dn
import io;
import cli;

parser = cli.command("greet").positional("count", "count");
io.println(parser.parse(["one", "two"]).failure_or("ok")); // unexpected positional argument 'two'
```
