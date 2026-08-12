/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Crypt.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

using namespace OpenRCT2::Crypt;

class OpenRCT2FNV1aAlgorithm final : public FNV1aAlgorithm
{
private:
    static constexpr uint64_t kOffset = 0xCBF29CE484222325ULL;
    static constexpr uint64_t kPrime = 0x00000100000001B3ULL;

    uint64_t _data = kOffset;
    uint8_t _rem[8]{};
    size_t _remLen{};

    // Read a uint64_t from 8 bytes, consistently interpreting bytes as LE on all platforms
    static uint64_t ReadLE64(const uint8_t* src)
    {
        uint64_t val{};
        std::memcpy(&val, src, sizeof(val));
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        val = __builtin_bswap64(val);
#endif
        return val;
    }

    void ProcessRemainder()
    {
        if (_remLen > 0)
        {
            uint64_t temp{};
            std::memcpy(&temp, _rem, _remLen);
            _data ^= ReadLE64(reinterpret_cast<const uint8_t*>(&temp));
            _data *= kPrime;
            _remLen = 0;
        }
    }

public:
    HashAlgorithm* Clear() override
    {
        _data = kOffset;
        return this;
    }

    HashAlgorithm* Update(const void* data, size_t dataLen) override
    {
        if (dataLen == 0)
            return this;

        auto src = static_cast<const uint8_t*>(data);
        if (_remLen > 0)
        {
            // We have remainder, so fill rest of it with bytes from src
            auto fillLen = sizeof(uint64_t) - _remLen;
            assert(_remLen + fillLen <= sizeof(uint64_t));
            std::memcpy(_rem + _remLen, src, fillLen);
            src += fillLen;
            _remLen += fillLen;
            dataLen -= fillLen;
            ProcessRemainder();
        }

        // Process every block of 8 bytes, consistently as LE-ordered integers
        while (dataLen >= sizeof(uint64_t))
        {
            auto temp = ReadLE64(src);
            src += sizeof(uint64_t);
            _data ^= temp;
            _data *= kPrime;
            dataLen -= sizeof(uint64_t);
        }

        // Store the remaining data (< 8 bytes)
        if (dataLen > 0)
        {
            _remLen = dataLen;
            std::memcpy(&_rem, src, dataLen);
        }
        return this;
    }

    Result Finish() override
    {
        ProcessRemainder();

        Result res;
        auto val = _data;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        val = __builtin_bswap64(val);
#endif
        std::memcpy(res.data(), &val, sizeof(val));
        return res;
    }
};

namespace OpenRCT2::Crypt
{
    std::unique_ptr<FNV1aAlgorithm> CreateFNV1a()
    {
        return std::make_unique<OpenRCT2FNV1aAlgorithm>();
    }
} // namespace OpenRCT2::Crypt
