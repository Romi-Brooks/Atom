// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file SceneSchema.hpp
 * @brief Turns a parsed JSONC document into a validated SceneDesc (T2.1 rules).
 *
 * Two stages, two error styles, on purpose:
 *   - syntax     -> SceneInvalidDocument, one error, "uri:line:column: message"
 *                   (the file cannot be understood, so nothing else is knowable);
 *   - validation -> every problem is collected (up to 32) and reported with the
 *                   entity and field path, so one edit session fixes all of them
 *                   instead of one per run.
 *
 * Validation is strict by design: unknown fields are errors, not warnings. A typo
 * that silently does nothing is the worst failure mode for hand-written content.
 */

#ifndef ATOM_SCENE_SCENESCHEMA_HPP
#define ATOM_SCENE_SCENESCHEMA_HPP

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include <Error/Error.hpp>
#include <Scene/SceneTypes.hpp>

namespace atom::scene {

struct ParseOutcome {
        // Present only when there were zero diagnostics.
        std::optional<SceneDesc> scene{};
        std::vector<Diagnostic> diagnostics{};

        [[nodiscard]] auto Ok() const noexcept -> bool {
            return scene.has_value();
        }
};

struct PrefabOutcome {
        // Present only when there were zero diagnostics.
        std::optional<EntityDesc> entity{};
        std::vector<Diagnostic> diagnostics{};

        [[nodiscard]] auto Ok() const noexcept -> bool {
            return entity.has_value();
        }
};

// Supplies a prefab's base entity when a scene instance names one. The loader backs
// it with the VFS and a per-load cache; a test backs it with a map. Returning an
// Error is how "the prefab itself is broken" reaches the scene's diagnostics, with
// the failed document's own message.
using PrefabProvider = std::function<Result<EntityDesc>(std::string_view uri)>;

// Parses and validates `text`. `uri` is copied into every diagnostic and is used
// verbatim in messages (it is the caller's res:// path).
//
// When an entity names a prefab, `provider` is called for it; without a provider a
// prefab reference is a diagnostic rather than a silent empty entity (T2.3).
[[nodiscard]] auto ParseScene(std::string_view text, std::string_view uri, const PrefabProvider& provider = {})
    -> ParseOutcome;

// Parses a prefab document: `{ schema_version, kind: "prefab", id, entity }`. The
// entity body is the same shape a scene entity uses, minus `id` - a prefab carries no
// identity because every instance supplies its own.
[[nodiscard]] auto ParsePrefab(std::string_view text, std::string_view uri) -> PrefabOutcome;

// Doc size limits the validator enforces as well, so a caller that parses by other
// means cannot bypass them.
inline constexpr std::size_t kMaxDiagnostics = 32;
inline constexpr std::size_t kMaxDiagnosticsPerEntity = 8;
inline constexpr std::size_t kMaxEntities = 65536;

} // namespace atom::scene

#endif // ATOM_SCENE_SCENESCHEMA_HPP
