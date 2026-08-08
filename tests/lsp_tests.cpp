#include "lsp/lsp_server.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }

    return true;
}

std::string lsp_message(const std::string& body) {
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

bool has_completion(const std::vector<dune::lsp::CompletionItem>& completions, const std::string& label) {
    for (const dune::lsp::CompletionItem& completion : completions) {
        if (completion.label == label) {
            return true;
        }
    }

    return false;
}

std::string with_test_print(const std::string& source) {
    return source + "\nfn print<T>(__printed_value: T): unit { return; }";
}

std::size_t utf16_length(std::string_view value) {
    std::size_t length = 0;
    for (std::size_t index = 0; index < value.size();) {
        const unsigned char lead = static_cast<unsigned char>(value[index]);
        std::size_t width = 1;
        std::size_t units = 1;
        if ((lead & 0xe0U) == 0xc0U && index + 1 < value.size()) {
            width = 2;
        } else if ((lead & 0xf0U) == 0xe0U && index + 2 < value.size()) {
            width = 3;
        } else if ((lead & 0xf8U) == 0xf0U && index + 3 < value.size()) {
            width = 4;
            units = 2;
        }
        index += width;
        length += units;
    }
    return length;
}

std::size_t semantic_name_index(const std::vector<std::string>& names, std::string_view name) {
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (names[index] == name) {
            return index;
        }
    }
    return std::numeric_limits<std::size_t>::max();
}

std::size_t semantic_modifier_mask(std::string_view name) {
    const std::size_t index = semantic_name_index(dune::lsp::semantic_token_modifiers(), name);
    return index == std::numeric_limits<std::size_t>::max() ? 0 : std::size_t{1} << index;
}

std::optional<dune::lsp::SemanticToken>
semantic_token_at_byte_offset(const std::string& source, const std::vector<dune::lsp::SemanticToken>& tokens,
                              std::size_t byte_offset, std::size_t byte_length) {
    const std::size_t line = static_cast<std::size_t>(
        std::count(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(byte_offset), '\n'));
    const std::size_t line_start = byte_offset == 0 ? 0 : source.rfind('\n', byte_offset - 1) + 1;
    const std::size_t character = utf16_length(std::string_view(source).substr(line_start, byte_offset - line_start));
    const std::size_t length = utf16_length(std::string_view(source).substr(byte_offset, byte_length));
    for (const dune::lsp::SemanticToken& token : tokens) {
        if (token.line == line && token.start_character == character && token.length == length) {
            return token;
        }
    }
    return std::nullopt;
}

bool expect_semantic_token(const std::string& source, const std::vector<dune::lsp::SemanticToken>& tokens,
                           std::string_view unique_context, std::size_t offset_in_context, std::size_t byte_length,
                           std::string_view expected_type, std::size_t required_modifiers, const char* message) {
    const std::size_t context = source.find(unique_context);
    if (!expect(context != std::string::npos, message)) {
        return false;
    }
    const std::optional<dune::lsp::SemanticToken> token =
        semantic_token_at_byte_offset(source, tokens, context + offset_in_context, byte_length);
    if (!expect(token.has_value(), message)) {
        return false;
    }
    const dune::lsp::SemanticToken actual = token.value_or(dune::lsp::SemanticToken{});
    const std::size_t expected_index = semantic_name_index(dune::lsp::semantic_token_types(), expected_type);
    return expect(actual.token_type == expected_index &&
                      (actual.token_modifiers & required_modifiers) == required_modifiers,
                  message);
}

bool diagnoses_valid_source() {
    const std::vector<dune::lsp::Diagnostic> diagnostics =
        dune::lsp::diagnose_source("fn add(a: int, b: int): int { return a + b; } "
                                   "const HIDDEN: int = 7; fn hidden(): int { return HIDDEN; } "
                                   "total: int = add(10, hidden());");

    return expect(diagnostics.empty(), "expected no diagnostics for valid source");
}

bool diagnoses_type_errors_with_range() {
    const std::vector<dune::lsp::Diagnostic> diagnostics = dune::lsp::diagnose_source("x: int = true;");

    bool passed = true;
    passed = expect(diagnostics.size() == 1, "expected one diagnostic") && passed;
    if (!diagnostics.empty()) {
        passed = expect(diagnostics[0].line == 1, "expected diagnostic line") && passed;
        passed = expect(diagnostics[0].start_column == 10, "expected diagnostic start column") && passed;
        passed = expect(diagnostics[0].end_column == 13, "expected diagnostic end column") && passed;
        passed = expect(diagnostics[0].message.find("expected type 'int' but got 'bool'") != std::string::npos,
                        "expected type mismatch message") &&
                 passed;
    }

    return passed;
}

