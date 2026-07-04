#include "lsp/lsp_server.hpp"

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
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

bool completes_keywords_and_local_symbols() {
    const std::vector<dune::lsp::CompletionItem> completions =
        dune::lsp::complete_source("fn add(a: int, b: int): int { return a + b; }\ntotal: int = add(10, 20);");

    bool passed = true;
    passed = expect(has_completion(completions, "method"), "expected keyword completion") && passed;
    passed = expect(has_completion(completions, "in"), "expected for-in keyword completion") && passed;
    passed = expect(has_completion(completions, "fn"), "expected fn keyword completion") && passed;
    passed = expect(has_completion(completions, "real64"), "expected type completion") && passed;
    passed = expect(has_completion(completions, "add"), "expected function completion") && passed;
    passed = expect(has_completion(completions, "total"), "expected local variable completion") && passed;
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
        dune::lsp::hover_source("total: int = 42;\nprint(total);", {}, {}, 1, 7);

    bool passed = true;
    passed = expect(hover.has_value(), "expected hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("total: int") != std::string::npos, "expected variable hover") && passed;
    }
    return passed;
}

bool hovers_for_in_loop_variable() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source("values: [int] = [1, 2, 3];\nfor value in values { print(value); }", {}, {}, 1, 30);

    bool passed = true;
    passed = expect(hover.has_value(), "expected for-in loop variable hover") && passed;
    if (hover.has_value()) {
        passed =
            expect(hover->contents.find("value: int") != std::string::npos, "expected for-in variable hover type") &&
            passed;
    }
    return passed;
}

bool hovers_typed_record_methods() {
    const std::optional<dune::lsp::Hover> hover = dune::lsp::hover_source(
        "import matrix;\nvalues = matrix.vector([1, 2, 3]);\nprint(values.mean());", {}, {}, 2, 14);

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
        dune::lsp::hover_source("import math;\nprint(math.square(5));", {}, {}, 1, 13);

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
        dune::lsp::hover_source("import matrix;\nleft = matrix.vector([1, 2]);\n"
                                "right = matrix.vector([1, 2, 3]);\nprint(left.dot(right));",
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
        dune::lsp::definition_source("total: int = 42;\nprint(total);", {}, {}, 1, 7);

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
        "record Point { x: int, y: int }\np: Point = Point { x: 1, y: 2 };\nprint(p.x);", {}, {}, 2, 8);

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
        dune::lsp::definition_source("import math;\nprint(math.square(3));", {}, {}, 0, 8);

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
        dune::lsp::definition_source("import math;\nprint(math.square(3));", {}, {}, 1, 12);

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
        dune::lsp::definition_source("import matrix;\nv = matrix.vector([1, 2, 3]);\nprint(v.mean());", {}, {}, 2, 9);

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
        dune::lsp::hover_source("// brief: Squares a value.\n"
                                "// param value: the number to square\n"
                                "// returns: value * value\n"
                                "fn square(value: int): int { return value * value; }\n"
                                "print(square(3));",
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
    const std::optional<dune::lsp::Hover> hover = dune::lsp::hover_source("// The running total of every input.\n"
                                                                          "total: int = 0;\n"
                                                                          "print(total);",
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
        dune::lsp::hover_source("import math;\nprint(math.square(5));", {}, {}, 1, 13);

    bool passed = true;
    passed = expect(hover.has_value(), "expected module member hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("Square of") != std::string::npos,
                        "expected doc comment from module file in hover") &&
                 passed;
    }
    return passed;
}

bool defines_module_from_from_import() {
    // Regression: `from array import ...` must register `array` as a module so
    // go-to-definition on the module name opens the module file.
    const std::optional<dune::lsp::DefinitionLocation> definition =
        dune::lsp::definition_source("from array import range, sum;\nprint(range(1, 3));", {}, {}, 0, 6);

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
        dune::lsp::definition_source("import math as m;\nprint(m.square(3));", {}, {}, 0, 15);

    bool passed = true;
    passed = expect(definition.has_value(), "expected definition for module alias") && passed;
    if (definition.has_value()) {
        passed = expect(definition->uri.find("math.dn") != std::string::npos, "expected aliased module uri") && passed;
    }
    return passed;
}

bool hovers_aliased_module_member() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source("import math as m;\nprint(m.square(3));", {}, {}, 1, 9);

    bool passed = true;
    passed = expect(hover.has_value(), "expected hover for aliased module member") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("square<T is numeric>(value: T): T") != std::string::npos,
                        "expected aliased module function hover") &&
                 passed;
    }
    return passed;
}

bool serves_lsp_definition() {
    const std::string uri = "file:///tmp/main.dn";
    const std::string source = "fn value(): int { return 7; }\\nprint(value());";
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
    const std::string source = "import math;\\ntotal: int = 42;\\nprint(math.);\\nprint(total);";
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
    passed = completes_keywords_and_local_symbols() && passed;
    passed = completes_imported_module_members() && passed;
    passed = completes_typed_record_methods() && passed;
    passed = hovers_local_symbols() && passed;
    passed = hovers_for_in_loop_variable() && passed;
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
    passed = defines_module_from_from_import() && passed;
    passed = defines_selective_import_symbol() && passed;
    passed = hovers_selective_import_symbol() && passed;
    passed = defines_aliased_module_name() && passed;
    passed = hovers_aliased_module_member() && passed;
    passed = serves_lsp_definition() && passed;
    passed = publishes_lsp_diagnostics() && passed;
    passed = serves_lsp_completions_and_hover() && passed;
    return passed ? 0 : 1;
}
