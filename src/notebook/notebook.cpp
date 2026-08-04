#include "notebook.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace dune::notebook {

namespace {

struct JsonValue {
    enum class Kind {
        null_value,
        boolean,
        number,
        string,
        array,
        object,
    };

    Kind kind = Kind::null_value;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view source) : source_(source) {}

    JsonValue parse() {
        JsonValue value = parse_value();
        skip_whitespace();
        if (position_ != source_.size()) {
            fail("unexpected trailing JSON content");
        }
        return value;
    }

private:
    JsonValue parse_value() {
        skip_whitespace();
        if (position_ >= source_.size()) {
            fail("expected JSON value");
        }

        switch (source_[position_]) {
        case 'n':
            consume_literal("null");
            return {};
        case 't':
            consume_literal("true");
            return JsonValue{JsonValue::Kind::boolean, true};
        case 'f':
            consume_literal("false");
            return JsonValue{JsonValue::Kind::boolean, false};
        case '"': {
            JsonValue value;
            value.kind = JsonValue::Kind::string;
            value.string = parse_string();
            return value;
        }
        case '[':
            return parse_array();
        case '{':
            return parse_object();
        default:
            if (source_[position_] == '-' || std::isdigit(static_cast<unsigned char>(source_[position_])) != 0) {
                return parse_number();
            }
            fail("expected JSON value");
        }
    }

    JsonValue parse_array() {
        ++position_;
        JsonValue value;
        value.kind = JsonValue::Kind::array;
        skip_whitespace();
        if (consume(']')) {
            return value;
        }

        while (true) {
            value.array.push_back(parse_value());
            skip_whitespace();
            if (consume(']')) {
                return value;
            }
            expect(',', "expected ',' between array items");
        }
    }

    JsonValue parse_object() {
        ++position_;
        JsonValue value;
        value.kind = JsonValue::Kind::object;
        skip_whitespace();
        if (consume('}')) {
            return value;
        }

        while (true) {
            skip_whitespace();
            if (position_ >= source_.size() || source_[position_] != '"') {
                fail("expected JSON object key");
            }
            const std::string key = parse_string();
            skip_whitespace();
            expect(':', "expected ':' after JSON object key");
            value.object.insert_or_assign(key, parse_value());
            skip_whitespace();
            if (consume('}')) {
                return value;
            }
            expect(',', "expected ',' between object members");
        }
    }

    JsonValue parse_number() {
        const std::size_t start = position_;
        if (source_[position_] == '-') {
            ++position_;
        }
        if (position_ >= source_.size()) {
            fail("incomplete JSON number");
        }
        if (source_[position_] == '0') {
            ++position_;
        } else {
            require_digits("expected digits in JSON number");
        }
        if (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            require_digits("expected digits after decimal point");
        }
        if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
            ++position_;
            if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) {
                ++position_;
            }
            require_digits("expected exponent digits");
        }

        JsonValue value;
        value.kind = JsonValue::Kind::number;
        try {
            value.number = std::stod(std::string(source_.substr(start, position_ - start)));
        } catch (const std::exception&) {
            fail("invalid JSON number");
        }
        if (!std::isfinite(value.number)) {
            fail("JSON number must be finite");
        }
        return value;
    }

    std::string parse_string() {
        expect('"', "expected JSON string");
        std::string value;
        while (position_ < source_.size()) {
            const char current = source_[position_++];
            if (current == '"') {
                return value;
            }
            if (static_cast<unsigned char>(current) < 0x20) {
                fail("control character in JSON string");
            }
            if (current != '\\') {
                value += current;
                continue;
            }
            if (position_ >= source_.size()) {
                fail("incomplete JSON escape");
            }

            const char escaped = source_[position_++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                value += escaped;
                break;
            case 'b':
                value += '\b';
                break;
            case 'f':
                value += '\f';
                break;
            case 'n':
                value += '\n';
                break;
            case 'r':
                value += '\r';
                break;
            case 't':
                value += '\t';
                break;
            case 'u':
                append_utf8(value, parse_codepoint());
                break;
            default:
                fail("invalid JSON escape");
            }
        }
        fail("unterminated JSON string");
    }

    unsigned int parse_codepoint() {
        unsigned int value = parse_hex_quad();
        if (value >= 0xd800 && value <= 0xdbff) {
            if (position_ + 1 >= source_.size() || source_[position_] != '\\' || source_[position_ + 1] != 'u') {
                fail("high surrogate must be followed by a low surrogate");
            }
            position_ += 2;
            const unsigned int low = parse_hex_quad();
            if (low < 0xdc00 || low > 0xdfff) {
                fail("invalid low surrogate");
            }
            value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
        } else if (value >= 0xdc00 && value <= 0xdfff) {
            fail("unexpected low surrogate");
        }
        return value;
    }

    unsigned int parse_hex_quad() {
        if (position_ + 4 > source_.size()) {
            fail("incomplete JSON unicode escape");
        }
        unsigned int value = 0;
        for (int index = 0; index < 4; ++index) {
            const char character = source_[position_++];
            value <<= 4;
            if (character >= '0' && character <= '9') {
                value += static_cast<unsigned int>(character - '0');
            } else if (character >= 'a' && character <= 'f') {
                value += static_cast<unsigned int>(character - 'a' + 10);
            } else if (character >= 'A' && character <= 'F') {
                value += static_cast<unsigned int>(character - 'A' + 10);
            } else {
                fail("invalid JSON unicode escape");
            }
        }
        return value;
    }

    static void append_utf8(std::string& output, unsigned int codepoint) {
        if (codepoint <= 0x7f) {
            output += static_cast<char>(codepoint);
        } else if (codepoint <= 0x7ff) {
            output += static_cast<char>(0xc0 | (codepoint >> 6));
            output += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else if (codepoint <= 0xffff) {
            output += static_cast<char>(0xe0 | (codepoint >> 12));
            output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            output += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else {
            output += static_cast<char>(0xf0 | (codepoint >> 18));
            output += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
            output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            output += static_cast<char>(0x80 | (codepoint & 0x3f));
        }
    }

    void consume_literal(std::string_view literal) {
        if (source_.substr(position_, literal.size()) != literal) {
            fail("invalid JSON literal");
        }
        position_ += literal.size();
    }

    void require_digits(std::string_view message) {
        const std::size_t start = position_;
        while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_])) != 0) {
            ++position_;
        }
        if (start == position_) {
            fail(message);
        }
    }

    bool consume(char character) {
        if (position_ < source_.size() && source_[position_] == character) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char character, std::string_view message) {
        if (!consume(character)) {
            fail(message);
        }
    }

    void skip_whitespace() {
        while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_])) != 0) {
            ++position_;
        }
    }

    [[noreturn]] void fail(std::string_view message) const {
        throw std::runtime_error("invalid .dnb JSON at byte " + std::to_string(position_ + 1) + ": " +
                                 std::string(message));
    }

    std::string_view source_;
    std::size_t position_ = 0;
};

