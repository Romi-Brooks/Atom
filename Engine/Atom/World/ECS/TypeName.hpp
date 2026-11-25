/**
 * @file           : TypeName.hpp
 * @brief          : A readable name for a type, for diagnostics and error details.
 *
 * `typeid(T).name()` is not usable for messages: GCC and Clang return a mangled
 * symbol ("N4atom3ecs8PositionE") and MSVC returns "struct atom::ecs::Position".
 * Neither is something a log line should show a person. This reads the name the
 * compiler already spells out in its own function signature instead, which is
 * readable on all three toolchains and costs nothing at run time.
 *
 * The result is a `string_view` into a static string literal, so it is valid for
 * the lifetime of the program and never allocates.
 *
 * Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ECS_TYPE_NAME_HPP
#define ATOM_ECS_TYPE_NAME_HPP

#include <string_view>

namespace atom::ecs {

#if defined (_MSC_VER) && !defined (__clang__)
#define ATOM_ECS_PRETTY_FUNCTION __FUNCSIG__
#else
#define ATOM_ECS_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

// Not for direct use: the raw compiler spelling, which the parser below trims.
template <typename T> constexpr auto RawTypeName() noexcept -> std::string_view {
    return ATOM_ECS_PRETTY_FUNCTION;
}

/**
 * @brief Readable name of T, e.g. "Position" for atom::ecs::Position.
 *
 * The two branches exist because the toolchains spell their signatures
 * differently:
 *
 *   GCC/Clang: "constexpr std::string_view atom::ecs::RawTypeName() [with T = Position;
 *               std::string_view = std::basic_string_view<char>]"
 *   MSVC:      "class std::basic_string_view<char,...> __cdecl atom::ecs::RawTypeName<
 *               struct atom::ecs::Position>(void)"
 *
 * A name that cannot be parsed is returned as-is rather than producing an empty
 * string, so a future toolchain change degrades to "ugly but present" instead of
 * silently losing the type from a diagnostic.
**/
template <typename T> constexpr auto TypeName() noexcept -> std::string_view {
    constexpr std::string_view raw = RawTypeName<T>();

#if defined (_MSC_VER) && !defined (__clang__)
    constexpr std::string_view marker = "RawTypeName<";
    const std::size_t start = raw.find(marker);
    if (start == std::string_view::npos)
        return raw;

    std::size_t begin = start + marker.size();
    const std::size_t end = raw.find('>', begin);
    if (end == std::string_view::npos)
        return raw;
    std::string_view name = raw.substr(begin, end - begin);

    // MSVC prefixes the elaborated type specifier; drop it. Checked after a space
    // so a type whose own name ends in "struct" is not damaged.
    for (const std::string_view prefix : {"struct ", "class ", "enum ", "union "}) {
        if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0) {
            name.remove_prefix(prefix.size());
            break;
        }
    }
    return name;
#else
    constexpr std::string_view marker = "T = ";
    // Searched from the right so a type name that itself contains "T = " cannot
    // move the match.
    const std::size_t start = raw.rfind(marker);
    if (start == std::string_view::npos)
        return raw;

    const std::size_t begin = start + marker.size();
    // The type ends at whichever of ";" (another template parameter follows) or
    // "]" (T was the last one) comes first.
    const std::size_t semicolon = raw.find(';', begin);
    const std::size_t bracket = raw.find(']', begin);
    const std::size_t end = (semicolon == std::string_view::npos)   ? bracket
                            : (bracket == std::string_view::npos)   ? semicolon
                            : (semicolon < bracket ? semicolon : bracket);
    if (end == std::string_view::npos)
        return raw;
    return raw.substr(begin, end - begin);
#endif
}

#undef ATOM_ECS_PRETTY_FUNCTION

} // namespace atom::ecs

#endif // ATOM_ECS_TYPE_NAME_HPP
