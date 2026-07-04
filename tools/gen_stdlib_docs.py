#!/usr/bin/env python3
"""Generate mdBook stdlib reference pages from the doc-comments in stdlib/*.dn.

For every module the script extracts the public (exported) declarations — free
functions, constants, type aliases, receiver methods, records and choices — along
with the contiguous `//` / `///` comment block written directly above each one,
and renders them to docs/src/stdlib/<module>.md. Record and choice bodies list
their exported fields and methods as members.

This is intentionally a light, line-based reader (not the real Dune parser); it
mirrors how the compiler attaches a leading comment block to a declaration. It is
a stepping stone toward a full `dune doc` generator (issue #103).

Run from the repository root:  python3 tools/gen_stdlib_docs.py
"""

from __future__ import annotations

import pathlib
import re

REPO = pathlib.Path(__file__).resolve().parent.parent
STDLIB = REPO / "stdlib"
OUT = REPO / "docs" / "src" / "stdlib"
DESCRIPTIONS = REPO / "docs" / "stdlib_descriptions"

DECL_RE = re.compile(r"^(?:export\s+)?(?:foreign\s+)?(?:static\s+)?(fn|const|record|choice|type|method)\b")
FIELD_RE = re.compile(r"^export\s+[A-Za-z_]\w*\s*:")

# A short blurb for each module, keyed by file stem, shown under the heading.
MODULE_BLURBS = {
    "array": "Generic helpers and higher-order pipelines for arrays.",
    "math": "Numeric constants and generic math functions.",
    "matrix": "A small NumPy-style foundation: vectors and matrices.",
    "text": "Text and glyph helpers.",
    "dict": "A hash-map style dictionary built in Dune.",
    "set": "A hash-set built in Dune.",
    "maybe": "The optional `Maybe<T>` choice and helpers.",
    "outcome": "The result-style `Outcome<T, E>` choice and helpers.",
    "autograd": "Scalar reverse-mode automatic differentiation.",
    "canvas": "Deterministic SVG canvas and immediate-mode GUI widgets.",
    "random": "A small deterministic pseudo-random generator.",
    "regex": "Safe ASCII regular expressions for validation and text cleanup.",
    "runtime": "Runtime helpers such as `panic`.",
    "io": "Standard input/output: print, read a line, and flush streams.",
    "fmt": "String formatting with `{}` placeholders.",
    "cli": "Command-line argument parsing and help output.",
    "fs": "File-system access (read, write, list).",
    "process": "Process access: arguments and environment.",
    "csv": "CSV parsing and numeric-matrix I/O.",
    "display": "The `Display` contract and `show` helper.",
    "log": "Levelled diagnostics with stderr output and filtering.",
    "plot": "Deterministic SVG/HTML chart rendering.",
    "collections": "Shared collection utilities.",
    "assert": "Assertion helpers for tests.",
}


def clean_comment(line: str) -> str:
    stripped = line.strip()
    if stripped.startswith("///"):
        stripped = stripped[3:]
    elif stripped.startswith("//"):
        stripped = stripped[2:]
    if stripped.startswith(" "):
        stripped = stripped[1:]
    return stripped.rstrip()


def signature(kind: str, code: str) -> str:
    code = re.sub(r"^export\s+", "", code).strip()
    cuts = []
    if kind in ("fn", "method", "record", "choice"):
        cuts.append(code.find("{"))
    if kind in ("fn", "method", "const"):
        # `foreign fn ... = "symbol"` and `const NAME: T = value` both end at `=`.
        cuts.append(code.find("="))
    positions = [cut for cut in cuts if cut != -1]
    if positions:
        code = code[: min(positions)]
    return code.rstrip().rstrip(";").rstrip()


def load_description(stem: str) -> str:
    path = DESCRIPTIONS / f"{stem}.md"
    return path.read_text().strip() if path.exists() else ""


def _match_tag(line: str, tag: str) -> str | None:
    """If `line` opens with `tag:` / `tag `, return the remaining text, else None."""
    if not line.startswith(tag):
        return None
    rest = line[len(tag) :]
    if rest.startswith(":"):
        return rest[1:].strip()
    if rest == "" or rest.startswith(" "):
        return rest.strip()
    return None


def parse_doc(doc: str) -> tuple[list[str], list[tuple[str, str]], str, list[str]]:
    """Split a doc-comment into (description lines, params, returns, example lines).

    Recognises the structured tags brief/param/returns/example, mirroring the
    compiler's hover and `dune doc` renderers so the site agrees with them.
    """
    description: list[str] = []
    params: list[tuple[str, str]] = []
    returns = ""
    example: list[str] = []
    in_example = False

    for raw in doc.splitlines():
        line = raw.strip()

        rest = _match_tag(line, "brief")
        if rest is not None:
            if rest:
                description.append(rest)
            continue
        rest = _match_tag(line, "param")
        if rest is not None:
            if ":" in rest:
                name, detail = rest.split(":", 1)
            elif " " in rest:
                name, detail = rest.split(" ", 1)
            else:
                name, detail = rest, ""
            if name.strip():
                params.append((name.strip(), detail.strip()))
            continue
        rest = _match_tag(line, "returns")
        if rest is None:
            rest = _match_tag(line, "return")
        if rest is not None:
            returns = rest
            continue
        rest = _match_tag(line, "example")
        if rest is not None:
            in_example = True
            if rest:
                example.append(rest)
            continue

        if in_example:
            example.append(line)
        else:
            description.append(line)

    return description, params, returns, example


LIST_ITEM_RE = re.compile(r"^(?:[-*+]\s|\d+[.)]\s)")