const JsonValue& require_member(const JsonValue& object, std::string_view name) {
    if (object.kind != JsonValue::Kind::object) {
        throw std::runtime_error("invalid .dnb: expected JSON object");
    }
    const auto member = object.object.find(std::string(name));
    if (member == object.object.end()) {
        throw std::runtime_error("invalid .dnb: missing '" + std::string(name) + "'");
    }
    return member->second;
}

const JsonValue* find_member(const JsonValue& object, std::string_view name) {
    if (object.kind != JsonValue::Kind::object) {
        return nullptr;
    }
    const auto member = object.object.find(std::string(name));
    return member == object.object.end() ? nullptr : &member->second;
}

std::string require_string(const JsonValue& value, std::string_view field) {
    if (value.kind == JsonValue::Kind::string) {
        return value.string;
    }
    throw std::runtime_error("invalid .dnb: '" + std::string(field) + "' must be a string");
}

std::string require_source(const JsonValue& value) {
    if (value.kind == JsonValue::Kind::string) {
        return value.string;
    }
    if (value.kind == JsonValue::Kind::array) {
        std::string joined;
        for (const JsonValue& line : value.array) {
            if (line.kind != JsonValue::Kind::string) {
                throw std::runtime_error("invalid .dnb: 'source' array must contain strings");
            }
            joined += line.string;
        }
        return joined;
    }
    throw std::runtime_error("invalid .dnb: 'source' must be a string or an array of strings");
}

std::size_t require_unsigned(const JsonValue& value, std::string_view field) {
    if (value.kind != JsonValue::Kind::number || value.number < 0 || std::floor(value.number) != value.number ||
        value.number >= static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("invalid .dnb: '" + std::string(field) + "' must be a non-negative integer");
    }
    return static_cast<std::size_t>(value.number);
}

