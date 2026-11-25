/**
 * @file           : ImageBatchDecoder.hpp
 * @brief          : Engine-level batch image decoding over the job system.
 *
 * Mirrors AudioMetadataReader::ReadBatch: one decode job per file, results
 * returned in input order, and a sequential fallback when the job system is
 * not running. This is the value-semantics path (CPU-side DecodedImage) for
 * callers that want a pile of decoded images without going through the
 * ResourceManager cache; the cached path remains DecodedImageLoader.
 *
 * Every entry point returns atom::Result, so a caller can tell "the file is
 * missing" (FsNotFound) from "the bytes are not a decodable image"
 * (AssetImageDecodeFailed). The older optional-only shape collapsed both into
 * "nothing", which is exactly what CORE-004 removed elsewhere.
**/

#ifndef ATOM_MEDIA_IMAGE_BATCH_DECODER_HPP
#define ATOM_MEDIA_IMAGE_BATCH_DECODER_HPP

#include <string>
#include <vector>

#include <Atom/Core/Error/Error.hpp>
#include <Atom/Resources/Filesystem/FileSystem.hpp>
#include <Atom/Resources/Media/Image/ImageDecoder.hpp>

#include <Atom/Build/MediaImageExport.hpp>

namespace atom::image {

class ATOM_MEDIA_IMAGE_API ImageBatchDecoder {
    public:
        // Decodes a single image file through the VFS. A failure carries the
        // reason: the VFS error (FsNotFound, FsIoError, ...) or
        // AssetImageDecodeFailed when the decoder rejects the bytes.
        static auto Decode(const fs::IFileSystem& filesystem, const fs::AssetPath& path) -> Result<DecodedImage>;

        // Convenience overload: parses `path` as an AssetPath and resolves it
        // through the process-wide default Vfs. An unparseable URI returns the
        // AssetPath parse error instead of a generic failure.
        static auto Decode(const std::string& path) -> Result<DecodedImage>;

        // Decodes many images in parallel through the engine job system: one
        // job per path, results returned in input order (a failed slot keeps its
        // own reason). Blocks until all decodes finish (the calling thread helps
        // run pending jobs rather than idling). Falls back to sequential decoding
        // when the job system is not running.
        static auto DecodeBatch(const fs::IFileSystem& filesystem, const std::vector<fs::AssetPath>& paths)
            -> std::vector<Result<DecodedImage>>;

        // Convenience overload: parses each string and resolves through the
        // default Vfs, mirroring Decode above.
        static auto DecodeBatch(const std::vector<std::string>& paths) -> std::vector<Result<DecodedImage>>;
};

} // namespace atom::image

#endif // ATOM_MEDIA_IMAGE_BATCH_DECODER_HPP
