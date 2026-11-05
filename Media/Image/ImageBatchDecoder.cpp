/**
 * @file           : ImageBatchDecoder.cpp
 * @brief          : Job-system-backed batch image decoding implementation.
**/

#include "ImageBatchDecoder.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <Concurrency/JobSystem.hpp>
#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>

namespace atom::image {
namespace {

// Opens, reads, and decodes a single image file. Kept as a free function so
// both the single-file entry point and the per-job lambda share the exact same
// path. Returns nullopt on open/read/decode failure; the reason is logged once.
auto DecodeOne(const fs::IFileSystem& filesystem, const fs::AssetPath& path)
    -> std::optional<DecodedImage> {
    std::unique_ptr<fs::IFile> file{};
    if (filesystem.OpenRead(path, file) != fs::Result::Success || !file) {
        LOG_WARNING(atom::log::image::Decoder,
                    "Failed to open image through VFS: " + std::string{path.String()});
        return std::nullopt;
    }

    std::vector<std::byte> bytes{};
    if (fs::ReadAll(*file, bytes) != fs::Result::Success) {
        LOG_WARNING(atom::log::image::Decoder,
                    "Failed to read image through VFS: " + std::string{path.String()});
        return std::nullopt;
    }

    DecodedImage image = DecodeImageMemory(bytes);
    if (!image.IsValid())
        return std::nullopt; // DecodeImageMemory already logged the specific reason.
    return image;
}

} // namespace

auto ImageBatchDecoder::Decode(const fs::IFileSystem& filesystem, const fs::AssetPath& path)
    -> std::optional<DecodedImage> {
    return DecodeOne(filesystem, path);
}

auto ImageBatchDecoder::Decode(const std::string& path) -> std::optional<DecodedImage> {
    auto asset_path = fs::AssetPath::Parse(path);
    if (!asset_path) {
        LOG_WARNING(atom::log::image::Decoder,
                    "Invalid asset path '" + path + "': " + asset_path.error().Message());
        return std::nullopt;
    }
    return Decode(fs::Vfs::GetInstance(), *asset_path);
}

auto ImageBatchDecoder::DecodeBatch(const fs::IFileSystem& filesystem,
                                    const std::vector<fs::AssetPath>& paths)
    -> std::vector<std::optional<DecodedImage>> {
    std::vector<std::optional<DecodedImage>> results(paths.size());

    auto& jobs = atom::job::JobSystem::GetInstance();
    if (!jobs.IsRunning()) {
        // 调度器未启动：退化为顺序解码，保证调用方永远正确。
        for (std::size_t i = 0; i < paths.size(); ++i)
            results[i] = DecodeOne(filesystem, paths[i]);
        return results;
    }

    // 每个文件一个 job：DecodeOne 各自 open 独立 IFile，互不共享可变状态，
    // 可安全并发。写入 results[i] 的索引互不相同，无数据竞争。
    std::vector<atom::job::JobHandle> handles;
    handles.reserve(paths.size());
    for (std::size_t i = 0; i < paths.size(); ++i) {
        const fs::AssetPath path = paths[i];
        handles.push_back(jobs.Schedule("image:decode", [&filesystem, &results, i, path] {
            results[i] = DecodeOne(filesystem, path);
        }));
    }
    jobs.WaitAll(handles);
    return results;
}

auto ImageBatchDecoder::DecodeBatch(const std::vector<std::string>& paths)
    -> std::vector<std::optional<DecodedImage>> {
    // 先顺序解析字符串（解析很廉价，无需并行）；无效路径置默认空 AssetPath，
    // 下游 DecodeOne 会因 InvalidPath 返回 nullopt，结果顺序仍与输入一致。
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
