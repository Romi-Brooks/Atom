/**
 * @file           : ImageBatchDecoder.cpp
 * @brief          : Job-system-backed batch image decoding implementation.
**/

#include "ImageBatchDecoder.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <Atom/Core/Concurrency/JobSystem.hpp>
#include <Atom/Resources/Filesystem/Vfs.hpp>
#include <Atom/Core/Log/LogSystem.hpp>

namespace atom::image {
namespace {

auto DecodeFailure(Error error) -> std::unexpected<Error> {
    return std::unexpected<Error>{std::move(error)};
}

// Opens, reads, and decodes a single image file. Kept as a free function so
// both the single-file entry point and the per-job lambda share the exact same
// path. Each failure returns the reason and is logged once here; the caller only
// decides whether to surface it.
auto DecodeOne(const fs::IFileSystem& filesystem, const fs::AssetPath& path) -> Result<DecodedImage> {
    std::unique_ptr<fs::IFile> file{};
    const auto opened = filesystem.OpenRead(path, file);
    if (opened != fs::Result::Success || !file) {
        LOG_WARNING(atom::log::image::Decoder,
                    "Failed to open image through VFS: " + std::string{path.String()});
        auto error = fs::ToError(opened, "ImageBatchDecoder::Decode", path.String());
        return DecodeFailure(std::move(error));
    }

    std::vector<std::byte> bytes{};
    const auto read = fs::ReadAll(*file, bytes);
    if (read != fs::Result::Success) {
        LOG_WARNING(atom::log::image::Decoder,
                    "Failed to read image through VFS: " + std::string{path.String()});
        auto error = fs::ToError(read, "ImageBatchDecoder::ReadAll", path.String());
        return DecodeFailure(std::move(error));
    }

    DecodedImage image = DecodeImageMemory(bytes);
    if (!image.IsValid()) {
        // DecodeImageMemory already logged the specific stb reason; the Error
        // carries the category so callers can act without parsing logs.
        auto error = MakeError(ErrorCode::AssetImageDecodeFailed, Module::Asset, "ImageBatchDecoder::Decode")
                         .WithResource(path.String())
                         .WithDetail("the image decoder rejected the encoded bytes");
        return DecodeFailure(std::move(error));
    }
    return image;
}

// A slot that a job never filled keeps this reason instead of pretending the
// image was missing.
auto PendingSlotError(const fs::AssetPath& path) -> Error {
    return MakeError(ErrorCode::AssetLoadFailed, Module::Asset, "ImageBatchDecoder::DecodeBatch")
        .WithResource(path.String())
        .WithDetail("the decode job did not run");
}

} // namespace

auto ImageBatchDecoder::Decode(const fs::IFileSystem& filesystem, const fs::AssetPath& path) -> Result<DecodedImage> {
    return DecodeOne(filesystem, path);
}

auto ImageBatchDecoder::Decode(const std::string& path) -> Result<DecodedImage> {
    auto asset_path = fs::AssetPath::Parse(path);
    if (!asset_path) {
        // The caller only passed a string, so the parse rule is the whole
        // diagnostic - forward it rather than collapsing to "nothing".
        auto error = asset_path.error();
        return DecodeFailure(std::move(error));
    }
    return Decode(fs::Vfs::GetInstance(), *asset_path);
}

auto ImageBatchDecoder::DecodeBatch(const fs::IFileSystem& filesystem, const std::vector<fs::AssetPath>& paths)
    -> std::vector<Result<DecodedImage>> {
    // Pre-size the result vector so each job can write its own index. Distinct
    // indices in an already-sized vector are safe to write concurrently.
    std::vector<Result<DecodedImage>> results;
    results.reserve(paths.size());
    for (const auto& path : paths)
        results.push_back(DecodeFailure(PendingSlotError(path)));

    auto& jobs = atom::job::JobSystem::GetInstance();
    // 每个文件一个 job：DecodeOne 各自 open 独立 IFile，互不共享可变状态，
    // 可安全并发。写入 results[i] 的索引互不相同，无数据竞争。
    std::vector<atom::job::JobHandle> handles;
    handles.reserve(paths.size());
    for (std::size_t i = 0; i < paths.size(); ++i) {
        const fs::AssetPath path = paths[i];
        handles.push_back(jobs.ScheduleOrRun("image:decode", [&filesystem, &results, i, path] {
            results[i] = DecodeOne(filesystem, path);
        }));
    }
    jobs.WaitAll(handles);
    return results;
}

auto ImageBatchDecoder::DecodeBatch(const std::vector<std::string>& paths) -> std::vector<Result<DecodedImage>> {
    // 先顺序解析字符串（解析很廉价，无需并行）；无效路径置默认空 AssetPath，
    // 下游 DecodeOne 会因 InvalidPath 失败，结果顺序仍与输入一致。
    std::vector<fs::AssetPath> asset_paths;
    asset_paths.reserve(paths.size());
    for (const std::string& path : paths) {
        auto parsed = fs::AssetPath::Parse(path);
        if (!parsed) {
            LOG_WARNING(atom::log::image::Decoder,
                        "Invalid asset path '" + path + "': " + parsed.error().Message());
            asset_paths.emplace_back();
        } else {
            asset_paths.push_back(*parsed);
        }
    }
    return DecodeBatch(fs::Vfs::GetInstance(), asset_paths);
}

} // namespace atom::image
