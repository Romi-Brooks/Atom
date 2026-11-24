// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Sprite/SpriteAtlas.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <Filesystem/AssetPath.hpp>
#include <Utilities/Json/JsonReader.hpp>

namespace atom::sprite {

namespace {

using json::Diagnostic;
using json::Kind;
using json::Value;

constexpr std::string_view kAtlasKeys[] = {"schema_version", "kind", "id", "texture", "filter", "frames"};
constexpr std::string_view kFrameKeys[] = {"rect", "size", "pivot"};
constexpr std::size_t kMaxFrames = 4096;

struct Ctx final {
        std::string_view uri{};
        std::vector<Diagnostic>* diagnostics = nullptr;

        [[nodiscard]] auto Full() const noexcept -> bool {
            return diagnostics == nullptr || diagnostics->size() >= kMaxDiagnostics;
        }
};

auto Report(Ctx& ctx, const std::uint32_t line, const std::uint32_t column, std::string field, std::string message)
    -> void {
    if (ctx.Full())
        return;
    Diagnostic diagnostic{};
    diagnostic.uri = std::string{ctx.uri};
    diagnostic.line = line;
    diagnostic.column = column;
    diagnostic.field = std::move(field);
    diagnostic.message = std::move(message);
    ctx.diagnostics->push_back(std::move(diagnostic));
}

auto KindName(const Kind kind) -> std::string_view {
    switch (kind) {
        case Kind::Null:
            return "null";
        case Kind::Bool:
            return "a boolean";
        case Kind::Number:
            return "a number";
        case Kind::String:
            return "a string";
        case Kind::Array:
            return "an array";
        case Kind::Object:
            return "an object";
    }
    return "a value";
}

auto Levenshtein(const std::string_view left, const std::string_view right) -> std::size_t {
    const std::size_t lhs = left.size();
    const std::size_t rhs = right.size();
    std::vector<std::size_t> previous(rhs + 1);
    std::vector<std::size_t> current(rhs + 1);
    for (std::size_t j = 0; j <= rhs; ++j)
        previous[j] = j;
    for (std::size_t i = 1; i <= lhs; ++i) {
        current[0] = i;
        for (std::size_t j = 1; j <= rhs; ++j) {
            const std::size_t cost = left[i - 1] == right[j - 1] ? 0 : 1;
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost});
        }
        std::swap(previous, current);
    }
    return previous[rhs];
}

auto SuggestField(const std::string_view actual, const std::span<const std::string_view> allowed) -> std::string {
    std::string_view best{};
    std::size_t best_distance = 3;
    for (const auto candidate : allowed) {
        const auto distance = Levenshtein(actual, candidate);
        if (distance < best_distance) {
            best_distance = distance;
            best = candidate;
        }
    }
    if (best.empty())
        return {};
    return " (did you mean \"" + std::string{best} + "\"?)";
}

auto CheckKeys(Ctx& ctx, const Value& object, const std::string& prefix,
               const std::span<const std::string_view> allowed) -> void {
    for (const auto& member : object.members) {
        if (std::ranges::find(allowed, member.key) != allowed.end())
            continue;
        Report(ctx, member.line, member.column, prefix.empty() ? member.key : prefix + "." + member.key,
               "unknown field \"" + member.key + "\"" + SuggestField(member.key, allowed));
    }
}

auto ReadString(Ctx& ctx, const Value& value, const std::string& field, std::string& out, const std::size_t max_length)
    -> bool {
    if (!value.IsString()) {
        Report(ctx, value.line, value.column, field, "expected a string, got " + std::string{KindName(value.kind)});
        return false;
    }
    if (value.string.size() > max_length) {
        Report(ctx, value.line, value.column, field,
               "string is longer than " + std::to_string(max_length) + " bytes");
        return false;
    }
    out = value.string;
    return true;
}

auto ReadFloat(Ctx& ctx, const Value& value, const std::string& field, float& out) -> bool {
    if (!value.IsNumber()) {
        Report(ctx, value.line, value.column, field, "expected a number, got " + std::string{KindName(value.kind)});
        return false;
    }
    const auto as_float = static_cast<float>(value.number);
    if (!std::isfinite(as_float)) {
        Report(ctx, value.line, value.column, field, "number is out of range for a 32-bit float");
        return false;
    }
    out = as_float;
    return true;
}

