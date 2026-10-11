/**
  * @file           : PackageFormat.hpp
  * @brief          : APKG container layout shared by the reader and the writer.
  * @attention      : Header-only, so Atom_Packager can include it without
  *                   linking Atom_FS. This is the single definition of the
  *                   on-disk contract; the reader (PackageFileSystem) and the
  *                   writer (PackageBuilder) must never restate these values.
  *
  * Layout overview (all integers little-endian, no padding beyond what the
  * fixed sizes below imply — fields are encoded byte-by-byte, never memcpy'd
  * from structs):
  *
  *   [0, 64)                 fixed header
  *   [data_offset, ...)      chunk payload region (concatenated chunk bytes)
  *   [manifest_offset, ...)  manifest: "APKM" + section directory + sections
  *
  * The manifest holds four sections addressed by a directory, so future
  * sections (tags, dependencies, cook metadata) can be appended without
  * breaking this reader: unknown section kinds are skipped.
  *
  * Integrity: header_hash covers the first 56 header bytes; manifest_hash in
  * the header covers the whole manifest region; each chunk and each asset
  * carries a content FNV-1a-64. Hash comparison must be followed by a full
  * path comparison on lookup so a collision can never alias two assets.
  * @date           : 2026/10/7
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_FILESYSTEM_PACKAGE_FORMAT_HPP
#define ATOM_FILESYSTEM_PACKAGE_FORMAT_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace atom::fs::package_format {

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

// Little-endian file magic, written first and verified on load.
inline constexpr char magic[4] = {'A', 'P', 'K', 'G'};
// Container version, written right after the magic and verified on load.
inline constexpr std::uint16_t version = 2;
// Manifest magic, first four bytes of the manifest region.
inline constexpr char manifest_magic[4] = {'A', 'P', 'K', 'M'};

// ---------------------------------------------------------------------------
// Fixed header (64 bytes)
// ---------------------------------------------------------------------------

inline constexpr std::uint64_t header_size = 64;
inline constexpr std::uint64_t header_hash_coverage = 56; // bytes [0, 56) feed header_hash

namespace header_field {
inline constexpr std::uint64_t magic = 0;           // char[4]
inline constexpr std::uint64_t version = 4;         // u16
inline constexpr std::uint64_t flags = 6;           // u16, reserved, must be 0
inline constexpr std::uint64_t uuid = 8;            // byte[16], random per build
inline constexpr std::uint64_t data_offset = 24;    // u64, absolute offset of chunk region
inline constexpr std::uint64_t manifest_offset = 32;// u64, absolute offset of manifest
inline constexpr std::uint64_t manifest_size = 40;  // u64, manifest byte count
inline constexpr std::uint64_t manifest_hash = 48;  // u64, FNV-1a-64 over manifest
inline constexpr std::uint64_t header_hash = 56;    // u64, FNV-1a-64 over bytes [0, 56)
} // namespace header_field

// ---------------------------------------------------------------------------
// Manifest: magic(4) + section_count u32 + entries + section payloads
// ---------------------------------------------------------------------------

// Section directory entry: kind u32, offset u64 (relative to manifest start),
// size u64. 20 bytes.
inline constexpr std::uint64_t section_entry_size = 20;

namespace section_kind {
inline constexpr std::uint32_t string_table = 1;
inline constexpr std::uint32_t chunk_table = 2;
inline constexpr std::uint32_t asset_table = 3;
inline constexpr std::uint32_t directory_table = 4;
} // namespace section_kind

// String table section: count u32, then count x (offset u32, length u32)
// relative to the blob that follows the index. Index 0 is always "".
inline constexpr std::uint64_t string_ref_size = 8;

// Chunk table entry (40 bytes):
//   data_offset u64  absolute file offset of the stored bytes
//   stored_size u64  bytes on disk (== logical_size while compression==store)
//   logical_size u64 bytes after decompression
//   content_hash u64 FNV-1a-64 over the logical bytes
//   compression u16  0 = store; anything else is rejected by this reader
//   flags u16        reserved, must be 0
//   reserved u32
inline constexpr std::uint64_t chunk_entry_size = 40;
inline constexpr std::uint16_t compression_store = 0;

// Asset table entry (44 bytes):
//   path_hash u64    FNV-1a-64 over the normalized package path bytes
//   path_str u32     string table index of the full path
//   parent_dir u32   directory table index owning this asset
//   kind u16         mirrors atom::asset::AssetKind values (see below)
//   flags u16        reserved, must be 0
//   logical_size u64 total asset bytes (sum of its chunks' logical sizes)
//   content_hash u64 FNV-1a-64 over the whole logical asset
//   chunk_start u32  first chunk table index
//   chunk_count u32  chunk run length (chunks are contiguous per asset)
inline constexpr std::uint64_t asset_entry_size = 44;

// Asset kind values mirror atom::asset::AssetKind (uint8_t) so a package can
// be classified without linking Atom_Assets. Keep in sync manually; the
// writer asserts on write.
namespace asset_kind {
inline constexpr std::uint16_t unknown = 0;
inline constexpr std::uint16_t binary = 1;
inline constexpr std::uint16_t texture = 2;
inline constexpr std::uint16_t audio = 3;
inline constexpr std::uint16_t shader = 4;
inline constexpr std::uint16_t config = 5;
inline constexpr std::uint16_t script = 6;
inline constexpr std::uint16_t font = 7;
inline constexpr std::uint16_t model = 8;
inline constexpr std::uint16_t scene = 9;
inline constexpr std::uint16_t video = 10;
} // namespace asset_kind

// Human-readable name for an asset kind, shared by the writer, the reader and
// tooling (e.g. the packager GUI). Falls back to "Unknown" for out-of-range.
[[nodiscard]] inline auto KindName(std::uint16_t kind) -> const char* {
    switch (kind) {
    case asset_kind::texture: return "Texture";
    case asset_kind::audio: return "Audio";
    case asset_kind::shader: return "Shader";
    case asset_kind::config: return "Config";
    case asset_kind::script: return "Script";
    case asset_kind::font: return "Font";
    case asset_kind::model: return "Model";
    case asset_kind::scene: return "Scene";
    case asset_kind::video: return "Video";
    case asset_kind::binary: return "Binary";
    default: return "Unknown";
    }
}

// Directory table entry (24 bytes):
//   name_str u32     string table index of the bare directory name ("" = root)
//   parent u32       directory index of the parent (root uses no_parent)
//   first_child u32  first child directory index; children are contiguous
//   child_count u32
//   first_asset u32  first asset index owned by this directory; contiguous
//   asset_count u32
inline constexpr std::uint64_t directory_entry_size = 24;
inline constexpr std::uint32_t no_parent = 0xFFFF'FFFFu;
inline constexpr std::uint32_t root_directory = 0;

// ---------------------------------------------------------------------------
// Validation limits the reader enforces before trusting a manifest
// ---------------------------------------------------------------------------

inline constexpr std::uint64_t max_manifest_size = 64ull * 1024 * 1024;
inline constexpr std::uint64_t max_string_blob_size = 256ull * 1024 * 1024;
inline constexpr std::uint32_t max_asset_count = 1'000'000;
inline constexpr std::uint32_t max_chunk_count = 4'000'000;
inline constexpr std::uint32_t max_directory_count = 1'000'000;
inline constexpr std::uint32_t max_string_count = 4'000'000;
inline constexpr std::uint64_t max_path_length = 4096;
inline constexpr std::uint32_t max_section_count = 64;

// ---------------------------------------------------------------------------
// FNV-1a-64 — the only hash the container uses
// ---------------------------------------------------------------------------

inline constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ull;
inline constexpr std::uint64_t fnv_prime = 1099511628211ull;

[[nodiscard]] inline auto FnvaUpdate(std::uint64_t hash, const void* data, std::size_t size) -> std::uint64_t {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= fnv_prime;
    }
    return hash;
}

[[nodiscard]] inline auto Fnva(const void* data, std::size_t size) -> std::uint64_t {
    return FnvaUpdate(fnv_offset_basis, data, size);
}

[[nodiscard]] inline auto Fnva(std::string_view text) -> std::uint64_t {
    return Fnva(text.data(), text.size());
}

// ---------------------------------------------------------------------------
// Little-endian field codec — the only sanctioned way to (de)serialize
// ---------------------------------------------------------------------------

inline void WriteU16(std::byte* out, std::uint16_t value) {
    out[0] = std::byte(value & 0xFF);
    out[1] = std::byte((value >> 8) & 0xFF);
}

inline void WriteU32(std::byte* out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i)
        out[i] = std::byte((value >> (8 * i)) & 0xFF);
}

inline void WriteU64(std::byte* out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i)
        out[i] = std::byte((value >> (8 * i)) & 0xFF);
}

[[nodiscard]] inline auto ReadU16(const std::byte* in) -> std::uint16_t {
    return static_cast<std::uint16_t>(std::to_integer<unsigned>(in[0]) |
                                      (std::to_integer<unsigned>(in[1]) << 8));
}

[[nodiscard]] inline auto ReadU32(const std::byte* in) -> std::uint32_t {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value |= std::to_integer<std::uint32_t>(in[i]) << (8 * i);
    return value;
}

[[nodiscard]] inline auto ReadU64(const std::byte* in) -> std::uint64_t {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value |= std::to_integer<std::uint64_t>(in[i]) << (8 * i);
    return value;
}

// ---------------------------------------------------------------------------
// Package path validation — shared so writer and reader agree byte-for-byte
// ---------------------------------------------------------------------------

// A normalized package path: UTF-8, '/'-separated, no leading/trailing '/',
// no empty segment, no '.' or '..', no backslash, no NUL, no host-absolute
// prefix. Directories are represented implicitly by path prefixes.
[[nodiscard]] inline auto IsValidPackagePath(std::string_view path) -> bool {
    if (path.empty() || path.size() > max_path_length)
        return false;
    if (path.front() == '/' || path.back() == '/')
        return false;
    if (path.find('\\') != std::string_view::npos || path.find('\0') != std::string_view::npos)
        return false;
    if (path.find("//") != std::string_view::npos)
        return false;
    // Reject drive letters and host-absolute prefixes ("C:", "C:/").
    if (path.size() >= 2 && path[1] == ':')
        return false;

    std::size_t segment_start = 0;
    while (segment_start <= path.size()) {
        const std::size_t slash = path.find('/', segment_start);
        const std::size_t segment_end = slash == std::string_view::npos ? path.size() : slash;
        const std::string_view segment = path.substr(segment_start, segment_end - segment_start);
        if (segment.empty() || segment == "." || segment == "..")
            return false;
        if (slash == std::string_view::npos)
            break;
        segment_start = slash + 1;
    }
    return true;
}

// The bare filename of a package path (text after the last '/').
[[nodiscard]] inline auto PackagePathName(std::string_view path) -> std::string_view {
    const std::size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// The parent directory of a package path, "" for top-level entries.
[[nodiscard]] inline auto PackagePathParent(std::string_view path) -> std::string_view {
    const std::size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view{} : path.substr(0, slash);
}

} // namespace atom::fs::package_format

#endif // ATOM_FILESYSTEM_PACKAGE_FORMAT_HPP