std::string json_escape(std::string_view text) {
    std::string escaped;
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char character : text) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (character < 0x20) {
                escaped += "\\u00";
                escaped += digits[character >> 4];
                escaped += digits[character & 0x0f];
            } else {
                escaped += static_cast<char>(character);
            }
            break;
        }
    }
    return escaped;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open notebook '" + path.string() + "'");
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string html_escape(std::string_view text) {
    std::string escaped;
    for (const char character : text) {
        switch (character) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

std::string_view trim_whitespace(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

bool is_svg_output(std::string_view text) {
    text = trim_whitespace(text);
    return text.size() > 10 && text.starts_with("<svg") &&
           (text[4] == '>' || std::isspace(static_cast<unsigned char>(text[4])) != 0) && text.ends_with("</svg>");
}

std::string svg_data_uri(std::string_view svg) {
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string encoded = "data:image/svg+xml;charset=utf-8,";
    svg = trim_whitespace(svg);
    encoded.reserve(encoded.size() + svg.size() * 3);
    for (const unsigned char character : svg) {
        if (std::isalnum(character) != 0 || character == '-' || character == '.' || character == '_' ||
            character == '~') {
            encoded += static_cast<char>(character);
        } else {
            encoded += '%';
            encoded += digits[character >> 4];
            encoded += digits[character & 0x0f];
        }
    }
    return encoded;
}

struct MathNode {
    std::string html;
    bool movable_limits = false;
};

const std::unordered_map<std::string, std::string>& latex_symbols() {
    static const std::unordered_map<std::string, std::string> symbols = {
        {"alpha", "α"},
        {"beta", "β"},
        {"gamma", "γ"},
        {"delta", "δ"},
        {"epsilon", "ϵ"},
        {"varepsilon", "ε"},
        {"zeta", "ζ"},
        {"eta", "η"},
        {"theta", "θ"},
        {"vartheta", "ϑ"},
        {"iota", "ι"},
        {"kappa", "κ"},
        {"lambda", "λ"},
        {"mu", "μ"},
        {"nu", "ν"},
        {"xi", "ξ"},
        {"omicron", "ο"},
        {"pi", "π"},
        {"varpi", "ϖ"},
        {"rho", "ρ"},
        {"varrho", "ϱ"},
        {"sigma", "σ"},
        {"varsigma", "ς"},
        {"tau", "τ"},
        {"upsilon", "υ"},
        {"phi", "ϕ"},
        {"varphi", "φ"},
        {"chi", "χ"},
        {"psi", "ψ"},
        {"omega", "ω"},
        {"Gamma", "Γ"},
        {"Delta", "Δ"},
        {"Theta", "Θ"},
        {"Lambda", "Λ"},
        {"Xi", "Ξ"},
        {"Pi", "Π"},
        {"Sigma", "Σ"},
        {"Upsilon", "Υ"},
        {"Phi", "Φ"},
        {"Psi", "Ψ"},
        {"Omega", "Ω"},
        {"pm", "±"},
        {"mp", "∓"},
        {"times", "×"},
        {"div", "÷"},
        {"cdot", "⋅"},
        {"ast", "∗"},
        {"star", "⋆"},
        {"circ", "∘"},
        {"bullet", "•"},
        {"oplus", "⊕"},
        {"otimes", "⊗"},
        {"le", "≤"},
        {"leq", "≤"},
        {"ge", "≥"},
        {"geq", "≥"},
        {"ne", "≠"},
        {"neq", "≠"},
        {"approx", "≈"},
        {"sim", "∼"},
        {"simeq", "≃"},
        {"equiv", "≡"},
        {"propto", "∝"},
        {"ll", "≪"},
        {"gg", "≫"},
        {"in", "∈"},
        {"notin", "∉"},
        {"ni", "∋"},
        {"subset", "⊂"},
        {"supset", "⊃"},
        {"subseteq", "⊆"},
        {"supseteq", "⊇"},
        {"cup", "∪"},
        {"cap", "∩"},
        {"setminus", "∖"},
        {"emptyset", "∅"},
        {"forall", "∀"},
        {"exists", "∃"},
        {"nexists", "∄"},
        {"neg", "¬"},
        {"land", "∧"},
        {"lor", "∨"},
        {"wedge", "∧"},
        {"vee", "∨"},
        {"to", "→"},
        {"rightarrow", "→"},
        {"leftarrow", "←"},
        {"leftrightarrow", "↔"},
        {"Rightarrow", "⇒"},
        {"Leftarrow", "⇐"},
        {"Leftrightarrow", "⇔"},
        {"mapsto", "↦"},
        {"uparrow", "↑"},
        {"downarrow", "↓"},
        {"partial", "∂"},
        {"nabla", "∇"},
        {"infty", "∞"},
        {"ell", "ℓ"},
        {"hbar", "ℏ"},
        {"Re", "ℜ"},
        {"Im", "ℑ"},
        {"aleph", "ℵ"},
        {"prime", "′"},
        {"angle", "∠"},
        {"perp", "⊥"},
        {"parallel", "∥"},
        {"mid", "|"},
        {"langle", "⟨"},
        {"rangle", "⟩"},
        {"lceil", "⌈"},
        {"rceil", "⌉"},
        {"lfloor", "⌊"},
        {"rfloor", "⌋"},
        {"ldots", "…"},
        {"cdots", "⋯"},
        {"vdots", "⋮"},
        {"ddots", "⋱"},
        {"therefore", "∴"},
        {"because", "∵"},
        {"sum", "∑"},
        {"prod", "∏"},
        {"coprod", "∐"},
        {"int", "∫"},
        {"iint", "∬"},
        {"iiint", "∭"},
        {"oint", "∮"},
        {"bigcup", "⋃"},
        {"bigcap", "⋂"},
        {"bigvee", "⋁"},
        {"bigwedge", "⋀"},
    };
    return symbols;
}

const std::unordered_set<std::string>& latex_named_operators() {
    static const std::unordered_set<std::string> operators = {
        "sin",  "cos",  "tan", "cot", "sec", "csc", "arcsin", "arccos", "arctan", "sinh",
        "cosh", "tanh", "log", "ln",  "exp", "det", "dim",    "gcd",    "ker",    "hom",
        "arg",  "deg",  "min", "max", "inf", "sup", "lim",    "liminf", "limsup", "Pr",
    };
    return operators;
}

const std::unordered_set<std::string>& latex_limit_operators() {
    static const std::unordered_set<std::string> operators = {
        "sum", "prod",   "coprod", "bigcup", "bigcap", "bigvee", "bigwedge",
        "lim", "liminf", "limsup", "min",    "max",    "inf",    "sup",
    };
    return operators;
}

class LatexMathParser {
public:
    explicit LatexMathParser(std::string_view source) : source_(source) {}

    std::string parse() {
        return parse_expression('\0');
    }

private:
    std::string parse_expression(char closing) {
        std::string result;
        while (position_ < source_.size()) {
            skip_spaces();
            if (position_ >= source_.size()) {
                break;
            }
            if (closing != '\0' && source_[position_] == closing) {
                ++position_;
                break;
            }
            result += parse_scripted_atom().html;
        }
        return result;
    }

    MathNode parse_scripted_atom() {
        MathNode base = parse_atom();
        skip_spaces();
        if (source_.substr(position_).starts_with("\\limits")) {
            position_ += 7;
            skip_spaces();
            base.movable_limits = true;
        }

        std::optional<std::string> lower;
        std::optional<std::string> upper;
        while (position_ < source_.size() && (source_[position_] == '_' || source_[position_] == '^')) {
            const char marker = source_[position_++];
            MathNode argument = parse_argument();
            if (marker == '_') {
                lower = std::move(argument.html);
            } else {
                upper = std::move(argument.html);
            }
            skip_spaces();
        }
        if (!lower && !upper) {
            return base;
        }
        if (lower && upper) {
            const std::string tag = base.movable_limits ? "munderover" : "msubsup";
            return {"<" + tag + ">" + base.html + "<mrow>" + *lower + "</mrow><mrow>" + *upper + "</mrow></" + tag +
                        ">",
                    false};
        }
        const bool is_lower = lower.has_value();
        const std::string tag = base.movable_limits ? (is_lower ? "munder" : "mover") : (is_lower ? "msub" : "msup");
        const std::string& script = is_lower ? *lower : *upper;
        return {"<" + tag + ">" + base.html + "<mrow>" + script + "</mrow></" + tag + ">", false};
    }

    MathNode parse_argument() {
        skip_spaces();
        if (position_ < source_.size() && source_[position_] == '{') {
            ++position_;
            return {"<mrow>" + parse_expression('}') + "</mrow>", false};
        }
        return parse_scripted_atom();
    }

    MathNode parse_atom() {
        if (position_ >= source_.size()) {
            return {};
        }
        const char character = source_[position_];
        if (character == '{') {
            ++position_;
            return {"<mrow>" + parse_expression('}') + "</mrow>", false};
        }
        if (character == '\\') {
            return parse_command();
        }
        if (std::isdigit(static_cast<unsigned char>(character)) != 0 ||
            (character == '.' && position_ + 1 < source_.size() &&
             std::isdigit(static_cast<unsigned char>(source_[position_ + 1])) != 0)) {
            const std::size_t start = position_++;
            while (position_ < source_.size() &&
                   (std::isdigit(static_cast<unsigned char>(source_[position_])) != 0 || source_[position_] == '.')) {
                ++position_;
            }
            return {"<mn>" + html_escape(source_.substr(start, position_ - start)) + "</mn>", false};
        }
        if (std::isalpha(static_cast<unsigned char>(character)) != 0) {
            ++position_;
            return {"<mi>" + html_escape(source_.substr(position_ - 1, 1)) + "</mi>", false};
        }

        const std::size_t glyph_size = utf8_glyph_size(static_cast<unsigned char>(character));
        const std::size_t available = std::min(glyph_size, source_.size() - position_);
        const std::string glyph = html_escape(source_.substr(position_, available));
        position_ += available;
        if (std::string_view("+-*/=(),[]<>|:;!").find(character) != std::string_view::npos) {
            return {"<mo>" + (character == '-' ? std::string("−") : glyph) + "</mo>", false};
        }
        if (character == '\'') {
            return {"<mo>′</mo>", false};
        }
        return {"<mi>" + glyph + "</mi>", false};
    }

    MathNode parse_command() {
        ++position_;
        if (position_ >= source_.size()) {
            return {"<mo>\\</mo>", false};
        }
        std::string command;
        if (std::isalpha(static_cast<unsigned char>(source_[position_])) != 0) {
            const std::size_t start = position_;
            while (position_ < source_.size() && std::isalpha(static_cast<unsigned char>(source_[position_])) != 0) {
                ++position_;
            }
            command = std::string(source_.substr(start, position_ - start));
        } else {
            command.assign(1, source_[position_++]);
        }

        if (command == "frac" || command == "dfrac" || command == "tfrac") {
            const MathNode numerator = parse_argument();
            const MathNode denominator = parse_argument();
            return {"<mfrac><mrow>" + numerator.html + "</mrow><mrow>" + denominator.html + "</mrow></mfrac>", false};
        }
        if (command == "binom") {
            const MathNode upper = parse_argument();
            const MathNode lower = parse_argument();
            return {"<mrow><mo stretchy=\"true\">(</mo><mfrac linethickness=\"0\"><mrow>" + upper.html +
                        "</mrow><mrow>" + lower.html + "</mrow></mfrac><mo stretchy=\"true\">)</mo></mrow>",
                    false};
        }
        if (command == "sqrt") {
            skip_spaces();
            std::optional<std::string> index;
            if (position_ < source_.size() && source_[position_] == '[') {
                ++position_;
                const std::size_t start = position_;
                while (position_ < source_.size() && source_[position_] != ']') {
                    ++position_;
                }
                index = LatexMathParser(source_.substr(start, position_ - start)).parse();
                if (position_ < source_.size()) {
                    ++position_;
                }
            }
            const MathNode radicand = parse_argument();
            if (index) {
                return {"<mroot><mrow>" + radicand.html + "</mrow><mrow>" + *index + "</mrow></mroot>", false};
            }
            return {"<msqrt><mrow>" + radicand.html + "</mrow></msqrt>", false};
        }
        if (command == "text" || command == "textrm" || command == "mbox") {
            return {"<mtext>" + html_escape(parse_raw_group()) + "</mtext>", false};
        }
        if (command == "operatorname") {
            return {"<mi mathvariant=\"normal\">" + html_escape(parse_raw_group()) + "</mi>", true};
        }
        if (command == "begin") {
            return parse_environment(parse_raw_group());
        }
        if (command == "left" || command == "right" || command == "middle") {
            const std::string delimiter = read_delimiter();
            if (delimiter.empty()) {
                return {"<mrow></mrow>", false};
            }
            return {"<mo stretchy=\"true\">" + html_escape(delimiter) + "</mo>", false};
        }

        static const std::unordered_map<std::string, std::pair<std::string, std::string>> styles = {
            {"mathrm", {"mathvariant", "normal"}},          {"mathbf", {"mathvariant", "bold"}},
            {"mathit", {"mathvariant", "italic"}},          {"mathsf", {"mathvariant", "sans-serif"}},
            {"mathtt", {"mathvariant", "monospace"}},       {"mathbb", {"mathvariant", "double-struck"}},
            {"mathcal", {"mathvariant", "script"}},         {"mathfrak", {"mathvariant", "fraktur"}},
            {"boldsymbol", {"mathvariant", "bold-italic"}},
        };
        if (const auto style = styles.find(command); style != styles.end()) {
            const MathNode content = parse_argument();
            return {"<mstyle " + style->second.first + "=\"" + style->second.second + "\">" + content.html +
                        "</mstyle>",
                    false};
        }

        if (command == "bar" || command == "overline") {
            const MathNode content = parse_argument();
            return {"<mrow class=\"latex-overbar\"><mrow>" + content.html + "</mrow></mrow>", false};
        }

        static const std::unordered_map<std::string, std::string> accents = {
            {"hat", "^"},  {"widehat", "^"}, {"vec", "→"},   {"tilde", "~"}, {"widetilde", "~"}, {"dot", "˙"},
            {"ddot", "¨"}, {"breve", "˘"},   {"check", "ˇ"}, {"acute", "´"}, {"grave", "`"},
        };
        if (const auto accent = accents.find(command); accent != accents.end()) {
            const MathNode content = parse_argument();
            return {"<mover accent=\"true\"><mrow>" + content.html + "</mrow><mo>" + accent->second + "</mo></mover>",
                    false};
        }
        if (command == "underline") {
            const MathNode content = parse_argument();
            return {"<munder accentunder=\"true\"><mrow>" + content.html + "</mrow><mo>_</mo></munder>", false};
        }

        if (command == "," || command == ":" || command == ";" || command == "quad" || command == "qquad" ||
            command == " " || command == "!") {
            const std::string width = command == "qquad"  ? "2em"
                                      : command == "quad" ? "1em"
                                      : command == ";"    ? "0.278em"
                                      : command == ":"    ? "0.222em"
                                      : command == "!"    ? "-0.167em"
                                                          : "0.167em";
            return {"<mspace width=\"" + width + "\"/>", false};
        }

        if (const auto symbol = latex_symbols().find(command); symbol != latex_symbols().end()) {
            return {"<mo>" + symbol->second + "</mo>", latex_limit_operators().contains(command)};
        }
        if (latex_named_operators().contains(command)) {
            return {"<mi mathvariant=\"normal\">" + html_escape(command) + "</mi>",
                    latex_limit_operators().contains(command)};
        }
        if (command == "%" || command == "$" || command == "#" || command == "_" || command == "{" || command == "}" ||
            command == "&") {
            return {"<mo>" + html_escape(command) + "</mo>", false};
        }
        return {"<mtext>\\" + html_escape(command) + "</mtext>", false};
    }

    MathNode parse_environment(const std::string& environment) {
        const std::string closing = "\\end{" + environment + "}";
        const std::size_t end = source_.find(closing, position_);
        if (end == std::string_view::npos) {
            return {"<mtext>\\begin{" + html_escape(environment) + "}</mtext>", false};
        }
        std::string content(source_.substr(position_, end - position_));
        position_ = end + closing.size();
        if (environment == "array") {
            std::string_view view = trim_whitespace(content);
            if (view.starts_with("{")) {
                const std::size_t specification_end = view.find('}');
                if (specification_end != std::string_view::npos) {
                    content = std::string(view.substr(specification_end + 1));
                }
            }
        }

        const auto rows = split_environment(content);
        std::string table = "<mtable columnspacing=\"1em\" rowspacing=\"0.35em\">";
        for (const auto& row : rows) {
            table += "<mtr>";
            for (const std::string& cell : row) {
                table += "<mtd><mrow>" + LatexMathParser(cell).parse() + "</mrow></mtd>";
            }
            table += "</mtr>";
        }
        table += "</mtable>";

        std::string left;
        std::string right;
        if (environment == "pmatrix") {
            left = "(";
            right = ")";
        } else if (environment == "bmatrix") {
            left = "[";
            right = "]";
        } else if (environment == "Bmatrix") {
            left = "{";
            right = "}";
        } else if (environment == "vmatrix") {
            left = "|";
            right = "|";
        } else if (environment == "Vmatrix") {
            left = "‖";
            right = "‖";
        } else if (environment == "cases") {
            left = "{";
        }
        if (!left.empty()) {
            table = "<mrow><mo stretchy=\"true\">" + html_escape(left) + "</mo>" + table;
            if (!right.empty()) {
                table += "<mo stretchy=\"true\">" + html_escape(right) + "</mo>";
            }
            table += "</mrow>";
        }
        return {table, false};
    }

    static std::vector<std::vector<std::string>> split_environment(std::string_view content) {
        std::vector<std::vector<std::string>> rows(1);
        rows.back().emplace_back();
        int depth = 0;
        for (std::size_t index = 0; index < content.size(); ++index) {
            const char character = content[index];
            if (character == '{') {
                ++depth;
            } else if (character == '}' && depth > 0) {
                --depth;
            }
            if (depth == 0 && character == '&') {
                rows.back().emplace_back();
                continue;
            }
            if (depth == 0 && character == '\\' && index + 1 < content.size() && content[index + 1] == '\\') {
                rows.emplace_back();
                rows.back().emplace_back();
                ++index;
                continue;
            }
            rows.back().back() += character;
        }
        return rows;
    }

    std::string parse_raw_group() {
        skip_spaces();
        if (position_ >= source_.size() || source_[position_] != '{') {
            return {};
        }
        ++position_;
        const std::size_t start = position_;
        int depth = 1;
        while (position_ < source_.size() && depth > 0) {
            if (source_[position_] == '{') {
                ++depth;
            } else if (source_[position_] == '}') {
                --depth;
            }
            ++position_;
        }
        const std::size_t end = depth == 0 ? position_ - 1 : position_;
        return std::string(source_.substr(start, end - start));
    }

    std::string read_delimiter() {
        skip_spaces();
        if (position_ >= source_.size()) {
            return {};
        }
        if (source_[position_] != '\\') {
            const char delimiter = source_[position_++];
            return delimiter == '.' ? std::string() : std::string(1, delimiter);
        }
        ++position_;
        const std::size_t start = position_;
        if (position_ < source_.size() && std::isalpha(static_cast<unsigned char>(source_[position_])) != 0) {
            while (position_ < source_.size() && std::isalpha(static_cast<unsigned char>(source_[position_])) != 0) {
                ++position_;
            }
        } else if (position_ < source_.size()) {
            ++position_;
        }
        const std::string name(source_.substr(start, position_ - start));
        static const std::unordered_map<std::string, std::string> delimiters = {
            {"langle", "⟨"}, {"rangle", "⟩"}, {"lceil", "⌈"}, {"rceil", "⌉"}, {"lfloor", "⌊"}, {"rfloor", "⌋"},
            {"vert", "|"},   {"Vert", "‖"},   {"|", "‖"},     {"{", "{"},     {"}", "}"},      {".", ""},
        };
        if (const auto delimiter = delimiters.find(name); delimiter != delimiters.end()) {
            return delimiter->second;
        }
        return name;
    }

    void skip_spaces() {
        while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_])) != 0) {
            ++position_;
        }
    }

    static std::size_t utf8_glyph_size(unsigned char lead) {
        if ((lead & 0x80U) == 0)
            return 1;
        if ((lead & 0xE0U) == 0xC0U)
            return 2;
        if ((lead & 0xF0U) == 0xE0U)
            return 3;
        if ((lead & 0xF8U) == 0xF0U)
            return 4;
        return 1;
    }

    std::string_view source_;
    std::size_t position_ = 0;
};