bool diagnoses_non_exhaustive_when_with_missing_variants() {
    const std::vector<dune::lsp::Diagnostic> diagnostics =
        dune::lsp::diagnose_source("choice State { Ready, Running(int), Failed(text) }\n"
                                   "state: State = Ready;\n"
                                   "label: text = when state { Ready => \"ready\"; };\n");

    bool passed = true;
    passed = expect(diagnostics.size() == 1, "expected one non-exhaustive when diagnostic") && passed;
    if (!diagnostics.empty()) {
        passed = expect(diagnostics[0].message.find("missing variants: Running, Failed") != std::string::npos,
                        "expected LSP diagnostic to list every missing variant") &&
                 passed;
    }

    return passed;
}

bool completes_keywords_and_local_symbols() {
    const std::vector<dune::lsp::CompletionItem> completions =
        dune::lsp::complete_source("fn add(a: int, b: int): int { return a + b; }\ntotal: int = add(10, 20);");

    bool passed = true;
    passed = expect(has_completion(completions, "method"), "expected keyword completion") && passed;
    passed = expect(has_completion(completions, "in"), "expected for-in keyword completion") && passed;
    passed = expect(has_completion(completions, "fn"), "expected fn keyword completion") && passed;
    passed = expect(has_completion(completions, "defer"), "expected defer keyword completion") && passed;
    passed = expect(has_completion(completions, "real64"), "expected type completion") && passed;
    passed = expect(has_completion(completions, "add"), "expected function completion") && passed;
    passed = expect(has_completion(completions, "total"), "expected local variable completion") && passed;
    return passed;
}

bool supports_defer_diagnostics_and_semantic_highlighting() {
    bool passed = true;
    const std::vector<dune::lsp::Diagnostic> valid =
        dune::lsp::diagnose_source(with_test_print("defer print(1);"));
    passed = expect(valid.empty(), "expected valid defer source to have no diagnostics") && passed;

    const std::vector<dune::lsp::Diagnostic> invalid = dune::lsp::diagnose_source("defer 42;");
    passed = expect(invalid.size() == 1, "expected one non-unit defer diagnostic") && passed;
    if (!invalid.empty()) {
        passed = expect(invalid[0].message.find("deferred expression must return 'unit'") != std::string::npos,
                        "expected defer result diagnostic") &&
                 passed;
    }

    const std::string source = with_test_print("defer print(1);");
    const std::vector<dune::lsp::SemanticToken> tokens = dune::lsp::semantic_tokens_source(source);
    passed = expect_semantic_token(source, tokens, "defer print", 0, 5, "keyword", 0,
                                   "expected defer semantic keyword token") &&
             passed;
    return passed;
}

bool completes_imported_module_members() {
    const std::vector<dune::lsp::CompletionItem> completions =
        dune::lsp::complete_source("import math;\nprint(math.);", {}, {}, 1, 11);

    bool passed = true;
    passed = expect(has_completion(completions, "square"), "expected module function completion") && passed;
    passed = expect(has_completion(completions, "PI"), "expected module constant completion") && passed;
    return passed;
}

bool completes_typed_record_methods() {
    const std::vector<dune::lsp::CompletionItem> completions = dune::lsp::complete_source(
        "import matrix;\nvalues = matrix.vector([1, 2, 3]);\nprint(values.);", {}, {}, 2, 13);

    bool passed = true;
    passed = expect(has_completion(completions, "dot"), "expected vector method completion") && passed;
    passed = expect(has_completion(completions, "mean"), "expected reduction method completion") && passed;
    passed = expect(has_completion(completions, "reshape"), "expected new vector method completion") && passed;
    return passed;
}

bool hovers_local_symbols() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("total: int = 42;\nprint(total);"), {}, {}, 1, 7);

    bool passed = true;
    passed = expect(hover.has_value(), "expected hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("total: int") != std::string::npos, "expected variable hover") && passed;
    }
    return passed;
}

bool hovers_generic_type_aliases() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source("type Pair<T, U> = (T, U);\nvalue: Pair<int, text> = (1, \"one\");", {}, {}, 1, 8);

    bool passed = true;
    passed = expect(hover.has_value(), "expected generic type alias hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("type Pair<T, U> = (T, U)") != std::string::npos,
                        "expected generic parameters in type alias hover") &&
                 passed;
    }
    return passed;
}

bool hovers_for_in_loop_variable() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("values: [int] = [1, 2, 3];\n"
                                                "for value in values { print(value); }"),
                                {}, {}, 1, 30);

    bool passed = true;
    passed = expect(hover.has_value(), "expected for-in loop variable hover") && passed;
    if (hover.has_value()) {
        passed =
            expect(hover->contents.find("value: int") != std::string::npos, "expected for-in variable hover type") &&
            passed;
    }
    return passed;
}

