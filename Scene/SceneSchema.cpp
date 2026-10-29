// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Scene/SceneSchema.hpp>

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

namespace atom::scene {

namespace {

using json::Kind;
using json::Value;

// --- diagnostics ------------------------------------------------------------

struct Ctx final {
        std::string_view uri{};
        std::vector<Diagnostic>* diagnostics = nullptr;
        std::size_t per_entity = 0;

        [[nodiscard]] auto Full() const noexcept -> bool {
            return diagnostics == nullptr || diagnostics->size() >= kMaxDiagnostics;
        }
};

// Everything entity-scoped goes through here so the per-entity cap is applied in
// one place: an entity with 50 bad fields must not push out the errors from the
// other entities.
auto ReportAt(Ctx& ctx, const std::uint32_t line, const std::uint32_t column, const std::string& entity,
              std::string field, std::string message) -> void {
    if (ctx.Full())
        return;
    if (!entity.empty()) {
        if (ctx.per_entity >= kMaxDiagnosticsPerEntity)
            return;
        ++ctx.per_entity;
    }
    Diagnostic diagnostic{};
    diagnostic.uri = std::string{ctx.uri};
    diagnostic.line = line;
    diagnostic.column = column;
    diagnostic.entity = entity;
    diagnostic.field = std::move(field);
    diagnostic.message = std::move(message);
    ctx.diagnostics->push_back(std::move(diagnostic));
}

auto Report(Ctx& ctx, const Value& at, const std::string& entity, std::string field, std::string message) -> void {
    ReportAt(ctx, at.line, at.column, entity, std::move(field), std::move(message));
}

// --- small utilities --------------------------------------------------------

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

// Only suggests when the key is a slip away from a legal one: a genuine attempt at
// a new field should read as "unknown field", not as "did you mean ...".
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

auto JoinFields(const std::string_view prefix, const std::string_view name) -> std::string {
    std::string out{prefix};
    if (!out.empty())
        out.push_back('.');
    out.append(name);
    return out;
}

auto CheckKeys(Ctx& ctx, const Value& object, const std::string& entity, const std::string_view prefix,
               const std::span<const std::string_view> allowed) -> void {
    for (const auto& member : object.members) {
        if (std::ranges::find(allowed, member.key) != allowed.end())
            continue;
        ReportAt(ctx, member.line, member.column, entity, JoinFields(prefix, member.key),
                 "unknown field \"" + member.key + "\"" + SuggestField(member.key, allowed));
    }
}

// --- typed readers ----------------------------------------------------------

auto ReadString(Ctx& ctx, const Value& value, const std::string& entity, const std::string& field, std::string& out,
                const std::size_t max_length) -> bool {
    if (!value.IsString()) {
        Report(ctx, value, entity, field, "expected a string, got " + std::string{KindName(value.kind)});
        return false;
    }
    if (value.string.size() > max_length) {
        Report(ctx, value, entity, field, "string is longer than " + std::to_string(max_length) + " bytes");
        return false;
    }
    out = value.string;
    return true;
}

auto ReadBool(Ctx& ctx, const Value& value, const std::string& entity, const std::string& field, bool& out) -> bool {
    if (!value.IsBool()) {
        Report(ctx, value, entity, field, "expected a boolean, got " + std::string{KindName(value.kind)});
        return false;
    }
    out = value.boolean;
    return true;
}

auto ReadFloat(Ctx& ctx, const Value& value, const std::string& entity, const std::string& field, float& out) -> bool {
    if (!value.IsNumber()) {
        Report(ctx, value, entity, field, "expected a number, got " + std::string{KindName(value.kind)});
        return false;
    }
    const auto as_float = static_cast<float>(value.number);
    if (!std::isfinite(as_float)) {
        Report(ctx, value, entity, field, "number is out of range for a 32-bit float");
        return false;
    }
    out = as_float;
    return true;
}

// Integers may not be written as floats: "2.0" reads as a number to a human but
// would silently round for a caller expecting an id or a layer.
auto ReadInteger(Ctx& ctx, const Value& value, const std::string& entity, const std::string& field,
                 std::int64_t& out) -> bool {
    if (!value.IsNumber()) {
        Report(ctx, value, entity, field, "expected an integer, got " + std::string{KindName(value.kind)});
        return false;
    }
    if (std::floor(value.number) != value.number) {
        Report(ctx, value, entity, field, "expected an integer, got a fractional number");
        return false;
    }
    if (value.number < -2147483648.0 || value.number > 2147483647.0) {
        Report(ctx, value, entity, field, "integer is out of the 32-bit range");
        return false;
    }
    out = static_cast<std::int64_t>(value.number);
    return true;
}

// Entity ids are unsigned 32-bit, while every other integer in the format is a
// signed 32-bit value (layer, sort_key, colour channels). One reader for both would
// either shrink the id space or let a layer value overflow into negative numbers.
auto ReadUint32(Ctx& ctx, const Value& value, const std::string& entity, const std::string& field,
                std::uint32_t& out) -> bool {
    if (!value.IsNumber()) {
        Report(ctx, value, entity, field, "expected an integer, got " + std::string{KindName(value.kind)});
        return false;
    }
    if (std::floor(value.number) != value.number) {
        Report(ctx, value, entity, field, "expected an integer, got a fractional number");
        return false;
    }
    if (value.number < 0.0 || value.number > 4294967295.0) {
        Report(ctx, value, entity, field, "must fit in an unsigned 32-bit integer");
        return false;
    }
    out = static_cast<std::uint32_t>(value.number);
    return true;
}

auto ReadVec2(Ctx& ctx, const Value& value, const std::string& entity, const std::string& field, algo::Vec2& out)
    -> bool {
    if (!value.IsArray()) {
        Report(ctx, value, entity, field, "expected an array of 2 numbers, got " + std::string{KindName(value.kind)});
        return false;
    }
    if (value.items.size() != 2) {
        Report(ctx, value, entity, field,
               "expected 2 numbers, got " + std::to_string(value.items.size()) + " element(s)");
        return false;
    }
    float x = 0.0F;
    float y = 0.0F;
    const bool got_x = ReadFloat(ctx, *value.items[0], entity, field + "[0]", x);
    const bool got_y = ReadFloat(ctx, *value.items[1], entity, field + "[1]", y);
    if (!got_x || !got_y)
        return false;
    out = {x, y};
    return true;
}

auto ReadRect(Ctx& ctx, const Value& value, const std::string& entity, const std::string& field, algo::Rect& out)
    -> bool {
    if (!value.IsArray()) {
        Report(ctx, value, entity, field, "expected an array of 4 numbers, got " + std::string{KindName(value.kind)});
        return false;
    }
    if (value.items.size() != 4) {
        Report(ctx, value, entity, field,
               "expected 4 numbers, got " + std::to_string(value.items.size()) + " element(s)");
        return false;
    }
    float components[4] = {0.0F, 0.0F, 0.0F, 0.0F};
    bool ok = true;
    for (std::size_t i = 0; i < 4; ++i)
        ok = ReadFloat(ctx, *value.items[i], entity, field + "[" + std::to_string(i) + "]", components[i]) && ok;
    if (!ok)
        return false;
    out = {components[0], components[1], components[2], components[3]};
    return true;
}

auto ReadColor(Ctx& ctx, const Value& value, const std::string& entity, const std::string& field, color::Color& out)
    -> bool {
    if (!value.IsArray()) {
        Report(ctx, value, entity, field, "expected an array of 4 integers, got " + std::string{KindName(value.kind)});
        return false;
    }
    if (value.items.size() != 4) {
        Report(ctx, value, entity, field,
               "expected 4 integers, got " + std::to_string(value.items.size()) + " element(s)");
        return false;
    }
    std::uint8_t channels[4] = {0, 0, 0, 255};
    bool ok = true;
    for (std::size_t i = 0; i < 4; ++i) {
        std::int64_t component = 0;
        if (!ReadInteger(ctx, *value.items[i], entity, field + "[" + std::to_string(i) + "]", component)) {
            ok = false;
            continue;
        }
        if (component < 0 || component > 255) {
            Report(ctx, *value.items[i], entity, field + "[" + std::to_string(i) + "]",
                   "colour channel must be in 0..255, got " + std::to_string(component));
            ok = false;
            continue;
        }
        channels[i] = static_cast<std::uint8_t>(component);
    }
    if (!ok)
        return false;
    out = {channels[0], channels[1], channels[2], channels[3]};
    return true;
}

auto IsResUri(const std::string& text) -> bool {
    fs::AssetPath path{};
    if (!fs::AssetPath::TryParse(text, path))
        return false;
    return path.Mount() == "res" && !path.IsRoot();
}

// --- component blocks -------------------------------------------------------

constexpr std::string_view kTransformKeys[] = {"position", "rotation", "scale"};
constexpr std::string_view kSpriteKeys[] = {"texture", "source",  "size",    "pivot", "tint",
                                            "layer",   "sort_key", "flip_x", "flip_y"};
constexpr std::string_view kCameraKeys[] = {"zoom", "viewport", "active"};
constexpr std::string_view kEntityKeys[] = {"id", "name", "tags", "transform", "sprite", "camera", "lifetime"};
// A prefab instance is `{ id, prefab, overrides? }` and nothing else.
constexpr std::string_view kPrefabInstanceKeys[] = {"id", "prefab", "overrides"};
// Overridable per block. What is *absent* is the point: a prefab instance may move,
// tint, layer and re-time an entity, but not change what it is.
constexpr std::string_view kOverrideKeys[] = {"name", "tags", "transform", "sprite", "camera", "lifetime"};
constexpr std::string_view kOverrideTransformKeys[] = {"position", "rotation", "scale"};
constexpr std::string_view kOverrideSpriteKeys[] = {"tint", "layer", "sort_key", "flip_x", "flip_y"};
constexpr std::string_view kOverrideCameraKeys[] = {"zoom", "viewport", "active"};
constexpr std::string_view kPrefabDocumentKeys[] = {"schema_version", "kind", "id", "entity"};
// A prefab body is a scene entity body minus identity: every instance supplies its own
// id, so an id here would be read by nobody and silently ignored.
constexpr std::string_view kPrefabEntityKeys[] = {"name", "tags", "transform", "sprite", "camera", "lifetime"};
constexpr std::string_view kSceneKeys[] = {"schema_version", "id", "name", "entities"};
constexpr std::string_view kTagActive = "active";
constexpr std::string_view kTagStatic = "static";
constexpr std::string_view kTagRejected = "pending_destroy";

auto ReadTransform(Ctx& ctx, const Value& value, const std::string& entity, TransformDesc& out) -> void {
    if (!value.IsObject()) {
        Report(ctx, value, entity, "transform", "expected an object, got " + std::string{KindName(value.kind)});
        return;
    }
    CheckKeys(ctx, value, entity, "transform", kTransformKeys);
    if (const auto* member = value.Find("position"); member != nullptr)
        (void)ReadVec2(ctx, *member, entity, "transform.position", out.position);
    if (const auto* member = value.Find("rotation"); member != nullptr)
        (void)ReadFloat(ctx, *member, entity, "transform.rotation", out.rotation);
    if (const auto* member = value.Find("scale"); member != nullptr) {
        algo::Vec2 scale{1.0F, 1.0F};
        if (ReadVec2(ctx, *member, entity, "transform.scale", scale)) {
            if (scale.GetX() == 0.0F || scale.GetY() == 0.0F)
                Report(ctx, *member, entity, "transform.scale", "scale must be non-zero on both axes");
            else
                out.scale = scale;
        }
    }
}

auto ReadSprite(Ctx& ctx, const Value& value, const std::string& entity, SpriteDesc& out) -> bool {
    if (!value.IsObject()) {
        Report(ctx, value, entity, "sprite", "expected an object, got " + std::string{KindName(value.kind)});
        return false;
    }
    CheckKeys(ctx, value, entity, "sprite", kSpriteKeys);

    bool ok = true;
    if (const auto* member = value.Find("texture"); member == nullptr) {
        Report(ctx, value, entity, "sprite.texture", "sprite requires a \"texture\" URI");
        ok = false;
    } else if (ReadString(ctx, *member, entity, "sprite.texture", out.texture_uri, 512)) {
        if (!IsResUri(out.texture_uri)) {
            Report(ctx, *member, entity, "sprite.texture",
                   "texture must be a res:// asset URI without a root or traversal, got \"" + out.texture_uri + "\"");
            ok = false;
        }
    } else {
        ok = false;
    }

    if (const auto* member = value.Find("source"); member != nullptr) {
        algo::Rect source{};
        if (ReadRect(ctx, *member, entity, "sprite.source", source)) {
            if (source.width <= 0.0F || source.height <= 0.0F) {
                Report(ctx, *member, entity, "sprite.source", "source width and height must be greater than zero");
                ok = false;
            } else {
                out.source = source;
                out.has_source = true;
            }
        } else {
            ok = false;
        }
    }

    if (const auto* member = value.Find("size"); member != nullptr) {
        algo::Vec2 size{};
        if (ReadVec2(ctx, *member, entity, "sprite.size", size)) {
            if (size.GetX() <= 0.0F || size.GetY() <= 0.0F) {
                Report(ctx, *member, entity, "sprite.size", "size must be greater than zero on both axes");
                ok = false;
            } else {
                out.size = size;
                out.has_size = true;
            }
        } else {
            ok = false;
        }
    }

    if (const auto* member = value.Find("pivot"); member != nullptr)
        ok = ReadVec2(ctx, *member, entity, "sprite.pivot", out.pivot) && ok;
    if (const auto* member = value.Find("tint"); member != nullptr)
        ok = ReadColor(ctx, *member, entity, "sprite.tint", out.tint) && ok;
    if (const auto* member = value.Find("layer"); member != nullptr) {
        std::int64_t layer = 0;
        if (ReadInteger(ctx, *member, entity, "sprite.layer", layer))
            out.layer = static_cast<std::int32_t>(layer);
        else
            ok = false;
    }
    if (const auto* member = value.Find("sort_key"); member != nullptr) {
        std::int64_t sort_key = 0;
        if (ReadInteger(ctx, *member, entity, "sprite.sort_key", sort_key))
            out.sort_key = static_cast<std::int32_t>(sort_key);
        else
            ok = false;
    }
    if (const auto* member = value.Find("flip_x"); member != nullptr)
        ok = ReadBool(ctx, *member, entity, "sprite.flip_x", out.flip_x) && ok;
    if (const auto* member = value.Find("flip_y"); member != nullptr)
        ok = ReadBool(ctx, *member, entity, "sprite.flip_y", out.flip_y) && ok;
    return ok;
}

auto ReadCamera(Ctx& ctx, const Value& value, const std::string& entity, CameraDesc& out) -> bool {
    if (!value.IsObject()) {
        Report(ctx, value, entity, "camera", "expected an object, got " + std::string{KindName(value.kind)});
        return false;
    }
    CheckKeys(ctx, value, entity, "camera", kCameraKeys);

    bool ok = true;
    if (const auto* member = value.Find("zoom"); member != nullptr) {
        float zoom = 1.0F;
        if (ReadFloat(ctx, *member, entity, "camera.zoom", zoom)) {
            if (zoom <= 0.0F) {
                Report(ctx, *member, entity, "camera.zoom", "zoom must be greater than zero");
                ok = false;
            } else {
                out.zoom = zoom;
            }
        } else {
            ok = false;
        }
    }
    if (const auto* member = value.Find("viewport"); member != nullptr) {
        algo::Vec2 viewport{};
        if (ReadVec2(ctx, *member, entity, "camera.viewport", viewport)) {
            if (viewport.GetX() < 0.0F || viewport.GetY() < 0.0F) {
                Report(ctx, *member, entity, "camera.viewport", "viewport must not be negative");
                ok = false;
            } else {
                out.viewport = viewport;
            }
        } else {
            ok = false;
        }
    }
    if (const auto* member = value.Find("active"); member != nullptr)
        ok = ReadBool(ctx, *member, entity, "camera.active", out.active) && ok;
    return ok;
}

auto ReadTags(Ctx& ctx, const Value& value, const std::string& entity, EntityDesc& out) -> bool {
    if (!value.IsArray()) {
        Report(ctx, value, entity, "tags", "expected an array of strings, got " + std::string{KindName(value.kind)});
        return false;
    }
    bool ok = true;
    for (const auto& item : value.items) {
        if (!item->IsString()) {
            Report(ctx, *item, entity, "tags", "expected a string tag, got " + std::string{KindName(item->kind)});
            ok = false;
            continue;
        }
        if (item->string == kTagActive) {
            out.active = true;
        } else if (item->string == kTagStatic) {
            out.is_static = true;
        } else if (item->string == kTagRejected) {
            Report(ctx, *item, entity, "tags",
                   "\"" + item->string + "\" is a runtime-internal tag and cannot be authored");
            ok = false;
        } else {
            Report(ctx, *item, entity, "tags", "unknown tag \"" + item->string + "\" (allowed: \"active\", \"static\")");
            ok = false;
        }
    }
    return ok;
}

auto EntityLabel(const std::uint32_t id, const std::string& name) -> std::string {
    std::string label = "entity " + std::to_string(id);
    if (!name.empty())
        label += " (\"" + name + "\")";
    return label;
}

// The entity body: everything except the identity (`id`), the prefab reference and
// the override block. Split out because a prefab document carries exactly this and
// nothing else.
auto ReadEntityBody(Ctx& ctx, const Value& value, const std::string& label, EntityDesc& out) -> bool {
    bool ok = true;
    if (const auto* member = value.Find("name"); member != nullptr) {
        if (ReadString(ctx, *member, label, "name", out.name, 64))
            out.has_name = !out.name.empty();
        else
            ok = false;
    }
    if (const auto* member = value.Find("tags"); member != nullptr)
        ok = ReadTags(ctx, *member, label, out) && ok;
    if (const auto* member = value.Find("transform"); member != nullptr)
        ReadTransform(ctx, *member, label, out.transform);
    if (const auto* member = value.Find("sprite"); member != nullptr) {
        SpriteDesc sprite{};
        if (ReadSprite(ctx, *member, label, sprite))
            out.sprite = std::move(sprite);
        else
            ok = false;
    }
    if (const auto* member = value.Find("camera"); member != nullptr) {
        CameraDesc camera{};
        if (ReadCamera(ctx, *member, label, camera))
            out.camera = camera;
        else
            ok = false;
    }
    if (const auto* member = value.Find("lifetime"); member != nullptr) {
        float lifetime = 0.0F;
        if (ReadFloat(ctx, *member, label, "lifetime", lifetime)) {
            if (lifetime < 0.0F) {
                Report(ctx, *member, label, "lifetime", "lifetime must not be negative");
                ok = false;
            } else {
                out.lifetime = lifetime;
            }
        } else {
            ok = false;
        }
    }
    return ok;
}

// Applies an `overrides` block onto a prefab's base body.
//
// The whitelist is per block on purpose: a prefab instance may move, tint, layer and
// re-time an entity, but it may not change what the entity *is* (its texture, its
// source rect, its size, its pivot, its collision shape later). Identity fields are
// rejected with the same "unknown field" machinery, so the error names the field and
// suggests a legal neighbour instead of silently ignoring the attempt.
auto ApplyOverrides(Ctx& ctx, const Value& overrides, const std::string& label, EntityDesc& target) -> bool {
    if (!overrides.IsObject()) {
        Report(ctx, overrides, label, "overrides",
               "expected an object, got " + std::string{KindName(overrides.kind)});
        return false;
    }
    CheckKeys(ctx, overrides, label, "overrides", kOverrideKeys);

    bool ok = true;

    if (const auto* member = overrides.Find("name"); member != nullptr) {
        if (ReadString(ctx, *member, label, "overrides.name", target.name, 64))
            target.has_name = !target.name.empty();
        else
            ok = false;
    }

    if (const auto* member = overrides.Find("tags"); member != nullptr) {
        // Replace, not union: "the instance is static" and "the instance is not
        // static" must both be expressible, and a union could express neither.
        target.active = false;
        target.is_static = false;
        ok = ReadTags(ctx, *member, label + " (overrides)", target) && ok;
    }

    if (const auto* member = overrides.Find("transform"); member != nullptr) {
        if (!member->IsObject()) {
            Report(ctx, *member, label, "overrides.transform",
                   "expected an object, got " + std::string{KindName(member->kind)});
            ok = false;
        } else {
            CheckKeys(ctx, *member, label, "overrides.transform", kOverrideTransformKeys);
            if (const auto* value = member->Find("position"); value != nullptr)
                (void)ReadVec2(ctx, *value, label, "overrides.transform.position", target.transform.position);
            if (const auto* value = member->Find("rotation"); value != nullptr)
                (void)ReadFloat(ctx, *value, label, "overrides.transform.rotation", target.transform.rotation);
            if (const auto* value = member->Find("scale"); value != nullptr) {
                algo::Vec2 scale{1.0F, 1.0F};
                if (ReadVec2(ctx, *value, label, "overrides.transform.scale", scale)) {
                    if (scale.GetX() == 0.0F || scale.GetY() == 0.0F)
                        Report(ctx, *value, label, "overrides.transform.scale",
                               "scale must be non-zero on both axes");
                    else
                        target.transform.scale = scale;
                }
            }
        }
    }

    if (const auto* member = overrides.Find("sprite"); member != nullptr) {
        if (!target.sprite.has_value()) {
            Report(ctx, *member, label, "overrides.sprite",
                   "this prefab has no sprite, so there is nothing to override");
            ok = false;
        } else if (!member->IsObject()) {
            Report(ctx, *member, label, "overrides.sprite",
                   "expected an object, got " + std::string{KindName(member->kind)});
            ok = false;
        } else {
            CheckKeys(ctx, *member, label, "overrides.sprite", kOverrideSpriteKeys);
            auto& sprite = *target.sprite;
            if (const auto* value = member->Find("tint"); value != nullptr)
                ok = ReadColor(ctx, *value, label, "overrides.sprite.tint", sprite.tint) && ok;
            if (const auto* value = member->Find("layer"); value != nullptr) {
                std::int64_t layer = 0;
                if (ReadInteger(ctx, *value, label, "overrides.sprite.layer", layer))
                    sprite.layer = static_cast<std::int32_t>(layer);
                else
                    ok = false;
            }
            if (const auto* value = member->Find("sort_key"); value != nullptr) {
                std::int64_t sort_key = 0;
                if (ReadInteger(ctx, *value, label, "overrides.sprite.sort_key", sort_key))
                    sprite.sort_key = static_cast<std::int32_t>(sort_key);
                else
                    ok = false;
            }
            if (const auto* value = member->Find("flip_x"); value != nullptr)
                ok = ReadBool(ctx, *value, label, "overrides.sprite.flip_x", sprite.flip_x) && ok;
            if (const auto* value = member->Find("flip_y"); value != nullptr)
                ok = ReadBool(ctx, *value, label, "overrides.sprite.flip_y", sprite.flip_y) && ok;
        }
    }

    if (const auto* member = overrides.Find("camera"); member != nullptr) {
        if (!target.camera.has_value()) {
            Report(ctx, *member, label, "overrides.camera",
                   "this prefab has no camera, so there is nothing to override");
            ok = false;
        } else if (!member->IsObject()) {
            Report(ctx, *member, label, "overrides.camera",
                   "expected an object, got " + std::string{KindName(member->kind)});
            ok = false;
        } else {
            CheckKeys(ctx, *member, label, "overrides.camera", kOverrideCameraKeys);
            auto& camera = *target.camera;
            if (const auto* value = member->Find("zoom"); value != nullptr) {
                float zoom = 1.0F;
                if (ReadFloat(ctx, *value, label, "overrides.camera.zoom", zoom)) {
                    if (zoom <= 0.0F) {
                        Report(ctx, *value, label, "overrides.camera.zoom", "zoom must be greater than zero");
                        ok = false;
                    } else {
                        camera.zoom = zoom;
                    }
                } else {
                    ok = false;
                }
            }
            if (const auto* value = member->Find("viewport"); value != nullptr) {
                algo::Vec2 viewport{};
                if (ReadVec2(ctx, *value, label, "overrides.camera.viewport", viewport)) {
                    if (viewport.GetX() < 0.0F || viewport.GetY() < 0.0F) {
                        Report(ctx, *value, label, "overrides.camera.viewport", "viewport must not be negative");
                        ok = false;
                    } else {
                        camera.viewport = viewport;
                    }
                } else {
                    ok = false;
                }
            }
            if (const auto* value = member->Find("active"); value != nullptr)
                ok = ReadBool(ctx, *value, label, "overrides.camera.active", camera.active) && ok;
        }
    }

    if (const auto* member = overrides.Find("lifetime"); member != nullptr) {
        float lifetime = 0.0F;
        if (ReadFloat(ctx, *member, label, "overrides.lifetime", lifetime)) {
            if (lifetime < 0.0F) {
                Report(ctx, *member, label, "overrides.lifetime", "lifetime must not be negative");
                ok = false;
            } else {
                target.lifetime = lifetime;
            }
        } else {
            ok = false;
        }
    }

    return ok;
}

// Reads `id` first, because every other diagnostic is labelled with it.
//
// A prefab instance is a different shape on purpose: `{ id, prefab, overrides? }`.
// The body lives in the prefab file, so an instance that also carried `sprite` would
// have two sources for the same field - it is rejected as an unknown field instead.
auto ReadEntity(Ctx& ctx, const Value& value, const std::size_t index, const PrefabProvider& provider, EntityDesc& out)
    -> bool {
    const auto index_label = "entities[" + std::to_string(index) + "]";
    if (!value.IsObject()) {
        Report(ctx, value, {}, index_label, "expected an object, got " + std::string{KindName(value.kind)});
        return false;
    }

    const auto* id_member = value.Find("id");
    if (id_member == nullptr) {
        Report(ctx, value, {}, index_label + ".id", "entity requires an \"id\"");
        return false;
    }
    out.line = value.line;
    out.column = value.column;
    if (!ReadUint32(ctx, *id_member, {}, index_label + ".id", out.id))
        return false;
    if (out.id == 0) {
        Report(ctx, *id_member, {}, index_label + ".id", "entity id must be at least 1 (0 means \"no entity\")");
        return false;
    }

    const auto* prefab_member = value.Find("prefab");
    if (prefab_member == nullptr) {
        if (value.Find("overrides") != nullptr) {
            Report(ctx, value, EntityLabel(out.id, {}), "overrides",
                   "\"overrides\" requires a \"prefab\": a plain entity has nothing to override");
            return false;
        }
        CheckKeys(ctx, value, EntityLabel(out.id, {}), "", kEntityKeys);
        EntityDesc body{};
        body.id = out.id;
        body.line = out.line;
        body.column = out.column;
        if (!ReadEntityBody(ctx, value, EntityLabel(out.id, {}), body))
            return false;
        // Re-label diagnostics that came from the body with the name we just learned.
        out = std::move(body);
        return true;
    }

    CheckKeys(ctx, value, EntityLabel(out.id, {}), "", kPrefabInstanceKeys);

    std::string prefab_uri{};
    if (!ReadString(ctx, *prefab_member, EntityLabel(out.id, {}), "prefab", prefab_uri, 512))
        return false;
    if (!IsResUri(prefab_uri)) {
        Report(ctx, *prefab_member, EntityLabel(out.id, {}), "prefab",
               "prefab must be a res:// asset URI without a root or traversal, got \"" + prefab_uri + "\"");
        return false;
    }
    if (!provider) {
        Report(ctx, *prefab_member, EntityLabel(out.id, {}), "prefab",
               "this scene names a prefab, but no prefab provider was supplied to the loader");
        return false;
    }

    auto base = provider(prefab_uri);
    if (!base.has_value()) {
        Report(ctx, *prefab_member, EntityLabel(out.id, {}), "prefab",
               "prefab \"" + prefab_uri + "\" could not be loaded: " + base.error().Message());
        return false;
    }

    EntityDesc merged = std::move(*base);
    // A prefab never carries identity; the instance does.
    merged.id = out.id;
    merged.line = out.line;
    merged.column = out.column;

    const auto label = EntityLabel(merged.id, merged.name);
    if (const auto* overrides = value.Find("overrides"); overrides != nullptr) {
        if (!ApplyOverrides(ctx, *overrides, label, merged))
            return false;
    }

    out = std::move(merged);
    return true;
}
} // namespace

auto Diagnostic::Format() const -> std::string {
    std::string out = uri;
    if (line > 0) {
        out += ":";
        out += std::to_string(line);
        out += ":";
        out += std::to_string(column);
    }
    if (!entity.empty()) {
        out += ": ";
        out += entity;
    }
    if (!field.empty()) {
        out += ": ";
        out += field;
    }
    if (!message.empty()) {
        out += ": ";
        out += message;
    }
    return out;
}

auto ParseScene(const std::string_view text, const std::string_view uri, const PrefabProvider& provider) -> ParseOutcome {
    ParseOutcome outcome{};

    json::SyntaxError syntax{};
    auto document = json::TryParse(text, syntax);
    if (!document.has_value()) {
        // One syntax error is all that can be reported: nothing after the bad token
        // is trustworthy, so collecting more would be noise.
        Diagnostic diagnostic{};
        diagnostic.uri = std::string{uri};
        diagnostic.line = syntax.line;
        diagnostic.column = syntax.column;
        diagnostic.message = syntax.message;
        outcome.diagnostics.push_back(std::move(diagnostic));
        return outcome;
    }

    const Value& root = *document;
    if (!root.IsObject()) {
        Diagnostic diagnostic{};
        diagnostic.uri = std::string{uri};
        diagnostic.line = root.line;
        diagnostic.column = root.column;
        diagnostic.message = "the document root must be an object, got " + std::string{KindName(root.kind)};
        outcome.diagnostics.push_back(std::move(diagnostic));
        return outcome;
    }

    Ctx ctx{};
    ctx.uri = uri;
    ctx.diagnostics = &outcome.diagnostics;

    // schema_version first: without a known version nothing else may be interpreted,
    // so a version problem ends validation immediately with one clear message.
    const auto* version = root.Find("schema_version");
    if (version == nullptr) {
        Diagnostic diagnostic{};
        diagnostic.uri = std::string{uri};
        diagnostic.line = root.line;
        diagnostic.column = root.column;
        diagnostic.field = "schema_version";
        diagnostic.message =
            "scene requires \"schema_version\" (this build reads " + std::to_string(kSchemaVersion) + ")";
        outcome.diagnostics.push_back(std::move(diagnostic));
        return outcome;
    }
    std::int64_t version_value = 0;
    if (!ReadInteger(ctx, *version, {}, "schema_version", version_value))
        return outcome;
    if (version_value != static_cast<std::int64_t>(kSchemaVersion)) {
        Diagnostic diagnostic{};
        diagnostic.uri = std::string{uri};
        diagnostic.line = version->line;
        diagnostic.column = version->column;
        diagnostic.field = "schema_version";
        diagnostic.message = "unsupported schema_version " + std::to_string(version_value) + " (this build reads " +
                             std::to_string(kSchemaVersion) + "); refusing to guess";
        outcome.diagnostics.push_back(std::move(diagnostic));
        return outcome;
    }

    SceneDesc scene{};
    scene.schema_version = kSchemaVersion;

    CheckKeys(ctx, root, {}, "", kSceneKeys);

    if (const auto* id = root.Find("id"); id == nullptr) {
        Diagnostic diagnostic{};
        diagnostic.uri = std::string{uri};
        diagnostic.line = root.line;
        diagnostic.column = root.column;
        diagnostic.field = "id";
        diagnostic.message = "scene requires an \"id\"";
        outcome.diagnostics.push_back(std::move(diagnostic));
    } else if (ReadString(ctx, *id, {}, "id", scene.id, 64)) {
        if (scene.id.empty() || scene.id.front() < 'a' || scene.id.front() > 'z')
            Report(ctx, *id, {}, "id", "scene id must start with a lowercase letter");
        for (const char c : scene.id) {
            const bool legal = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
            if (!legal) {
                Report(ctx, *id, {}, "id",
                       std::string{"scene id may only contain a-z, 0-9 and '_' (found '"} + c + "')");
                break;
            }
        }
    }

    if (const auto* name = root.Find("name"); name != nullptr) {
        if (ReadString(ctx, *name, {}, "name", scene.name, 64))
            scene.has_name = !scene.name.empty();
    }

    const auto* entities = root.Find("entities");
    if (entities == nullptr) {
        Report(ctx, root, {}, "entities", "scene requires an \"entities\" array");
    } else if (!entities->IsArray()) {
        Report(ctx, *entities, {}, "entities", "expected an array, got " + std::string{KindName(entities->kind)});
    } else if (entities->items.size() > kMaxEntities) {
        Report(ctx, *entities, {}, "entities",
               "scene has more than " + std::to_string(kMaxEntities) + " entities");
    } else {
        scene.entities.reserve(entities->items.size());
        for (std::size_t index = 0; index < entities->items.size(); ++index) {
            ctx.per_entity = 0; // the cap is per entity, not per document
            EntityDesc entity{};
            if (ReadEntity(ctx, *entities->items[index], index, provider, entity))
                scene.entities.push_back(std::move(entity));
        }

        // Duplicate ids point at both positions: that is what the author needs to
        // decide which entity to renumber.
        for (std::size_t index = 0; index < scene.entities.size(); ++index) {
            for (std::size_t earlier = 0; earlier < index; ++earlier) {
                const auto& duplicate = scene.entities[index];
                if (scene.entities[earlier].id != duplicate.id)
                    continue;
                Report(ctx, *entities->items[index], EntityLabel(duplicate.id, duplicate.name), "id",
                       "duplicate entity id " + std::to_string(duplicate.id) + " (first seen at line " +
                           std::to_string(scene.entities[earlier].line) + ")");
                break;
            }
        }

        // Camera uniqueness (schema document §6.3).
        std::size_t camera_count = 0;
        std::size_t active_count = 0;
        std::size_t first_camera = kNoActiveCamera;
        for (std::size_t index = 0; index < scene.entities.size(); ++index) {
            if (!scene.entities[index].camera.has_value())
                continue;
            ++camera_count;
            if (first_camera == kNoActiveCamera)
                first_camera = index;
            if (scene.entities[index].camera->active) {
                ++active_count;
                scene.active_camera = index;
            }
        }
        if (active_count > 1) {
            Report(ctx, *entities, {}, "entities",
                   "only one camera may set \"active\": true, found " + std::to_string(active_count));
            scene.active_camera = kNoActiveCamera;
        } else if (active_count == 0 && camera_count > 1) {
            Report(ctx, *entities, {}, "entities",
                   "the scene has " + std::to_string(camera_count) +
                       " cameras but none sets \"active\": true; mark the one to use");
            scene.active_camera = kNoActiveCamera;
        } else if (active_count == 0 && camera_count == 1) {
            scene.active_camera = first_camera;
        }
    }

    if (outcome.diagnostics.empty())
        outcome.scene = std::move(scene);
    return outcome;
}

auto ParsePrefab(const std::string_view text, const std::string_view uri) -> PrefabOutcome {
    PrefabOutcome outcome{};

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
        Report(ctx, root, {}, {}, "the prefab root must be an object, got " + std::string{KindName(root.kind)});
        return outcome;
    }

