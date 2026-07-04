# Documentation site

This directory is an [mdBook](https://rust-lang.github.io/mdBook/) project. It is
published to GitHub Pages by `.github/workflows/docs.yml` on every push to `main`
that touches the docs, the stdlib sources, or the generator.

## Build locally

Install mdBook (`cargo install mdbook`, or download a release binary), then:

```sh
python3 tools/gen_stdlib_docs.py   # regenerate the stdlib reference pages
mdbook serve docs                  # live preview at http://localhost:3000
# or
mdbook build docs                  # output in docs/book/
```

## Structure

- `src/SUMMARY.md` — the table of contents (edit this when adding a page).
- `src/language/` — hand-written language reference.
- `src/guides/` — hand-written guides.
- `src/stdlib/` — **generated** by `tools/gen_stdlib_docs.py` from the
  doc-comments in `stdlib/*.dn`. Do not edit these by hand; edit the `.dn`
  source and regenerate. Adding a new stdlib module also needs a line in
  `SUMMARY.md`.

The generated stdlib pages are committed as a snapshot so the book builds from a
plain checkout; CI regenerates them so the published site always matches the
stdlib source.