bool hovers_known_record_for_in_loop_variable() {
    const std::optional<dune::lsp::Hover> hover = dune::lsp::hover_source(
        with_test_print("import set;\nseen: set.Set = set.Set.new(); seen.add(\"a\");\nfor value in seen { print(value); }"),
        {}, {}, 2, 28);

    bool passed = true;
    passed = expect(hover.has_value(), "expected record-backed for-in loop variable hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("value: text") != std::string::npos,
                        "expected record-backed for-in variable hover type") &&
                 passed;
    }
    return passed;
}

bool hovers_typed_record_methods() {
    const std::optional<dune::lsp::Hover> hover = dune::lsp::hover_source(
        with_test_print("import matrix;\nvalues = matrix.vector([1, 2, 3]);\nprint(values.mean());"), {}, {}, 2, 14);

    bool passed = true;
    passed = expect(hover.has_value(), "expected receiver method hover") && passed;
    if (hover.has_value()) {
        passed =
            expect(hover->contents.find("mean(): real") != std::string::npos, "expected typed receiver method hover") &&
            passed;
    }
    return passed;
}

bool hovers_imported_module_members() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("import math;\nprint(math.square(5));"), {}, {}, 1, 13);

    bool passed = true;
    passed = expect(hover.has_value(), "expected module member hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("square<T is numeric>(value: T): T") != std::string::npos,
                        "expected module function hover") &&
                 passed;
    }
    return passed;
}

bool hovers_inferred_call_assignments() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("import matrix;\nleft = matrix.vector([1, 2]);\n"
                                                "right = matrix.vector([1, 2, 3]);\nprint(left.dot(right));"),
                                {}, {}, 3, 7);

    bool passed = true;
    passed = expect(hover.has_value(), "expected inferred assignment hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("left: matrix.Vector<int>") != std::string::npos,
                        "expected inferred matrix vector hover") &&
                 passed;
    }
    return passed;
}

bool defines_function_from_call() {
    const std::optional<dune::lsp::DefinitionLocation> definition = dune::lsp::definition_source(
        "fn add(a: int, b: int): int { return a + b; }\ntotal: int = add(10, 20);", {}, {}, 1, 14);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for function call") && passed;
    if (definition.has_value()) {
        passed = expect(definition->line == 1, "expected function definition line") && passed;
        passed = expect(definition->column == 4, "expected function definition column") && passed;
        passed = expect(definition->length == 3, "expected function name length") && passed;
    }
    return passed;
}

bool defines_local_variable() {
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source(with_test_print("total: int = 42;\nprint(total);"), {}, {}, 1, 7);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for local variable") && passed;
    if (definition.has_value()) {
        passed =
            expect(definition->line == 1 && definition->column == 1, "expected variable definition position") && passed;
    }
    return passed;
}

bool defines_record_type() {
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source("record Point { x: int, y: int }\np: Point = Point { x: 1, y: 2 };", {}, {}, 1, 3);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for record type") && passed;
    if (definition.has_value()) {
        passed =
            expect(definition->line == 1 && definition->column == 1, "expected record definition position") && passed;
    }
    return passed;
}

bool defines_function_parameter() {
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source("fn scale(factor: int): int { return factor * 2; }", {}, {}, 0, 38);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for parameter") && passed;
    if (definition.has_value()) {
        passed = expect(definition->line == 1 && definition->column == 10, "expected parameter definition position") &&
                 passed;
    }
    return passed;
}

bool defines_record_field() {
    const std::optional<dune::lsp::DefinitionLocation> definition = dune::lsp::definition_source(
        with_test_print("record Point { x: int, y: int }\np: Point = Point { x: 1, y: 2 };\nprint(p.x);"), {}, {}, 2,
        8);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for record field") && passed;
    if (definition.has_value()) {
        passed =
            expect(definition->line == 1 && definition->column == 16, "expected field definition position") && passed;
    }
    return passed;
}

bool defines_choice_variant() {
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source("choice Maybe { Present(int), Absent }\nm: Maybe = Absent;", {}, {}, 1, 12);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for choice variant") && passed;
    if (definition.has_value()) {
        passed =
            expect(definition->line == 1 && definition->column == 30, "expected variant definition position") && passed;
    }
    return passed;
}

bool resolves_no_definition_for_literal() {
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source("x = 42;", {}, {}, 0, 5);
    return expect(!definition.has_value(), "expected no definition for a numeric literal");
}

bool defines_module_from_import() {
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source(with_test_print("import math;\nprint(math.square(3));"), {}, {}, 0, 8);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for module import") && passed;
    if (definition.has_value()) {
        passed = expect(definition->uri.find("math.dn") != std::string::npos, "expected module file uri") && passed;
        passed = expect(definition->line == 1, "expected module file top") && passed;
    }
    return passed;
}