std::string render_latex_math(std::string_view source, bool display) {
    const std::string math = LatexMathParser(source).parse();
    return "<math class=\"latex-math\" xmlns=\"http://www.w3.org/1998/Math/MathML\" display=\"" +
           std::string(display ? "block" : "inline") + "\" aria-label=\"" + html_escape(source) +
           "\"><semantics><mrow>" + math + "</mrow><annotation encoding=\"application/x-tex\">" + html_escape(source) +
           "</annotation></semantics></math>";
}

std::size_t find_unescaped(std::string_view text, std::string_view delimiter, std::size_t start) {
    std::size_t position = start;
    while ((position = text.find(delimiter, position)) != std::string_view::npos) {
        std::size_t slashes = 0;
        for (std::size_t index = position; index > 0 && text[index - 1] == '\\'; --index) {
            ++slashes;
        }
        if (slashes % 2 == 0) {
            return position;
        }
        position += delimiter.size();
    }
    return std::string_view::npos;
}

std::string inline_markdown(std::string_view text) {
    std::string rendered;
    std::size_t position = 0;
    while (position < text.size()) {
        if (text[position] == '\\' && position + 1 < text.size() && text[position + 1] == '$') {
            rendered += '$';
            position += 2;
            continue;
        }
        if (text[position] == '`') {
            const std::size_t end = text.find('`', position + 1);
            if (end != std::string_view::npos) {
                rendered += "<code>" + html_escape(text.substr(position + 1, end - position - 1)) + "</code>";
                position = end + 1;
                continue;
            }
        }
        if (position + 1 < text.size() && text[position] == '\\' && text[position + 1] == '(') {
            const std::size_t end = find_unescaped(text, "\\)", position + 2);
            if (end != std::string_view::npos) {
                rendered += render_latex_math(text.substr(position + 2, end - position - 2), false);
                position = end + 2;
                continue;
            }
        }
        if (text[position] == '$' && (position + 1 >= text.size() || text[position + 1] != '$')) {
            const std::size_t end = find_unescaped(text, "$", position + 1);
            if (end != std::string_view::npos && end > position + 1) {
                rendered += render_latex_math(text.substr(position + 1, end - position - 1), false);
                position = end + 1;
                continue;
            }
        }
        if (position + 1 < text.size() && text[position] == '*' && text[position + 1] == '*') {
            const std::size_t end = text.find("**", position + 2);
            if (end != std::string_view::npos) {
                rendered += "<strong>" + inline_markdown(text.substr(position + 2, end - position - 2)) + "</strong>";
                position = end + 2;
                continue;
            }
        }
        if (text[position] == '*') {
            const std::size_t end = text.find('*', position + 1);
            if (end != std::string_view::npos) {
                rendered += "<em>" + inline_markdown(text.substr(position + 1, end - position - 1)) + "</em>";
                position = end + 1;
                continue;
            }
        }
        const unsigned char lead = static_cast<unsigned char>(text[position]);
        const std::size_t glyph_size = (lead & 0x80U) == 0       ? 1
                                       : (lead & 0xE0U) == 0xC0U ? 2
                                       : (lead & 0xF0U) == 0xE0U ? 3
                                       : (lead & 0xF8U) == 0xF0U ? 4
                                                                 : 1;
        const std::size_t available = std::min(glyph_size, text.size() - position);
        rendered += html_escape(text.substr(position, available));
        position += available;
    }
    return rendered;
}

