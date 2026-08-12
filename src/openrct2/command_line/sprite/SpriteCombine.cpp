/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../../core/Endianness.h"
#include "../../core/FileStream.h"
#include "../../drawing/Drawing.h"
#include "SpriteCommands.h"

#include <cstdint>

namespace OpenRCT2::CommandLine::Sprite
{
    ExitCode combine(const char** argv, int32_t argc)
    {
        if (argc < 4)
        {
            fprintf(stdout, "usage: sprite combine <index file> <image file> <output>\n");
            return ExitCode::fail;
        }

        const utf8* indexFile = argv[1];
        const utf8* dataFile = argv[2];
        const utf8* outputPath = argv[3];

        auto fileHeader = FileStream(indexFile, FileMode::open);
        auto fileData = FileStream(dataFile, FileMode::open);
        auto fileHeaderSize = fileHeader.GetLength();
        auto fileDataSize = fileData.GetLength();

        uint32_t numEntries = fileHeaderSize / sizeof(StoredG1Element);

        G1Header header = {};
        header.numEntries = numEntries;
        header.totalSize = fileDataSize;
        FileStream outputStream(outputPath, FileMode::write);

        G1Header headerToWrite = header;
        headerToWrite.numEntries = SWAP_IF_BE(headerToWrite.numEntries);
        headerToWrite.totalSize = SWAP_IF_BE(headerToWrite.totalSize);
        outputStream.Write(&headerToWrite, sizeof(G1Header));
        auto g1Elements32 = std::make_unique<StoredG1Element[]>(numEntries);
        fileHeader.Read(g1Elements32.get(), numEntries * sizeof(StoredG1Element));
        for (uint32_t i = 0; i < numEntries; i++)
        {
            g1Elements32[i].offset = SWAP_IF_BE(g1Elements32[i].offset);
            g1Elements32[i].width = SWAP_IF_BE(g1Elements32[i].width);
            g1Elements32[i].height = SWAP_IF_BE(g1Elements32[i].height);
            g1Elements32[i].xOffset = SWAP_IF_BE(g1Elements32[i].xOffset);
            g1Elements32[i].yOffset = SWAP_IF_BE(g1Elements32[i].yOffset);
            g1Elements32[i].flags = SWAP_IF_BE(g1Elements32[i].flags);
            g1Elements32[i].zoomedOffset = SWAP_IF_BE(g1Elements32[i].zoomedOffset);

            // RCT1 used zoomed offsets that counted from the beginning of the file, rather than from the current sprite.
            if (g1Elements32[i].flags.has(G1Flag::hasZoomSprite))
            {
                g1Elements32[i].zoomedOffset = i - g1Elements32[i].zoomedOffset;
            }

            g1Elements32[i].offset = SWAP_IF_BE(g1Elements32[i].offset);
            g1Elements32[i].width = SWAP_IF_BE(g1Elements32[i].width);
            g1Elements32[i].height = SWAP_IF_BE(g1Elements32[i].height);
            g1Elements32[i].xOffset = SWAP_IF_BE(g1Elements32[i].xOffset);
            g1Elements32[i].yOffset = SWAP_IF_BE(g1Elements32[i].yOffset);
            g1Elements32[i].flags = SWAP_IF_BE(g1Elements32[i].flags);
            g1Elements32[i].zoomedOffset = SWAP_IF_BE(g1Elements32[i].zoomedOffset);

            outputStream.Write(&g1Elements32[i], sizeof(StoredG1Element));
        }

        std::vector<uint8_t> data;
        data.resize(fileDataSize);
        fileData.Read(data.data(), fileDataSize);
        outputStream.Write(data.data(), fileDataSize);

        return ExitCode::ok;
    }
} // namespace OpenRCT2::CommandLine::Sprite
