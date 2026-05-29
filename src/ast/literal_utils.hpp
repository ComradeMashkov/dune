#pragma once

#include "ast/ast.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dune {

inline std::string integer_literal_suffix(std::string_view lexeme) {
    static constexpr std::string_view suffixes[] = {"usize", "isize", "u64", "u32", "u16",
                                                    "u8",    "i64",   "i32", "i16", "i8"};
    for (const std::string_view suffix : suffixes) {
        if (lexeme.size() >= suffix.size() && lexeme.substr(lexeme.size() - suffix.size()) == suffix) {
            return std::string(suffix);
        }
    }

    return {};
}

inline bool has_integer_literal_suffix(std::string_view lexeme) {
    return !integer_literal_suffix(lexeme).empty();
}

inline TypeAnnotation integer_literal_suffix_type(std::string_view lexeme) {
    const std::string suffix = integer_literal_suffix(lexeme);
    if (suffix.empty()) {
        return {};
    }

    if (suffix == "i8") {
        return TypeAnnotation{true, Type{ValueType::i8_type, nullptr}};
    }
    if (suffix == "i16") {
        return TypeAnnotation{true, Type{ValueType::i16_type, nullptr}};
    }
    if (suffix == "i32") {
        return TypeAnnotation{true, Type{ValueType::i32_type, nullptr}};
    }
    if (suffix == "i64") {
        return TypeAnnotation{true, Type{ValueType::i64_type, nullptr}};
    }
    if (suffix == "isize") {
        return TypeAnnotation{true, Type{ValueType::isize_type, nullptr}};
    }
    if (suffix == "u8") {
        return TypeAnnotation{true, Type{ValueType::u8_type, nullptr}};
    }
    if (suffix == "u16") {
        return TypeAnnotation{true, Type{ValueType::u16_type, nullptr}};
    }
    if (suffix == "u32") {
        return TypeAnnotation{true, Type{ValueType::u32_type, nullptr}};
    }
    if (suffix == "u64") {
        return TypeAnnotation{true, Type{ValueType::u64_type, nullptr}};
    }

    return TypeAnnotation{true, Type{ValueType::usize_type, nullptr}};
}

inline std::string integer_literal_digits(std::string_view lexeme) {
    const std::string suffix = integer_literal_suffix(lexeme);
    if (!suffix.empty()) {
        lexeme.remove_suffix(suffix.size());
    }

    if (lexeme.size() >= 2 && lexeme[0] == '0' &&
        (lexeme[1] == 'x' || lexeme[1] == 'X' || lexeme[1] == 'b' || lexeme[1] == 'B')) {
        lexeme.remove_prefix(2);
    }

    std::string result;
    result.reserve(lexeme.size());
    for (const char current : lexeme) {
        if (current != '_') {
            result += current;
        }
    }

    return result;
}

inline int integer_literal_base(std::string_view lexeme) {
    const std::string suffix = integer_literal_suffix(lexeme);
    if (!suffix.empty()) {
        lexeme.remove_suffix(suffix.size());
    }

    if (lexeme.size() >= 2 && lexeme[0] == '0') {
        if (lexeme[1] == 'x' || lexeme[1] == 'X') {
            return 16;
        }
        if (lexeme[1] == 'b' || lexeme[1] == 'B') {
            return 2;
        }
    }

    return 10;
}

inline unsigned long long parse_unsigned_integer_literal(std::string_view lexeme) {
    const std::string digits = integer_literal_digits(lexeme);
    return std::stoull(digits, nullptr, integer_literal_base(lexeme));
}

inline std::string llvm_integer_literal(std::string_view lexeme) {
    return std::to_string(parse_unsigned_integer_literal(lexeme));
}

inline std::string clean_real_literal(std::string_view lexeme) {
    std::string result;
    result.reserve(lexeme.size());
    for (const char current : lexeme) {
        if (current != '_') {
            result += current;
        }
    }
    return result;
}

} // namespace dune
