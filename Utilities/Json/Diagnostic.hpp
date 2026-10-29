// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Diagnostic.hpp
 * @brief One located problem inside a document, rendered as a single log line.
 *
 * Shared by every document format (scenes, prefabs, sprite atlases, save data later)
 * because the *shape* of the complaint is the same and the format of the line is what
 * authors learn to read:
 *
 *   uri:line:column: entity 7 ("crate"): sprite.pivot: expected 2 numbers, got 1
 *
 * Header-only and dependency-free so that a leaf module like Sprite can emit
 * diagnostics without depending on the module whose vocabulary ("entity") happens to
 * appear in one of the fields: a sprite atlas fills `field` and leaves `entity` empty.
 */

#ifndef ATOM_UTILITIES_JSON_DIAGNOSTIC_HPP
#define ATOM_UTILITIES_JSON_DIAGNOSTIC_HPP

#include <cstdint>
#include <string>

namespace atom::json {

struct Diagnostic {
        std::string uri{};
        std::uint32_t line = 0;
        std::uint32_t column = 0;
        // Optional scope, pre-formatted by the producer (`entity 3 ("crate")`,
        // `prefab "crate"`). Empty when the problem is not scoped to one subject.
        std::string entity{};
        // Dot path from the subject root, e.g. "sprite.pivot". Empty when not
        // field-scoped.
        std::string field{};
        std::string message{};

        [[nodiscard]] auto Format() const -> std::string {
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
};

} // namespace atom::json

#endif // ATOM_UTILITIES_JSON_DIAGNOSTIC_HPP
