// mdBook ships highlight.js but without a grammar for Dune, so ```dune / ```dn
// code fences render as plain text. This registers a Dune grammar and
// re-highlights those blocks. It loads after book.js (which highlights
// synchronously with the language still unregistered), so it re-runs the
// highlighter on the Dune blocks itself.
(function () {
  function duneLanguage(hljs) {
    return {
      name: "Dune",
      aliases: ["dn"],
      keywords: {
        keyword:
          "fn method record choice contract with derive import from as module " +
          "export const static return if else while for in break continue defer when " +
          "is to type foreign print",
        literal: "true false",
        built_in:
          "int bool i8 i16 i32 i64 isize u8 u16 u32 u64 usize uint8 uint16 " +
          "uint32 uint64 real real32 real64 glyph text unit",
      },
      contains: [
        hljs.C_LINE_COMMENT_MODE, // // and ///
        hljs.C_BLOCK_COMMENT_MODE, // /* */ and /** */
        {
          className: "string",
          variants: [
            { begin: /r"/, end: /"/ }, // raw text literal
            { begin: /"/, end: /"/, contains: [hljs.BACKSLASH_ESCAPE] },
            { begin: /'/, end: /'/, contains: [hljs.BACKSLASH_ESCAPE] }, // glyph
          ],
        },
        hljs.C_NUMBER_MODE,
        // Record / choice / type names and generic parameters are capitalized.
        { className: "type", begin: /\b[A-Z][A-Za-z0-9_]*/, relevance: 0 },
      ],
    };
  }

  function run() {
    if (!window.hljs || typeof hljs.registerLanguage !== "function") {
      return;
    }
    if (!hljs.getLanguage("dune")) {
      hljs.registerLanguage("dune", duneLanguage);
    }

    var highlight = hljs.highlightElement || hljs.highlightBlock;
    var blocks = document.querySelectorAll(
      "code.language-dune, code.language-dn"
    );
    Array.prototype.forEach.call(blocks, function (block) {
      if (block.dataset) {
        delete block.dataset.highlighted;
      }
      block.className = block.className.replace(/\bhljs\b/, "").trim();
      highlight.call(hljs, block);
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", run);
  } else {
    run();
  }
})();
