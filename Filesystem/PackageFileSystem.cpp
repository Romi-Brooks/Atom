/**
  * @file           : PackageFileSystem.cpp
  * @brief          : APKG package backend implementation.
  * @attention      : Reads the APKG container (fixed header + manifest, see
  *                   Filesystem/PackageFormat.hpp).
**/

#include "PackageFileSystem.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <filesystem>

#include <Filesystem/PackageFormat.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom::fs {
namespace {

namespace native_fs = std::filesystem;

namespace pf = atom::fs::package_format;

struct PackageEntry {
        std::string relative{};
        uint64_t offset = 0;
        uint64_t size = 0;
        std::uint16_t kind = pf::asset_kind::unknown;
        uint64_t content_hash = 0;
};

[[nodiscard]] auto NormalizeRelative(std::string_view relative) -> std::string {
    std::string normalized{relative};
    std::ranges::replace(normalized, '\\', '/');
    while (!normalized.empty() && normalized.front() == '/')
        normalized.erase(normalized.begin());
    while (!normalized.empty() && normalized.back() == '/')
        normalized.pop_back();
    return normalized;
}

// One stored chunk of an asset. compression == store, so stored == logical.
struct ChunkSpan {
        uint64_t offset = 0; // absolute file offset
        uint64_t size = 0;
};

// Multi-span file used by the reader: an asset is a run of chunks whose
// logical bytes concatenate into the asset content.
class PackageReadFile final : public IFile {
    public:
        PackageReadFile(std::ifstream stream, std::vector<ChunkSpan> spans, const uint64_t size)
            : stream_(std::move(stream)), spans_(std::move(spans)), size_(size) {}

        [[nodiscard]] auto IsOpen() const -> bool {
            return stream_.is_open();
        }
        [[nodiscard]] auto Size() const -> uint64_t override {
            return size_;
        }
        [[nodiscard]] auto Tell() const -> uint64_t override {
            return position_;
        }

        auto Seek(const uint64_t offset) -> Result override {
            if (offset > size_)
                return Result::OutOfRange;
            position_ = offset;
            return Result::Success;
        }

        auto ReadAt(const uint64_t offset, const std::span<std::byte> destination) -> Result override {
            if (offset > size_ || destination.size() > size_ - offset)
                return Result::OutOfRange;
            return ReadLogical(offset, destination);
        }

        auto ReadNext(std::span<std::byte> destination) -> Result override {
            if (position_ > size_ || destination.size() > size_ - position_)
                return Result::OutOfRange;
            const Result status = ReadLogical(position_, destination);
            if (status == Result::Success)
                position_ += destination.size();
            return status;
        }

    private:
        auto ReadLogical(uint64_t offset, std::span<std::byte> destination) -> Result {
            std::size_t written = 0;
            while (written < destination.size()) {
                // Locate the chunk containing the current logical offset.
                uint64_t chunk_base = 0;
                const ChunkSpan* hit = nullptr;
                for (const ChunkSpan& span : spans_) {
                    if (offset < chunk_base + span.size) {
                        hit = &span;
                        break;
                    }
                    chunk_base += span.size;
                }
                if (hit == nullptr)
                    return Result::IoError; // logical offset beyond all chunks

                const uint64_t within = offset - chunk_base;
                const auto available = static_cast<std::size_t>(hit->size - within);
                const auto take = std::min(available, destination.size() - written);

                stream_.clear();
                stream_.seekg(static_cast<std::streamoff>(hit->offset + within), std::ios::beg);
                if (!stream_)
                    return Result::IoError;
                stream_.read(reinterpret_cast<char*>(destination.data() + written), static_cast<std::streamsize>(take));
                if (!stream_)
                    return Result::IoError;

                written += take;
                offset += take;
            }
            return Result::Success;
        }

