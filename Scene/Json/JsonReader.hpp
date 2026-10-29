// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file JsonReader.hpp
 * @brief A strict JSONC reader (JSON + `//` comments) with line/column diagnostics.
 *
 * Scene documents are hand-written, so the reader's job is not only to accept
 * them but to explain what is wrong at the exact place it is wrong. That is why
 * this is a tokenizer feeding a recursive-descent parser rather than a library
 * wrapper: a `line:column` is always one token away here, and `//` comments have
 * to survive lexing anyway (a `res://` string contains `//` and must not be
 * treated as a comment - the tokenizer knows because it lexes strings first).
 *
 * What is rejected, and why:
 *   - trailing commas            -> almost always a half-edited file;
 *   - duplicate object keys      -> the second value silently wins in most
 *                                   parsers, which is the worst outcome;
 *   - NaN / Infinity / hex       -> not JSON, and produces NaN in the world;
 *   - raw control characters in strings;
 *   - more than `max_depth` (64) -> a stack-overflow guard, not a style rule;
 *   - anything after the root value (trailing garbage).
 *
 * The DOM keeps the source position on every value and object member so the
 * schema validator can point at the offending field rather than at the object.
 */

#ifndef ATOM_SCENE_JSON_JSONREADER_HPP
#define ATOM_SCENE_JSON_JSONREADER_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Error/Error.hpp>

namespace atom::scene::json {

enum class Kind : std::uint8_t { Null, Bool, Number, String, Array, Object };

struct Value;
using ValuePtr = std::unique_ptr<Value>;

// One "key": value in an object, with the position of the key itself.
struct Member {
        std::string key;
        ValuePtr value;
        std::uint32_t line = 0;
        std::uint32_t column = 0;
};

struct Value {
        Kind kind = Kind::Null;
        bool boolean = false;
        double number = 0.0;
        std::string string{};
        std::vector<ValuePtr> items{}; // Array
        std::vector<Member> members{}; // Object
        // 1-based position of the first character of the value.
        std::uint32_t line = 0;
        std::uint32_t column = 0;

        [[nodiscard]] auto IsObject() const noexcept -> bool {
            return kind == Kind::Object;
        }
        [[nodiscard]] auto IsArray() const noexcept -> bool {
            return kind == Kind::Array;
        }
        [[nodiscard]] auto IsString() const noexcept -> bool {
            return kind == Kind::String;
        }
        [[nodiscard]] auto IsNumber() const noexcept -> bool {
            return kind == Kind::Number;
        }
        [[nodiscard]] auto IsBool() const noexcept -> bool {
            return kind == Kind::Bool;
        }

        // Object member lookup; nullptr when absent. Duplicate keys cannot occur
        // (the parser rejects them), so the first match is the only match.
        [[nodiscard]] auto Find(std::string_view key) const noexcept -> const Value*;
};

// One syntax problem, with the position it was found at.
struct SyntaxError {
        std::uint32_t line = 1;
        std::uint32_t column = 1;
        std::string message{};
};

// Reads a whole JSONC document. On failure returns nullopt and fills `error`;
// callers that want a value-style failure use Parse() below.
[[nodiscard]] auto TryParse(std::string_view text, SyntaxError& error) -> std::optional<Value>;

// Same, as a Result. The Error carries `SceneInvalidDocument` with a
// "<line>:<column>: <message>" detail, formatted for a log line by appending the
// uri at the call site.
[[nodiscard]] auto Parse(std::string_view text) -> Result<Value>;

// "1:2: expected ',' or '}'" - exposed so callers can build the full
// "uri:line:column: message" themselves.
[[nodiscard]] auto FormatPosition(std::uint32_t line, std::uint32_t column, std::string_view message)
    -> std::string;

} // namespace atom::scene::json

#endif // ATOM_SCENE_JSON_JSONREADER_HPP
