#ifndef DUSK_MOD_LOADER_HPP
#define DUSK_MOD_LOADER_HPP

#include <filesystem>
#include <optional>
#include <memory>
#include "dusk/arc_loader.hpp"
#include <map>

namespace dusk {
    std::optional<std::filesystem::path> GetModFilePath(const char* dvdPath);
    void InitModLoader();
    void TriggerReload();
    void LoadModSettings();
    size_t GetModCount();
    const std::vector<std::string>& GetModLogs();
    const std::vector<std::filesystem::path>& GetModFolders();
    const std::map<std::filesystem::path, std::shared_ptr<ArcDirectory>>& GetParsedArchives();
    void LogArchiveLoad(const char* arcName);
    void LogFileLoad(const char* arcName, const char* fileName);
    void AddModLog(const std::string& msg);
    
    bool IsModEnabled(const std::string& modName);
    void SetModEnabled(const std::string& modName, bool enabled);
    bool IsFileEnabled(const std::string& modName, const std::string& filePath);
    void SetFileEnabled(const std::string& modName, const std::string& filePath, bool enabled);
    void SetAllFilesEnabled(const std::shared_ptr<ArcDirectory>& dir, const std::string& arcName, const std::string& modName, bool enabled);
    void SaveModSettings();
}

#endif // DUSK_MOD_LOADER_HPP
