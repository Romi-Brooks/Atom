/**
 * @file           : ImageBatchDecoder.hpp
 * @brief          : Engine-level batch image decoding over the job system.
 *
 * Mirrors AudioMetadataReader::ReadBatch: one decode job per file, results
 * returned in input order, and a sequential fallback when the job system is
 * not running. This is the value-semantics path (CPU-side DecodedImage) for
 * callers that want a pile of decoded images without going through the
 * ResourceManager cache; the cached path remains DecodedImageLoader.
**/

#ifndef ATOM_MEDIA_IMAGE_BATCH_DECODER_HPP
#define ATOM_MEDIA_IMAGE_BATCH_DECODER_HPP

#include <optional>
#include <string>
#include <vector>

#include <Filesystem/FileSystem.hpp>
#include <Media/Image/ImageDecoder.hpp>

namespace atom::image {

class ImageBatchDecoder {
    public:
        // Decodes a single image file through the VFS into a CPU-side
        // DecodedImage. Returns nullopt when the file cannot be opened, read,
        // or decoded (the reason is logged via the image::Decoder channel).
        static auto Decode(const fs::IFileSystem& filesystem, const fs::AssetPath& path)
            -> std::optional<DecodedImage>;

        // Convenience overload: parses `path` as an AssetPath and resolves it
        // through the process-wide default Vfs, mirroring Decode above.
        static auto Decode(const std::string& path) -> std::optional<DecodedImage>;

        // Decodes many images in parallel through the engine job system: one
        // job per path, results returned in input order (nullopt when a file
        // cannot be decoded). Blocks until all decodes finish (the calling
        // thread helps run pending jobs rather than idling). Falls back to
        // sequential decoding when the job system is not running.
        static auto DecodeBatch(const fs::IFileSystem& filesystem,
                                const std::vector<fs::AssetPath>& paths)
            -> std::vector<std::optional<DecodedImage>>;

        // Convenience overload: parses each string and resolves through the
        // default Vfs, mirroring DecodeBatch above.
        static auto DecodeBatch(const std::vector<std::string>& paths)
            -> std::vector<std::optional<DecodedImage>>;
};

} // namespace atom::image

#endif // ATOM_MEDIA_IMAGE_BATCH_DECODER_HPP