struct DisplayMathBlock {
    std::string source;
    std::size_t last_line = 0;
};

std::optional<DisplayMathBlock> display_math_block(const std::vector<std::string>& lines, std::size_t first_line) {
    const std::string_view opening_line = trim_whitespace(lines[first_line]);
    std::string_view opening;
    std::string_view closing;
    if (opening_line.starts_with("$$")) {
        opening = "$$";
        closing = "$$";
    } else if (opening_line.starts_with("\\[")) {
        opening = "\\[";
        closing = "\\]";
    } else {
        return std::nullopt;
    }

    std::string source(opening_line.substr(opening.size()));
    if (const std::size_t close = find_unescaped(source, closing, 0); close != std::string::npos) {
        if (!trim_whitespace(std::string_view(source).substr(close + closing.size())).empty()) {
            return std::nullopt;
        }
        source.resize(close);
        return DisplayMathBlock{std::move(source), first_line};
    }

    for (std::size_t line = first_line + 1; line < lines.size(); ++line) {
        const std::string_view candidate = lines[line];
        const std::size_t close = find_unescaped(candidate, closing, 0);
        if (close == std::string_view::npos) {
            if (!source.empty())
                source += '\n';
            source += candidate;
            continue;
        }
        if (!trim_whitespace(candidate.substr(close + closing.size())).empty()) {
            return std::nullopt;
        }
        if (!source.empty())
            source += '\n';
        source += candidate.substr(0, close);
        return DisplayMathBlock{std::move(source), line};
    }
    return std::nullopt;
}