        std::ifstream stream_{};
        std::vector<ChunkSpan> spans_{};
        uint64_t size_ = 0;
        uint64_t position_ = 0;
};

struct Directory {
        std::string name{};      // bare name, "" for root
        std::string full_path{}; // package-relative path, "" for root
        uint32_t parent = pf::no_parent;
        uint32_t first_child = 0;
        uint32_t child_count = 0;
        uint32_t first_asset = 0;
        uint32_t asset_count = 0;
};

// ---------------------------------------------------------------------------
// Loading (fixed header + manifest)
// ---------------------------------------------------------------------------

[[nodiscard]] auto ReadExactAt(std::ifstream& input, const uint64_t offset, std::byte* destination,
                               const std::size_t size) -> bool {
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input)
        return false;
    input.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size));
    return static_cast<bool>(input);
}

struct Manifest {
        std::vector<std::string> strings{};
        std::vector<ChunkSpan> chunks{}; // one entry per chunk table row
        std::vector<PackageEntry> files{};
        std::vector<std::vector<ChunkSpan>> file_spans{}; // parallel to files
        std::vector<Directory> dirs{};
        std::unordered_map<std::string, std::size_t> file_index{};
        std::unordered_map<std::string, std::uint32_t> dir_index{};
};

auto LoadManifest(std::ifstream& input, const uint64_t package_size, Manifest& manifest) -> Result {
    if (package_size < pf::header_size)
        return Result::IoError;

    // ---- Header ----
    std::array<std::byte, pf::header_size> header{};
    if (!ReadExactAt(input, 0, header.data(), header.size()))
        return Result::IoError;
    if (!std::equal(std::begin(pf::magic), std::end(pf::magic),
                    reinterpret_cast<const char*>(header.data() + pf::header_field::magic)))
        return Result::IoError;
    if (pf::ReadU16(header.data() + pf::header_field::version) != pf::version)
        return Result::IoError;
    if (pf::ReadU16(header.data() + pf::header_field::flags) != 0)
        return Result::IoError;
    if (pf::ReadU64(header.data() + pf::header_field::header_hash) !=
        pf::Fnva(header.data(), pf::header_hash_coverage))
        return Result::IoError;

    const uint64_t data_offset = pf::ReadU64(header.data() + pf::header_field::data_offset);
    const uint64_t manifest_offset = pf::ReadU64(header.data() + pf::header_field::manifest_offset);
    const uint64_t manifest_size = pf::ReadU64(header.data() + pf::header_field::manifest_size);
    const uint64_t manifest_hash = pf::ReadU64(header.data() + pf::header_field::manifest_hash);

    if (data_offset < pf::header_size || manifest_offset < data_offset)
        return Result::IoError;
    if (manifest_size < 8 || manifest_size > pf::max_manifest_size)
        return Result::IoError;
    if (manifest_offset > package_size || manifest_size > package_size - manifest_offset)
        return Result::IoError;

    // ---- Manifest region ----
    std::vector<std::byte> region(manifest_size);
    if (!ReadExactAt(input, manifest_offset, region.data(), region.size()))
        return Result::IoError;
    if (pf::Fnva(region.data(), region.size()) != manifest_hash)
        return Result::IoError;
    if (!std::equal(std::begin(pf::manifest_magic), std::end(pf::manifest_magic),
                    reinterpret_cast<const char*>(region.data())))
        return Result::IoError;

    const uint32_t section_count = pf::ReadU32(region.data() + 4);
    if (section_count == 0 || section_count > pf::max_section_count)
        return Result::IoError;
    const uint64_t directory_size = 8 + static_cast<uint64_t>(section_count) * pf::section_entry_size;
    if (directory_size > manifest_size)
        return Result::IoError;

    struct SectionRef {
            uint64_t offset = 0;
            uint64_t size = 0;
            bool present = false;
    };
    SectionRef string_section{};
    SectionRef chunk_section{};
    SectionRef asset_section{};
    SectionRef dir_section{};

    for (uint32_t i = 0; i < section_count; ++i) {
        const std::byte* entry = region.data() + 8 + static_cast<uint64_t>(i) * pf::section_entry_size;
        const uint32_t kind = pf::ReadU32(entry);
        const uint64_t offset = pf::ReadU64(entry + 4);
        const uint64_t size = pf::ReadU64(entry + 12);
        if (offset < directory_size || offset > manifest_size || size > manifest_size - offset)
            return Result::IoError;

        SectionRef ref{offset, size, true};
        switch (kind) {
        case pf::section_kind::string_table:
            if (string_section.present)
                return Result::IoError;
            string_section = ref;
            break;
        case pf::section_kind::chunk_table:
            if (chunk_section.present)
                return Result::IoError;
            chunk_section = ref;
            break;
        case pf::section_kind::asset_table:
            if (asset_section.present)
                return Result::IoError;
            asset_section = ref;
            break;
        case pf::section_kind::directory_table:
            if (dir_section.present)
                return Result::IoError;
            dir_section = ref;
            break;
        default:
            break; // unknown sections are skipped for forward compatibility
        }
    }
    if (!string_section.present || !chunk_section.present || !asset_section.present || !dir_section.present)
        return Result::IoError;

    // ---- String table ----
    {
        const std::byte* section = region.data() + string_section.offset;
        if (string_section.size < 4)
            return Result::IoError;
        const uint32_t count = pf::ReadU32(section);
        if (count == 0 || count > pf::max_string_count)
            return Result::IoError;
        const uint64_t index_size = 4 + static_cast<uint64_t>(count) * pf::string_ref_size;
        if (index_size > string_section.size)
            return Result::IoError;
        const uint64_t blob_size = string_section.size - index_size;
        if (blob_size > pf::max_string_blob_size)
            return Result::IoError;
        const std::byte* blob = section + index_size;

        manifest.strings.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* ref = section + 4 + static_cast<uint64_t>(i) * pf::string_ref_size;
            const uint32_t offset = pf::ReadU32(ref);
            const uint32_t length = pf::ReadU32(ref + 4);
            if (offset > blob_size || length > blob_size - offset)
                return Result::IoError;
            std::string value(reinterpret_cast<const char*>(blob + offset), length);
            if (!atom::IsValidUtf8(value))
                return Result::IoError;
            manifest.strings.push_back(std::move(value));
        }
        if (!manifest.strings.front().empty())
            return Result::IoError; // string 0 is always ""
    }

    // ---- Chunk table ----
    {
        const std::byte* section = region.data() + chunk_section.offset;
        if (chunk_section.size < 4)
            return Result::IoError;
        const uint32_t count = pf::ReadU32(section);
        if (count > pf::max_chunk_count)
            return Result::IoError;
        if (4 + static_cast<uint64_t>(count) * pf::chunk_entry_size != chunk_section.size)
            return Result::IoError;

        manifest.chunks.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* row = section + 4 + static_cast<uint64_t>(i) * pf::chunk_entry_size;
            const uint64_t chunk_data_offset = pf::ReadU64(row);
            const uint64_t stored_size = pf::ReadU64(row + 8);
            const uint64_t logical_size = pf::ReadU64(row + 16);
            // content_hash (row + 24) is verified by tools, not on the read path.
            const uint16_t compression = pf::ReadU16(row + 32);
            const uint16_t flags = pf::ReadU16(row + 34);

            if (compression != pf::compression_store)
                return Result::IoError;
            if (stored_size != logical_size)
                return Result::IoError;
            if (flags != 0)
                return Result::IoError;
            if (chunk_data_offset < data_offset || chunk_data_offset > manifest_offset ||
                stored_size > manifest_offset - chunk_data_offset)
                return Result::IoError;
            manifest.chunks.push_back(ChunkSpan{chunk_data_offset, stored_size});
        }
    }

    // ---- Asset table ----
    struct AssetRow {
            std::string path{};
            uint32_t parent_dir = 0;
            uint64_t logical_size = 0;
            std::uint16_t kind = pf::asset_kind::unknown;
            uint64_t content_hash = 0;
            uint32_t chunk_start = 0;
            uint32_t chunk_count = 0;
    };
    std::vector<AssetRow> asset_rows;
    {
        const std::byte* section = region.data() + asset_section.offset;
        if (asset_section.size < 4)
            return Result::IoError;
        const uint32_t count = pf::ReadU32(section);
        if (count == 0 || count > pf::max_asset_count)
            return Result::IoError;
        if (4 + static_cast<uint64_t>(count) * pf::asset_entry_size != asset_section.size)
            return Result::IoError;

        asset_rows.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* row = section + 4 + static_cast<uint64_t>(i) * pf::asset_entry_size;
            const uint64_t path_hash = pf::ReadU64(row);
            const uint32_t path_str = pf::ReadU32(row + 8);
            const uint32_t parent_dir = pf::ReadU32(row + 12);
            const uint16_t kind = pf::ReadU16(row + 16);
            const uint16_t flags = pf::ReadU16(row + 18);
            const uint64_t logical_size = pf::ReadU64(row + 20);
            const uint64_t content_hash = pf::ReadU64(row + 28);
            const uint32_t chunk_start = pf::ReadU32(row + 36);
            const uint32_t chunk_count = pf::ReadU32(row + 40);

            if (flags != 0 || kind > pf::asset_kind::video)
                return Result::IoError;
            if (path_str >= manifest.strings.size())
                return Result::IoError;
            if (chunk_count == 0 || chunk_start > manifest.chunks.size() ||
                chunk_count > manifest.chunks.size() - chunk_start)
                return Result::IoError;

            const std::string& path = manifest.strings[path_str];
            if (!pf::IsValidPackagePath(path))
                return Result::IoError;
            if (pf::Fnva(path) != path_hash)
                return Result::IoError;

            asset_rows.push_back(AssetRow{path, parent_dir, logical_size, kind, content_hash, chunk_start, chunk_count});
        }
    }

    // ---- Directory table ----
    {
        const std::byte* section = region.data() + dir_section.offset;
        if (dir_section.size < 4)
            return Result::IoError;
        const uint32_t count = pf::ReadU32(section);
        if (count == 0 || count > pf::max_directory_count)
            return Result::IoError;
        if (4 + static_cast<uint64_t>(count) * pf::directory_entry_size != dir_section.size)
            return Result::IoError;

        manifest.dirs.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* row = section + 4 + static_cast<uint64_t>(i) * pf::directory_entry_size;
            const uint32_t name_str = pf::ReadU32(row);
            const uint32_t parent = pf::ReadU32(row + 4);
            const uint32_t first_child = pf::ReadU32(row + 8);
            const uint32_t child_count = pf::ReadU32(row + 12);
            const uint32_t first_asset = pf::ReadU32(row + 16);
            const uint32_t asset_count = pf::ReadU32(row + 20);

            if (name_str >= manifest.strings.size())
                return Result::IoError;
            if (child_count > 0 && (first_child >= count || child_count > count - first_child))
                return Result::IoError;
            if (asset_count > 0 && (first_asset >= asset_rows.size() || asset_count > asset_rows.size() - first_asset))
                return Result::IoError;

            Directory& dir = manifest.dirs[i];
            dir.name = manifest.strings[name_str];
            dir.parent = parent;
            dir.first_child = first_child;
            dir.child_count = child_count;
            dir.first_asset = first_asset;
            dir.asset_count = asset_count;
        }

        // Root: index 0, empty name, no parent. Every other node must point at
        // an already-seen parent, which both bounds depth and kills cycles.
        Directory& root = manifest.dirs[pf::root_directory];
        if (!root.name.empty() || root.parent != pf::no_parent)
            return Result::IoError;
        manifest.dir_index.emplace("", pf::root_directory);
        for (uint32_t i = 1; i < count; ++i) {
            Directory& dir = manifest.dirs[i];
            if (dir.parent == pf::no_parent || dir.parent >= i)
                return Result::IoError;
            if (dir.name.empty() || dir.name.find('/') != std::string::npos)
                return Result::IoError;
            dir.full_path = manifest.dirs[dir.parent].full_path.empty()
                                ? dir.name
                                : manifest.dirs[dir.parent].full_path + "/" + dir.name;
            if (!manifest.dir_index.emplace(dir.full_path, i).second)
                return Result::IoError;
        }
    }

    // ---- Assets: validate against directories, build file index ----
    manifest.files.reserve(asset_rows.size());
    manifest.file_spans.reserve(asset_rows.size());
    for (uint32_t i = 0; i < asset_rows.size(); ++i) {
        const AssetRow& row = asset_rows[i];
        if (row.parent_dir >= manifest.dirs.size())
            return Result::IoError;

        // The asset's parent chain must match its path prefix.
        const std::string expected_parent{pf::PackagePathParent(row.path)};
        const auto dir_it = manifest.dir_index.find(expected_parent);
        if (dir_it == manifest.dir_index.end() || dir_it->second != row.parent_dir)
            return Result::IoError;

        uint64_t logical_sum = 0;
        std::vector<ChunkSpan> spans;
        spans.reserve(row.chunk_count);
        for (uint32_t c = 0; c < row.chunk_count; ++c) {
            const ChunkSpan& span = manifest.chunks[row.chunk_start + c];
            if (logical_sum > row.logical_size || span.size > row.logical_size - logical_sum)
                return Result::IoError;
            logical_sum += span.size;
            spans.push_back(span);
        }
        if (logical_sum != row.logical_size)
            return Result::IoError;

        if (!manifest.file_index.emplace(row.path, manifest.files.size()).second)
            return Result::IoError;
        manifest.files.push_back(
            PackageEntry{row.path, spans.front().offset, row.logical_size, row.kind, row.content_hash});
        manifest.file_spans.push_back(std::move(spans));
    }

    // Directory asset runs must reference the assets that claim them.
    for (uint32_t i = 0; i < manifest.dirs.size(); ++i) {
        const Directory& dir = manifest.dirs[i];
        for (uint32_t a = 0; a < dir.asset_count; ++a) {
            if (asset_rows[dir.first_asset + a].parent_dir != i)
                return Result::IoError;
        }
    }

    return Result::Success;
}

} // namespace

