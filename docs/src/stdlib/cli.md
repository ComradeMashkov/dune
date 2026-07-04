# `cli`

Command-line argument parsing and help output.

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
- `fn as_bool(name: text): maybe.Maybe<bool>` — Parse a named option or flag as a bool.
- `fn as_real64(name: text): maybe.Maybe<real64>` — Parse a named option as a real64. Invalid or absent values return Absent.
- `fn flag(name: text): bool` — True when a flag was supplied. — e.g. `cli.command("g").flag("v", "v", "").parse(["-v"]).value_or(cli.empty_result()).flag("v")  // 1`
- `fn positional(index: int): maybe.Maybe<text>` — Positional value by index.
- `fn positional(name: text): maybe.Maybe<text>` — Positional value by declared name.
- `fn is_help(): bool` — True when parsing stopped for --help or -h.
- `fn is_version(): bool` — True when parsing stopped for --version.
- `fn output_text(): text` — Help or version text produced by --help / --version.

### `record Command`

Builder for command-line parsers.

**Methods:**

- `fn about(description: text): Command` — Set one-line command help.
- `fn version(version_text: text): Command` — Set version output returned by --version.
- `fn option(name: text, short: text, description: text, default_value: text): Command` — Add an option with a default value. — e.g. `cli.command("g").flag("v", "v", "").option("name", "n", "Name", "world").parse(["-v"]).value_or(cli.empty_result()).text("name").value_or("")  // world`
- `fn required_option(name: text, short: text, description: text): Command` — Add a required option that has no default.
- `fn flag(name: text, short: text, description: text): Command` — Add a boolean flag. — e.g. `cli.command("g").flag("verbose", "v", "Verbose").parse(["-v"]).value_or(cli.empty_result()).flag("verbose")  // 1`
- `fn positional(name: text, description: text): Command` — Add a required positional argument.
- `fn optional_positional(name: text, description: text): Command` — Add an optional positional argument.
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