std::string render_markdown(std::string_view markdown) {
    std::istringstream input{std::string(markdown)};
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) {
        lines.push_back(std::move(line));
    }
    std::string html;
    bool in_list = false;
    bool in_code = false;
    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const std::string& line = lines[line_index];
        if (line.starts_with("```")) {
            if (in_list) {
                html += "</ul>";
                in_list = false;
            }
            html += in_code ? "</code></pre>" : "<pre><code>";
            in_code = !in_code;
            continue;
        }
        if (in_code) {
            html += html_escape(line) + "\n";
            continue;
        }
        if (const auto math = display_math_block(lines, line_index)) {
            if (in_list) {
                html += "</ul>";
                in_list = false;
            }
            html += "<div class=\"math-display\">" + render_latex_math(math->source, true) + "</div>";
            line_index = math->last_line;
            continue;
        }
        if (line.starts_with("- ")) {
            if (!in_list) {
                html += "<ul>";
                in_list = true;
            }
            html += "<li>" + inline_markdown(std::string_view(line).substr(2)) + "</li>";
            continue;
        }
        if (in_list) {
            html += "</ul>";
            in_list = false;
        }
        if (line.empty()) {
            continue;
        }

        std::size_t heading = 0;
        while (heading < line.size() && heading < 6 && line[heading] == '#') {
            ++heading;
        }
        if (heading > 0 && heading < line.size() && line[heading] == ' ') {
            html += "<h" + std::to_string(heading) + ">" + inline_markdown(std::string_view(line).substr(heading + 1)) +
                    "</h" + std::to_string(heading) + ">";
        } else {
            html += "<p>" + inline_markdown(line) + "</p>";
        }
    }
    if (in_list) {
        html += "</ul>";
    }
    if (in_code) {
        html += "</code></pre>";
    }
    return html;
}

std::string source_name(const Document& document, const Cell& cell, std::size_t cell_index) {
    const std::string path = document.path.empty() ? "<notebook>" : document.path.string();
    const std::string identity = cell.id.empty() ? std::to_string(cell_index + 1) : cell.id;
    return path + "#cell-" + identity;
}

} // namespace

