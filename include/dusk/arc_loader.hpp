#ifndef DUSK_ARC_LOADER_HPP
#define DUSK_ARC_LOADER_HPP

#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <cstdint>

namespace dusk {

struct ArcFile {
    std::string name;
    uint32_t offset;
    uint32_t size;
};

struct ArcDirectory {
    std::string name;
    std::vector<std::shared_ptr<ArcDirectory>> subdirs;
    std::vector<ArcFile> files;
};

class ArcLoader {
public:
    static std::shared_ptr<ArcDirectory> Load(const std::filesystem::path& arcPath);
};

}

#endif // DUSK_ARC_LOADER_HPP
