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
    "random": "A small deterministic pseudo-random generator.",
    "runtime": "Runtime helpers such as `panic`.",
    "fs": "File-system access (read, write, list).",
    "process": "Process access: arguments and environment.",
    "csv": "CSV parsing and numeric-matrix I/O.",
    "display": "The `Display` contract and `show` helper.",
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


def first_line(text: str) -> str:
    for line in text.splitlines():
        if line.strip():
            return line.strip()
    return ""


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
    out += [f"> Auto-generated from `stdlib/{stem}.dn` by `tools/gen_stdlib_docs.py`.", ""]

    if not entries:
        out += ["_This module exposes no public declarations._", ""]

    for entry in entries:
        out += [f"### `{entry['sig']}`", ""]
        if entry["doc"]:
            out += [entry["doc"], ""]
        if entry["members"]:
            fields = [member for member in entry["members"] if member["kind"] == "field"]
            methods = [member for member in entry["members"] if member["kind"] != "field"]
            if fields:
                out += ["**Fields:**", ""]
                for member in fields:
                    summary = first_line(member["doc"])
                    suffix = f" — {summary}" if summary else ""
                    out += [f"- `{member['sig']}`{suffix}"]
                out += [""]
            if methods:
                out += ["**Methods:**", ""]
                for member in methods:
                    summary = first_line(member["doc"])
                    suffix = f" — {summary}" if summary else ""
                    out += [f"- `{member['sig']}`{suffix}"]
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
