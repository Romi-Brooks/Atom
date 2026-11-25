/**
  * @file           : LogChannelMacros.hpp
  * @author         : Romi Brooks
  * @brief          : Channel-domain definition machinery
  * @attention      : Included by LogSystem.hpp; can also be included standalone
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_LOGCHANNELMACROS_HPP
#define ATOM_LOGCHANNELMACROS_HPP

// Standard Library
#include <cstddef>
#include <iterator>
#include <string_view>

#include <Atom/Build/CoreExport.hpp>

#ifdef ATOM_LOGSYSTEM_HPP
namespace atom {

// Runtime metadata registration used by the debugger. This declaration is
// intentionally ImGui-free; the implementation lives in Atom_Log.
ATOM_CORE_API void RegisterLogChannelDomain(std::string_view prefix, const std::string_view* names, std::size_t count);

} // namespace atom

// The generated companion symbols (channel_names / channel_registered) are
// snake_case like every other value in the engine, which means they are not
// uniquified by the enum name. That is safe because a channel domain owns its
// namespace (see CODING_STANDARD.md 3.1.1): one namespace holds exactly one
// ATOM_DEFINE_CHANNELS block.
#define ATOM_CHANNEL_RUNTIME_REGISTRATION(EnumName, DomainPrefix)                                                     \
    inline const bool channel_registered = [] {                                                                     \
        ::atom::RegisterLogChannelDomain(DomainPrefix, channel_names, std::size(channel_names));                     \
        return true;                                                                                                 \
    }();
#else
#define ATOM_CHANNEL_RUNTIME_REGISTRATION(EnumName, DomainPrefix)
#endif

// Channel-domain definition machinery (ATOM_DEFINE_CHANNELS and the FOR_EACH
// infrastructure underneath).
// Design and usage: see Log/Doc/LogSystem.md.

// FOR_EACH infrastructure (up to 16 channels per domain). The dispatcher is
// deliberately short: MSVC's traditional preprocessor mis-handles the wider
// 64-way form in this project, while 16 covers every engine and sample domain.
#define ATOM_FE_0(M, ...)
#define ATOM_FE_1(M, a) M(a)
#define ATOM_FE_2(M, a, ...) M(a) ATOM_FE_1(M, __VA_ARGS__)
#define ATOM_FE_3(M, a, ...) M(a) ATOM_FE_2(M, __VA_ARGS__)
#define ATOM_FE_4(M, a, ...) M(a) ATOM_FE_3(M, __VA_ARGS__)
#define ATOM_FE_5(M, a, ...) M(a) ATOM_FE_4(M, __VA_ARGS__)
#define ATOM_FE_6(M, a, ...) M(a) ATOM_FE_5(M, __VA_ARGS__)
#define ATOM_FE_7(M, a, ...) M(a) ATOM_FE_6(M, __VA_ARGS__)
#define ATOM_FE_8(M, a, ...) M(a) ATOM_FE_7(M, __VA_ARGS__)
#define ATOM_FE_9(M, a, ...) M(a) ATOM_FE_8(M, __VA_ARGS__)
#define ATOM_FE_10(M, a, ...) M(a) ATOM_FE_9(M, __VA_ARGS__)
#define ATOM_FE_11(M, a, ...) M(a) ATOM_FE_10(M, __VA_ARGS__)
#define ATOM_FE_12(M, a, ...) M(a) ATOM_FE_11(M, __VA_ARGS__)
#define ATOM_FE_13(M, a, ...) M(a) ATOM_FE_12(M, __VA_ARGS__)
#define ATOM_FE_14(M, a, ...) M(a) ATOM_FE_13(M, __VA_ARGS__)
#define ATOM_FE_15(M, a, ...) M(a) ATOM_FE_14(M, __VA_ARGS__)
#define ATOM_FE_16(M, a, ...) M(a) ATOM_FE_15(M, __VA_ARGS__)

#define ATOM_FE_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, NAME, ...) NAME

#define ATOM_FOR_EACH(M, ...)                                                                                          \
    ATOM_FE_GET_MACRO(__VA_ARGS__, ATOM_FE_16, ATOM_FE_15, ATOM_FE_14, ATOM_FE_13, ATOM_FE_12, ATOM_FE_11, ATOM_FE_10, \
                      ATOM_FE_9, ATOM_FE_8, ATOM_FE_7, ATOM_FE_6, ATOM_FE_5, ATOM_FE_4, ATOM_FE_3, ATOM_FE_2,          \
                      ATOM_FE_1, ATOM_FE_0)(M, __VA_ARGS__)

// Channel list element unpacking
#define ATOM_CHANNEL_ENUM_MEMBER(pair) ATOM_CHANNEL_ENUM_IMPL pair
#define ATOM_CHANNEL_ENUM_IMPL(cppName, displayName) cppName,
#define ATOM_CHANNEL_NAME_ENTRY(pair) ATOM_CHANNEL_NAME_IMPL pair
#define ATOM_CHANNEL_NAME_IMPL(cppName, displayName) displayName,
#define ATOM_CHANNEL_VALUE_ENTRY(pair) ATOM_CHANNEL_VALUE_IMPL pair
#define ATOM_CHANNEL_VALUE_IMPL(cppName, displayName) inline constexpr Channel cppName = Channel::cppName;

// Channel-domain definition: expose values directly under their domain,
// e.g. atom::log::audio::Music. The Channel enum remains the value type used by
// the generic resolver and is intentionally an implementation detail of the
// public spelling.
#define ATOM_DEFINE_CHANNELS(DomainNs, EnumName, DomainPrefix, ...)                                                   \
    namespace DomainNs {                                                                                               \
    enum class EnumName {                                                                                              \
        ATOM_FOR_EACH(ATOM_CHANNEL_ENUM_MEMBER, __VA_ARGS__)                                                           \
        COUNT                                                                                                          \
    };                                                                                                                 \
    ATOM_FOR_EACH(ATOM_CHANNEL_VALUE_ENTRY, __VA_ARGS__)                                                              \
    inline constexpr std::string_view channel_names[] = {                                                             \
        ATOM_FOR_EACH(ATOM_CHANNEL_NAME_ENTRY, __VA_ARGS__)                                                            \
    };                                                                                                                 \
    ATOM_CHANNEL_RUNTIME_REGISTRATION(EnumName, DomainPrefix)                                                         \
    [[nodiscard]] constexpr auto GetChannelName(const EnumName channel) -> std::string_view {                          \
        if (static_cast<std::size_t>(channel) >= std::size(channel_names)) {                                           \
            return "Unknown";                                                                                          \
        }                                                                                                              \
        return channel_names[static_cast<std::size_t>(channel)];                                                       \
    }                                                                                                                  \
    [[nodiscard]] constexpr auto GetChannelPrefix(const EnumName) -> std::string_view { return DomainPrefix; }         \
    } // namespace DomainNs

#endif // ATOM_LOGCHANNELMACROS_HPP