bool defines_module_member() {
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source(with_test_print("import math;\nprint(math.square(3));"), {}, {}, 1, 12);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for module member") && passed;
    if (definition.has_value()) {
        passed = expect(definition->uri.find("math.dn") != std::string::npos, "expected member module uri") && passed;
        // Assert the column of the `square` name (after `fn `) rather than a line
        // number, which shifts when stdlib comments change.
        passed = expect(definition->column == 4, "expected square declaration column") && passed;
    }
    return passed;
}

bool defines_receiver_method() {
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source(with_test_print("import matrix;\nv = matrix.vector([1, 2, 3]);\nprint(v.mean());"),
                                     {}, {}, 2, 9);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for receiver method") && passed;
    if (definition.has_value()) {
        passed =
            expect(definition->uri.find("matrix.dn") != std::string::npos, "expected receiver method module uri") &&
            passed;
    }
    return passed;
}

bool hovers_local_doc_comment_with_tags() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("// brief: Squares a value.\n"
                                                "// param value: the number to square\n"
                                                "// returns: value * value\n"
                                                "fn square(value: int): int { return value * value; }\n"
                                                "print(square(3));"),
                                {}, {}, 4, 9);

    bool passed = true;
    passed = expect(hover.has_value(), "expected hover with doc comment") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("square(value: int): int") != std::string::npos,
                        "expected function signature") &&
                 passed;
        passed = expect(hover->contents.find("Squares a value.") != std::string::npos, "expected brief text") && passed;
        passed = expect(hover->contents.find("**Parameters:**") != std::string::npos, "expected parameters section") &&
                 passed;
        passed = expect(hover->contents.find("`value`") != std::string::npos, "expected parameter name") && passed;
        passed = expect(hover->contents.find("**Returns:** value * value") != std::string::npos,
                        "expected returns section") &&
                 passed;
    }
    return passed;
}

bool hovers_plain_comment_as_prose() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("// The running total of every input.\n"
                                                "total: int = 0;\n"
                                                "print(total);"),
                                {}, {}, 2, 7);

    bool passed = true;
    passed = expect(hover.has_value(), "expected hover with prose comment") && passed;
    if (hover.has_value()) {
        passed =
            expect(hover->contents.find("total: int") != std::string::npos, "expected variable signature") && passed;
        passed = expect(hover->contents.find("The running total of every input.") != std::string::npos,
                        "expected prose description") &&
                 passed;
    }
    return passed;
}

bool hovers_module_member_doc_comment() {
    // math.square carries a comment in stdlib/math.dn; hover should surface it.
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("import math;\nprint(math.square(5));"), {}, {}, 1, 13);

    bool passed = true;
    passed = expect(hover.has_value(), "expected module member hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("Square of") != std::string::npos,
                        "expected doc comment from module file in hover") &&
                 passed;
    }
    return passed;
}

bool hovers_record_field_doc_comment() {
    // A comment above a record field shows when hovering `value.field`. The
    // field name is a single glyph, exercising the token-boundary resolution.
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("record Point {\n"
                                                "    // The horizontal coordinate.\n"
                                                "    x: int,\n"
                                                "    y: int,\n"
                                                "}\n"
                                                "p: Point = Point { x: 1, y: 2 };\n"
                                                "print(p.x);"),
                                {}, {}, 6, 8);

    bool passed = true;
    passed = expect(hover.has_value(), "expected record field hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("x: int") != std::string::npos, "expected field type") && passed;
        passed = expect(hover->contents.find("The horizontal coordinate.") != std::string::npos,
                        "expected field doc comment") &&
                 passed;
    }
    return passed;
}

bool hovers_record_method_doc_comment() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("record Counter {\n"
                                                "    value: int,\n"
                                                "\n"
                                                "    // Returns the current value.\n"
                                                "    fn get(): int { return this.value; }\n"
                                                "}\n"
                                                "c: Counter = Counter { value: 5 };\n"
                                                "print(c.get());"),
                                {}, {}, 7, 9);

    bool passed = true;
    passed = expect(hover.has_value(), "expected record method hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("get(") != std::string::npos, "expected method signature") && passed;
        passed = expect(hover->contents.find("Returns the current value.") != std::string::npos,
                        "expected method doc comment") &&
                 passed;
    }
    return passed;
}

bool defines_module_from_from_import() {
    // Regression: `from array import ...` must register `array` as a module so
    // go-to-definition on the module name opens the module file.
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source(with_test_print("from array import range, sum;\nprint(range(1, 3));"), {}, {}, 0,
                                     6);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for from-import module name") && passed;
    if (definition.has_value()) {
        passed = expect(definition->uri.find("array.dn") != std::string::npos, "expected array module uri") && passed;
        passed = expect(definition->line == 1, "expected array module file top") && passed;
    }
    return passed;
}

