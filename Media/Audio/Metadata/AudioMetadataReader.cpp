/**
  * @file           : AudioMetadataReader.cpp
  * @author         : Romi Brooks
  * @brief          : TagLib-backed implementation of AudioMetadataReader.
  * @attention      : TagLib headers are confined to this translation unit.
  * @date           : 2026/8/20
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "AudioMetadataReader.hpp"

#include <cstddef>
#include <cstring>
#include <span>

#include <fileref.h>
#include <tag.h>
#include <tbytevector.h>
#include <tiostream.h>
#include <tvariant.h>

#include <Concurrency/JobSystem.hpp>
#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>

namespace atom::audio {
namespace {

// Adapts an atom::fs::IFile into the read-only TagLib::IOStream contract. TagLib
// never sees a native path: every read/seek is forwarded to IFile::ReadAt, whose
// cursor is independent of IFile's own. Only the read side is implemented — the
// write/insert/remove/truncate operations are no-ops, because metadata reading is
// strictly read-only.
class VfsIoStream final : public TagLib::IOStream {
    public:
        explicit VfsIoStream(atom::fs::IFile& file, std::string name)
            : file_(file), size_(file.Size()), name_(std::move(name)) {}

        auto name() const -> TagLib::FileName override {
#ifdef _WIN32
            // FileName(const wchar_t*) is the wide path constructor; we have no
            // real path, so hand back an empty wide string. TagLib only uses name()
            // for diagnostics and format auto-detection is done from the stream
            // bytes, not the name.
            static const std::wstring empty_file_name{};
            return TagLib::FileName{empty_file_name.c_str()};
#else
            return name_.c_str();
#endif
        }

        auto readBlock(const size_t length) -> TagLib::ByteVector override {
            if (length == 0)
                return {};
            const auto remaining = pos_ < size_ ? size_ - pos_ : 0;
            const auto to_read = length < remaining ? length : static_cast<size_t>(remaining);
            if (to_read == 0)
                return {};
            TagLib::ByteVector result(static_cast<unsigned int>(to_read));
            const std::span<std::byte> destination{reinterpret_cast<std::byte*>(result.data()), to_read};
            if (file_.ReadAt(pos_, destination) != atom::fs::Result::Success)
                return {};
            pos_ += to_read;
            return result;
        }

        auto writeBlock(const TagLib::ByteVector&) -> void override {}
        auto insert(const TagLib::ByteVector&, TagLib::offset_t, size_t) -> void override {}
        auto removeBlock(TagLib::offset_t, size_t) -> void override {}
        auto readOnly() const -> bool override { return true; }
        auto isOpen() const -> bool override { return true; }

        auto seek(const TagLib::offset_t offset, const Position position) -> void override {
            switch (position) {
            case Position::Beginning:
                pos_ = static_cast<uint64_t>(offset);
                break;
            case Position::Current:
                pos_ = static_cast<uint64_t>(static_cast<int64_t>(pos_) + offset);
                break;
            case Position::End:
                pos_ = static_cast<uint64_t>(static_cast<int64_t>(size_) + offset);
                break;
            }
        }

        auto clear() -> void override {}

        auto tell() const -> TagLib::offset_t override { return static_cast<TagLib::offset_t>(pos_); }

        auto length() -> TagLib::offset_t override { return static_cast<TagLib::offset_t>(size_); }

        auto truncate(TagLib::offset_t) -> void override {}

    private:
        atom::fs::IFile& file_;
        uint64_t size_ = 0;
        uint64_t pos_ = 0;
        std::string name_{};
};

// Reads the whole IFile into memory and hands TagLib a ByteVectorStream. Kept as
// a fallback path for any code path that needs seek-heavy random access; the
// streaming VfsIoStream above is preferred for bounded memory.
auto ReadMetadataFromStream(TagLib::IOStream& stream) -> std::optional<AudioMetadata> {
    TagLib::FileRef file(&stream);
    if (file.isNull() || !file.tag()) {
        LOG_WARNING(atom::log::audio::Metadata, "No metadata found in stream");
        return std::nullopt;
    }

    AudioMetadata meta;
    const auto* tag = file.tag();
    meta.title = tag->title().to8Bit(true);
    meta.artist = tag->artist().to8Bit(true);
    meta.album = tag->album().to8Bit(true);
    meta.comment = tag->comment().to8Bit(true);
    meta.genre = tag->genre().to8Bit(true);
    meta.year = tag->year();
    meta.track = tag->track();

    const auto pictures = file.complexProperties("PICTURE");
    if (!pictures.isEmpty()) {
        const auto& picture = pictures.front();
        const auto bytes = picture.value("data").toByteVector();
        if (!bytes.isEmpty()) {
            meta.artworkMimeType = picture.value("mimeType").toString().to8Bit(true);
            const auto* begin = reinterpret_cast<const uint8_t*>(bytes.data());
            meta.artworkData.assign(begin, begin + bytes.size());
            LOG_DEBUG(atom::log::audio::Metadata, "Extracted embedded artwork: " + meta.artworkMimeType +
                                                       ", " + std::to_string(meta.artworkData.size()) + " bytes");
        } else {
            LOG_WARNING(atom::log::audio::Metadata, "Embedded artwork entry has no image data");
        }
    } else {
        LOG_DEBUG(atom::log::audio::Metadata, "No embedded artwork found");
    }

    if (file.audioProperties()) {
        const auto* props = file.audioProperties();
        meta.durationSeconds = static_cast<uint32_t>(props->lengthInSeconds());
        meta.bitrateKbps = static_cast<uint32_t>(props->bitrate());
        meta.sampleRate = static_cast<uint32_t>(props->sampleRate());
        meta.channels = static_cast<uint16_t>(props->channels());
    }

    LOG_INFO(atom::log::audio::Metadata, "Read metadata (title='" + meta.title + "', artist='" + meta.artist +
                                             "', duration=" + std::to_string(meta.durationSeconds) + "s)");
    return meta;
}

} // namespace

auto AudioMetadataReader::Read(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path)
    -> std::optional<AudioMetadata> {
    try {
        std::unique_ptr<atom::fs::IFile> file{};
        if (filesystem.OpenRead(path, file) != atom::fs::Result::Success || !file) {
            LOG_WARNING(atom::log::audio::Metadata,
                        "Failed to open audio file through VFS: " + std::string{path.String()});
            return std::nullopt;
        }

        VfsIoStream stream{*file, std::string{path.String()}};
        return ReadMetadataFromStream(stream);
    } catch (...) {
        LOG_WARNING(atom::log::audio::Metadata,
                    "Failed to read metadata: " + std::string{path.String()});
        return std::nullopt;
    }
}

auto AudioMetadataReader::Read(const std::string& path) -> std::optional<AudioMetadata> {
    auto asset_path = atom::fs::AssetPath::Parse(path);
    if (!asset_path) {
        // The reason is what makes this message useful: the caller only passed a
        // string, so "which rule rejected it" is the whole diagnostic.
        LOG_WARNING(atom::log::audio::Metadata,
                    "Invalid asset path '" + path + "': " + asset_path.error().Message());
        return std::nullopt;
    }
    return Read(atom::fs::Vfs::GetInstance(), *asset_path);
}

auto AudioMetadataReader::ReadBatch(const atom::fs::IFileSystem& filesystem,
                                    const std::vector<atom::fs::AssetPath>& paths)
    -> std::vector<std::optional<AudioMetadata>> {
    std::vector<std::optional<AudioMetadata>> results(paths.size());

    auto& jobs = atom::job::JobSystem::GetInstance();
    if (!jobs.IsRunning()) {
        // 调度器未启动：退化为顺序读取，保证调用方永远正确。
        for (std::size_t i = 0; i < paths.size(); ++i) {
            results[i] = Read(filesystem, paths[i]);
        }
        return results;
    }

    // 每个文件一个 job：Read 各自 open 独立 IFile（VFS 只读挂载表 + 逐文件句柄），
    // 互不共享可变状态，可安全并发。写入 results[i] 的索引互不相同，无数据竞争。
    std::vector<atom::job::JobHandle> handles;
    handles.reserve(paths.size());
    for (std::size_t i = 0; i < paths.size(); ++i) {
        const atom::fs::AssetPath path = paths[i];
        handles.push_back(jobs.Schedule("audio:metadata", [&filesystem, &results, i, path] {
            results[i] = Read(filesystem, path);
        }));
    }
    jobs.WaitAll(handles);
    return results;
}

auto AudioMetadataReader::ReadBatch(const std::vector<std::string>& paths)
    -> std::vector<std::optional<AudioMetadata>> {
    // 先顺序解析字符串（解析很廉价，无需并行）；无效路径置默认空 AssetPath，
    // 下游 Read 会因 InvalidPath 返回 nullopt，结果顺序仍与输入一致。
    std::vector<atom::fs::AssetPath> asset_paths;
    asset_paths.reserve(paths.size());
    for (const std::string& path : paths) {
        auto parsed = atom::fs::AssetPath::Parse(path);
        if (!parsed) {
            LOG_WARNING(atom::log::audio::Metadata,
                        "Invalid asset path '" + path + "': " + parsed.error().Message());
            asset_paths.emplace_back();
        } else {
            asset_paths.push_back(*parsed);
        }
    }
    return ReadBatch(atom::fs::Vfs::GetInstance(), asset_paths);
}

} // namespace atom::audio
