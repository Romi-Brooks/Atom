/**
 * @file           : JsonReaderTest.cpp
 * @brief          : T2.2 - the JSONC reader and its diagnostics.
 *
 * Scene files are hand-written, so the reader's failure modes matter as much as its
 * success modes. Every rejection here is a behaviour the schema document promises,
 * and each assertion also pins the *position*, because "parse failed" without a line
 * number is what makes hand-editing miserable.
 */

#include <Scene/Json/JsonReader.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstdint>
#include <string>

namespace {

using atom::scene::json::Kind;
using atom::scene::json::Parse;
using atom::scene::json::TryParse;

// Parses and expects failure, returning the syntax error position and message.
struct Failure {
        bool failed = false;
        std::uint32_t line = 0;
        std::uint32_t column = 0;
        std::string message{};
};

auto ExpectFailure(const std::string_view text) -> Failure {
    atom::scene::json::SyntaxError error{};
    const auto value = TryParse(text, error);
    Failure failure{};
    failure.failed = !value.has_value();
    failure.line = error.line;
    failure.column = error.column;
    failure.message = error.message;
    return failure;
}

auto Contains(const std::string& haystack, const std::string_view needle) -> bool {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

auto main() -> int {
    // --- 1. Values, nesting and positions ---------------------------------
    {
        const auto parsed = Parse(R"({
            "a": 1,
            "b": [true, false, null],
            "c": { "d": "text" },
            "e": -1.5e2
        })");
        ATOM_CHECK(parsed.has_value());
        ATOM_CHECK(parsed->IsObject());
        ATOM_CHECK(parsed->members.size() == 4);

        const auto* a = parsed->Find("a");
        ATOM_CHECK(a != nullptr && a->IsNumber() && a->number == 1.0);
        // Position points at the value, not the key: that is what a reader needs.
        ATOM_CHECK(a->line == 2);
        ATOM_CHECK(a->column == 18);

        const auto* b = parsed->Find("b");
        ATOM_CHECK(b != nullptr && b->IsArray() && b->items.size() == 3);
        ATOM_CHECK(b->items[0]->kind == Kind::Bool && b->items[0]->boolean);
        ATOM_CHECK(b->items[2]->kind == Kind::Null);

        const auto* c = parsed->Find("c");
        ATOM_CHECK(c != nullptr && c->IsObject());
        const auto* d = c->Find("d");
        ATOM_CHECK(d != nullptr && d->IsString() && d->string == "text");

        const auto* e = parsed->Find("e");
        ATOM_CHECK(e != nullptr && e->IsNumber());
        ATOM_CHECK_NEAR(static_cast<float>(e->number), -150.0F);
    }

    // --- 2. // comments are skipped, but not inside strings ----------------
    {
        // The "res://" case is the one that matters: a naive comment stripper
        // destroys every texture URI in every scene file.
        const auto parsed = Parse(R"({
            // a line comment before a value
            "texture": "res://textures/player.png", // trailing comment
            "not_a_comment": "a // b"
        })");
        ATOM_CHECK(parsed.has_value());
        ATOM_CHECK(parsed->Find("texture")->string == "res://textures/player.png");
        ATOM_CHECK(parsed->Find("not_a_comment")->string == "a // b");
    }

