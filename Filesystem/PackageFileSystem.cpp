/**
  * @file           : PackageFileSystem.cpp
  * @brief          : APKG package backend implementation.
  * @attention      : Reads APKG v1 (frozen, trailing file table) and APKG v2
  *                   (fixed header + manifest, see Filesystem/PackageFormatV2.hpp).
**/

#include "PackageFileSystem.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <filesystem>

#include <Filesystem/PackageFormat.hpp>
#include <Filesystem/PackageFormatV2.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom::fs {
namespace {

namespace native_fs = std::filesystem;

// The APKG v1 layout (magic, version and the limits the reader enforces) lives
// in PackageFormat.hpp so the writer and the reader cannot drift apart.
namespace package_format = atom::fs::package_format;
namespace pf2 = atom::fs::package_format_v2;

struct PackageEntry {
        std::string relative{};
        uint64_t offset = 0;
        uint64_t size = 0;
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

[[nodiscard]] auto IsSafePackageRelative(const std::string& relative) -> bool {
    if (relative.empty())
        return false;
    if (relative.find('\0') != std::string::npos)
        return false;
    if (relative.starts_with('/') || relative.contains("//"))
        return false;

    const native_fs::path path = atom::PathFromUtf8(relative);
    if (path.is_absolute() || path.has_root_name())
        return false;
    for (const auto& part : path) {
        if (part == ".." || part == ".")
            return false;
    }
    return true;
}

// Single-span file used by the v1 reader.
class PackageReadFile final : public IFile {
    public:
        PackageReadFile(std::ifstream stream, const uint64_t base, const uint64_t size)
            : stream_(std::move(stream)), base_(base), size_(size) {}

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
            if (!RangeFits(offset, destination.size()))
                return Result::OutOfRange;
            if (destination.empty())
                return Result::Success;

            stream_.clear();
            stream_.seekg(static_cast<std::streamoff>(base_ + offset), std::ios::beg);
            if (!stream_)
                return Result::IoError;
            stream_.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(destination.size()));
            return stream_ ? Result::Success : Result::IoError;
        }

        auto ReadNext(std::span<std::byte> destination) -> Result override {
            if (!RangeFits(position_, destination.size()))
                return Result::OutOfRange;
            if (destination.empty())
                return Result::Success;

            stream_.clear();
            stream_.seekg(static_cast<std::streamoff>(base_ + position_), std::ios::beg);
            if (!stream_)
                return Result::IoError;
            stream_.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(destination.size()));
            if (!stream_)
                return Result::IoError;
            position_ += destination.size();
            return Result::Success;
        }

    private:
        [[nodiscard]] auto RangeFits(const uint64_t offset, const std::size_t length) const -> bool {
            return offset <= size_ && length <= size_ - offset &&
                   offset <= static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max()) &&
                   length <= static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
        }

        std::ifstream stream_{};
        uint64_t base_ = 0;
        uint64_t size_ = 0;
        uint64_t position_ = 0;
};

// One stored chunk of a v2 asset. compression == store, so stored == logical.
struct ChunkSpan {
        uint64_t offset = 0; // absolute file offset
        uint64_t size = 0;
};

// Multi-span file used by the v2 reader: an asset is a run of chunks whose
// logical bytes concatenate into the asset content.
class PackageReadFileV2 final : public IFile {
    public:
        PackageReadFileV2(std::ifstream stream, std::vector<ChunkSpan> spans, const uint64_t size)
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

struct V2Directory {
        std::string name{};      // bare name, "" for root
        std::string full_path{}; // package-relative path, "" for root
        uint32_t parent = pf2::no_parent;
        uint32_t first_child = 0;
        uint32_t child_count = 0;
        uint32_t first_asset = 0;
        uint32_t asset_count = 0;
};

// ---------------------------------------------------------------------------
// v1 loading (frozen format, unchanged behavior)
// ---------------------------------------------------------------------------

auto LoadV1(std::ifstream& input, const uint64_t package_size, std::vector<PackageEntry>& entries,
            std::unordered_map<std::string, std::size_t>& index) -> Result {
    uint32_t file_count = 0;
    input.read(reinterpret_cast<char*>(&file_count), sizeof(file_count));
    if (!input || file_count == 0 || file_count > package_format::max_file_count)
        return Result::IoError;

    input.seekg(-static_cast<std::streamoff>(sizeof(uint64_t)), std::ios::end);
    if (!input)
        return Result::IoError;
    uint64_t table_offset = 0;
    input.read(reinterpret_cast<char*>(&table_offset), sizeof(table_offset));
    if (!input || table_offset < package_format::min_header_size || table_offset >= package_size)
        return Result::IoError;

    input.seekg(static_cast<std::streamoff>(table_offset));
    if (!input)
        return Result::IoError;

    entries.reserve(file_count);
    for (uint32_t i = 0; i < file_count; ++i) {
        uint16_t name_length = 0;
        input.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));
        if (!input || name_length == 0 || name_length > package_format::max_name_length)
            return Result::IoError;