bool defines_selective_import_symbol() {
    // Go-to-definition on a symbol pulled in unqualified by `from array import`
    // lands on its declaration in the module file.
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source("from array import range, sum;\nvalues = range(1, 3);", {}, {}, 1, 10);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for selective import symbol") && passed;
    if (definition.has_value()) {
        passed =
            expect(definition->uri.find("array.dn") != std::string::npos, "expected selective symbol module uri") &&
            passed;
    }
    return passed;
}

bool hovers_selective_import_symbol() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source("from array import range, sum;\nvalues = range(1, 3);", {}, {}, 1, 10);

    bool passed = true;
    passed = expect(hover.has_value(), "expected hover for selective import symbol") && passed;
    if (hover.has_value()) {
        passed =
            expect(hover->contents.find("range(") != std::string::npos, "expected range signature in hover") && passed;
    }
    return passed;
}

bool defines_aliased_module_name() {
    // `import math as m;` — go-to-definition on the alias opens the module file.
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source(with_test_print("import math as m;\nprint(m.square(3));"), {}, {}, 0, 15);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for module alias") && passed;
    if (definition.has_value()) {
        passed = expect(definition->uri.find("math.dn") != std::string::npos, "expected aliased module uri") && passed;
    }
    return passed;
}

bool hovers_aliased_module_member() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source(with_test_print("import math as m;\nprint(m.square(3));"), {}, {}, 1, 9);

    bool passed = true;
    passed = expect(hover.has_value(), "expected hover for aliased module member") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("square<T is numeric>(value: T): T") != std::string::npos,
                        "expected aliased module function hover") &&
                 passed;
    }
    return passed;
}

bool exposes_complete_semantic_token_legend() {
    const std::vector<std::string>& types = dune::lsp::semantic_token_types();
    const std::vector<std::string>& modifiers = dune::lsp::semantic_token_modifiers();
    bool passed = true;
    for (const std::string_view type :
         {"namespace", "type", "struct", "enum", "interface", "typeParameter", "parameter", "variable", "property",
          "enumMember", "function", "method", "keyword", "comment", "string", "number", "operator", "decorator"}) {
        passed = expect(semantic_name_index(types, type) != std::numeric_limits<std::size_t>::max(),
                        "expected semantic token type in legend") &&
                 passed;
    }
    for (const std::string_view modifier :
         {"declaration", "definition", "readonly", "static", "defaultLibrary", "documentation", "modification"}) {
        passed = expect(semantic_name_index(modifiers, modifier) != std::numeric_limits<std::size_t>::max(),
                        "expected semantic token modifier in legend") &&
                 passed;
    }
    return passed;
}

bool highlights_semantic_symbols_and_literals() {
    const std::string source = "/** Computes a boxed value. */\n"
                               "module sample;\n"
                               "import math as m;\n"
                               "record Box<T> {\n"
                               "    value: T,\n"
                               "    static fn make(value: T): Box<T> { return Box { value: value }; },\n"
                               "    fn get(): T { return this.value; }\n"
                               "}\n"
                               "contract Show { show(value: text): text; }\n"
                               "choice Maybe<T> { Some(T), None }\n"
                               "type IntBox = Box<int>;\n"
                               "foreknown const LIMIT: int = 3;\n"
                               "foreign fn native(value: int): int = \"native\";\n"
                               "fn compute(input: IntBox): int {\n"
                               "    made: Box<int> = Box.make(2);\n"
                               "    local: int = m.square(made.get()) + input.value + LIMIT;\n"
                               "    return native(local);\n"
                               "}\n";
    const std::vector<dune::lsp::SemanticToken> tokens = dune::lsp::semantic_tokens_source(source);
    const std::size_t declaration = semantic_modifier_mask("declaration");
    const std::size_t definition = semantic_modifier_mask("definition");
    const std::size_t readonly = semantic_modifier_mask("readonly");
    const std::size_t static_modifier = semantic_modifier_mask("static");
    const std::size_t default_library = semantic_modifier_mask("defaultLibrary");
    const std::size_t documentation = semantic_modifier_mask("documentation");

    bool passed = true;
    passed = expect_semantic_token(source, tokens, "/** Computes a boxed value. */", 0, 30, "comment", documentation,
                                   "expected documentation comment token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "module sample", 7, 6, "namespace", declaration | definition,
                                   "expected module declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "import math", 7, 4, "namespace", default_library,
                                   "expected standard-library namespace token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "record Box", 7, 3, "struct", declaration | definition,
                                   "expected record declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "Box<T>", 4, 1, "typeParameter", declaration | definition,
                                   "expected generic declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "    value: T", 4, 5, "property", declaration | definition,
                                   "expected field declaration token") &&
             passed;
    passed =
        expect_semantic_token(source, tokens, "fn make", 3, 4, "method", declaration | definition | static_modifier,
                              "expected static method declaration token") &&
        passed;
    passed = expect_semantic_token(source, tokens, "make(value", 5, 5, "parameter", declaration,
                                   "expected parameter declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "return this.value", 7, 4, "parameter", readonly,
                                   "expected receiver parameter reference") &&
             passed;
    passed = expect_semantic_token(source, tokens, "contract Show", 9, 4, "interface", declaration | definition,
                                   "expected contract declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "Some(T)", 0, 4, "enumMember", declaration | definition,
                                   "expected choice variant token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "type IntBox", 5, 6, "type", declaration | definition,
                                   "expected type alias declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "foreknown const LIMIT", 0, 9, "keyword", 0,
                                   "expected foreknown keyword token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "const LIMIT", 6, 5, "variable", declaration | readonly,
                                   "expected readonly constant declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "fn native", 3, 6, "function", declaration | definition,
                                   "expected foreign function token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "\"native\"", 0, 8, "string", 0, "expected string literal token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "local: int", 0, 5, "variable", declaration,
                                   "expected local declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "Box.make", 4, 4, "method", static_modifier,
                                   "expected static method reference token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "m.square", 0, 1, "namespace", default_library,
                                   "expected module reference token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "m.square", 2, 6, "function", default_library,
                                   "expected module function token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "made.get", 5, 3, "method", 0, "expected receiver method token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "input.value", 0, 5, "parameter", 0,
                                   "expected parameter reference token") &&
             passed;
    passed =
        expect_semantic_token(source, tokens, "input.value", 6, 5, "property", 0, "expected member property token") &&
        passed;
    passed = expect_semantic_token(source, tokens, "value + LIMIT", 6, 1, "operator", 0, "expected operator token") &&
             passed;
    passed =
        expect_semantic_token(source, tokens, "= 3;", 2, 1, "number", 0, "expected numeric literal token") && passed;
    passed = expect_semantic_token(source, tokens, "native(local)", 0, 6, "function", 0,
                                   "expected direct function reference token") &&
             passed;
    return passed;
}

