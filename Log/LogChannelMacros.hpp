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

// Channel-domain definition machinery (ATOM_DEFINE_CHANNELS and the FOR_EACH
// infrastructure underneath).
// Design and usage: see Log/Doc/LogSystem.md.

// FOR_EACH infrastructure (up to 64 channels per domain)
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
#define ATOM_FE_17(M, a, ...) M(a) ATOM_FE_16(M, __VA_ARGS__)
#define ATOM_FE_18(M, a, ...) M(a) ATOM_FE_17(M, __VA_ARGS__)
#define ATOM_FE_19(M, a, ...) M(a) ATOM_FE_18(M, __VA_ARGS__)
#define ATOM_FE_20(M, a, ...) M(a) ATOM_FE_19(M, __VA_ARGS__)
#define ATOM_FE_21(M, a, ...) M(a) ATOM_FE_20(M, __VA_ARGS__)
#define ATOM_FE_22(M, a, ...) M(a) ATOM_FE_21(M, __VA_ARGS__)
#define ATOM_FE_23(M, a, ...) M(a) ATOM_FE_22(M, __VA_ARGS__)
#define ATOM_FE_24(M, a, ...) M(a) ATOM_FE_23(M, __VA_ARGS__)
#define ATOM_FE_25(M, a, ...) M(a) ATOM_FE_24(M, __VA_ARGS__)
#define ATOM_FE_26(M, a, ...) M(a) ATOM_FE_25(M, __VA_ARGS__)
#define ATOM_FE_27(M, a, ...) M(a) ATOM_FE_26(M, __VA_ARGS__)
#define ATOM_FE_28(M, a, ...) M(a) ATOM_FE_27(M, __VA_ARGS__)
#define ATOM_FE_29(M, a, ...) M(a) ATOM_FE_28(M, __VA_ARGS__)
#define ATOM_FE_30(M, a, ...) M(a) ATOM_FE_29(M, __VA_ARGS__)
#define ATOM_FE_31(M, a, ...) M(a) ATOM_FE_30(M, __VA_ARGS__)
#define ATOM_FE_32(M, a, ...) M(a) ATOM_FE_31(M, __VA_ARGS__)
#define ATOM_FE_33(M, a, ...) M(a) ATOM_FE_32(M, __VA_ARGS__)
#define ATOM_FE_34(M, a, ...) M(a) ATOM_FE_33(M, __VA_ARGS__)
#define ATOM_FE_35(M, a, ...) M(a) ATOM_FE_34(M, __VA_ARGS__)
#define ATOM_FE_36(M, a, ...) M(a) ATOM_FE_35(M, __VA_ARGS__)
#define ATOM_FE_37(M, a, ...) M(a) ATOM_FE_36(M, __VA_ARGS__)
#define ATOM_FE_38(M, a, ...) M(a) ATOM_FE_37(M, __VA_ARGS__)
#define ATOM_FE_39(M, a, ...) M(a) ATOM_FE_38(M, __VA_ARGS__)
#define ATOM_FE_40(M, a, ...) M(a) ATOM_FE_39(M, __VA_ARGS__)
#define ATOM_FE_41(M, a, ...) M(a) ATOM_FE_40(M, __VA_ARGS__)
#define ATOM_FE_42(M, a, ...) M(a) ATOM_FE_41(M, __VA_ARGS__)
#define ATOM_FE_43(M, a, ...) M(a) ATOM_FE_42(M, __VA_ARGS__)
#define ATOM_FE_44(M, a, ...) M(a) ATOM_FE_43(M, __VA_ARGS__)
#define ATOM_FE_45(M, a, ...) M(a) ATOM_FE_44(M, __VA_ARGS__)
#define ATOM_FE_46(M, a, ...) M(a) ATOM_FE_45(M, __VA_ARGS__)
#define ATOM_FE_47(M, a, ...) M(a) ATOM_FE_46(M, __VA_ARGS__)
#define ATOM_FE_48(M, a, ...) M(a) ATOM_FE_47(M, __VA_ARGS__)
#define ATOM_FE_49(M, a, ...) M(a) ATOM_FE_48(M, __VA_ARGS__)
#define ATOM_FE_50(M, a, ...) M(a) ATOM_FE_49(M, __VA_ARGS__)
#define ATOM_FE_51(M, a, ...) M(a) ATOM_FE_50(M, __VA_ARGS__)
#define ATOM_FE_52(M, a, ...) M(a) ATOM_FE_51(M, __VA_ARGS__)
#define ATOM_FE_53(M, a, ...) M(a) ATOM_FE_52(M, __VA_ARGS__)
#define ATOM_FE_54(M, a, ...) M(a) ATOM_FE_53(M, __VA_ARGS__)
#define ATOM_FE_55(M, a, ...) M(a) ATOM_FE_54(M, __VA_ARGS__)
#define ATOM_FE_56(M, a, ...) M(a) ATOM_FE_55(M, __VA_ARGS__)
#define ATOM_FE_57(M, a, ...) M(a) ATOM_FE_56(M, __VA_ARGS__)
#define ATOM_FE_58(M, a, ...) M(a) ATOM_FE_57(M, __VA_ARGS__)
#define ATOM_FE_59(M, a, ...) M(a) ATOM_FE_58(M, __VA_ARGS__)
#define ATOM_FE_60(M, a, ...) M(a) ATOM_FE_59(M, __VA_ARGS__)
#define ATOM_FE_61(M, a, ...) M(a) ATOM_FE_60(M, __VA_ARGS__)
#define ATOM_FE_62(M, a, ...) M(a) ATOM_FE_61(M, __VA_ARGS__)
#define ATOM_FE_63(M, a, ...) M(a) ATOM_FE_62(M, __VA_ARGS__)
#define ATOM_FE_64(M, a, ...) M(a) ATOM_FE_63(M, __VA_ARGS__)