class PackageFileSystem::Impl final {
    public:
        Impl(std::string mount, std::string package_path)
            : mount_(std::move(mount)), package_path_(std::move(package_path)) {}

        [[nodiscard]] auto Resolve(const AssetPath& path) const -> Result {
            if (!path.IsValid())
                return Result::InvalidPath;
            if (path.Mount() != mount_)
                return Result::NotFound;
            return Result::Success;
        }

        [[nodiscard]] auto FindFile(const std::string& relative) const -> const PackageEntry* {
            const auto it = index_.find(relative);
            return it == index_.end() ? nullptr : &entries_[it->second];
        }

        auto OpenEntry(const PackageEntry& entry) const -> std::unique_ptr<IFile> {
            std::ifstream stream{atom::PathFromUtf8(package_path_), std::ios::binary};
            if (!stream.is_open())
                return nullptr;

            const auto it = index_.find(entry.relative);
            if (it == index_.end() || it->second >= spans_.size())
                return nullptr;
            auto file = std::make_unique<PackageReadFile>(std::move(stream), spans_[it->second], entry.size);
            return file->IsOpen() ? std::move(file) : nullptr;
        }

        std::string mount_{};
        std::string package_path_{};
        std::vector<PackageEntry> entries_{};
        std::unordered_map<std::string, std::size_t> index_{};