auto ReadVec2(Ctx& ctx, const Value& value, const std::string& field, algo::Vec2& out) -> bool {
    if (!value.IsArray() || value.items.size() != 2) {
        Report(ctx, value.line, value.column, field,
               "expected an array of 2 numbers, got " +
                   (value.IsArray() ? std::to_string(value.items.size()) + " element(s)"
                                    : std::string{KindName(value.kind)}));
        return false;
    }
    float x = 0.0F;
    float y = 0.0F;
    const bool got_x = ReadFloat(ctx, *value.items[0], field + "[0]", x);
    const bool got_y = ReadFloat(ctx, *value.items[1], field + "[1]", y);
    if (!got_x || !got_y)
        return false;
    out = {x, y};
    return true;
}

auto ReadRect(Ctx& ctx, const Value& value, const std::string& field, algo::Rect& out) -> bool {
    if (!value.IsArray() || value.items.size() != 4) {
        Report(ctx, value.line, value.column, field,
               "expected an array of 4 numbers, got " +
                   (value.IsArray() ? std::to_string(value.items.size()) + " element(s)"
                                    : std::string{KindName(value.kind)}));
        return false;
    }
    float components[4] = {0.0F, 0.0F, 0.0F, 0.0F};
    bool ok = true;
    for (std::size_t i = 0; i < 4; ++i)
        ok = ReadFloat(ctx, *value.items[i], field + "[" + std::to_string(i) + "]", components[i]) && ok;
    if (!ok)
        return false;
    out = {components[0], components[1], components[2], components[3]};
    return true;
}

auto IsResUri(const std::string& text) -> bool {
    fs::AssetPath path{};
    if (!fs::AssetPath::TryParse(text, path))
        return false;
    return path.Mount() == "res" && !path.IsRoot();
}

auto ReadFrame(Ctx& ctx, const json::Member& member, AtlasFrame& out) -> bool {
    const auto field = "frames." + member.key;
    out.name = member.key;
    out.line = member.line;
    out.column = member.column;

    if (member.value == nullptr)
        return false;
    const Value& value = *member.value;
    if (!value.IsObject()) {
        Report(ctx, value.line, value.column, field,
               "expected an object, got " + std::string{KindName(value.kind)});
        return false;
    }
    CheckKeys(ctx, value, field, kFrameKeys);

    bool ok = true;
    const auto* rect = value.Find("rect");
    if (rect == nullptr) {
        Report(ctx, value.line, value.column, field + ".rect", "a frame requires a \"rect\": [x, y, width, height]");
        return false;
    }
    if (ReadRect(ctx, *rect, field + ".rect", out.source)) {
        if (out.source.width <= 0.0F || out.source.height <= 0.0F) {
            Report(ctx, rect->line, rect->column, field + ".rect", "rect width and height must be greater than zero");
            ok = false;
        }
    } else {
        ok = false;
    }

    // Default first, so the "1 texel = 1 px unless the author says otherwise" rule is
    // applied exactly once, here.
    out.size = {out.source.width, out.source.height};

    if (const auto* size = value.Find("size"); size != nullptr) {
        algo::Vec2 parsed{};
        if (ReadVec2(ctx, *size, field + ".size", parsed)) {
            if (parsed.GetX() <= 0.0F || parsed.GetY() <= 0.0F) {
                Report(ctx, size->line, size->column, field + ".size",
                       "size must be greater than zero on both axes");
                ok = false;
            } else {
                out.size = parsed;
            }
        } else {
            ok = false;
        }
    }

    if (const auto* pivot = value.Find("pivot"); pivot != nullptr) {
        algo::Vec2 parsed{};
        if (ReadVec2(ctx, *pivot, field + ".pivot", parsed))
            out.pivot = parsed;
        else
            ok = false;
    }

    return ok;
}

} // namespace

auto FilterName(const AtlasFilter filter) -> std::string_view {
    switch (filter) {
        case AtlasFilter::Linear:
            return "linear";
        case AtlasFilter::Nearest:
            return "nearest";
    }
    return "linear";
}

auto SpriteAtlas::Find(const std::string_view name) const -> const AtlasFrame* {
    const auto found = frame_index.find(std::string{name});
    if (found == frame_index.end())
        return nullptr;
    return &frames[found->second];
}

auto SpriteAtlas::FrameNames(const std::size_t limit) const -> std::string {
    std::string out{};
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (i == limit) {
            out += ", ... (" + std::to_string(frames.size() - limit) + " more)";
            break;
        }
        if (i != 0)
            out += ", ";
        out += frames[i].name;
    }
    return out;
}

