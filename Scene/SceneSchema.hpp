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

#include <optional>
#include <string_view>
#include <vector>

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

// Parses and validates `text`. `uri` is copied into every diagnostic and is used
// verbatim in messages (it is the caller's res:// path).
[[nodiscard]] auto ParseScene(std::string_view text, std::string_view uri) -> ParseOutcome;

// Doc size limits the validator enforces as well, so a caller that parses by other
// means cannot bypass them.
inline constexpr std::size_t kMaxDiagnostics = 32;
inline constexpr std::size_t kMaxDiagnosticsPerEntity = 8;
inline constexpr std::size_t kMaxEntities = 65536;

} // namespace atom::scene

#endif // ATOM_SCENE_SCENESCHEMA_HPP