        // Directory table and per-file chunk spans.
        std::vector<Directory> dirs_{};
        std::unordered_map<std::string, std::uint32_t> dir_index_{};
        std::vector<std::vector<ChunkSpan>> spans_{}; // parallel to entries_
};

PackageFileSystem::PackageFileSystem(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
PackageFileSystem::~PackageFileSystem() = default;

auto PackageFileSystem::Create(const std::string_view mount, const std::string_view package_utf8,
                               std::unique_ptr<PackageFileSystem>& output) -> Result {
    output.reset();
    if (package_utf8.empty() || !atom::IsValidUtf8(std::string{package_utf8}))
        return Result::InvalidPath;

    AssetPath mountRoot{};
    if (!AssetPath::TryParse(std::string{mount} + "://", mountRoot) || mountRoot.Mount() != mount)
        return Result::InvalidPath;

    const native_fs::path package_path = atom::PathFromUtf8(std::string{package_utf8});
    std::error_code error;
    if (!native_fs::exists(package_path, error) || error)
        return Result::NotFound;
    if (!native_fs::is_regular_file(package_path, error) || error)
        return Result::NotFile;
    const uintmax_t package_size = native_fs::file_size(package_path, error);
    if (error || package_size < pf::header_size)
        return Result::IoError;

    std::ifstream input{package_path, std::ios::binary};
    if (!input.is_open())
        return Result::IoError;

    auto impl = std::make_unique<Impl>(std::string{mount}, atom::PathToUtf8(package_path));

    Manifest manifest{};
    const Result loaded = LoadManifest(input, package_size, manifest);
    if (loaded != Result::Success)
        return loaded;
    impl->entries_ = std::move(manifest.files);
    impl->index_ = std::move(manifest.file_index);
    impl->dirs_ = std::move(manifest.dirs);
    impl->dir_index_ = std::move(manifest.dir_index);
    impl->spans_ = std::move(manifest.file_spans);

    output = std::unique_ptr<PackageFileSystem>{new PackageFileSystem{std::move(impl)}};
    return Result::Success;
}

auto PackageFileSystem::Stat(const AssetPath& path, FileInfo& output) const -> Result {
    const Result resolved = impl_->Resolve(path);
    if (resolved != Result::Success)
        return resolved;

    if (path.IsRoot()) {
        output = {EntryType::Directory, 0};
        return Result::Success;
    }

    const std::string relative = NormalizeRelative(path.Relative());
    if (const PackageEntry* file = impl_->FindFile(relative)) {
        output = {EntryType::File, file->size};
        return Result::Success;
    }

    if (impl_->dir_index_.contains(relative)) {
        output = {EntryType::Directory, 0};
        return Result::Success;
    }
    return Result::NotFound;
}

auto PackageFileSystem::OpenRead(const AssetPath& path, std::unique_ptr<IFile>& output) const -> Result {
    output.reset();
    FileInfo info{};
    const Result status = Stat(path, info);
    if (status != Result::Success)
        return status;
    if (info.type != EntryType::File)
        return Result::NotFile;

    const std::string relative = NormalizeRelative(path.Relative());
    const PackageEntry* entry = impl_->FindFile(relative);
    if (!entry)
        return Result::NotFound;
    auto file = impl_->OpenEntry(*entry);
    if (!file)
        return Result::IoError;
    output = std::move(file);
    return Result::Success;
}

auto PackageFileSystem::List(const AssetPath& directory, std::vector<DirectoryEntry>& output) const -> Result {
    output.clear();
    FileInfo info{};
    const Result status = Stat(directory, info);
    if (status != Result::Success)
        return status;
    if (info.type != EntryType::Directory)
        return Result::NotDirectory;

    const std::string relative = directory.IsRoot() ? std::string{} : NormalizeRelative(directory.Relative());
    const auto dir_it = impl_->dir_index_.find(relative);
    if (dir_it == impl_->dir_index_.end())
        return Result::NotFound;
    const Directory& dir = impl_->dirs_[dir_it->second];

    output.reserve(dir.child_count + dir.asset_count);
    for (uint32_t c = 0; c < dir.child_count; ++c)
        output.push_back(DirectoryEntry{impl_->dirs_[dir.first_child + c].name, {EntryType::Directory, 0}});
    for (uint32_t a = 0; a < dir.asset_count; ++a) {
        const PackageEntry& entry = impl_->entries_[dir.first_asset + a];
        output.push_back(
            DirectoryEntry{std::string{pf::PackagePathName(entry.relative)}, {EntryType::File, entry.size}});
    }
    std::ranges::sort(output, {}, &DirectoryEntry::name);
    return Result::Success;
}

auto PackageFileSystem::EnumerateAssets(std::vector<PackagedAssetInfo>& output) const -> Result {
    output.clear();
    output.reserve(impl_->entries_.size());
    for (const PackageEntry& entry : impl_->entries_)
        output.push_back(PackagedAssetInfo{entry.relative, entry.kind, entry.size, entry.content_hash});
    std::ranges::sort(output, {}, &PackagedAssetInfo::path);
    return Result::Success;
}

} // namespace atom::fs