Document parse(std::string_view json, std::filesystem::path path) {
    const JsonValue root = JsonParser(json).parse();
    if (root.kind != JsonValue::Kind::object) {
        throw std::runtime_error("invalid .dnb: document root must be an object");
    }

    const std::size_t version = require_unsigned(require_member(root, "dune_notebook"), "dune_notebook");
    if (version != 1) {
        throw std::runtime_error("unsupported .dnb format version " + std::to_string(version));
    }

    Document document;
    document.format_version = version;
    document.path = std::move(path);
    if (const JsonValue* metadata = find_member(root, "metadata")) {
        if (metadata->kind != JsonValue::Kind::object) {
            throw std::runtime_error("invalid .dnb: 'metadata' must be an object");
        }
        if (const JsonValue* title = find_member(*metadata, "title")) {
            document.title = require_string(*title, "metadata.title");
        }
    }

    const JsonValue& cells = require_member(root, "cells");
    if (cells.kind != JsonValue::Kind::array) {
        throw std::runtime_error("invalid .dnb: 'cells' must be an array");
    }

    std::unordered_set<std::string> cell_ids;
    for (std::size_t index = 0; index < cells.array.size(); ++index) {
        const JsonValue& encoded = cells.array[index];
        Cell cell;
        if (const JsonValue* id = find_member(encoded, "id")) {
            cell.id = require_string(*id, "id");
        }
        if (cell.id.empty()) {
            cell.id = "cell-" + std::to_string(index + 1);
        }
        if (!cell_ids.insert(cell.id).second) {
            throw std::runtime_error("invalid .dnb: duplicate cell id '" + cell.id + "'");
        }

        const std::string type = require_string(require_member(encoded, "cell_type"), "cell_type");
        if (type == "markdown") {
            cell.kind = CellKind::markdown;
        } else if (type == "code") {
            cell.kind = CellKind::code;
        } else {
            throw std::runtime_error("invalid .dnb: unsupported cell_type '" + type + "'");
        }
        cell.source = require_source(require_member(encoded, "source"));

        if (cell.kind == CellKind::code) {
            if (const JsonValue* count = find_member(encoded, "execution_count");
                count != nullptr && count->kind != JsonValue::Kind::null_value) {
                cell.execution_count = require_unsigned(*count, "execution_count");
            }
            if (const JsonValue* outputs = find_member(encoded, "outputs")) {
                if (outputs->kind != JsonValue::Kind::array) {
                    throw std::runtime_error("invalid .dnb: 'outputs' must be an array");
                }
                for (const JsonValue& output : outputs->array) {
                    const std::string type =
                        require_string(require_member(output, "output_type"), "outputs.output_type");
                    if (type != "stream") {
                        continue;
                    }
                    const std::string name = require_string(require_member(output, "name"), "outputs.name");
                    const std::string text = require_string(require_member(output, "text"), "outputs.text");
                    if (name == "stdout") {
                        cell.output += text;
                    } else if (name == "stderr") {
                        cell.error += text;
                    }
                }
            }
        }
        document.cells.push_back(std::move(cell));
    }

    return document;
}

std::string serialize(const Document& document) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"dune_notebook\": 1,\n";
    json << "  \"metadata\": {\n";
    json << "    \"title\": \"" << json_escape(document.title) << "\"\n";
    json << "  },\n";
    json << "  \"cells\": [\n";
    for (std::size_t index = 0; index < document.cells.size(); ++index) {
        const Cell& cell = document.cells[index];
        json << "    {\n";
        json << "      \"id\": \"" << json_escape(cell.id.empty() ? "cell-" + std::to_string(index + 1) : cell.id)
             << "\",\n";
        json << "      \"cell_type\": \"" << (cell.kind == CellKind::markdown ? "markdown" : "code") << "\",\n";
        json << "      \"source\": \"" << json_escape(cell.source) << "\"";
        if (cell.kind == CellKind::code) {
            json << ",\n";
            if (cell.execution_count == 0) {
                json << "      \"execution_count\": null,\n";
            } else {
                json << "      \"execution_count\": " << cell.execution_count << ",\n";
            }
            json << "      \"outputs\": [";
            const bool has_output = !cell.output.empty() || !cell.error.empty();
            if (has_output) {
                json << '\n';
            }
            bool wrote_output = false;
            if (!cell.output.empty()) {
                json << "        {\"output_type\": \"stream\", \"name\": \"stdout\", \"text\": \""
                     << json_escape(cell.output) << "\"}";
                wrote_output = true;
            }
            if (!cell.error.empty()) {
                json << (wrote_output ? ",\n" : "\n")
                     << "        {\"output_type\": \"stream\", \"name\": \"stderr\", \"text\": \""
                     << json_escape(cell.error) << "\"}";
            }
            if (has_output) {
                json << '\n';
                json << "      ";
            }
            json << "]\n";
        } else {
            json << '\n';
        }
        json << "    }" << (index + 1 == document.cells.size() ? "\n" : ",\n");
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

Document read(const std::filesystem::path& path) {
    return parse(read_text(path), path);
}

void write(const Document& document, const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not open notebook '" + path.string() + "' for writing");
    }
    output << serialize(document);
}