        std::string name(name_length, '\0');
        input.read(name.data(), name_length);
        if (!input || !atom::IsValidUtf8(name))
            return Result::IoError;
        name = NormalizeRelative(name);
        if (!IsSafePackageRelative(name))
            return Result::IoError;

        uint64_t offset = 0;
        uint64_t size = 0;
        input.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        input.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!input || offset < package_format::min_header_size || offset > table_offset || size > table_offset - offset)
            return Result::IoError;

        uint8_t type_length = 0;
        input.read(reinterpret_cast<char*>(&type_length), sizeof(type_length));
        if (!input)
            return Result::IoError;
        if (type_length > 0) {
            std::string type(type_length, '\0');
            input.read(type.data(), type_length);
            if (!input)
                return Result::IoError;
        }

        if (index.contains(name))
            return Result::IoError;
        index[name] = entries.size();
        entries.push_back(PackageEntry{name, offset, size});
    }
    return Result::Success;
}

// ---------------------------------------------------------------------------
// v2 loading (fixed header + manifest)
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

struct V2Manifest {
        std::vector<std::string> strings{};
        std::vector<ChunkSpan> chunks{}; // one entry per chunk table row
        std::vector<PackageEntry> files{};
        std::vector<std::vector<ChunkSpan>> file_spans{}; // parallel to files
        std::vector<V2Directory> dirs{};
        std::unordered_map<std::string, std::size_t> file_index{};
        std::unordered_map<std::string, std::uint32_t> dir_index{};
};