auto ParseAtlas(const std::string_view text, const std::string_view uri) -> AtlasParseOutcome {
    AtlasParseOutcome outcome{};

    json::SyntaxError syntax{};
    auto document = json::TryParse(text, syntax);
    if (!document.has_value()) {
        Diagnostic diagnostic{};
        diagnostic.uri = std::string{uri};
        diagnostic.line = syntax.line;
        diagnostic.column = syntax.column;
        diagnostic.message = syntax.message;
        outcome.diagnostics.push_back(std::move(diagnostic));
        return outcome;
    }

    const Value& root = *document;
    Ctx ctx{};
    ctx.uri = uri;
    ctx.diagnostics = &outcome.diagnostics;

    if (!root.IsObject()) {
        Report(ctx, root.line, root.column, {}, "the atlas root must be an object, got " + std::string{KindName(root.kind)});
        return outcome;
    }

    // Version, then kind: without a known version nothing else may be interpreted, and
    // `kind` catches "a scene file was passed as an atlas" with a message that says so.
    const auto* version = root.Find("schema_version");
    if (version == nullptr) {
        Report(ctx, root.line, root.column, "schema_version",
               "atlas requires \"schema_version\" (this build reads " + std::to_string(kSchemaVersion) + ")");
        return outcome;
    }
    if (!version->IsNumber() || version->number != static_cast<double>(kSchemaVersion)) {
        Report(ctx, version->line, version->column, "schema_version",
               "unsupported schema_version (this build reads " + std::to_string(kSchemaVersion) +
                   "); refusing to guess");
        return outcome;
    }

    const auto* kind = root.Find("kind");
    if (kind == nullptr) {
        Report(ctx, root.line, root.column, "kind", "atlas requires \"kind\": \"atlas\"");
        return outcome;
    }
    std::string kind_text{};
    if (!ReadString(ctx, *kind, "kind", kind_text, 16) || kind_text != "atlas") {
        if (ctx.diagnostics->empty())
            Report(ctx, kind->line, kind->column, "kind", "expected \"atlas\", got \"" + kind_text + "\"");
        return outcome;
    }

    CheckKeys(ctx, root, {}, kAtlasKeys);

    SpriteAtlas atlas{};
    atlas.schema_version = kSchemaVersion;

    if (const auto* id = root.Find("id"); id == nullptr) {
        Report(ctx, root.line, root.column, "id", "atlas requires an \"id\"");
    } else if (ReadString(ctx, *id, "id", atlas.id, 64)) {
        for (const char c : atlas.id) {
            const bool legal = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            if (!legal) {
                Report(ctx, id->line, id->column, "id",
                       std::string{"atlas id may only contain a-z, 0-9, '_' and '-' (found '"} + c + "')");
                break;
            }
        }
    }

    if (const auto* texture = root.Find("texture"); texture == nullptr) {
        Report(ctx, root.line, root.column, "texture", "atlas requires a \"texture\" URI");
    } else if (ReadString(ctx, *texture, "texture", atlas.texture_uri, 512)) {
        if (!IsResUri(atlas.texture_uri)) {
            Report(ctx, texture->line, texture->column, "texture",
                   "texture must be a res:// asset URI without a root or traversal, got \"" + atlas.texture_uri + "\"");
        }
    }

    if (const auto* filter = root.Find("filter"); filter != nullptr) {
        std::string filter_text{};
        if (ReadString(ctx, *filter, "filter", filter_text, 16)) {
            if (filter_text == "linear") {
                atlas.filter = AtlasFilter::Linear;
            } else if (filter_text == "nearest") {
                atlas.filter = AtlasFilter::Nearest;
            } else {
                // The allowed values are listed, because "nearest" vs "nearest-neighbour"
                // vs "point" is a vocabulary question, not a bug report.
                Report(ctx, filter->line, filter->column, "filter",
                       "unknown filter \"" + filter_text + "\" (allowed: \"linear\", \"nearest\")");
            }
        }
    }

    const auto* frames = root.Find("frames");
    if (frames == nullptr) {
        Report(ctx, root.line, root.column, "frames", "atlas requires a \"frames\" object");
    } else if (!frames->IsObject()) {
        Report(ctx, frames->line, frames->column, "frames",
               "expected an object keyed by frame name, got " + std::string{KindName(frames->kind)});
    } else if (frames->members.empty()) {
        Report(ctx, frames->line, frames->column, "frames", "atlas has no frames");
    } else if (frames->members.size() > kMaxFrames) {
        Report(ctx, frames->line, frames->column, "frames",
               "atlas has more than " + std::to_string(kMaxFrames) + " frames");
    } else {
        atlas.frames.reserve(frames->members.size());
        for (const auto& member : frames->members) {
            AtlasFrame frame{};
            if (!ReadFrame(ctx, member, frame))
                continue;
            atlas.frame_index.emplace(frame.name, atlas.frames.size());
            atlas.frames.push_back(std::move(frame));
        }
    }

    if (outcome.diagnostics.empty())
        outcome.atlas = std::move(atlas);
    return outcome;
}

} // namespace atom::sprite
