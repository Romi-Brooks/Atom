/**
 * @file           : FontFileLoader.hpp
 * @brief          : Loads a font file's bytes as a shareable FontData resource.
 *
 * The FreeType face is deliberately NOT created here: FT_Face construction is
 * render-thread (or, later, per-worker — see the text roadmap W2) work, not a
 * worker-shareable resource. Carrying only the bytes lets the 20+ MB read happen
 * off-thread through ResourceManager::AcquireAsync while the face stays owned by
 * the render thread.
**/

#ifndef ATOM_RENDER_TEXT_FONT_FILE_LOADER_HPP
#define ATOM_RENDER_TEXT_FONT_FILE_LOADER_HPP

#include <cstddef>
#include <memory>
#include <vector>

#include <Asset/IResourceLoader.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom::render {

// Raw font file bytes. Handed to Font::CreateFromMemory (or a future
// per-worker FreeType provider) on the render thread.
struct FontData final {
        std::vector<std::byte> bytes{};
};

// Reads a font file into FontData through the VFS (AssetKind::Font). The
// "sister loader" to DecodedImageLoader: it lets font bytes enter the same
// URI/identity + async-acquire machinery as textures, scripts and audio.
class FontFileLoader final : public asset::TypedResourceLoader<FontData> {
    public:
        [[nodiscard]] auto Kind() const -> asset::AssetKind override {
            return asset::AssetKind::Font;
        }

    protected:
        [[nodiscard]] auto LoadTyped(const fs::IFileSystem& filesystem, const asset::ResourceId& id,
                                     std::shared_ptr<FontData>& output) -> Result<void> override;
};

} // namespace atom::render

#endif // ATOM_RENDER_TEXT_FONT_FILE_LOADER_HPP
