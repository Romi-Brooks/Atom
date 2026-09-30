/**
  * @file           : PackageFormat.hpp
  * @author         : Romi Brooks
  * @brief          : APKG container layout shared by the reader and the writer.
  * @attention      : Header-only, so Atom_Packager can include it without
  *                   linking Atom_FS. This is the single definition of the
  *                   on-disk contract; the reader (PackageFileSystem) and the
  *                   writer (Packager) must never restate these values.
  * @date           : 2026/10/1
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_FILESYSTEM_PACKAGE_FORMAT_HPP
#define ATOM_FILESYSTEM_PACKAGE_FORMAT_HPP

#include <cstdint>

namespace atom::fs::package_format {

// Little-endian file magic, written first and verified on load.
inline constexpr char magic[4] = {'A', 'P', 'K', 'G'};
// Container version, written right after the magic. Bump it for any layout
// change a v1 reader cannot ignore.
inline constexpr std::uint16_t version = 1;

// Validation limits the reader enforces before trusting a header.
inline constexpr std::uint64_t min_header_size = 10; // magic + version + file count
inline constexpr std::uint32_t max_file_count = 1'000'000;
inline constexpr std::uint16_t max_name_length = 4096;

} // namespace atom::fs::package_format

#endif // ATOM_FILESYSTEM_PACKAGE_FORMAT_HPP