bool highlights_lambda_parameters_captures_and_calls() {
    const std::string source = "factor: int = 3;\n"
                               "scale: fn(int): int = fn(value: int): int { value * factor };\n"
                               "result = scale(4);\n";
    const std::vector<dune::lsp::SemanticToken> tokens = dune::lsp::semantic_tokens_source(source);
    const std::size_t declaration = semantic_modifier_mask("declaration");
    bool passed = true;
    passed = expect_semantic_token(source, tokens, "fn(value", 3, 5, "parameter", declaration,
                                   "expected lambda parameter declaration token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "{ value *", 2, 5, "parameter", 0,
                                   "expected lambda parameter reference token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "* factor", 2, 6, "variable", 0,
                                   "expected captured variable reference token") &&
             passed;
    passed =
        expect_semantic_token(source, tokens, "scale(4)", 0, 5, "variable", 0, "expected function-value call token") &&
        passed;
    return passed;
}

bool highlights_utf8_with_utf16_ranges() {
    const std::string source = "// 🙂 docs\nvalue: text = \"é\" + \"🙂\";\n";
    const std::vector<dune::lsp::SemanticToken> tokens = dune::lsp::semantic_tokens_source(source);
    bool passed = true;
    passed = expect_semantic_token(source, tokens, "// 🙂 docs", 0, 12, "comment", 0,
                                   "expected UTF-8 comment semantic token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "\"é\"", 0, 4, "string", 0,
                                   "expected UTF-8 text literal semantic token") &&
             passed;
    passed = expect_semantic_token(source, tokens, "\"🙂\"", 0, 6, "string", 0,
                                   "expected astral text literal semantic token") &&
             passed;

    const std::size_t second_string = source.find("\"🙂\"");
    const std::optional<dune::lsp::SemanticToken> token =
        semantic_token_at_byte_offset(source, tokens, second_string, std::string("\"🙂\"").size());
    passed = expect(token.has_value() && token->length == 4, "expected astral literal length in UTF-16 code units") &&
             passed;
    return passed;
}

bool highlights_valid_prefix_of_incomplete_source() {
    const std::string source = "foreknown fn unfinished(value: text";
    const std::vector<dune::lsp::SemanticToken> tokens = dune::lsp::semantic_tokens_source(source);
    const std::size_t declaration = semantic_modifier_mask("declaration");
    const std::size_t default_library = semantic_modifier_mask("defaultLibrary");
    bool passed = true;
    passed = expect_semantic_token(source, tokens, "foreknown", 0, 9, "keyword", 0,
                                   "expected keyword in incomplete source") &&
             passed;
    passed = expect_semantic_token(source, tokens, "fn unfinished", 3, 10, "function", declaration,
                                   "expected function in incomplete source") &&
             passed;
    passed = expect_semantic_token(source, tokens, "text", 0, 4, "type", default_library,
                                   "expected builtin type in incomplete source") &&
             passed;
    return passed;
}

bool foreknown_keyword_has_no_definition() {
    const std::string source = "foreknown const LIMIT: int = 3;\nvalue: int = LIMIT;";
    bool passed = true;
    passed = expect(!dune::lsp::definition_source(source, {}, {}, 0, 2).has_value(),
                    "expected no definition for foreknown keyword") &&
             passed;
    passed = expect(!dune::lsp::definition_source(source, {}, {}, 0, 11).has_value(),
                    "expected no definition for const keyword") &&
             passed;
    return passed;
}

bool serves_lsp_semantic_tokens_and_keyword_definition_regression() {
    const std::string uri = "file:///tmp/semantic.dn";
    const std::string source = "foreknown const LIMIT: int = 3;\\nvalue: int = LIMIT;";
    const std::string initialize = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string opened = R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")" +
                               uri + R"(","languageId":"dune","version":1,"text":")" + source + R"("}}})";
    const std::string semantic =
        R"({"jsonrpc":"2.0","id":2,"method":"textDocument/semanticTokens/full","params":{"textDocument":{"uri":")" +
        uri + R"("}}})";
    const std::string definition =
        R"({"jsonrpc":"2.0","id":3,"method":"textDocument/definition","params":{"textDocument":{"uri":")" + uri +
        R"("},"position":{"line":0,"character":2}}})";
    const std::string shutdown = R"({"jsonrpc":"2.0","id":4,"method":"shutdown","params":null})";
    const std::string exit = R"({"jsonrpc":"2.0","method":"exit","params":null})";

    std::istringstream input(lsp_message(initialize) + lsp_message(opened) + lsp_message(semantic) +
                             lsp_message(definition) + lsp_message(shutdown) + lsp_message(exit));
    std::ostringstream output;
    dune::lsp::run(input, output);
    const std::string text = output.str();

    bool passed = true;
    passed =
        expect(text.find("\"semanticTokensProvider\"") != std::string::npos, "expected semantic tokens capability") &&
        passed;
    passed =
        expect(text.find("\"tokenTypes\":[\"namespace\"") != std::string::npos, "expected semantic token legend") &&
        passed;
    passed = expect(text.find("\"id\":2,\"result\":{\"data\":[") != std::string::npos,
                    "expected full semantic token response") &&
             passed;
    const std::string keyword_data =
        "\"data\":[0,0,9," + std::to_string(semantic_name_index(dune::lsp::semantic_token_types(), "keyword")) + ",0";
    passed = expect(text.find(keyword_data) != std::string::npos,
                    "expected delta-encoded foreknown keyword semantic token") &&
             passed;
    passed = expect(text.find("\"id\":3,\"result\":null") != std::string::npos,
                    "expected null definition response for foreknown") &&
             passed;
    return passed;
}