auto LoadV2(std::ifstream& input, const uint64_t package_size, V2Manifest& manifest) -> Result {
    if (package_size < pf2::header_size)
        return Result::IoError;

    // ---- Header ----
    std::array<std::byte, pf2::header_size> header{};
    if (!ReadExactAt(input, 0, header.data(), header.size()))
        return Result::IoError;
    if (!std::equal(std::begin(pf2::magic), std::end(pf2::magic),
                    reinterpret_cast<const char*>(header.data() + pf2::header_field::magic)))
        return Result::IoError;
    if (pf2::ReadU16(header.data() + pf2::header_field::version) != pf2::version)
        return Result::IoError;
    if (pf2::ReadU16(header.data() + pf2::header_field::flags) != 0)
        return Result::IoError;
    if (pf2::ReadU64(header.data() + pf2::header_field::header_hash) !=
        pf2::Fnva(header.data(), pf2::header_hash_coverage))
        return Result::IoError;

    const uint64_t data_offset = pf2::ReadU64(header.data() + pf2::header_field::data_offset);
    const uint64_t manifest_offset = pf2::ReadU64(header.data() + pf2::header_field::manifest_offset);
    const uint64_t manifest_size = pf2::ReadU64(header.data() + pf2::header_field::manifest_size);
    const uint64_t manifest_hash = pf2::ReadU64(header.data() + pf2::header_field::manifest_hash);

    if (data_offset < pf2::header_size || manifest_offset < data_offset)
        return Result::IoError;
    if (manifest_size < 8 || manifest_size > pf2::max_manifest_size)
        return Result::IoError;
    if (manifest_offset > package_size || manifest_size > package_size - manifest_offset)
        return Result::IoError;

    // ---- Manifest region ----
    std::vector<std::byte> region(manifest_size);
    if (!ReadExactAt(input, manifest_offset, region.data(), region.size()))
        return Result::IoError;
    if (pf2::Fnva(region.data(), region.size()) != manifest_hash)
        return Result::IoError;
    if (!std::equal(std::begin(pf2::manifest_magic), std::end(pf2::manifest_magic),
                    reinterpret_cast<const char*>(region.data())))
        return Result::IoError;

    const uint32_t section_count = pf2::ReadU32(region.data() + 4);
    if (section_count == 0 || section_count > pf2::max_section_count)
        return Result::IoError;
    const uint64_t directory_size = 8 + static_cast<uint64_t>(section_count) * pf2::section_entry_size;
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
        const std::byte* entry = region.data() + 8 + static_cast<uint64_t>(i) * pf2::section_entry_size;
        const uint32_t kind = pf2::ReadU32(entry);
        const uint64_t offset = pf2::ReadU64(entry + 4);
        const uint64_t size = pf2::ReadU64(entry + 12);
        if (offset < directory_size || offset > manifest_size || size > manifest_size - offset)
            return Result::IoError;

        SectionRef ref{offset, size, true};
        switch (kind) {
        case pf2::section_kind::string_table:
            if (string_section.present)
                return Result::IoError;
            string_section = ref;
            break;
        case pf2::section_kind::chunk_table:
            if (chunk_section.present)
                return Result::IoError;
            chunk_section = ref;
            break;
        case pf2::section_kind::asset_table:
            if (asset_section.present)
                return Result::IoError;
            asset_section = ref;
            break;
        case pf2::section_kind::directory_table:
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
        const uint32_t count = pf2::ReadU32(section);
        if (count == 0 || count > pf2::max_string_count)
            return Result::IoError;
        const uint64_t index_size = 4 + static_cast<uint64_t>(count) * pf2::string_ref_size;
        if (index_size > string_section.size)
            return Result::IoError;
        const uint64_t blob_size = string_section.size - index_size;
        if (blob_size > pf2::max_string_blob_size)
            return Result::IoError;
        const std::byte* blob = section + index_size;

        manifest.strings.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* ref = section + 4 + static_cast<uint64_t>(i) * pf2::string_ref_size;
            const uint32_t offset = pf2::ReadU32(ref);
            const uint32_t length = pf2::ReadU32(ref + 4);
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
        const uint32_t count = pf2::ReadU32(section);
        if (count > pf2::max_chunk_count)
            return Result::IoError;
        if (4 + static_cast<uint64_t>(count) * pf2::chunk_entry_size != chunk_section.size)
            return Result::IoError;

        manifest.chunks.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* row = section + 4 + static_cast<uint64_t>(i) * pf2::chunk_entry_size;
            const uint64_t chunk_data_offset = pf2::ReadU64(row);
            const uint64_t stored_size = pf2::ReadU64(row + 8);
            const uint64_t logical_size = pf2::ReadU64(row + 16);
            // content_hash (row + 24) is verified by tools, not on the read path.
            const uint16_t compression = pf2::ReadU16(row + 32);
            const uint16_t flags = pf2::ReadU16(row + 34);

            if (compression != pf2::compression_store)
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
    struct V2AssetRow {
            std::string path{};
            uint32_t parent_dir = 0;
            uint64_t logical_size = 0;
            uint32_t chunk_start = 0;
            uint32_t chunk_count = 0;
    };
    std::vector<V2AssetRow> asset_rows;
    {
        const std::byte* section = region.data() + asset_section.offset;
        if (asset_section.size < 4)
            return Result::IoError;
        const uint32_t count = pf2::ReadU32(section);
        if (count == 0 || count > pf2::max_asset_count)
            return Result::IoError;
        if (4 + static_cast<uint64_t>(count) * pf2::asset_entry_size != asset_section.size)
            return Result::IoError;

        asset_rows.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* row = section + 4 + static_cast<uint64_t>(i) * pf2::asset_entry_size;
            const uint64_t path_hash = pf2::ReadU64(row);
            const uint32_t path_str = pf2::ReadU32(row + 8);
            const uint32_t parent_dir = pf2::ReadU32(row + 12);
            const uint16_t kind = pf2::ReadU16(row + 16);
            const uint16_t flags = pf2::ReadU16(row + 18);
            const uint64_t logical_size = pf2::ReadU64(row + 20);
            const uint32_t chunk_start = pf2::ReadU32(row + 36);
            const uint32_t chunk_count = pf2::ReadU32(row + 40);

            if (flags != 0 || kind > pf2::asset_kind::video)
                return Result::IoError;
            if (path_str >= manifest.strings.size())
                return Result::IoError;
            if (chunk_count == 0 || chunk_start > manifest.chunks.size() ||
                chunk_count > manifest.chunks.size() - chunk_start)
                return Result::IoError;

            const std::string& path = manifest.strings[path_str];
            if (!pf2::IsValidPackagePath(path))
                return Result::IoError;
            if (pf2::Fnva(path) != path_hash)
                return Result::IoError;

            asset_rows.push_back(V2AssetRow{path, parent_dir, logical_size, chunk_start, chunk_count});
        }
    }

    // ---- Directory table ----
    {
        const std::byte* section = region.data() + dir_section.offset;
        if (dir_section.size < 4)
            return Result::IoError;
        const uint32_t count = pf2::ReadU32(section);
        if (count == 0 || count > pf2::max_directory_count)
            return Result::IoError;
        if (4 + static_cast<uint64_t>(count) * pf2::directory_entry_size != dir_section.size)
            return Result::IoError;

        manifest.dirs.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* row = section + 4 + static_cast<uint64_t>(i) * pf2::directory_entry_size;
            const uint32_t name_str = pf2::ReadU32(row);
            const uint32_t parent = pf2::ReadU32(row + 4);
            const uint32_t first_child = pf2::ReadU32(row + 8);
            const uint32_t child_count = pf2::ReadU32(row + 12);
            const uint32_t first_asset = pf2::ReadU32(row + 16);
            const uint32_t asset_count = pf2::ReadU32(row + 20);

            if (name_str >= manifest.strings.size())
                return Result::IoError;
            if (child_count > 0 && (first_child >= count || child_count > count - first_child))
                return Result::IoError;
            if (asset_count > 0 && (first_asset >= asset_rows.size() || asset_count > asset_rows.size() - first_asset))
                return Result::IoError;

            V2Directory& dir = manifest.dirs[i];
            dir.name = manifest.strings[name_str];
            dir.parent = parent;
            dir.first_child = first_child;
            dir.child_count = child_count;
            dir.first_asset = first_asset;
            dir.asset_count = asset_count;
        }

        // Root: index 0, empty name, no parent. Every other node must point at
        // an already-seen parent, which both bounds depth and kills cycles.
        V2Directory& root = manifest.dirs[pf2::root_directory];
        if (!root.name.empty() || root.parent != pf2::no_parent)
            return Result::IoError;
        manifest.dir_index.emplace("", pf2::root_directory);
        for (uint32_t i = 1; i < count; ++i) {
            V2Directory& dir = manifest.dirs[i];
            if (dir.parent == pf2::no_parent || dir.parent >= i)
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
        const V2AssetRow& row = asset_rows[i];
        if (row.parent_dir >= manifest.dirs.size())
            return Result::IoError;

        // The asset's parent chain must match its path prefix.
        const std::string expected_parent{pf2::PackagePathParent(row.path)};
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
        manifest.files.push_back(PackageEntry{row.path, spans.front().offset, row.logical_size});
        manifest.file_spans.push_back(std::move(spans));
    }

    // Directory asset runs must reference the assets that claim them.
    for (uint32_t i = 0; i < manifest.dirs.size(); ++i) {
        const V2Directory& dir = manifest.dirs[i];
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

            if (!is_v2_) {
                auto file = std::make_unique<PackageReadFile>(std::move(stream), entry.offset, entry.size);
                return file->IsOpen() ? std::move(file) : nullptr;
            }

            const auto it = index_.find(entry.relative);
            if (it == index_.end() || it->second >= v2_spans_.size())
                return nullptr;
            auto file = std::make_unique<PackageReadFileV2>(std::move(stream), v2_spans_[it->second], entry.size);
            return file->IsOpen() ? std::move(file) : nullptr;
        }

        std::string mount_{};
        std::string package_path_{};
        std::vector<PackageEntry> entries_{};
        std::unordered_map<std::string, std::size_t> index_{};

        // v2 only: real directory table and per-file chunk spans.
        bool is_v2_ = false;
        std::vector<V2Directory> v2_dirs_{};
        std::unordered_map<std::string, std::uint32_t> v2_dir_index_{};
        std::vector<std::vector<ChunkSpan>> v2_spans_{}; // parallel to entries_
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
    if (error || package_size < package_format::min_header_size + sizeof(uint64_t))
        return Result::IoError;

    std::ifstream input{package_path, std::ios::binary};
    if (!input.is_open())
        return Result::IoError;

    char magic[4]{};
    input.read(magic, 4);
    if (!input || !std::equal(std::begin(package_format::magic), std::end(package_format::magic), magic))
        return Result::IoError;

    uint16_t version = 0;
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!input)
        return Result::IoError;

    auto impl = std::make_unique<Impl>(std::string{mount}, atom::PathToUtf8(package_path));

    if (version == package_format::version) {
        const Result loaded = LoadV1(input, package_size, impl->entries_, impl->index_);
        if (loaded != Result::Success)
            return loaded;
    } else if (version == pf2::version) {
        V2Manifest manifest{};
        const Result loaded = LoadV2(input, package_size, manifest);
        if (loaded != Result::Success)
            return loaded;
        impl->is_v2_ = true;
        impl->entries_ = std::move(manifest.files);
        impl->index_ = std::move(manifest.file_index);
        impl->v2_dirs_ = std::move(manifest.dirs);
        impl->v2_dir_index_ = std::move(manifest.dir_index);
        impl->v2_spans_ = std::move(manifest.file_spans);
    } else {
        return Result::IoError;
    }

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

    if (impl_->is_v2_) {
        if (impl_->v2_dir_index_.contains(relative)) {
            output = {EntryType::Directory, 0};
            return Result::Success;
        }
        return Result::NotFound;
    }

    const std::string prefix = relative + "/";
    for (const PackageEntry& entry : impl_->entries_) {
        if (entry.relative.starts_with(prefix)) {
            output = {EntryType::Directory, 0};
            return Result::Success;
        }
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

    if (impl_->is_v2_) {
        const std::string relative = directory.IsRoot() ? std::string{} : NormalizeRelative(directory.Relative());
        const auto dir_it = impl_->v2_dir_index_.find(relative);
        if (dir_it == impl_->v2_dir_index_.end())
            return Result::NotFound;
        const V2Directory& dir = impl_->v2_dirs_[dir_it->second];

        output.reserve(dir.child_count + dir.asset_count);
        for (uint32_t c = 0; c < dir.child_count; ++c)
            output.push_back(DirectoryEntry{impl_->v2_dirs_[dir.first_child + c].name, {EntryType::Directory, 0}});
        for (uint32_t a = 0; a < dir.asset_count; ++a) {
            const PackageEntry& entry = impl_->entries_[dir.first_asset + a];
            output.push_back(
                DirectoryEntry{std::string{pf2::PackagePathName(entry.relative)}, {EntryType::File, entry.size}});
        }
        std::ranges::sort(output, {}, &DirectoryEntry::name);
        return Result::Success;
    }

    const std::string prefix = directory.IsRoot() ? std::string{} : NormalizeRelative(directory.Relative()) + "/";
    std::map<std::string, DirectoryEntry> merged{};
    for (const PackageEntry& entry : impl_->entries_) {
        if (!prefix.empty()) {
            if (!entry.relative.starts_with(prefix))
                continue;
            const std::string remainder = entry.relative.substr(prefix.size());
            if (remainder.empty())
                continue;
            const std::size_t slash = remainder.find('/');
            const std::string name = remainder.substr(0, slash == std::string::npos ? remainder.size() : slash);
            if (slash == std::string::npos)
                merged.insert_or_assign(name, DirectoryEntry{name, {EntryType::File, entry.size}});
            else
                merged.try_emplace(name, DirectoryEntry{name, {EntryType::Directory, 0}});
            continue;
        }

        const std::size_t slash = entry.relative.find('/');
        const std::string name = entry.relative.substr(0, slash == std::string::npos ? entry.relative.size() : slash);
        if (slash == std::string::npos)
            merged.insert_or_assign(name, DirectoryEntry{name, {EntryType::File, entry.size}});
        else
            merged.try_emplace(name, DirectoryEntry{name, {EntryType::Directory, 0}});
    }

    output.reserve(merged.size());
    for (auto& [name, entry] : merged)
        output.push_back(std::move(entry));
    std::ranges::sort(output, {}, &DirectoryEntry::name);
    return Result::Success;
}

} // namespace atom::fs
