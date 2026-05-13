#include "dusk/arc_loader.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/logging.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <functional>
#include <algorithm>
#include "fmt/format.h"

namespace dusk {

// Helper to swap endianness (Big to Little)
inline uint16_t swap16(uint16_t v) { return (v << 8) | (v >> 8); }
inline uint32_t swap32(uint32_t v) { 
    return ((v << 24) & 0xff000000) | ((v << 8) & 0x00ff0000) | ((v >> 8) & 0x0000ff00) | ((v >> 24) & 0x000000ff); 
}

std::shared_ptr<ArcDirectory> ArcLoader::Load(const std::filesystem::path& arcPath) {
    std::ifstream file(arcPath, std::ios::binary);
    if (!file) return nullptr;

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    if (fileSize < 0x20) return nullptr;
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(fileSize);
    file.read((char*)data.data(), fileSize);

    // Validate Magic
    if (std::string((char*)data.data(), 4) != "RARC") return nullptr;

    // Info Block (starting at 0x20)
    auto read32 = [&](size_t offset) { return swap32(*(uint32_t*)(data.data() + offset)); };
    auto read16 = [&](size_t offset) { return swap16(*(uint16_t*)(data.data() + offset)); };

    const size_t infoStart = 0x20;
    uint32_t nodeCount = read32(infoStart);
    uint32_t nodeOffset = read32(infoStart + 4);
    uint32_t fileEntryCount = read32(infoStart + 8);
    uint32_t fileEntryOffset = read32(infoStart + 12);
    uint32_t stringTableSize = read32(infoStart + 16);
    uint32_t stringTableOffset = read32(infoStart + 20);

    const char* stringTable = (char*)(data.data() + infoStart + stringTableOffset);
    
    // Nodes & Entries Arrays
    struct Node { uint32_t type; uint32_t nameOffset; uint16_t nameHash; uint16_t numEntries; uint32_t firstEntryIndex; };
    struct Entry { uint16_t id; uint16_t hash; uint8_t type; uint8_t pad; uint16_t nameOffset; uint32_t dataOffset; uint32_t dataSize; uint32_t zero; };

    std::vector<Node> nodes;
    size_t nOff = infoStart + nodeOffset;
    for(uint32_t i=0; i<nodeCount; ++i) {
        nodes.push_back({ read32(nOff), read32(nOff+4), read16(nOff+8), read16(nOff+10), read32(nOff+12) });
        nOff += 16;
    }

    std::vector<Entry> entries;
    size_t eOff = infoStart + fileEntryOffset;
    for(uint32_t i=0; i<fileEntryCount; ++i) {
        entries.push_back({ read16(eOff), read16(eOff+2), data[eOff+4], data[eOff+5], read16(eOff+6), read32(eOff+8), read32(eOff+12), read32(eOff+16) });
        eOff += 20;
    }

    auto getString = [&](uint32_t offset) { return std::string(stringTable + offset); };

    std::function<std::shared_ptr<ArcDirectory>(int)> processNode = [&](int nodeIdx) -> std::shared_ptr<ArcDirectory> {
        if (nodeIdx < 0 || nodeIdx >= (int)nodes.size()) return nullptr;

        const auto& node = nodes[nodeIdx];
        auto dir = std::make_shared<ArcDirectory>();
        dir->name = getString(node.nameOffset);

        for (int i = 0; i < node.numEntries; ++i) {
            auto& entry = entries[node.firstEntryIndex + i];
            std::string name = getString(entry.nameOffset);
            if (name == "." || name == "..") continue;

            if (entry.type == 0x02) { 
                auto subdir = processNode(entry.dataOffset);
                if (subdir) dir->subdirs.push_back(subdir);
            } else { 
                dir->files.push_back({name, entry.dataOffset, entry.dataSize});
            }
        }
        return dir;
    };

    return processNode(0);
}

}
