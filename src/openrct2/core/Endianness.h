/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#include <bit>

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define RCT2_BIG_ENDIAN 1
#else
#    define RCT2_BIG_ENDIAN 0
#endif

#if RCT2_BIG_ENDIAN
#    define SWAP_IF_BE(x) ByteSwapBE(x)
#else
#    define SWAP_IF_BE(x) (x)
#endif

#if RCT2_BIG_ENDIAN
#    define SWAP_IF_LE(x) (x)
#else
#    define SWAP_IF_LE(x) ByteSwapBE(x)
#endif

template<size_t size>
struct ByteSwapT
{
};

template<>
struct ByteSwapT<1>
{
    using UIntType = uint8_t;
    static uint8_t SwapBE(uint8_t value)
    {
        return value;
    }
};

template<>
struct ByteSwapT<2>
{
    using UIntType = uint16_t;
    static uint16_t SwapBE(uint16_t value)
    {
        return __builtin_bswap16(value);
    }
};

template<>
struct ByteSwapT<4>
{
    using UIntType = uint32_t;
    static uint32_t SwapBE(uint32_t value)
    {
        return __builtin_bswap32(value);
    }
};

template<>
struct ByteSwapT<8>
{
    using UIntType = uint64_t;
    static uint64_t SwapBE(uint64_t value)
    {
        return __builtin_bswap64(value);
    }
};

template<typename T>
static T ByteSwapBE(const T& value)
{
    using ByteSwap = ByteSwapT<sizeof(T)>;
    using UIntType = ByteSwap::UIntType;

    if constexpr (std::is_enum_v<T> || std::is_integral_v<T>)
    {
        auto result = ByteSwap::SwapBE(static_cast<const UIntType>(value));
        return static_cast<T>(result);
    }
    else
    {
        UIntType temp = std::bit_cast<UIntType>(value);
        auto result = ByteSwap::SwapBE(temp);
        return std::bit_cast<T>(result);
    }
}
