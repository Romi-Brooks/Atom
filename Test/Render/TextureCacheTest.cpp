/**
 * @file           : TextureCacheTests.cpp
 * @brief          : TextureCache construction, empty-cache bookkeeping, and the
 *                   handle-lifetime contract that keeps cached handles inert
 *                   once the renderer's texture pool is gone.
**/

#include <Backend/Contracts/Render/IRender2DContext.hpp>
#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Render/Resources/TextureCache.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace {

// Minimal valid 1x1 RGB PNG (same fixture as ImageDecoderTest / ConsumerLoaderTest).
constexpr std::array<uint8_t, 78> tiny_png{
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8, 0xFF, 0xFF, 0x3F, 0x00, 0x05, 0xFE, 0x02, 0xFE, 0xA7,
    0x35, 0x81, 0x84, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0x8D, 0x42, 0x60, 0x82,
};

[[nodiscard]] auto TinyPng() -> std::span<const std::byte> {
    return {reinterpret_cast<const std::byte*>(tiny_png.data()), tiny_png.size()};
}

// Renderer2D only needs the 2D context slice, so a stub is enough to exercise
// texture creation and release without a GPU. It counts the textures the
// renderer hands back, which is what the lifetime assertions below observe.
class StubRenderDevice final : public atom::render::IRenderDevice, public atom::render::IRender2DContext {
    public:
        // --- IRenderDevice ---
        [[nodiscard]] auto BeginFrame() -> bool override {
            return true;
        }
        auto Clear(const atom::render::Color&) -> void override {}
        auto EndFrame() -> void override {}
        auto HandleResize(uint32_t, uint32_t) -> void override {}
        auto SetVSync(bool) -> bool override {
            return true;
        }
        [[nodiscard]] auto IsVSyncEnabled() const -> bool override {
            return false;
        }
        [[nodiscard]] auto GetOutputSize() const -> atom::algo::Vec2 override {
            return {1280.0f, 720.0f};
        }
        [[nodiscard]] auto GetBackendInfo() const -> const atom::render::RenderBackendInfo& override {
            return info_;
        }

        // --- IRender2DContext ---
        auto Initialize2D(const std::filesystem::path&) -> bool override {
            return true;
        }
        auto CreateTexture2D(uint32_t, uint32_t) -> atom::render::Texture2D override {
            return next_texture_++;
        }
        auto UpdateTexture2D(atom::render::Texture2D, const void*, uint32_t) -> bool override {
            return true;
        }
        auto UpdateTexture2DRegion(atom::render::Texture2D, uint32_t, uint32_t, uint32_t, uint32_t, const void*,
                                   uint32_t) -> bool override {
            return true;
        }
        auto DestroyTexture2D(atom::render::Texture2D) -> void override {
            ++destroyed_textures_;
        }
        auto CreateSampler2D(const atom::render::Sampler2DDesc&) -> atom::render::Sampler2D override {
            return next_sampler_++;
        }
        auto DestroySampler2D(atom::render::Sampler2D) -> void override {}
        auto SetPostProcess2D(const atom::render::PostProcess2DParams&) -> void override {}
        auto ResolvePostProcess2D() -> bool override {
            return true;
        }
        auto Submit2DFrame(const atom::render::Render2DFrame&) -> bool override {
            return true;
        }
        [[nodiscard]] auto GetMax2DVertices() const -> uint32_t override {
            return 65536;
        }
        [[nodiscard]] auto GetMax2DIndices() const -> uint32_t override {
            return 65536;
        }

        [[nodiscard]] auto DestroyedTextures() const -> uint32_t {
            return destroyed_textures_;
        }

    private:
        atom::render::RenderBackendInfo info_{};
        atom::render::Texture2D next_texture_ = 1;
        atom::render::Sampler2D next_sampler_ = 1;
        uint32_t destroyed_textures_ = 0;
};

} // namespace

