#pragma once

#include <string>

namespace dune::doc {

// Renders the exported public API of a single Dune source module to a Markdown
// reference page.
//
// `source` is the raw `.dn` text and `module_name` titles the page (typically
// the file stem). The module is lexed and parsed with the real front-end but
// NOT module-resolved or type-checked, so only this file's own declarations are
// documented (imported symbols do not leak in) and generation never fails on a
// downstream type error. Signatures reuse the type-checker's `type_name`, so
// they are accurate rather than regex-extracted.
//
// Only public declarations are shown: if the module exports anything, only
// exported declarations (and exported record members) appear; a module with no
// `export` at all is treated as fully public.
std::string render_module(const std::string& source, const std::string& module_name);

} // namespace dune::doc
