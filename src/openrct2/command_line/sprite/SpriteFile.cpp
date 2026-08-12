/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "SpriteFile.h"

#include "../../core/Endianness.h"
#include "../../core/FileStream.h"
#include "../../drawing/ImageImporter.h"

namespace OpenRCT2::CommandLine::Sprite
{
    std::optional<SpriteFile> SpriteFile::Open(const utf8* path)
    {
        try
        {
            FileStream stream(path, FileMode::open);

            SpriteFile spriteFile;
            stream.Read(&spriteFile.Header, sizeof(G1Header));
            spriteFile.Header.numEntries = SWAP_IF_BE(spriteFile.Header.numEntries);
            spriteFile.Header.totalSize = SWAP_IF_BE(spriteFile.Header.totalSize);

            if (spriteFile.Header.numEntries > 0)
            {
                spriteFile.Entries.reserve(spriteFile.Header.numEntries);

                for (uint32_t i = 0; i < spriteFile.Header.numEntries; ++i)
                {
                    StoredG1Element entry32bit{};
                    stream.Read(&entry32bit, sizeof(entry32bit));
                    entry32bit.offset = SWAP_IF_BE(entry32bit.offset);
                    entry32bit.width = SWAP_IF_BE(entry32bit.width);
                    entry32bit.height = SWAP_IF_BE(entry32bit.height);
                    entry32bit.xOffset = SWAP_IF_BE(entry32bit.xOffset);
                    entry32bit.yOffset = SWAP_IF_BE(entry32bit.yOffset);
                    entry32bit.flags = SWAP_IF_BE(entry32bit.flags);
                    entry32bit.zoomedOffset = SWAP_IF_BE(entry32bit.zoomedOffset);
                    G1Element entry{};

                    entry.offset = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(entry32bit.offset));
                    entry.width = entry32bit.width;
                    entry.height = entry32bit.height;
                    entry.xOffset = entry32bit.xOffset;
                    entry.yOffset = entry32bit.yOffset;
                    entry.flags = entry32bit.flags;
                    entry.zoomedOffset = entry32bit.zoomedOffset;
                    spriteFile.Entries.push_back(std::move(entry));
                }
                spriteFile.Data.resize(spriteFile.Header.totalSize);
                stream.Read(spriteFile.Data.data(), spriteFile.Header.totalSize);
            }
            spriteFile.MakeEntriesAbsolute();
            return spriteFile;
        }
        catch (IOException&)
        {
            return std::nullopt;
        }
    }

    void SpriteFile::MakeEntriesAbsolute()
    {
        if (!isAbsolute)
        {
            for (auto& entry : Entries)
                entry.offset += reinterpret_cast<uintptr_t>(Data.data());
        }
        isAbsolute = true;
    }

    void SpriteFile::MakeEntriesRelative()
    {
        if (isAbsolute)
        {
            for (auto& entry : Entries)
                entry.offset -= reinterpret_cast<uintptr_t>(Data.data());
        }
        isAbsolute = false;
    }

    void SpriteFile::AddImage(Drawing::ImageImportResult& image)
    {
        Header.numEntries++;
        // New image will have its data inserted after previous image
        uint8_t* newElementOffset = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(Header.totalSize));
        Header.totalSize += static_cast<uint32_t>(image.Buffer.size());

        {
            ScopedRelativeSpriteFile scopedRelative(*this);
            Entries.push_back(image.Element);
            Entries.back().offset = newElementOffset;
            const auto& buffer = image.Buffer;
            Data.insert(Data.end(), buffer.begin(), buffer.end());
        }
    }

    void SpriteFile::addPalette(Drawing::PaletteImportResult& palette)
    {
        AddImage(*reinterpret_cast<Drawing::ImageImportResult*>(&palette));
    }

    bool SpriteFile::Save(const utf8* path)
    {
        try
        {
            FileStream stream(path, FileMode::write);
            G1Header headerToWrite = Header;
            headerToWrite.numEntries = SWAP_IF_BE(headerToWrite.numEntries);
            headerToWrite.totalSize = SWAP_IF_BE(headerToWrite.totalSize);
            stream.Write(&headerToWrite, sizeof(G1Header));

            if (Header.numEntries > 0)
            {
                ScopedRelativeSpriteFile scopedRelative(*this);

                for (const auto& entry : Entries)
                {
                    StoredG1Element entry32bit{};

                    entry32bit.offset = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(const_cast<uint8_t*>(entry.offset)));
                    entry32bit.width = entry.width;
                    entry32bit.height = entry.height;
                    entry32bit.xOffset = entry.xOffset;
                    entry32bit.yOffset = entry.yOffset;
                    entry32bit.flags = entry.flags;
                    entry32bit.zoomedOffset = entry.zoomedOffset;

                    entry32bit.offset = SWAP_IF_BE(entry32bit.offset);
                    entry32bit.width = SWAP_IF_BE(entry32bit.width);
                    entry32bit.height = SWAP_IF_BE(entry32bit.height);
                    entry32bit.xOffset = SWAP_IF_BE(entry32bit.xOffset);
                    entry32bit.yOffset = SWAP_IF_BE(entry32bit.yOffset);
                    entry32bit.flags = SWAP_IF_BE(entry32bit.flags);
                    entry32bit.zoomedOffset = SWAP_IF_BE(entry32bit.zoomedOffset);

                    stream.Write(&entry32bit, sizeof(entry32bit));
                }
#if RCT2_BIG_ENDIAN
                // Byte-swap RLE line offsets to LE for on-disk format
                for (const auto& entry : Entries)
                {
                    if (entry.flags.has(G1Flag::hasRLECompression) && entry.height > 0)
                    {
                        auto dataOffs = reinterpret_cast<uintptr_t>(entry.offset);
                        auto* lineOffsets = reinterpret_cast<uint16_t*>(Data.data() + dataOffs);
                        for (int32_t y = 0; y < entry.height; y++)
                        {
                            lineOffsets[y] = SWAP_IF_BE(lineOffsets[y]);
                        }
                        if (entry.flags.has(G1Flag::hasZoomSprite) && entry.zoomedOffset != 0)
                        {
                            auto zoomedOffs = dataOffs + entry.zoomedOffset;
                            auto* zoomedLineOffsets = reinterpret_cast<uint16_t*>(Data.data() + zoomedOffs);
                            auto zoomedHeight = entry.height > 1 ? entry.height >> 1 : 1;
                            for (int32_t y = 0; y < zoomedHeight; y++)
                            {
                                zoomedLineOffsets[y] = SWAP_IF_BE(zoomedLineOffsets[y]);
                            }
                        }
                    }
                }
#endif
                stream.Write(Data.data(), Header.totalSize);
            }
            return true;
        }
        catch (IOException&)
        {
            return false;
        }
    }

} // namespace OpenRCT2::CommandLine::Sprite
