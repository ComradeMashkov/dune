#include "doc/doc_generator.hpp"

#include "ast/ast.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp" // for dune::type_name

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dune::doc {
namespace {

std::string trim_ascii(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r");
    return text.substr(begin, end - begin + 1);
}

// Strips the leading `//` / `///` marker and one following space from an
// already-trimmed comment line.
std::string strip_comment_marker(const std::string& line) {
    std::string text = line;
    if (text.rfind("///", 0) == 0) {
        text = text.substr(3);
    } else if (text.rfind("//", 0) == 0) {
        text = text.substr(2);
    }
    if (!text.empty() && text.front() == ' ') {
        text = text.substr(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

// Matches a leading `tag:` / `tag ` prefix on a doc line and returns the rest.
// Mirrors the LSP hover renderer so `dune doc` and hover agree on tag handling.
bool match_doc_tag(const std::string& line, std::string_view tag, std::string& rest) {
    if (line.size() < tag.size() || line.compare(0, tag.size(), tag) != 0) {
        return false;
    }
    std::size_t index = tag.size();
    if (index < line.size() && line[index] == ':') {
        ++index;
    } else if (index != line.size() && line[index] != ' ') {
        return false;
    }
    rest = trim_ascii(line.substr(index));
    return true;
}

// Renders a stored doc-comment into Markdown. Recognises the structured tags
// brief/param/returns/example (Doxygen/JSDoc style); anything untagged is
// treated as plain description. Kept in step with the LSP hover renderer.
std::string render_doc_comment(const std::string& doc) {
    std::vector<std::string> description;
    std::vector<std::pair<std::string, std::string>> params;
    std::string returns;
    std::vector<std::string> example;
    bool saw_tag = false;

    std::size_t line_start = 0;
    while (line_start <= doc.size()) {
        const std::size_t newline = doc.find('\n', line_start);
        const std::size_t end = newline == std::string::npos ? doc.size() : newline;
        const std::string line = trim_ascii(doc.substr(line_start, end - line_start));
        line_start = newline == std::string::npos ? doc.size() + 1 : newline + 1;

        std::string rest;
        if (match_doc_tag(line, "brief", rest)) {
            saw_tag = true;
            if (!rest.empty()) {
                description.push_back(rest);
            }
        } else if (match_doc_tag(line, "param", rest)) {
            saw_tag = true;
            const std::size_t colon = rest.find(':');
            std::string name = colon == std::string::npos ? rest : trim_ascii(rest.substr(0, colon));
            std::string detail = colon == std::string::npos ? std::string{} : trim_ascii(rest.substr(colon + 1));
            if (name.empty()) {
                continue;
            }
            const std::size_t space = name.find(' ');
            if (colon == std::string::npos && space != std::string::npos) {
                detail = trim_ascii(name.substr(space + 1));
                name = name.substr(0, space);
            }
            params.emplace_back(std::move(name), std::move(detail));
        } else if (match_doc_tag(line, "returns", rest) || match_doc_tag(line, "return", rest)) {
            saw_tag = true;
            returns = rest;
        } else if (match_doc_tag(line, "example", rest)) {
            saw_tag = true;
            if (!rest.empty()) {
                example.push_back(rest);
            }
        } else if (saw_tag && !example.empty()) {
            example.push_back(line);
        } else {
            description.push_back(line);
        }
    }

    std::string markdown;
    auto append_block = [&markdown](const std::string& block) {
        if (block.empty()) {
            return;
        }
        if (!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += block;
    };

    std::string paragraph;
    std::string description_block;
    auto flush_paragraph = [&]() {
        if (paragraph.empty()) {
            return;
        }
        if (!description_block.empty()) {
            description_block += "\n\n";
        }
        description_block += paragraph;
        paragraph.clear();
    };
    for (const std::string& text : description) {
        if (text.empty()) {
            flush_paragraph();
            continue;
        }
        if (!paragraph.empty()) {
            paragraph += ' ';
        }
        paragraph += text;
    }
    flush_paragraph();
    append_block(description_block);

    if (!params.empty()) {
        std::string block = "**Parameters:**";
        for (const auto& [name, detail] : params) {
            block += "\n- `" + name + "`";
            if (!detail.empty()) {
                block += " — " + detail;
            }
        }
        append_block(block);
    }

    if (!returns.empty()) {
        append_block("**Returns:** " + returns);
    }

    if (!example.empty()) {
        std::string block = "**Example:**\n```dune";
        for (const std::string& text : example) {
            block += "\n" + text;
        }
        block += "\n```";
        append_block(block);
    }

    return markdown;
}

// The first non-empty line of a doc-comment, used as a one-line member summary.
// Skips structured tags except `brief`, whose text is the summary.
std::string summary_line(const std::string& doc) {
    std::size_t line_start = 0;
    while (line_start <= doc.size()) {
        const std::size_t newline = doc.find('\n', line_start);
        const std::size_t end = newline == std::string::npos ? doc.size() : newline;
        std::string line = trim_ascii(doc.substr(line_start, end - line_start));
        line_start = newline == std::string::npos ? doc.size() + 1 : newline + 1;
        if (line.empty()) {
            continue;
        }
        std::string rest;
        if (match_doc_tag(line, "brief", rest)) {
            return rest;
        }
        if (match_doc_tag(line, "param", rest) || match_doc_tag(line, "returns", rest) ||
            match_doc_tag(line, "return", rest) || match_doc_tag(line, "example", rest)) {
            continue;
        }
        return line;
    }
    return {};
}

std::string type_annotation_name(const TypeAnnotation& annotation, std::string_view fallback) {
    if (!annotation.has_type) {
        return std::string(fallback);
    }
    return type_name(annotation.type);
}

std::string generic_parameters_text(const std::vector<GenericParameter>& generics) {
    if (generics.empty()) {
        return {};
    }
    std::string text = "<";
    for (std::size_t index = 0; index < generics.size(); ++index) {
        if (index != 0) {
            text += ", ";
        }
        text += generics[index].name;
        for (std::size_t bound_index = 0; bound_index < generics[index].bounds.size(); ++bound_index) {
            text += bound_index == 0 ? " is " : " + ";
            text += generics[index].bounds[bound_index];
        }
    }
    text += ">";
    return text;
}

std::string parameter_list_text(const std::vector<Parameter>& parameters) {
    std::string text = "(";
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        if (index != 0) {
            text += ", ";
        }
        text += parameters[index].name + ": " + type_annotation_name(parameters[index].type, "int");
    }
    text += ")";
    return text;
}

// `name<...>(params): ret` — the callable core without the `fn` keyword, as
// contract methods are written in source.
std::string callable_core(const Statement& statement) {
    return statement.name + generic_parameters_text(statement.generic_parameters) +
           parameter_list_text(statement.parameters) + ": " + type_annotation_name(statement.type, "unit");
}

// `[foreign ][static ]fn name<...>(params): ret` — a full function/method signature.
std::string function_signature(const Statement& statement) {
    std::string signature;
    if (statement.is_extern) {
        signature += "foreign ";
    }
    if (statement.is_static_record_member) {
        signature += "static ";
    }
    signature += "fn " + callable_core(statement);
    return signature;
}

std::string const_signature(const Statement& statement) {
    std::string signature = "const " + statement.name;
    if (statement.type.has_type) {
        signature += ": " + type_name(statement.type.type);
    }
    return signature;
}

std::string type_alias_signature(const Statement& statement) {
    return "type " + statement.name + " = " + type_annotation_name(statement.type, "unknown");
}

std::string record_signature(const Statement& statement) {
    std::string signature = "record " + statement.name + generic_parameters_text(statement.generic_parameters);
    if (!statement.contracts.empty()) {
        signature += " with ";
        for (std::size_t index = 0; index < statement.contracts.size(); ++index) {
            if (index != 0) {
                signature += ", ";
            }
            signature += type_name(statement.contracts[index]);
        }
    }
    return signature;
}

std::string choice_signature(const Statement& statement) {
    return "choice " + statement.name + generic_parameters_text(statement.generic_parameters);
}

std::string field_signature(const Parameter& field) {
    return field.name + ": " + type_annotation_name(field.type, "unknown");
}

std::string variant_signature(const Parameter& variant) {
    if (variant.type.has_type) {
        return variant.name + "(" + type_name(variant.type.type) + ")";
    }
    return variant.name;
}

bool is_documentable(StatementKind kind) {
    switch (kind) {
    case StatementKind::function:
    case StatementKind::const_statement:
    case StatementKind::type_alias_statement:
    case StatementKind::struct_statement:
    case StatementKind::enum_statement:
    case StatementKind::contract_statement:
        return true;
    default:
        return false;
    }
}

void append_doc(std::vector<std::string>& out, const std::string& doc) {
    if (doc.empty()) {
        return;
    }
    const std::string rendered = render_doc_comment(doc);
    if (rendered.empty()) {
        return;
    }
    out.push_back(rendered);
    out.emplace_back();
}

void append_declaration(std::vector<std::string>& out, const Statement& statement, bool module_has_exports) {
    const auto is_public = [module_has_exports](bool exported) { return exported || !module_has_exports; };

    switch (statement.kind) {
    case StatementKind::function:
        out.push_back("### `" + function_signature(statement) + "`");
        out.emplace_back();
        append_doc(out, statement.doc_comment);
        break;

    case StatementKind::const_statement:
        out.push_back("### `" + const_signature(statement) + "`");
        out.emplace_back();
        append_doc(out, statement.doc_comment);
        break;

    case StatementKind::type_alias_statement:
        out.push_back("### `" + type_alias_signature(statement) + "`");
        out.emplace_back();
        append_doc(out, statement.doc_comment);
        break;

    case StatementKind::struct_statement: {
        out.push_back("### `" + record_signature(statement) + "`");
        out.emplace_back();
        append_doc(out, statement.doc_comment);

        std::vector<const Parameter*> fields;
        for (const Parameter& field : statement.parameters) {
            if (is_public(field.exported)) {
                fields.push_back(&field);
            }
        }
        if (!fields.empty()) {
            out.emplace_back("**Fields**");
            out.emplace_back();
            for (const Parameter* field : fields) {
                std::string line = "- `" + field_signature(*field) + "`";
                const std::string summary = summary_line(field->doc_comment);
                if (!summary.empty()) {
                    line += " — " + summary;
                }
                out.push_back(std::move(line));
            }
            out.emplace_back();
        }

        for (const Statement& method : statement.body) {
            if (method.kind != StatementKind::function || !is_public(method.exported)) {
                continue;
            }
            out.push_back("#### `" + function_signature(method) + "`");
            out.emplace_back();
            append_doc(out, method.doc_comment);
        }
        break;
    }

    case StatementKind::enum_statement: {
        out.push_back("### `" + choice_signature(statement) + "`");
        out.emplace_back();
        append_doc(out, statement.doc_comment);
        if (!statement.parameters.empty()) {
            out.emplace_back("**Variants**");
            out.emplace_back();
            for (const Parameter& variant : statement.parameters) {
                out.push_back("- `" + variant_signature(variant) + "`");
            }
            out.emplace_back();
        }
        break;
    }

    case StatementKind::contract_statement: {
        out.push_back("### `contract " + statement.name + "`");
        out.emplace_back();
        append_doc(out, statement.doc_comment);
        std::vector<const Statement*> methods;
        for (const Statement& method : statement.body) {
            if (method.kind == StatementKind::function) {
                methods.push_back(&method);
            }
        }
        if (!methods.empty()) {
            out.emplace_back("**Methods**");
            out.emplace_back();
            for (const Statement* method : methods) {
                std::string line = "- `" + callable_core(*method) + "`";
                const std::string summary = summary_line(method->doc_comment);
                if (!summary.empty()) {
                    line += " — " + summary;
                }
                out.push_back(std::move(line));
            }
            out.emplace_back();
        }
        break;
    }

    default:
        break;
    }
}

// The module-level doc is the first contiguous comment block at the top of the
// file, but only when a blank line separates it from the first declaration
// (matching the lexer, which breaks a comment block on a blank line, and the
// legacy gen_stdlib_docs.py). When the top block directly precedes a
// declaration it belongs to that declaration and is rendered there instead.
std::string extract_module_doc(const std::string& source) {
    std::vector<std::string> block;
    bool started = false;
    std::size_t line_start = 0;
    while (line_start <= source.size()) {
        const std::size_t newline = source.find('\n', line_start);
        const std::size_t end = newline == std::string::npos ? source.size() : newline;
        const std::string line = trim_ascii(source.substr(line_start, end - line_start));
        line_start = newline == std::string::npos ? source.size() + 1 : newline + 1;

        if (!started && line.empty()) {
            continue; // skip leading blank lines
        }
        if (line.rfind("//", 0) == 0) {
            started = true;
            block.push_back(strip_comment_marker(line));
            continue;
        }
        // First non-comment line after the block. A blank line means the block
        // is a standalone module doc; anything else is a declaration that owns it.
        if (line.empty() && !block.empty()) {
            std::string doc;
            for (std::size_t index = 0; index < block.size(); ++index) {
                if (index != 0) {
                    doc += "\n";
                }
                doc += block[index];
            }
            return trim_ascii(doc);
        }
        return {};
    }
    return {};
}

std::string join_lines(const std::vector<std::string>& lines) {
    std::string joined;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index != 0) {
            joined += "\n";
        }
        joined += lines[index];
    }
    while (!joined.empty() && (joined.back() == '\n' || joined.back() == ' ')) {
        joined.pop_back();
    }
    joined += "\n";
    return joined;
}

} // namespace

std::string render_module(const std::string& source, const std::string& module_name) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    const Program program = parser.parse();

    bool module_has_exports = false;
    for (const Statement& statement : program.statements) {
        if (is_documentable(statement.kind) && statement.exported) {
            module_has_exports = true;
            break;
        }
    }

    std::vector<std::string> out;
    out.push_back("# `" + module_name + "`");
    out.emplace_back();

    const std::string module_doc = render_doc_comment(extract_module_doc(source));
    if (!module_doc.empty()) {
        out.push_back(module_doc);
        out.emplace_back();
    }

    out.push_back("> Generated by `dune doc` from `" + module_name + ".dn`.");
    out.emplace_back();

    bool any = false;
    for (const Statement& statement : program.statements) {
        if (!is_documentable(statement.kind)) {
            continue;
        }
        if (!(statement.exported || !module_has_exports)) {
            continue;
        }
        any = true;
        append_declaration(out, statement, module_has_exports);
    }
    if (!any) {
        out.push_back("_This module exposes no public declarations._");
        out.emplace_back();
    }

    return join_lines(out);
}

} // namespace dune::doc