    // --- 3. Escapes --------------------------------------------------------
    {
        const auto parsed = Parse(R"({ "s": "tab\t newline\n quote\" backslash\\ slash\/ unicode\u00e9 pair\ud83d\ude00" })");
        ATOM_CHECK(parsed.has_value());
        const auto& value = parsed->Find("s")->string;
        ATOM_CHECK(Contains(value, "tab\t newline\n"));
        ATOM_CHECK(Contains(value, "quote\""));
        ATOM_CHECK(Contains(value, "backslash\\"));
        ATOM_CHECK(Contains(value, "slash/"));
        ATOM_CHECK(Contains(value, "unicode\xC3\xA9")); // é in UTF-8
        ATOM_CHECK(Contains(value, "pair\xF0\x9F\x98\x80")); // 😀 in UTF-8
    }

    // --- 4. Rejections: each one pins the promised behaviour ---------------
    {
        // Trailing comma.
        const auto trailing = ExpectFailure(R"({ "a": 1, })");
        ATOM_CHECK(trailing.failed);
        ATOM_CHECK(trailing.line == 1);
        ATOM_CHECK(Contains(trailing.message, "trailing comma"));

        // Duplicate keys keep the *first*, or they are rejected - never "last wins".
        const auto duplicate = ExpectFailure(R"({ "a": 1, "a": 2 })");
        ATOM_CHECK(duplicate.failed);
        ATOM_CHECK(Contains(duplicate.message, "duplicate key"));
        ATOM_CHECK(Contains(duplicate.message, "first seen at 1:3"));

        // NaN / Infinity are not JSON and would poison the world.
        ATOM_CHECK(ExpectFailure(R"({ "a": NaN })").failed);
        ATOM_CHECK(ExpectFailure(R"({ "a": Infinity })").failed);
        // Leading zeros and bare '.' are the classic hand-typo shapes.
        ATOM_CHECK(ExpectFailure(R"({ "a": 01 })").failed);
        ATOM_CHECK(ExpectFailure(R"({ "a": .5 })").failed);
        ATOM_CHECK(ExpectFailure(R"({ "a": 1. })").failed);

        // Unterminated string, raw newline inside a string, bad escape.
        ATOM_CHECK(ExpectFailure(R"({ "a": "unterminated )").failed);
        ATOM_CHECK(ExpectFailure("{\n \"a\": \"raw\nnewline\" }").failed);
        ATOM_CHECK(ExpectFailure(R"({ "a": "\q" })").failed);

        // Unpaired surrogates must not become invalid UTF-8.
        ATOM_CHECK(ExpectFailure(R"({ "a": "\ud800" })").failed);
        ATOM_CHECK(ExpectFailure(R"({ "a": "\udc00" })").failed);

        // Trailing garbage after the root value.
        const auto garbage = ExpectFailure(R"({ "a": 1 } trailing)");
        ATOM_CHECK(garbage.failed);
        ATOM_CHECK(Contains(garbage.message, "after the root value"));

        // Empty input and whitespace-only input.
        ATOM_CHECK(ExpectFailure("").failed);
        ATOM_CHECK(ExpectFailure("  \n // only a comment\n").failed);

        // Depth limit: 65 nested arrays must fail rather than overflow the stack.
        std::string deep{};
        for (int i = 0; i < 65; ++i)
            deep.push_back('[');
        for (int i = 0; i < 65; ++i)
            deep.push_back(']');
        const auto too_deep = ExpectFailure(deep);
        ATOM_CHECK(too_deep.failed);
        ATOM_CHECK(Contains(too_deep.message, "deeper than 64"));
    }

    // --- 5. BOM and CRLF, because both happen on Windows -------------------
    {
        const std::string with_bom = "\xEF\xBB\xBF{\r\n  \"a\": 1\r\n}\r\n";
        const auto parsed = Parse(with_bom);
        ATOM_CHECK(parsed.has_value());
        ATOM_CHECK(parsed->Find("a") != nullptr);
        // CRLF must not shift the reported line of a later value.
        ATOM_CHECK(parsed->Find("a")->line == 2);
    }

    // --- 6. The Result form carries position and code ---------------------
    {
        const auto parsed = Parse(R"({ "a": 1, })");
        ATOM_CHECK(!parsed.has_value());
        ATOM_CHECK(parsed.error().Code() == atom::ErrorCode::SceneInvalidDocument);
        // ModuleOf() derives the owning module from the code block, which is the
        // invariant that keeps a code from being filed under the wrong module.
        ATOM_CHECK(atom::ModuleOf(parsed.error().Code()) == atom::Module::Scene);
        ATOM_CHECK(parsed.error().MatchesModule());
        ATOM_CHECK(Contains(std::string{parsed.error().Detail()}, "1:11:"));
    }

    // --- 7. Deep-but-legal nesting still parses ---------------------------
    {
        const auto parsed = Parse(R"({ "a": [ [ [ 1 ] ] ] })");
        ATOM_CHECK(parsed.has_value());
        ATOM_CHECK(parsed->Find("a")->items[0]->items[0]->items[0]->number == 1.0);
    }

    return 0;
}