auto main() -> int {
    atom::render::Renderer2D renderer{};
    atom::render::resources::TextureCache cache{renderer};

    if (cache.LiveCount() != 0 || cache.CacheCapacity() != 0)
        return atom::test::Fail("Fresh TextureCache is not empty");
    if (cache.AcquireFromEncodedMemory("key", {}))
        return atom::test::Fail("Empty encoded buffer was accepted");
    if (cache.Evict("missing"))
        return atom::test::Fail("Evict of a missing key succeeded");

    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return atom::test::Fail("Could not create memory filesystem");
    atom::fs::AssetPath path{};
    if (!atom::fs::AssetPath::TryParse("res://textures/missing.png", path))
        return atom::test::Fail("Could not parse path");
    // No device/context: upload must fail closed without caching a live entry.
    if (cache.AcquireFromFilesystem(*memory, path))
        return atom::test::Fail("Acquire succeeded without a render device");

    cache.EvictAll();
    cache.FlushDeferredDestroys();

    // --- Handle lifetime across Renderer2D::Shutdown() -----------------------
    //
    // Regression: Renderer2D::Shutdown() destroys the textures it owns, but a
    // TextureHandle can still be alive at that point (a screen, a resource pack
    // or a cache is not required to drop its handles first). Releasing such a
    // handle used to dereference the freed texture through
    // GpuTextureRecord::~GpuTextureRecord -> EnqueueDeferredTextureDestroy,
    // which crashed the MusicCard example on exit.
    StubRenderDevice device{};
    auto live_renderer = std::make_unique<atom::render::Renderer2D>();
    if (!live_renderer->Initialize(device, "unused-shader-root"))
        return atom::test::Fail("Renderer2D failed to initialize on the stub device");
    auto live_cache = std::make_unique<atom::render::resources::TextureCache>(*live_renderer);

    auto handle = live_cache->AcquireFromEncodedMemory("cover", TinyPng());
    if (!handle.IsValid() || handle.Get() == nullptr)
        return atom::test::Fail("Cached texture handle is not valid after Acquire");

    // Shutdown with the handle still held: the pool is gone, so the handle must
    // report itself invalid rather than expose a freed texture.
    live_renderer->Shutdown();
    ATOM_CHECK(!handle.IsValid());
    ATOM_CHECK(handle.Get() == nullptr);
    // The white fallback texture and the cached cover were both handed back.
    ATOM_CHECK(device.DestroyedTextures() == 2);

    // Dropping the stale handle and its cache must be a safe no-op.
    handle = {};
    live_cache.reset();

    // The renderer may also be destroyed before the last handle is released.
    auto owned_renderer = std::make_unique<atom::render::Renderer2D>();
    ATOM_CHECK(owned_renderer->Initialize(device, "unused-shader-root"));
    auto owned_cache = std::make_unique<atom::render::resources::TextureCache>(*owned_renderer);
    auto owned_handle = owned_cache->AcquireFromEncodedMemory("cover", TinyPng());
    ATOM_CHECK(owned_handle.IsValid());
    owned_renderer.reset(); // ~Renderer2D -> Shutdown()
    ATOM_CHECK(!owned_handle.IsValid());
    ATOM_CHECK(owned_handle.Get() == nullptr);
    owned_handle = {};
    owned_cache.reset();

    // A re-initialized renderer hands out a fresh pool: new handles are valid,
    // and the ones from the previous generation stay inert.
    ATOM_CHECK(live_renderer->Initialize(device, "unused-shader-root"));
    auto revived_cache = std::make_unique<atom::render::resources::TextureCache>(*live_renderer);
    auto revived = revived_cache->AcquireFromEncodedMemory("cover", TinyPng());
    ATOM_CHECK(revived.IsValid());
    ATOM_CHECK(revived.Get() != nullptr);
    revived_cache->EvictAll();
    revived = {};
    revived_cache->FlushDeferredDestroys();
    live_renderer->Shutdown();
    live_renderer.reset();
    return 0;
}