    // Version first, for the same reason a scene checks it first: nothing else may be
    // interpreted under an unknown version.
    if (const auto* version = root.Find("schema_version"); version == nullptr) {
        Report(ctx, root, {}, "schema_version",
               "prefab requires \"schema_version\" (this build reads " + std::to_string(kSchemaVersion) + ")");
        return outcome;
    } else {
        std::int64_t version_value = 0;
        if (!ReadInteger(ctx, *version, {}, "schema_version", version_value))
            return outcome;
        if (version_value != static_cast<std::int64_t>(kSchemaVersion)) {
            Report(ctx, *version, {}, "schema_version",
                   "unsupported schema_version " + std::to_string(version_value) + " (this build reads " +
                       std::to_string(kSchemaVersion) + "); refusing to guess");
            return outcome;
        }
    }

    // `kind` catches "a scene file was passed where a prefab was expected" with a
    // message that says so, instead of a wall of unknown-field errors.
    const auto* kind = root.Find("kind");
    if (kind == nullptr) {
        Report(ctx, root, {}, "kind", "prefab requires \"kind\": \"prefab\"");
        return outcome;
    }
    std::string kind_text{};
    if (ReadString(ctx, *kind, {}, "kind", kind_text, 16)) {
        if (kind_text != "prefab") {
            Report(ctx, *kind, {}, "kind", "expected \"prefab\", got \"" + kind_text + "\"");
            return outcome;
        }
    } else {
        return outcome;
    }

