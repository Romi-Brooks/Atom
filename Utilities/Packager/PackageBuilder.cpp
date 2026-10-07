/**
  * @file           : PackageBuilder.cpp
  * @brief          : APKG v2 package writer implementation.
**/

// Self Dependency
#include "PackageBuilder.hpp"

// Standard Library
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <queue>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

#include <filesystem>

// Engine Headers
#include <Filesystem/PackageFormatV2.hpp>
#include <Log/LogSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom::tools {

namespace pf2 = atom::fs::package_format_v2;
namespace native_fs = std::filesystem;

namespace {

constexpr std::size_t io_block_size = 64 * 1024;

// Extension-based kind inference. The package format stores kinds that mirror
// atom::asset::AssetKind so the runtime can classify without re-sniffing
// extensions; unknown extensions fall back to binary.
[[nodiscard]] auto InferKind(const std::string& package_path) -> std::uint16_t {
    const std::size_t dot = package_path.rfind('.');
    if (dot == std::string::npos)
        return pf2::asset_kind::binary;
    std::string ext = package_path.substr(dot + 1);
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga")
        return pf2::asset_kind::texture;
    if (ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac")
        return pf2::asset_kind::audio;
    if (ext == "glsl" || ext == "spv" || ext == "vert" || ext == "frag" || ext == "comp")
        return pf2::asset_kind::shader;
    if (ext == "toml" || ext == "json" || ext == "xml" || ext == "ini" || ext == "cfg" || ext == "yaml" || ext == "yml")
        return pf2::asset_kind::config;
    if (ext == "lua")
        return pf2::asset_kind::script;
    if (ext == "ttf" || ext == "otf")
        return pf2::asset_kind::font;
    if (ext == "obj" || ext == "gltf" || ext == "glb" || ext == "fbx")
        return pf2::asset_kind::model;
    return pf2::asset_kind::binary;
}

void AppendU16(std::vector<std::byte>& out, std::uint16_t value) {
    std::byte bytes[2];
    pf2::WriteU16(bytes, value);
    out.insert(out.end(), std::begin(bytes), std::end(bytes));
}

void AppendU32(std::vector<std::byte>& out, std::uint32_t value) {
    std::byte bytes[4];
    pf2::WriteU32(bytes, value);
    out.insert(out.end(), std::begin(bytes), std::end(bytes));
}

void AppendU64(std::vector<std::byte>& out, std::uint64_t value) {
    std::byte bytes[8];
    pf2::WriteU64(bytes, value);
    out.insert(out.end(), std::begin(bytes), std::end(bytes));
}

void AppendBytes(std::vector<std::byte>& out, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::byte*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

// ---------------------------------------------------------------------------
// Plan model
// ---------------------------------------------------------------------------

struct DirNode {
        std::string name{};                  // bare directory name, "" for root
        std::string full_path{};             // package-relative dir path, "" for root
        std::uint32_t parent = pf2::no_parent;
        std::vector<std::uint32_t> children{}; // dir indices, sorted by name
        std::vector<std::uint32_t> assets{};   // asset indices, sorted by path

        // Filled during re-indexing.
        std::uint32_t first_child = 0;
        std::uint32_t first_asset = 0;
};

struct AssetPlan {
        std::string native_path{};
        std::string package_path{};
        std::uint32_t parent_dir = 0; // assigned dir index (after BFS)
        std::uint16_t kind = pf2::asset_kind::binary;
        std::uint64_t logical_size = 0;
        std::uint64_t content_hash = 0;
        std::uint64_t data_offset = 0; // absolute file offset of its single chunk
};

class StringTable {
    public:
        StringTable() { Intern(""); } // index 0 is always ""

        auto Intern(const std::string& value) -> std::uint32_t {
            if (const auto it = index_.find(value); it != index_.end())
                return it->second;
            const auto index = static_cast<std::uint32_t>(strings_.size());
            index_.emplace(value, index);
            offsets_.push_back(blob_size_);
            lengths_.push_back(static_cast<std::uint32_t>(value.size()));
            blob_size_ += value.size();
            strings_.push_back(value);
            return index;
        }

        [[nodiscard]] auto Count() const -> std::uint32_t { return static_cast<std::uint32_t>(strings_.size()); }
        [[nodiscard]] auto BlobSize() const -> std::uint64_t { return blob_size_; }
        [[nodiscard]] auto Strings() const -> const std::vector<std::string>& { return strings_; }
        [[nodiscard]] auto Offsets() const -> const std::vector<std::uint32_t>& { return offsets_; }
        [[nodiscard]] auto Lengths() const -> const std::vector<std::uint32_t>& { return lengths_; }

    private:
        std::vector<std::string> strings_{};
        std::vector<std::uint32_t> offsets_{};
        std::vector<std::uint32_t> lengths_{};
        std::unordered_map<std::string, std::uint32_t> index_{};
        std::uint64_t blob_size_ = 0;
};

[[nodiscard]] auto NormalizeSlashes(std::string path) -> std::string {
    std::ranges::replace(path, '\\', '/');
    while (!path.empty() && path.front() == '/')
        path.erase(path.begin());
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path;
}

// Registers the directory chain for a package path ("a/b/c.png" registers
// "a" and "a/b") and returns the node index of the innermost directory.
auto EnsureDirectory(std::vector<DirNode>& nodes, std::unordered_map<std::string, std::uint32_t>& by_path,
                     const std::string& dir_path) -> std::uint32_t {
    if (dir_path.empty())
        return pf2::root_directory;
    if (const auto it = by_path.find(dir_path); it != by_path.end())
        return it->second;

    const std::string parent_path{pf2::PackagePathParent(dir_path)};
    const std::uint32_t parent_index = EnsureDirectory(nodes, by_path, parent_path);

    const auto index = static_cast<std::uint32_t>(nodes.size());
    by_path.emplace(dir_path, index);
    DirNode node{};
    node.name = std::string{pf2::PackagePathName(dir_path)};
    node.full_path = dir_path;
    node.parent = parent_index;
    nodes.push_back(std::move(node));
    nodes[parent_index].children.push_back(index);
    return index;
}

} // namespace

PackageBuilder::Config::Config() = default;

auto PackageBuilder::AddFile(const std::string& native_path_utf8, const std::string& package_path) -> Result {
    if (!atom::IsValidUtf8(native_path_utf8) || !atom::IsValidUtf8(package_path))
        return Result::ErrorInvalidPath;
    if (!pf2::IsValidPackagePath(package_path))
        return Result::ErrorInvalidPath;

    std::error_code error;
    const native_fs::path native_path = atom::PathFromUtf8(native_path_utf8);
    if (!native_fs::exists(native_path, error) || error || !native_fs::is_regular_file(native_path, error) || error)
        return Result::ErrorInvalidPath;

    files_.push_back(PlannedFile{native_path_utf8, package_path});
    return Result::Success;
}

auto PackageBuilder::AddDirectory(const std::string& native_dir_utf8, const std::string& package_prefix) -> Result {
    if (!atom::IsValidUtf8(native_dir_utf8) || !atom::IsValidUtf8(package_prefix))
        return Result::ErrorInvalidPath;
    const std::string prefix = NormalizeSlashes(package_prefix);
    if (!prefix.empty() && !pf2::IsValidPackagePath(prefix))
        return Result::ErrorInvalidPath;

    std::error_code error;
    const native_fs::path root = atom::PathFromUtf8(native_dir_utf8);
    if (!native_fs::exists(root, error) || error || !native_fs::is_directory(root, error) || error)
        return Result::ErrorInvalidPath;

    std::size_t added = 0;
    for (const auto& entry : native_fs::recursive_directory_iterator(root, error)) {
        if (error)
            return Result::ErrorReadFile;
        if (!entry.is_regular_file(error) || error)
            continue;

        std::error_code rel_error;
        const native_fs::path relative = native_fs::relative(entry.path(), root, rel_error);
        if (rel_error || relative.empty())
            return Result::ErrorInvalidPath;

        std::string package_path = NormalizeSlashes(atom::PathToUtf8(relative));
        if (!prefix.empty())
            package_path = prefix + "/" + package_path;
        if (!pf2::IsValidPackagePath(package_path))
            return Result::ErrorInvalidPath;

        files_.push_back(PlannedFile{atom::PathToUtf8(entry.path()), std::move(package_path)});
        ++added;
    }
    return added > 0 ? Result::Success : Result::ErrorEmptyPackage;
}

auto PackageBuilder::PlannedFiles() const -> std::vector<std::string> {
    std::vector<std::string> paths;
    paths.reserve(files_.size());
    for (const auto& file : files_)
        paths.push_back(file.package_path);
    return paths;
}

auto PackageBuilder::Build(const std::string& output_utf8, const Config& config) -> Result {
    if (files_.empty())
        return Result::ErrorEmptyPackage;
    if (!atom::IsValidUtf8(output_utf8))
        return Result::ErrorInvalidPath;

    // ---- Plan validation: nothing is written before this completes. ----

    std::vector<AssetPlan> assets;
    assets.reserve(files_.size());
    {
        std::unordered_map<std::string, std::size_t> seen;
        for (const auto& file : files_) {
            if (!seen.emplace(file.package_path, assets.size()).second) {
                LOG_ERROR(atom::log::utilities::Packager,
                          "Duplicate package path: " + file.package_path);
                return Result::ErrorDuplicatePath;
            }
            AssetPlan plan{};
            plan.native_path = file.native_path;
            plan.package_path = file.package_path;
            plan.kind = InferKind(file.package_path);
            assets.push_back(std::move(plan));
        }
    }
    if (assets.size() > pf2::max_asset_count)
        return Result::ErrorInvalidPath;
    std::ranges::sort(assets, {}, &AssetPlan::package_path);

    // Directory tree. Root is index 0.
    std::vector<DirNode> dirs(1);
    std::unordered_map<std::string, std::uint32_t> dir_by_path;
    for (std::uint32_t i = 0; i < assets.size(); ++i) {
        const std::string parent{pf2::PackagePathParent(assets[i].package_path)};
        const std::uint32_t node = EnsureDirectory(dirs, dir_by_path, parent);
        // Temporarily stores the creation-order dir index; replaced after BFS.
        assets[i].parent_dir = node;
        dirs[node].assets.push_back(i);
    }
    if (dirs.size() > pf2::max_directory_count)
        return Result::ErrorInvalidPath;

    // BFS re-indexing so each node's children occupy a contiguous range.
    // Pass 1 assigns new indices in BFS order; pass 2 remaps parent/children
    // references through old->new and records contiguous child runs.
    {
        for (auto& node : dirs)
            std::ranges::sort(node.children, {}, [&](std::uint32_t child) { return dirs[child].name; });

        std::vector<std::uint32_t> bfs_order;
        bfs_order.reserve(dirs.size());
        std::vector<std::uint32_t> old_to_new(dirs.size(), 0);
        std::queue<std::uint32_t> queue;
        queue.push(pf2::root_directory);
        while (!queue.empty()) {
            const std::uint32_t old_index = queue.front();
            queue.pop();
            old_to_new[old_index] = static_cast<std::uint32_t>(bfs_order.size());
            bfs_order.push_back(old_index);
            for (const std::uint32_t child : dirs[old_index].children)
                queue.push(child);
        }

        std::vector<DirNode> ordered;
        ordered.reserve(dirs.size());
        for (const std::uint32_t old_index : bfs_order) {
            DirNode node = dirs[old_index];
            for (auto& child : node.children)
                child = old_to_new[child];
            if (node.parent != pf2::no_parent)
                node.parent = old_to_new[node.parent];
            node.first_child = node.children.empty() ? 0 : node.children.front();
            // BFS assigns children consecutively; assert the invariant holds.
            for (std::size_t k = 1; k < node.children.size(); ++k) {
                if (node.children[k] != node.children[k - 1] + 1)
                    return Result::ErrorInvalidPath;
            }
            ordered.push_back(std::move(node));
        }
        for (auto& asset : assets)
            asset.parent_dir = old_to_new[asset.parent_dir];
        dirs = std::move(ordered);
    }

    // Order assets so each directory owns a contiguous run, then record ranges.
    {
        std::vector<std::uint32_t> order(assets.size());
        for (std::uint32_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::ranges::sort(order, [&](std::uint32_t a, std::uint32_t b) {
            if (assets[a].parent_dir != assets[b].parent_dir)
                return assets[a].parent_dir < assets[b].parent_dir;
            return assets[a].package_path < assets[b].package_path;
        });
        std::vector<AssetPlan> sorted;
        sorted.reserve(assets.size());
        for (const std::uint32_t index : order)
            sorted.push_back(std::move(assets[index]));
        assets = std::move(sorted);

        for (auto& node : dirs) {
            node.first_asset = 0;
            node.assets.clear();
        }
        for (std::uint32_t i = 0; i < assets.size(); ++i)
            dirs[assets[i].parent_dir].assets.push_back(i);
        for (auto& node : dirs)
            node.first_asset = node.assets.empty() ? 0 : node.assets.front();
    }

    // ---- Data region: stream each asset, one stored chunk per asset. ----

    const native_fs::path output_path = atom::PathFromUtf8(output_utf8);
    native_fs::path temp_path = output_path;
    temp_path += ".tmp";

    std::error_code error;
    if (native_fs::exists(output_path, error) && !config.overwrite)
        return Result::ErrorWriteFailed;
    native_fs::remove(temp_path, error);

    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
        return Result::ErrorOpenOutput;

    // Reserve the fixed header.
    std::array<std::byte, pf2::header_size> header_block{};
    output.write(reinterpret_cast<const char*>(header_block.data()), header_block.size());

    std::uint64_t data_offset = pf2::header_size;
    std::vector<char> block(io_block_size);
    for (auto& asset : assets) {
        std::ifstream input(atom::PathFromUtf8(asset.native_path), std::ios::binary | std::ios::ate);
        if (!input.is_open()) {
            output.close();
            native_fs::remove(temp_path, error);
            return Result::ErrorReadFile;
        }
        const auto end = input.tellg();
        if (end < 0) {
            output.close();
            native_fs::remove(temp_path, error);
            return Result::ErrorReadFile;
        }
        asset.logical_size = static_cast<std::uint64_t>(end);
        asset.data_offset = data_offset;
        input.seekg(0, std::ios::beg);

        std::uint64_t hash = pf2::fnv_offset_basis;
        std::uint64_t remaining = asset.logical_size;
        while (remaining > 0) {
            const auto want = static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, io_block_size));
            input.read(block.data(), want);
            if (!input) {
                output.close();
                native_fs::remove(temp_path, error);
                return Result::ErrorReadFile;
            }
            output.write(block.data(), want);
            if (!output) {
                output.close();
                native_fs::remove(temp_path, error);
                return Result::ErrorWriteFailed;
            }
            hash = pf2::FnvaUpdate(hash, block.data(), static_cast<std::size_t>(want));
            remaining -= static_cast<std::uint64_t>(want);
            data_offset += static_cast<std::uint64_t>(want);
        }
        asset.content_hash = hash;

        if (config.verbose)
            LOG_INFO(atom::log::utilities::Packager,
                     "Packing(v2): " + asset.package_path + " (" + std::to_string(asset.logical_size) + " bytes)");
    }

    // ---- Manifest ----

    StringTable strings;
    std::vector<std::uint32_t> dir_name_refs;
    dir_name_refs.reserve(dirs.size());
    for (const auto& node : dirs)
        dir_name_refs.push_back(strings.Intern(node.name));
    std::vector<std::uint32_t> asset_path_refs;
    asset_path_refs.reserve(assets.size());
    for (const auto& asset : assets)
        asset_path_refs.push_back(strings.Intern(asset.package_path));

    if (strings.Count() > pf2::max_string_count || strings.BlobSize() > pf2::max_string_blob_size) {
        output.close();
        native_fs::remove(temp_path, error);
        return Result::ErrorInvalidPath;
    }

    std::vector<std::byte> string_section;
    AppendU32(string_section, strings.Count());
    for (std::uint32_t i = 0; i < strings.Count(); ++i) {
        AppendU32(string_section, strings.Offsets()[i]);
        AppendU32(string_section, strings.Lengths()[i]);
    }
    for (const auto& value : strings.Strings())
        AppendBytes(string_section, value.data(), value.size());

    std::vector<std::byte> chunk_section;
    AppendU32(chunk_section, static_cast<std::uint32_t>(assets.size()));
    for (const auto& asset : assets) {
        AppendU64(chunk_section, asset.data_offset);
        AppendU64(chunk_section, asset.logical_size); // stored == logical (store)
        AppendU64(chunk_section, asset.logical_size);
        AppendU64(chunk_section, asset.content_hash);
        AppendU16(chunk_section, pf2::compression_store);
        AppendU16(chunk_section, 0);
        AppendU32(chunk_section, 0);
    }

    std::vector<std::byte> asset_section;
    AppendU32(asset_section, static_cast<std::uint32_t>(assets.size()));
    for (std::uint32_t i = 0; i < assets.size(); ++i) {
        const auto& asset = assets[i];
        AppendU64(asset_section, pf2::Fnva(asset.package_path));
        AppendU32(asset_section, asset_path_refs[i]);
        AppendU32(asset_section, asset.parent_dir);
        AppendU16(asset_section, asset.kind);
        AppendU16(asset_section, 0);
        AppendU64(asset_section, asset.logical_size);
        AppendU64(asset_section, asset.content_hash);
        AppendU32(asset_section, i); // chunk_start: one chunk per asset
        AppendU32(asset_section, 1); // chunk_count
    }

    std::vector<std::byte> dir_section;
    AppendU32(dir_section, static_cast<std::uint32_t>(dirs.size()));
    for (std::uint32_t i = 0; i < dirs.size(); ++i) {
        const auto& node = dirs[i];
        AppendU32(dir_section, dir_name_refs[i]);
        AppendU32(dir_section, node.parent);
        AppendU32(dir_section, node.first_child);
        AppendU32(dir_section, static_cast<std::uint32_t>(node.children.size()));
        AppendU32(dir_section, node.first_asset);
        AppendU32(dir_section, static_cast<std::uint32_t>(node.assets.size()));
    }

    std::vector<std::byte> manifest;
    AppendBytes(manifest, pf2::manifest_magic, 4);
    constexpr std::uint32_t section_count = 4;
    AppendU32(manifest, section_count);

    const std::uint64_t directory_size = 4 + 4 + section_count * pf2::section_entry_size;
    const std::uint64_t string_offset = directory_size;
    const std::uint64_t chunk_offset = string_offset + string_section.size();
    const std::uint64_t asset_offset = chunk_offset + chunk_section.size();
    const std::uint64_t dir_offset = asset_offset + asset_section.size();

    AppendU32(manifest, pf2::section_kind::string_table);
    AppendU64(manifest, string_offset);
    AppendU64(manifest, string_section.size());
    AppendU32(manifest, pf2::section_kind::chunk_table);
    AppendU64(manifest, chunk_offset);
    AppendU64(manifest, chunk_section.size());
    AppendU32(manifest, pf2::section_kind::asset_table);
    AppendU64(manifest, asset_offset);
    AppendU64(manifest, asset_section.size());
    AppendU32(manifest, pf2::section_kind::directory_table);
    AppendU64(manifest, dir_offset);
    AppendU64(manifest, dir_section.size());

    manifest.insert(manifest.end(), string_section.begin(), string_section.end());
    manifest.insert(manifest.end(), chunk_section.begin(), chunk_section.end());
    manifest.insert(manifest.end(), asset_section.begin(), asset_section.end());
    manifest.insert(manifest.end(), dir_section.begin(), dir_section.end());

    if (manifest.size() > pf2::max_manifest_size) {
        output.close();
        native_fs::remove(temp_path, error);
        return Result::ErrorInvalidPath;
    }

    const std::uint64_t manifest_offset = data_offset;
    output.write(reinterpret_cast<const char*>(manifest.data()), static_cast<std::streamsize>(manifest.size()));
    if (!output) {
        output.close();
        native_fs::remove(temp_path, error);
        return Result::ErrorWriteFailed;
    }
    output.flush();
    const std::uint64_t total_size = manifest_offset + manifest.size();

    // ---- Header ----

    const std::uint64_t manifest_hash = pf2::Fnva(manifest.data(), manifest.size());

    // Deterministic UUID: derived from the manifest hash and asset count, so
    // identical inputs produce byte-identical packages.
    std::byte* uuid_out = header_block.data() + pf2::header_field::uuid;
    for (int i = 0; i < 16; ++i) {
        const auto shift = static_cast<unsigned>((i % 8) * 8);
        const auto source = i < 8 ? manifest_hash : manifest_hash ^ pf2::FnvaUpdate(pf2::fnv_offset_basis, &total_size, sizeof(total_size));
        uuid_out[i] = std::byte((source >> shift) & 0xFF);
    }

    std::memcpy(header_block.data() + pf2::header_field::magic, pf2::magic, 4);
    pf2::WriteU16(header_block.data() + pf2::header_field::version, pf2::version);
    pf2::WriteU16(header_block.data() + pf2::header_field::flags, 0);
    pf2::WriteU64(header_block.data() + pf2::header_field::data_offset, pf2::header_size);
    pf2::WriteU64(header_block.data() + pf2::header_field::manifest_offset, manifest_offset);
    pf2::WriteU64(header_block.data() + pf2::header_field::manifest_size, manifest.size());
    pf2::WriteU64(header_block.data() + pf2::header_field::manifest_hash, manifest_hash);
    pf2::WriteU64(header_block.data() + pf2::header_field::header_hash,
                  pf2::Fnva(header_block.data(), pf2::header_hash_coverage));

    output.seekp(0, std::ios::beg);
    output.write(reinterpret_cast<const char*>(header_block.data()), header_block.size());
    if (!output) {
        output.close();
        native_fs::remove(temp_path, error);
        return Result::ErrorWriteFailed;
    }
    output.close();

    // ---- Verification: re-read the temp package before replacing. ----

    {
        std::ifstream verify(temp_path, std::ios::binary);
        if (!verify.is_open()) {
            native_fs::remove(temp_path, error);
            return Result::ErrorVerificationFailed;
        }
        std::array<std::byte, pf2::header_size> header_check{};
        verify.read(reinterpret_cast<char*>(header_check.data()), header_check.size());
        if (!verify) {
            native_fs::remove(temp_path, error);
            return Result::ErrorVerificationFailed;
        }
        const std::uint64_t stored_header_hash = pf2::ReadU64(header_check.data() + pf2::header_field::header_hash);
        const std::uint64_t computed_header_hash = pf2::Fnva(header_check.data(), pf2::header_hash_coverage);
        if (stored_header_hash != computed_header_hash) {
            native_fs::remove(temp_path, error);
            return Result::ErrorVerificationFailed;
        }

        std::vector<std::byte> manifest_check(manifest.size());
        verify.seekg(static_cast<std::streamoff>(manifest_offset), std::ios::beg);
        verify.read(reinterpret_cast<char*>(manifest_check.data()), static_cast<std::streamsize>(manifest_check.size()));
        if (!verify || pf2::Fnva(manifest_check.data(), manifest_check.size()) != manifest_hash) {
            native_fs::remove(temp_path, error);
            return Result::ErrorVerificationFailed;
        }

        // Re-hash every chunk payload from disk.
        for (const auto& asset : assets) {
            verify.seekg(static_cast<std::streamoff>(asset.data_offset), std::ios::beg);
            std::uint64_t hash = pf2::fnv_offset_basis;
            std::uint64_t remaining = asset.logical_size;
            while (remaining > 0) {
                const auto want = static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, io_block_size));
                verify.read(block.data(), want);
                if (!verify) {
                    native_fs::remove(temp_path, error);
                    return Result::ErrorVerificationFailed;
                }
                hash = pf2::FnvaUpdate(hash, block.data(), static_cast<std::size_t>(want));
                remaining -= static_cast<std::uint64_t>(want);
            }
            if (hash != asset.content_hash) {
                native_fs::remove(temp_path, error);
                return Result::ErrorVerificationFailed;
            }
        }
    }

    // ---- Atomic replace ----

    if (native_fs::exists(output_path, error)) {
        // std::filesystem::rename cannot overwrite on Windows; remove the old
        // package only after the new one is fully written and verified.
        native_fs::remove(output_path, error);
        if (error)
            return Result::ErrorWriteFailed;
    }
    native_fs::rename(temp_path, output_path, error);
    if (error)
        return Result::ErrorWriteFailed;

    LOG_INFO(atom::log::utilities::Packager,
             "Packing(v2) completed: " + output_utf8 + " (" + std::to_string(assets.size()) + " assets, " +
                 std::to_string(total_size) + " bytes)");
    return Result::Success;
}

} // namespace atom::tools