std::string render_html(const Document& document) {
    const std::string title = document.title.empty() ? document.path.stem().string() : document.title;
    std::ostringstream html;
    html << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">";
    html << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    html << "<title>" << html_escape(title) << "</title><style>";
    html << R"css(:root{color-scheme:light;--page:#fff;--surface:#fff;--muted:#66707a;--border:#d5d8dc;
--text:#24292f;--accent:#2f7dbd;--orange:#f37726;--code:#f7f7f7;--danger:#c43c35;--danger-soft:#fff1f0}
@media(prefers-color-scheme:dark){:root{color-scheme:dark;--page:#1e1f22;--surface:#282a2e;--muted:#a6abb3;
--border:#41444a;--text:#e6e8eb;--accent:#67a9df;--orange:#ff8b42;--code:#202226;--danger:#ef7770;
--danger-soft:#442a2a}}*{box-sizing:border-box}body{margin:0;background:var(--page);color:var(--text);
font:14px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}.export-header{display:flex;align-items:center;
gap:14px;border-bottom:1px solid var(--border);background:var(--surface);padding:11px max(18px,calc((100vw - 1080px)/2))}
.brand{display:flex;align-items:center;gap:7px;font-weight:650}.mark{display:grid;width:24px;height:24px;place-items:center;
border:2px solid var(--orange);border-radius:50%;color:var(--orange);font-size:11px}.export-title{overflow:hidden;
text-overflow:ellipsis;white-space:nowrap}.export-label{margin-left:auto;color:var(--muted);font-size:12px}main{max-width:1080px;
margin:0 auto;padding:28px 18px 90px}.cell{display:grid;grid-template-columns:88px minmax(0,1fr);margin:8px 0}
.prompt{padding:8px 10px 0 0;color:var(--accent);text-align:right;white-space:nowrap;font:12px/1.5 ui-monospace,
monospace}.content{min-width:0}.code{margin:0;overflow:auto;border:1px solid var(--border);border-radius:2px;
background:var(--code);padding:9px 11px;font:13px/1.55 ui-monospace,monospace}.output{margin:5px 0 2px;
background:var(--surface);padding:7px 10px;white-space:pre-wrap;overflow-wrap:anywhere;font:13px/1.5 ui-monospace,
monospace}.rich-output{margin:8px 0 2px;overflow:auto;background:var(--surface);padding:8px}.rich-output img{display:block;
max-width:100%;height:auto;margin:0 auto}.error{border-left:3px solid var(--danger);background:var(--danger-soft);color:var(--danger)}.markdown{
padding:4px 12px 8px}.markdown h1,.markdown h2{border-bottom:1px solid var(--border);padding-bottom:.18em;
line-height:1.22}.markdown h1{font-size:1.85em}.markdown h2{font-size:1.45em}.markdown code{border-radius:2px;
background:var(--code);padding:.1em .3em}.markdown pre{overflow:auto;border:1px solid var(--border);border-radius:2px;
background:var(--code);padding:10px}.latex-math{color:currentColor;font-family:"STIX Two Math","Cambria Math",
"Times New Roman",serif;font-size:1.08em}.latex-overbar{border-top:.055em solid currentColor;padding-top:.16em}
.math-display{max-width:100%;margin:.8em 0;overflow-x:auto;padding:.15em 0;
text-align:center}.math-display>.latex-math{margin:0 auto}@media(max-width:700px){main{padding-inline:6px}.cell{grid-template-columns:58px minmax(0,1fr)}
.prompt{padding-right:5px;font-size:10px}.export-label{display:none}})css";
    html << "</style></head><body><header class=\"export-header\"><div class=\"brand\"><span class=\"mark\">D</span>"
            "Dune Notebook</div><strong class=\"export-title\">"
         << html_escape(title) << "</strong><span class=\"export-label\">Static export</span></header><main>";
    for (const Cell& cell : document.cells) {
        if (cell.kind == CellKind::markdown) {
            html << "<section class=\"cell\"><div class=\"prompt\"></div><div class=\"content markdown\">"
                 << render_markdown(cell.source) << "</div></section>";
            continue;
        }
        html << "<section class=\"cell\"><div class=\"prompt\">In [";
        if (cell.execution_count != 0) {
            html << cell.execution_count;
        }
        html << "]:</div><div class=\"content\">";
        html << "<pre class=\"code\"><code>" << html_escape(cell.source) << "</code></pre>";
        if (!cell.output.empty()) {
            if (is_svg_output(cell.output)) {
                html << "<figure class=\"rich-output\"><img alt=\"Chart output\" src=\"" << svg_data_uri(cell.output)
                     << "\"></figure>";
            } else {
                html << "<pre class=\"output\">" << html_escape(cell.output) << "</pre>";
            }
        }
        if (!cell.error.empty()) {
            html << "<pre class=\"output error\">" << html_escape(cell.error) << "</pre>";
        }
        html << "</div></section>";
    }
    html << "</main></body></html>\n";
    return html.str();
}

void apply_execution(Document& document, const ExecutionReport& report) {
    for (const CellExecution& execution : report.cells) {
        if (execution.cell_index >= document.cells.size() ||
            document.cells[execution.cell_index].kind != CellKind::code) {
            continue;
        }
        Cell& cell = document.cells[execution.cell_index];
        cell.execution_count = execution.execution_count;
        cell.output = execution.output;
        cell.error = execution.error;
    }
}

Kernel::Kernel(std::filesystem::path source_directory) : source_directory_(std::move(source_directory)) {
    rebuild_session();
}

ExecutionReport Kernel::execute(const Document& document, std::optional<std::size_t> through_cell) {
    std::vector<std::size_t> code_cells;
    for (std::size_t index = 0; index < document.cells.size(); ++index) {
        if (document.cells[index].kind == CellKind::code && (!through_cell.has_value() || index <= *through_cell)) {
            code_cells.push_back(index);
        }
    }

    ExecutionReport report;
    if (code_cells.empty()) {
        return report;
    }

    bool can_continue = executed_sources_.size() < code_cells.size();
    if (can_continue) {
        for (std::size_t index = 0; index < executed_sources_.size(); ++index) {
            const Cell& cell = document.cells[code_cells[index]];
            if (executed_ids_[index] != cell.id || executed_sources_[index] != cell.source) {
                can_continue = false;
                break;
            }
        }
    }
    if (!can_continue) {
        rebuild_session();
        executed_ids_.clear();
        executed_sources_.clear();
        executions_.clear();
    }

    report.cells = executions_;
    for (std::size_t index = 0; index < report.cells.size(); ++index) {
        report.cells[index].cell_index = code_cells[index];
    }
    for (std::size_t ordinal = executed_sources_.size(); ordinal < code_cells.size(); ++ordinal) {
        const std::size_t cell_index = code_cells[ordinal];
        const Cell& cell = document.cells[cell_index];
        const repl::EvaluationResult result = session_->evaluate(cell.source, source_name(document, cell, cell_index));
        CellExecution execution{cell_index, ++execution_count_, result.success, result.output, result.error};
        report.cells.push_back(execution);
        if (!result.success) {
            report.success = false;
            report.failed_cell = cell_index;
            return report;
        }
        executed_ids_.push_back(cell.id);
        executed_sources_.push_back(cell.source);
        executions_.push_back(std::move(execution));
    }
    return report;
}

void Kernel::reset() {
    executed_ids_.clear();
    executed_sources_.clear();
    executions_.clear();
    execution_count_ = 0;
    rebuild_session();
}

void Kernel::rebuild_session() {
    session_ = std::make_unique<repl::Session>(source_directory_);
}

} // namespace dune::notebook
