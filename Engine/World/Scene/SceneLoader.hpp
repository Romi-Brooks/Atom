// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file SceneLoader.hpp
 * @brief Reads a scene document from the VFS and builds an EntityWorld from it (T2.2).
 *
 * The loader is deliberately split in two so a caller can validate without paying
 * for a world, and build without re-reading:
 *
 *   Load()       VFS read -> ParseScene() -> BuildWorld()
 *   BuildWorld() SceneDesc -> a *new* EntityWorld
 *
 * Atomicity is the caller's: Load() never touches an existing world, it returns a
 * fresh one. The switch is `GameWorld::ReplaceEntities`, which is a single pointer
 * swap - a failed load leaves the running world untouched, and a successful one
 * cannot leave it half-built.
 *
 * Why the active camera is created first: Render/Scene2D picks the
 * lowest-entity-index camera, so creating the active one before the others is what
 * makes "exactly one active camera" true without the renderer needing to know
 * about an "active" flag it does not have.
 */

#ifndef ATOM_SCENE_SCENELOADER_HPP
#define ATOM_SCENE_SCENELOADER_HPP

#include <memory>

#include <ECS/EntityWorld.hpp>
#include <Error/Error.hpp>
#include <Filesystem/AssetPath.hpp>
#include <Scene/SceneTypes.hpp>

namespace atom::fs {
class IFileSystem;
}

namespace atom::scene {

struct LoadedScene {
        SceneDesc description{};
        // Never null on success. Owned by the caller; hand it to
        // GameWorld::ReplaceEntities to make it the running world.
        std::unique_ptr<ecs::EntityWorld> entities{};
};

class SceneLoader final {
    public:
        // Reads `path` through `filesystem` and builds the world. On any failure the
        // Error is `Scene*`-coded and its detail starts with "uri:line:column:" so it
        // can be logged or jumped to verbatim; every diagnostic is also logged at
        // warning level, so a 5-error file does not hide four of them.
        [[nodiscard]] static auto Load(const fs::IFileSystem& filesystem, const fs::AssetPath& path)
            -> Result<LoadedScene>;

        // Builds a world from an already-validated description. Fails only on an ECS
        // level problem (dead entity, duplicate component), which would mean the
        // description was not validated.
        [[nodiscard]] static auto BuildWorld(const SceneDesc& description) -> Result<std::unique_ptr<ecs::EntityWorld>>;
};

} // namespace atom::scene

#endif // ATOM_SCENE_SCENELOADER_HPP