    std::string prefab_id{};
    if (const auto* id = root.Find("id"); id == nullptr) {
        Report(ctx, root, {}, "id", "prefab requires an \"id\"");
        return outcome;
    } else if (ReadString(ctx, *id, {}, "id", prefab_id, 64)) {
        for (const char c : prefab_id) {
            const bool legal = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            if (!legal) {
                Report(ctx, *id, {}, "id",
                       std::string{"prefab id may only contain a-z, 0-9, '_' and '-' (found '"} + c + "')");
                return outcome;
            }
        }
    }

    CheckKeys(ctx, root, "prefab \"" + prefab_id + "\"", "", kPrefabDocumentKeys);

    const auto label = "prefab \"" + prefab_id + "\"";
    const auto* entity = root.Find("entity");
    if (entity == nullptr) {
        Report(ctx, root, label, "entity", "prefab requires an \"entity\" body");
    } else if (!entity->IsObject()) {
        Report(ctx, *entity, label, "entity",
               "expected an object, got " + std::string{KindName(entity->kind)});
    } else {
        CheckKeys(ctx, *entity, label, "entity", kPrefabEntityKeys);
        EntityDesc body{};
        if (ReadEntityBody(ctx, *entity, label, body))
            outcome.entity = std::move(body);
    }

    if (!outcome.diagnostics.empty())
        outcome.entity.reset();
    return outcome;
}

} // namespace atom::scene