bool serves_lsp_definition() {
    const std::string uri = "file:///tmp/main.dn";
    const std::string source = "fn value(): int { return 7; }\\nprint(value());\\n"
                               "fn print<T>(value: T): unit { return; }";
    const std::string initialize = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string opened = R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")" +
                               uri + R"(","languageId":"dune","version":1,"text":")" + source + R"("}}})";
    const std::string definition =
        R"({"jsonrpc":"2.0","id":2,"method":"textDocument/definition","params":{"textDocument":{"uri":")" + uri +
        R"("},"position":{"line":1,"character":8}}})";
    const std::string shutdown = R"({"jsonrpc":"2.0","id":3,"method":"shutdown","params":null})";
    const std::string exit = R"({"jsonrpc":"2.0","method":"exit","params":null})";

    std::istringstream input(lsp_message(initialize) + lsp_message(opened) + lsp_message(definition) +
                             lsp_message(shutdown) + lsp_message(exit));
    std::ostringstream output;
    dune::lsp::run(input, output);
    const std::string text = output.str();

    bool passed = true;
    passed = expect(text.find("\"definitionProvider\":true") != std::string::npos, "expected definition capability") &&
             passed;
    passed = expect(text.find("\"uri\":\"" + uri + "\"") != std::string::npos, "expected definition uri") && passed;
    passed = expect(text.find("\"range\":{\"start\":{\"line\":0,\"character\":3}") != std::string::npos,
                    "expected definition range at the function name") &&
             passed;
    return passed;
}

