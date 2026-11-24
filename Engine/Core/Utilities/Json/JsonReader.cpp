// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Utilities/Json/JsonReader.hpp>

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

namespace atom::json {

namespace {

constexpr std::uint32_t kMaxDepth = 64;
// A scene file is content, not a data dump: anything near this is a mistake.
constexpr std::size_t kMaxInputBytes = 16u * 1024u * 1024u;

auto IsDigit(const char c) -> bool {
    return c >= '0' && c <= '9';
}

auto IsWhitespace(const char c) -> bool {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Decodes one code point into UTF-8. Rejects unpaired surrogates and out-of-range
// values instead of emitting bytes that are not valid UTF-8.
auto AppendUtf8(std::string& out, const std::uint32_t code_point) -> bool {
    if (code_point <= 0x7F) {
        out.push_back(static_cast<char>(code_point));
        return true;
    }
    if (code_point <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0u | (code_point >> 6u)));
        out.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
        return true;
    }
    if (code_point <= 0xFFFF) {
        if (code_point >= 0xD800 && code_point <= 0xDFFF)
            return false;
        out.push_back(static_cast<char>(0xE0u | (code_point >> 12u)));
        out.push_back(static_cast<char>(0x80u | ((code_point >> 6u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
        return true;
    }
    if (code_point <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0u | (code_point >> 18u)));
        out.push_back(static_cast<char>(0x80u | ((code_point >> 12u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((code_point >> 6u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
        return true;
    }
    return false;
}

class Reader final {
    public:
        explicit Reader(const std::string_view text) : text_(text) {}

        auto ParseDocument(Value& root, SyntaxError& error) -> bool {
            SkipTrivia();
            if (AtEnd())
                return FailAt(error, 1, 1, "document is empty");
            if (!ParseValue(root, 1, error))
                return false;
            SkipTrivia();
            if (!AtEnd())
                return FailHere(error, "unexpected content after the root value");
            return true;
        }

    private:
        // --- position bookkeeping -------------------------------------------

        [[nodiscard]] auto AtEnd() const -> bool {
            return index_ >= text_.size();
        }
        [[nodiscard]] auto Peek() const -> char {
            return index_ < text_.size() ? text_[index_] : '\0';
        }
        [[nodiscard]] auto PeekAt(const std::size_t offset) const -> char {
            return index_ + offset < text_.size() ? text_[index_ + offset] : '\0';
        }
        auto Advance() -> void {
            if (AtEnd())
                return;
            if (text_[index_] == '\n') {
                ++line_;
                column_ = 1;
            } else {
                ++column_;
            }
            ++index_;
        }

        static auto FailAt(SyntaxError& error, const std::uint32_t line, const std::uint32_t column,
                           std::string message) -> bool {
            error = SyntaxError{line, column, std::move(message)};
            return false;
        }
        auto FailHere(SyntaxError& error, std::string message) const -> bool {
            return FailAt(error, line_, column_, std::move(message));
        }

        // Skips whitespace and `//` line comments. `res://` inside a string is safe
        // because strings are lexed as a unit: the comment rule is only reachable
        // between tokens. There are no block comments on purpose - they nest badly
        // in hand-edited files and hide stale code.
        auto SkipTrivia() -> void {
            while (!AtEnd()) {
                if (IsWhitespace(Peek())) {
                    Advance();
                    continue;
                }
                if (Peek() == '/' && PeekAt(1) == '/') {
                    while (!AtEnd() && Peek() != '\n')
                        Advance();
                    continue;
                }
                return;
            }
        }

        // --- values ----------------------------------------------------------

        auto ParseValue(Value& out, const std::uint32_t depth, SyntaxError& error) -> bool {
            if (depth > kMaxDepth)
                return FailHere(error, "nesting is deeper than 64 levels");
            SkipTrivia();
            if (AtEnd())
                return FailHere(error, "expected a value");

            out.line = line_;
            out.column = column_;

            switch (Peek()) {
                case '{':
                    return ParseObject(out, depth, error);
                case '[':
                    return ParseArray(out, depth, error);
                case '"':
                    out.kind = Kind::String;
                    return ParseString(out.string, error);
                case 't':
                    out.kind = Kind::Bool;
                    out.boolean = true;
                    return ParseLiteral("true", error);
                case 'f':
                    out.kind = Kind::Bool;
                    out.boolean = false;
                    return ParseLiteral("false", error);
                case 'n':
                    out.kind = Kind::Null;
                    return ParseLiteral("null", error);
                default:
                    if (Peek() == '-' || IsDigit(Peek()))
                        return ParseNumber(out, error);
                    return FailHere(error, std::string{"unexpected character '"} + Peek() + "'");
            }
        }

        auto ParseLiteral(const std::string_view literal, SyntaxError& error) -> bool {
            for (const char expected : literal) {
                if (AtEnd() || Peek() != expected)
                    return FailHere(error, "invalid literal");
                Advance();
            }
            if (!AtEnd() && (IsDigit(Peek()) || Peek() == '_'))
                return FailHere(error, "invalid literal");
            return true;
        }

        auto ParseNumber(Value& out, SyntaxError& error) -> bool {
            const std::size_t start = index_;
            if (Peek() == '-')
                Advance();

            if (AtEnd() || !IsDigit(Peek()))
                return FailHere(error, "expected a digit");
            if (Peek() == '0') {
                Advance();
                if (!AtEnd() && IsDigit(Peek()))
                    return FailHere(error, "leading zeros are not allowed");
            } else {
                while (!AtEnd() && IsDigit(Peek()))
                    Advance();
            }

            if (!AtEnd() && Peek() == '.') {
                Advance();
                if (AtEnd() || !IsDigit(Peek()))
                    return FailHere(error, "expected a digit after '.'");
                while (!AtEnd() && IsDigit(Peek()))
                    Advance();
            }

            if (!AtEnd() && (Peek() == 'e' || Peek() == 'E')) {
                Advance();
                if (!AtEnd() && (Peek() == '+' || Peek() == '-'))
                    Advance();
                if (AtEnd() || !IsDigit(Peek()))
                    return FailHere(error, "expected a digit in the exponent");
                while (!AtEnd() && IsDigit(Peek()))
                    Advance();
            }

            const std::string token{text_.substr(start, index_ - start)};
            char* end = nullptr;
            const double parsed = std::strtod(token.c_str(), &end);
            if (end == nullptr || *end != '\0')
                return FailHere(error, "invalid number '" + token + "'");
            // Overflow yields inf; NaN cannot come out of this grammar but is
            // rejected anyway, so no NaN can ever reach the world.
            if (!std::isfinite(parsed))
                return FailHere(error, "number out of range '" + token + "'");

            out.kind = Kind::Number;
            out.number = parsed;
            return true;
        }

        auto ParseString(std::string& out, SyntaxError& error) -> bool {
            Advance(); // opening quote
            while (true) {
                if (AtEnd())
                    return FailHere(error, "unterminated string");
                const char c = Peek();
                if (c == '"') {
                    Advance();
                    return true;
                }
                if (c == '\n' || c == '\r')
                    return FailHere(error, "unterminated string (raw newline)");
                if (static_cast<unsigned char>(c) < 0x20)
                    return FailHere(error, "raw control character in string");
                if (c != '\\') {
                    out.push_back(c);
                    Advance();
                    continue;
                }

                Advance(); // backslash
                if (AtEnd())
                    return FailHere(error, "unterminated escape");
                const char escape = Peek();
                switch (escape) {
                    case '"':
                        out.push_back('"');
                        break;
                    case '\\':
                        out.push_back('\\');
                        break;
                    case '/':
                        out.push_back('/');
                        break;
                    case 'b':
                        out.push_back('\b');
                        break;
                    case 'f':
                        out.push_back('\f');
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case 'u':
                        if (!ParseUnicodeEscape(out, error))
                            return false;
                        continue; // ParseUnicodeEscape consumed its digits already
                    default:
                        return FailHere(error, std::string{"unknown escape '\\"} + escape + "'");
                }
                Advance();
            }
        }

        auto ReadHex4(std::uint32_t& out, SyntaxError& error) -> bool {
            std::uint32_t value = 0;
            for (int i = 0; i < 4; ++i) {
                if (AtEnd())
                    return FailHere(error, "truncated \\u escape");
                const char c = Peek();
                std::uint32_t digit = 0;
                if (c >= '0' && c <= '9')
                    digit = static_cast<std::uint32_t>(c - '0');
                else if (c >= 'a' && c <= 'f')
                    digit = static_cast<std::uint32_t>(c - 'a') + 10U;
                else if (c >= 'A' && c <= 'F')
                    digit = static_cast<std::uint32_t>(c - 'A') + 10U;
                else
                    return FailHere(error, "invalid hex digit in \\u escape");
                value = (value << 4U) | digit;
                Advance();
            }
            out = value;
            return true;
        }

        auto ParseUnicodeEscape(std::string& out, SyntaxError& error) -> bool {
            Advance(); // 'u'
            std::uint32_t code_point = 0;
            if (!ReadHex4(code_point, error))
                return false;

            if (code_point >= 0xD800 && code_point <= 0xDBFF) {
                if (Peek() != '\\' || PeekAt(1) != 'u')
                    return FailHere(error, "unpaired surrogate in \\u escape");
                Advance(); // backslash
                Advance(); // 'u'
                std::uint32_t low = 0;
                if (!ReadHex4(low, error))
                    return false;
                if (low < 0xDC00 || low > 0xDFFF)
                    return FailHere(error, "invalid low surrogate in \\u escape");
                code_point = 0x10000U + ((code_point - 0xD800U) << 10U) + (low - 0xDC00U);
            } else if (code_point >= 0xDC00 && code_point <= 0xDFFF) {
                return FailHere(error, "unpaired surrogate in \\u escape");
            }

            if (!AppendUtf8(out, code_point))
                return FailHere(error, "code point out of range in \\u escape");
            return true;
        }

        auto ParseObject(Value& out, const std::uint32_t depth, SyntaxError& error) -> bool {
            out.kind = Kind::Object;
            Advance(); // '{'
            SkipTrivia();
            if (Peek() == '}') {
                Advance();
                return true;
            }

            while (true) {
                SkipTrivia();
                if (AtEnd())
                    return FailHere(error, "unterminated object");
                if (Peek() != '"')
                    return FailHere(error, "expected a string key");

                Member member{};
                member.line = line_;
                member.column = column_;
                if (!ParseString(member.key, error))
                    return false;

                // Duplicate keys are rejected: most parsers keep the last value, which
                // turns a copy/paste slip into a silent behaviour change.
                for (const auto& existing : out.members) {
                    if (existing.key != member.key)
                        continue;
                    return FailAt(error, member.line, member.column,
                                  "duplicate key \"" + member.key + "\" (first seen at " +
                                      std::to_string(existing.line) + ":" + std::to_string(existing.column) + ")");
                }

                SkipTrivia();
                if (Peek() != ':')
                    return FailHere(error, "expected ':' after the key");
                Advance();

                member.value = std::make_unique<Value>();
                if (!ParseValue(*member.value, depth + 1, error))
                    return false;
                out.members.push_back(std::move(member));

                SkipTrivia();
                if (Peek() == ',') {
                    Advance();
                    SkipTrivia();
                    if (Peek() == '}')
                        return FailHere(error, "trailing comma before '}'");
                    continue;
                }
                if (Peek() == '}') {
                    Advance();
                    return true;
                }
                return FailHere(error, "expected ',' or '}'");
            }
        }

        auto ParseArray(Value& out, const std::uint32_t depth, SyntaxError& error) -> bool {
            out.kind = Kind::Array;
            Advance(); // '['
            SkipTrivia();
            if (Peek() == ']') {
                Advance();
                return true;
            }

            while (true) {
                auto item = std::make_unique<Value>();
                if (!ParseValue(*item, depth + 1, error))
                    return false;
                out.items.push_back(std::move(item));

                SkipTrivia();
                if (Peek() == ',') {
                    Advance();
                    SkipTrivia();
                    if (Peek() == ']')
                        return FailHere(error, "trailing comma before ']'");
                    continue;
                }
                if (Peek() == ']') {
                    Advance();
                    return true;
                }
                return FailHere(error, "expected ',' or ']'");
            }
        }

        std::string_view text_{};
        std::size_t index_ = 0;
        std::uint32_t line_ = 1;
        std::uint32_t column_ = 1;
};

} // namespace

auto Value::Find(const std::string_view key) const noexcept -> const Value* {
    if (kind != Kind::Object)
        return nullptr;
    for (const auto& member : members) {
        if (member.key == key)
            return member.value.get();
    }
    return nullptr;
}

auto FormatPosition(const std::uint32_t line, const std::uint32_t column, const std::string_view message)
    -> std::string {
    return std::to_string(line) + ":" + std::to_string(column) + ": " + std::string{message};
}

auto TryParse(const std::string_view text, SyntaxError& error) -> std::optional<Value> {
    if (text.size() > kMaxInputBytes) {
        error = SyntaxError{1, 1, "document is larger than 16 MiB"};
        return std::nullopt;
    }
    // A UTF-8 BOM is a file-writing accident, not content.
    auto body = text;
    if (body.size() >= 3 && static_cast<unsigned char>(body[0]) == 0xEFU && static_cast<unsigned char>(body[1]) == 0xBBU &&
        static_cast<unsigned char>(body[2]) == 0xBFU) {
        body.remove_prefix(3);
    }

    Reader reader{body};
    Value root{};
    if (!reader.ParseDocument(root, error))
        return std::nullopt;
    return root;
}

auto Parse(const std::string_view text) -> Result<Value> {
    SyntaxError error{};
    auto root = TryParse(text, error);
    if (!root.has_value()) {
        return std::unexpected<Error>{MakeError(ErrorCode::InvalidDocument, Module::Core, "Json::Parse")
                                          .WithDetail(FormatPosition(error.line, error.column, error.message))};
    }
    return std::move(*root);
}

} // namespace atom::json