#define ATOM_FE_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20,    \
                          _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38,      \
                          _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56,      \
                          _57, _58, _59, _60, _61, _62, _63, _64, NAME, ...)                                             \
    NAME

#define ATOM_FOR_EACH(M, ...)                                                                                          \
    ATOM_FE_GET_MACRO(__VA_ARGS__, ATOM_FE_64, ATOM_FE_63, ATOM_FE_62, ATOM_FE_61, ATOM_FE_60, ATOM_FE_59, ATOM_FE_58, \
                      ATOM_FE_57, ATOM_FE_56, ATOM_FE_55, ATOM_FE_54, ATOM_FE_53, ATOM_FE_52, ATOM_FE_51, ATOM_FE_50,   \
                      ATOM_FE_49, ATOM_FE_48, ATOM_FE_47, ATOM_FE_46, ATOM_FE_45, ATOM_FE_44, ATOM_FE_43, ATOM_FE_42,   \
                      ATOM_FE_41, ATOM_FE_40, ATOM_FE_39, ATOM_FE_38, ATOM_FE_37, ATOM_FE_36, ATOM_FE_35, ATOM_FE_34,   \
                      ATOM_FE_33, ATOM_FE_32, ATOM_FE_31, ATOM_FE_30, ATOM_FE_29, ATOM_FE_28, ATOM_FE_27, ATOM_FE_26,   \
                      ATOM_FE_25, ATOM_FE_24, ATOM_FE_23, ATOM_FE_22, ATOM_FE_21, ATOM_FE_20, ATOM_FE_19, ATOM_FE_18,   \
                      ATOM_FE_17, ATOM_FE_16, ATOM_FE_15, ATOM_FE_14, ATOM_FE_13, ATOM_FE_12, ATOM_FE_11, ATOM_FE_10,   \
                      ATOM_FE_9, ATOM_FE_8, ATOM_FE_7, ATOM_FE_6, ATOM_FE_5, ATOM_FE_4, ATOM_FE_3, ATOM_FE_2,          \
                      ATOM_FE_1, ATOM_FE_0)(M, __VA_ARGS__)

// Channel list element unpacking
#define ATOM_CHANNEL_ENUM_MEMBER(pair) ATOM_CHANNEL_ENUM_IMPL pair
#define ATOM_CHANNEL_ENUM_IMPL(cppName, displayName) cppName,
#define ATOM_CHANNEL_NAME_ENTRY(pair) ATOM_CHANNEL_NAME_IMPL pair
#define ATOM_CHANNEL_NAME_IMPL(cppName, displayName) displayName,

// Channel domain definition: write channels + one-line injection
#define ATOM_DEFINE_CHANNELS(DomainNs, EnumName, DomainPrefix, ...)                                                   \
    namespace DomainNs {                                                                                               \
    enum class EnumName {                                                                                              \
        ATOM_FOR_EACH(ATOM_CHANNEL_ENUM_MEMBER, __VA_ARGS__)                                                           \
        COUNT                                                                                                          \
    };                                                                                                                 \
    inline constexpr std::string_view k##EnumName##Names[] = {                                                         \
        ATOM_FOR_EACH(ATOM_CHANNEL_NAME_ENTRY, __VA_ARGS__)                                                            \
    };                                                                                                                 \
    [[nodiscard]] constexpr auto GetChannelName(const EnumName channel) -> std::string_view {                          \
        if (static_cast<std::size_t>(channel) >= std::size(k##EnumName##Names)) {                                      \
            return "Unknown";                                                                                          \
        }                                                                                                              \
        return k##EnumName##Names[static_cast<std::size_t>(channel)];                                                  \
    }                                                                                                                  \
    [[nodiscard]] constexpr auto GetChannelPrefix(const EnumName) -> std::string_view { return DomainPrefix; }         \
    } // namespace DomainNs

#endif // ATOM_LOGCHANNELMACROS_HPP