def render_paragraph(run: list[str]) -> str:
    """Render one run of non-blank lines, keeping Markdown list items on their own
    lines while still word-wrapping ordinary prose into a single line."""
    out: list[str] = []
    prose: list[str] = []
    for text in run:
        if LIST_ITEM_RE.match(text):
            if prose:
                out.append(" ".join(prose))
                prose = []
            out.append(text)
        else:
            prose.append(text)
    if prose:
        out.append(" ".join(prose))
    return "\n".join(out)


def render_doc(doc: str) -> str:
    """Render a doc-comment's tags to Markdown (prose, Parameters, Returns, Example)."""
    description, params, returns, example = parse_doc(doc)
    blocks: list[str] = []

    paragraphs: list[str] = []
    current: list[str] = []
    for text in description:
        if not text:
            if current:
                paragraphs.append(render_paragraph(current))
                current = []
        else:
            current.append(text)
    if current:
        paragraphs.append(render_paragraph(current))
    if paragraphs:
        blocks.append("\n\n".join(paragraphs))

    if params:
        block = "**Parameters:**"
        for name, detail in params:
            block += f"\n- `{name}`" + (f" — {detail}" if detail else "")
        blocks.append(block)
    if returns:
        blocks.append(f"**Returns:** {returns}")
    if example:
        blocks.append("**Example:**\n```dune\n" + "\n".join(example) + "\n```")

    return "\n\n".join(blocks)


def member_line(sig: str, doc: str) -> str:
    """A record field/method bullet: signature, one-line summary, and any inline example."""
    description, _params, _returns, example = parse_doc(doc)
    summary = next((text for text in description if text), "")
    line = f"- `{sig}`"
    if summary:
        line += f" — {summary}"
    if len(example) == 1:
        line += f" — e.g. `{example[0]}`"
    return line


def parse_module(path: pathlib.Path) -> tuple[str, list[dict]]:
    lines = path.read_text().splitlines()
    has_exports = any(re.match(r"^\s*export\b", line) for line in lines)

    module_doc = ""
    entries: list[dict] = []
    pending: list[str] = []
    depth = 0
    current_record: dict | None = None
    record_depth: int | None = None
    seen_decl = False

    for line in lines:
        stripped = line.strip()

        if stripped.startswith("//"):
            pending.append(clean_comment(line))
            continue

        if stripped == "":
            # The first comment block in the file documents the module itself.
            if not seen_decl and not module_doc and pending:
                module_doc = "\n".join(pending).strip()
            pending = []
            continue

        decl = DECL_RE.match(stripped)
        is_export = stripped.startswith("export")
        public = is_export or not has_exports
        doc = "\n".join(pending).strip()

        if decl and public and depth == 0:
            seen_decl = True
            kind = decl.group(1)
            entry = {"sig": signature(kind, stripped), "doc": doc, "kind": kind, "members": []}
            entries.append(entry)
            if kind in ("record", "choice"):
                current_record = entry
                record_depth = depth
        elif current_record is not None and record_depth is not None and depth == record_depth + 1:
            if decl and is_export:
                kind = decl.group(1)
                current_record["members"].append(
                    {"sig": signature(kind, stripped), "doc": doc, "kind": kind}
                )
            elif FIELD_RE.match(stripped):
                field = re.sub(r"^export\s+", "", stripped).rstrip(",").strip()
                current_record["members"].append({"sig": field, "doc": doc, "kind": "field"})

        pending = []
        depth += stripped.count("{") - stripped.count("}")
        if current_record is not None and record_depth is not None and depth <= record_depth:
            current_record = None
            record_depth = None

    return module_doc, entries


def render(stem: str, module_doc: str, entries: list[dict]) -> str:
    out: list[str] = [f"# `{stem}`", ""]
    blurb = MODULE_BLURBS.get(stem)
    if blurb:
        out += [blurb, ""]
    if module_doc:
        out += [module_doc, ""]
    description = load_description(stem)
    if description:
        out += [description, ""]
    out += [f"> Auto-generated from `stdlib/{stem}.dn` by `tools/gen_stdlib_docs.py`.", ""]

    if not entries:
        out += ["_This module exposes no public declarations._", ""]

    for entry in entries:
        out += [f"### `{entry['sig']}`", ""]
        rendered = render_doc(entry["doc"])
        if rendered:
            out += [rendered, ""]
        if entry["members"]:
            fields = [member for member in entry["members"] if member["kind"] == "field"]
            methods = [member for member in entry["members"] if member["kind"] != "field"]
            if fields:
                out += ["**Fields:**", ""]
                for member in fields:
                    out += [member_line(member["sig"], member["doc"])]
                out += [""]
            if methods:
                out += ["**Methods:**", ""]
                for member in methods:
                    out += [member_line(member["sig"], member["doc"])]
                out += [""]

    return "\n".join(out).rstrip() + "\n"


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    stems = sorted(path.stem for path in STDLIB.glob("*.dn"))
    for stem in stems:
        module_doc, entries = parse_module(STDLIB / f"{stem}.dn")
        (OUT / f"{stem}.md").write_text(render(stem, module_doc, entries))
        print(f"generated docs/src/stdlib/{stem}.md ({len(entries)} declarations)")

    # An index page linking every module, for the stdlib section landing.
    index = ["# Standard library", "",
             "The standard library is ordinary Dune loaded from `.dn` files. Each page "
             "below is generated from that module's source doc-comments.", ""]
    for stem in stems:
        blurb = MODULE_BLURBS.get(stem, "")
        suffix = f" — {blurb}" if blurb else ""
        index.append(f"- [`{stem}`]({stem}.md){suffix}")
    index.append("")
    (OUT / "index.md").write_text("\n".join(index))
    print(f"generated docs/src/stdlib/index.md ({len(stems)} modules)")


if __name__ == "__main__":
    main()
