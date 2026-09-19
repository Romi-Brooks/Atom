/**
 * @file           : TextureCacheTests.cpp
 * @brief          : TextureCache construction and empty-cache bookkeeping.
**/

#include <Filesystem/MemoryFileSystem.hpp>
#include <Render/Resources/TextureCache.hpp>

#include <iostream>
#include <memory>
#include <vector>

namespace {

auto Fail(const std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

auto main() -> int {
    atom::render::Renderer2D renderer{};
    atom::render::resources::TextureCache cache{renderer};

    if (cache.LiveCount() != 0 || cache.CacheCapacity() != 0)
        return Fail("Fresh TextureCache is not empty");
    if (cache.AcquireFromEncodedMemory("key", {}))
        return Fail("Empty encoded buffer was accepted");
    if (cache.Evict("missing"))
        return Fail("Evict of a missing key succeeded");

    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return Fail("Could not create memory filesystem");
    atom::fs::AssetPath path{};
    if (!atom::fs::AssetPath::TryParse("res://textures/missing.png", path))
        return Fail("Could not parse path");
    // No device/context: upload must fail closed without caching a live entry.
    if (cache.AcquireFromFilesystem(*memory, path))
        return Fail("Acquire succeeded without a render device");

    cache.EvictAll();
    cache.FlushDeferredDestroys();
    return 0;
}
