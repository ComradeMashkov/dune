# `cli`

Command-line argument parsing and help output.

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

> Auto-generated from `stdlib/cli.dn` by `tools/gen_stdlib_docs.py`.

### `record OptionSpec`

A command-line option that accepts a text value.

### `record FlagSpec`

A command-line flag that is either present or absent.

### `record PositionalSpec`

A positional argument accepted after options and flags.

### `record ParsedValue`

Parsed value for a named option.

### `record ParsedFlag`

Parsed value for a named flag.

### `record ParsedPositional`

Parsed value for a positional argument.

### `record ParseResult`

Result returned by Command.parse().

**Methods:**

- `fn text(name: text): maybe.Maybe<text>` — Return the option value named `name`, if it was supplied or defaulted. — e.g. `cli.command("g").option("name", "n", "Name", "world").parse(["--name", "dune"]).value_or(cli.empty_result()).text("name").value_or("")  // dune`
- `fn as_int(name: text): maybe.Maybe<int>` — Parse a named option as an int. Invalid or absent values return Absent. — e.g. `cli.command("g").option("count", "c", "Count", "1").parse(["--count", "3"]).value_or(cli.empty_result()).as_int("count").value_or(0)  // 3`
- `fn as_bool(name: text): maybe.Maybe<bool>` — Parse a named option or flag as a bool. — e.g. `cli.command("t").option("wide", "w", "", "false").parse(["--wide", "true"]).value_or(cli.empty_result()).as_bool("wide").value_or(false)  // 1`
- `fn as_real64(name: text): maybe.Maybe<real64>` — Parse a named option as a real64. Invalid or absent values return Absent. — e.g. `cli.command("t").option("ratio", "r", "", "1.0").parse(["--ratio", "2.5"]).value_or(cli.empty_result()).as_real64("ratio").value_or(0.0)  // 2.5`
- `fn flag(name: text): bool` — True when a flag was supplied. — e.g. `cli.command("g").flag("v", "v", "").parse(["-v"]).value_or(cli.empty_result()).flag("v")  // 1`
- `fn positional(index: int): maybe.Maybe<text>` — Positional value by index. — e.g. `cli.command("t").positional("path", "").parse(["src"]).value_or(cli.empty_result()).positional(0).value_or("?")  // src`
- `fn positional(name: text): maybe.Maybe<text>` — Positional value by declared name. — e.g. `cli.command("t").positional("path", "").parse(["src"]).value_or(cli.empty_result()).positional("path").value_or("?")  // src`
- `fn is_help(): bool` — True when parsing stopped for --help or -h. — e.g. `cli.command("t").parse(["--help"]).value_or(cli.empty_result()).is_help()  // 1`
- `fn is_version(): bool` — True when parsing stopped for --version. — e.g. `cli.command("t").version("1.0").parse(["--version"]).value_or(cli.empty_result()).is_version()  // 1`
- `fn output_text(): text` — Help or version text produced by --help / --version. — e.g. `cli.command("greet").version("1.0.0").parse(["--version"]).value_or(cli.empty_result()).output_text()  // greet 1.0.0`

### `record Command`

Builder for command-line parsers.

**Methods:**

- `fn about(description: text): Command` — Set the one-line command summary shown at the top of `--help`. — e.g. `cli.command("greet").about("Greet someone by name")`
- `fn version(version_text: text): Command` — Set the version string; `--version` prints "<name> <version>". — e.g. `cli.command("greet").version("1.0.0").version_output()  // greet 1.0.0`
- `fn option(name: text, short: text, description: text, default_value: text): Command` — Add an option with a default value. — e.g. `cli.command("g").flag("v", "v", "").option("name", "n", "Name", "world").parse(["-v"]).value_or(cli.empty_result()).text("name").value_or("")  // world`
- `fn required_option(name: text, short: text, description: text): Command` — Add a required option that has no default.
- `fn flag(name: text, short: text, description: text): Command` — Add a boolean flag. — e.g. `cli.command("g").flag("verbose", "v", "Verbose").parse(["-v"]).value_or(cli.empty_result()).flag("verbose")  // 1`
- `fn positional(name: text, description: text): Command` — Add a required positional argument, read back by name or index. — e.g. `cli.command("greet").positional("count", "How many times to greet")`
- `fn optional_positional(name: text, description: text): Command` — Add an optional positional argument (no parse error when omitted). — e.g. `cli.command("greet").optional_positional("suffix", "Optional suffix")`
- `fn help_text(): text` — Deterministic help text for this command. — e.g. `cli.command("greet").help_text().len()  // 69`
- `fn version_output(): text` — Deterministic version text for this command. — e.g. `cli.command("tool").version("1.0").version_output()  // tool 1.0`
- `fn parse(args: [text]): outcome.Outcome<ParseResult, text>` — Parse command-line arguments. — e.g. `cli.command("g").parse(["extra"]).failure_or("")  // unexpected positional argument 'extra'`

### `fn command(name: text): Command`

Start a builder for a named command.

**Example:**
```dune
cli.command("greet").version_output()  // greet
```

### `fn empty_result(): ParseResult`

Empty parse result useful as a value_or fallback.

**Example:**
```dune
cli.empty_result().text("missing").value_or("none")  // none
```