bool publishes_lsp_diagnostics() {
    const std::string uri = "file:///tmp/bad.dn";
    const std::string initialize = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string opened = R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")" +
                               uri + R"(","languageId":"dune","version":1,"text":"x: int = true;"}}})";
    const std::string shutdown = R"({"jsonrpc":"2.0","id":2,"method":"shutdown","params":null})";
    const std::string exit = R"({"jsonrpc":"2.0","method":"exit","params":null})";

    std::istringstream input(lsp_message(initialize) + lsp_message(opened) + lsp_message(shutdown) + lsp_message(exit));
    std::ostringstream output;
    dune::lsp::run(input, output);
    const std::string text = output.str();

    bool passed = true;
    passed = expect(text.find("\"capabilities\"") != std::string::npos, "expected initialize response") && passed;
    passed = expect(text.find("textDocument/publishDiagnostics") != std::string::npos,
                    "expected diagnostics notification") &&
             passed;
    passed =
        expect(text.find("expected type 'int' but got 'bool'") != std::string::npos, "expected diagnostic message") &&
        passed;
    passed = expect(text.find("\"severity\":1") != std::string::npos, "expected error severity") && passed;
    return passed;
}

bool serves_lsp_completions_and_hover() {
    const std::string uri = "file:///tmp/main.dn";
    const std::string source = "import math;\\ntotal: int = 42;\\nprint(math.);\\nprint(total);\\n"
                               "fn print<T>(value: T): unit { return; }";
    const std::string initialize = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string opened = R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")" +
                               uri + R"(","languageId":"dune","version":1,"text":")" + source + R"("}}})";
    const std::string completion =
        R"({"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":")" + uri +
        R"("},"position":{"line":2,"character":11}}})";
    const std::string hover =
        R"({"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{"textDocument":{"uri":")" + uri +
        R"("},"position":{"line":3,"character":8}}})";
    const std::string shutdown = R"({"jsonrpc":"2.0","id":4,"method":"shutdown","params":null})";
    const std::string exit = R"({"jsonrpc":"2.0","method":"exit","params":null})";

    std::istringstream input(lsp_message(initialize) + lsp_message(opened) + lsp_message(completion) +
                             lsp_message(hover) + lsp_message(shutdown) + lsp_message(exit));
    std::ostringstream output;
    dune::lsp::run(input, output);
    const std::string text = output.str();

    bool passed = true;
    passed =
        expect(text.find("\"completionProvider\"") != std::string::npos, "expected completion capability") && passed;
    passed = expect(text.find("\"hoverProvider\":true") != std::string::npos, "expected hover capability") && passed;
    passed = expect(text.find("\"label\":\"square\"") != std::string::npos, "expected completion response") && passed;
    passed = expect(text.find("total: int") != std::string::npos, "expected hover response") && passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = diagnoses_valid_source() && passed;
    passed = diagnoses_type_errors_with_range() && passed;
    passed = diagnoses_non_exhaustive_when_with_missing_variants() && passed;
    passed = completes_keywords_and_local_symbols() && passed;
    passed = supports_defer_diagnostics_and_semantic_highlighting() && passed;
    passed = completes_imported_module_members() && passed;
    passed = completes_typed_record_methods() && passed;
    passed = hovers_local_symbols() && passed;
    passed = hovers_generic_type_aliases() && passed;
    passed = hovers_for_in_loop_variable() && passed;
    passed = hovers_known_record_for_in_loop_variable() && passed;
    passed = hovers_typed_record_methods() && passed;
    passed = hovers_imported_module_members() && passed;
    passed = hovers_inferred_call_assignments() && passed;
    passed = defines_function_from_call() && passed;
    passed = defines_local_variable() && passed;
    passed = defines_record_type() && passed;
    passed = defines_function_parameter() && passed;
    passed = defines_record_field() && passed;
    passed = defines_choice_variant() && passed;
    passed = resolves_no_definition_for_literal() && passed;
    passed = defines_module_from_import() && passed;
    passed = defines_module_member() && passed;
    passed = defines_receiver_method() && passed;
    passed = hovers_local_doc_comment_with_tags() && passed;
    passed = hovers_plain_comment_as_prose() && passed;
    passed = hovers_module_member_doc_comment() && passed;
    passed = hovers_record_field_doc_comment() && passed;
    passed = hovers_record_method_doc_comment() && passed;
    passed = defines_module_from_from_import() && passed;
    passed = defines_selective_import_symbol() && passed;
    passed = hovers_selective_import_symbol() && passed;
    passed = defines_aliased_module_name() && passed;
    passed = hovers_aliased_module_member() && passed;
    passed = exposes_complete_semantic_token_legend() && passed;
    passed = highlights_semantic_symbols_and_literals() && passed;
    passed = highlights_lambda_parameters_captures_and_calls() && passed;
    passed = highlights_utf8_with_utf16_ranges() && passed;
    passed = highlights_valid_prefix_of_incomplete_source() && passed;
    passed = foreknown_keyword_has_no_definition() && passed;
    passed = serves_lsp_semantic_tokens_and_keyword_definition_regression() && passed;
    passed = serves_lsp_definition() && passed;
    passed = publishes_lsp_diagnostics() && passed;
    passed = serves_lsp_completions_and_hover() && passed;
    return passed ? 0 : 1;
}
