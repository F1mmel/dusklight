#include "dusk/settings.h"
#include "dusk/config.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/main.h"
#include "dusk/logging.h"
#include "nlohmann/json.hpp"
#include "d/d_com_inf_game.h"
#include <vector>
#include <algorithm>
#include <fstream>
#include <map>

using json = nlohmann::json;

namespace dusk {
static std::vector<std::filesystem::path> s_modFolders;
static std::vector<std::string> s_modLogs;
static std::map<std::string, bool> s_modEnabled;
static std::map<std::string, std::map<std::string, bool>> s_modFilesEnabled;
static std::map<std::filesystem::path, std::shared_ptr<ArcDirectory>> s_parsedArchives;

void LoadModSettings() {
    s_modEnabled.clear();
    s_modFilesEnabled.clear();
    if (std::filesystem::exists(dusk::ConfigPath / "mod_settings.json")) {
        try {
            std::ifstream file(dusk::ConfigPath / "mod_settings.json");
            json j;
            file >> j;
            if (j.contains("mods")) {
                for (auto& [name, data] : j["mods"].items()) {
                    s_modEnabled[name] = data.value("enabled", true);
                    if (data.contains("files")) {
                        for (auto& [path, enabled] : data["files"].items()) {
                            s_modFilesEnabled[name][path] = enabled.get<bool>();
                        }
                    }
                }
            }
        } catch (...) {}
    }
}

void SaveModSettings() {
    json j;
    j["mods"] = json::object();
    for (const auto& [name, enabled] : s_modEnabled) {
        j["mods"][name] = {{"enabled", enabled}, {"files", s_modFilesEnabled[name]}};
    }
    std::ofstream file(dusk::ConfigPath / "mod_settings.json");
    file << j.dump(4);
}

bool IsModEnabled(const std::string& modName) {
    if (s_modEnabled.find(modName) == s_modEnabled.end()) return false;
    return s_modEnabled[modName];
}

void SetModEnabled(const std::string& modName, bool enabled) {
    s_modEnabled[modName] = enabled;
    SaveModSettings();
}

bool IsFileEnabled(const std::string& modName, const std::string& filePath) {
    auto itMod = s_modFilesEnabled.find(modName);
    if (itMod != s_modFilesEnabled.end()) {
        auto itFile = itMod->second.find(filePath);
        if (itFile != itMod->second.end()) {
            return itFile->second;
        }
    }
    return true;
}

void SetFileEnabled(const std::string& modName, const std::string& filePath, bool enabled) {
    s_modFilesEnabled[modName][filePath] = enabled;
    SaveModSettings();
}

void TriggerReload() {
    dusk::InitModLoader();
    dComIfGp_setNextStage(dComIfGp_getStartStageName(), dComIfGp_getStartStagePoint(), dComIfGp_roomControl_getStayNo(), dComIfGp_getStartStageLayer());
}

void InitModLoader() {
    LoadModSettings();
    s_modFolders.clear();
    s_modLogs.clear();
    s_parsedArchives.clear();

    std::filesystem::path modsDir = ConfigPath / "mods";
    std::error_code ec;
    if (!std::filesystem::exists(modsDir, ec)) {
        std::filesystem::create_directories(modsDir, ec);
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(modsDir, ec)) {
        if (entry.is_directory()) {
            std::string modName = entry.path().filename().string();
            if (IsModEnabled(modName)) {
                s_modFolders.push_back(entry.path());
                for (const auto& file : std::filesystem::recursive_directory_iterator(entry.path())) {
                    if (file.path().extension() == ".arc") {
                        auto arc = ArcLoader::Load(file.path());
                        s_parsedArchives[file.path()] = arc;
                        
                        auto registerBmds = [&](auto self, const std::shared_ptr<ArcDirectory>& dir, const std::string& arcName) -> void {
                            for (const auto& f : dir->files) {
                                if (f.name.ends_with(".bmd")) {
                                    if (s_modFilesEnabled[modName].find(arcName + "/" + f.name) == s_modFilesEnabled[modName].end()) {
                                        s_modFilesEnabled[modName][arcName + "/" + f.name] = true;
                                    }
                                }
                            }
                            for (const auto& subdir : dir->subdirs) self(self, subdir, arcName);
                        };
                        if (arc) registerBmds(registerBmds, arc, file.path().filename().string());
                    }
                }
            }
        }
    }
    SaveModSettings();
    std::sort(s_modFolders.begin(), s_modFolders.end());
    
    std::string msg = fmt::format("Mod loader active. Found {} active mods.", s_modFolders.size());
    DuskLog.info("{}", msg);
    s_modLogs.push_back(msg);
    
    for (const auto& folder : s_modFolders) {
        std::string log = fmt::format("Loaded mod: {}", folder.filename().string());
        DuskLog.info("{}", log);
        s_modLogs.push_back(log);
    }
}

std::optional<std::filesystem::path> GetModFilePath(const char* dvdPath) {
    if (!dvdPath) return std::nullopt;

    std::string pathStr = dvdPath;
    if (pathStr.starts_with('/')) pathStr = pathStr.substr(1);

    std::string filesPathStr = pathStr.starts_with("files/") ? pathStr : "files/" + pathStr;
    std::filesystem::path paths[] = { filesPathStr, pathStr };

    // Iteriere durch alle Mods (höchste Priorität zuerst, falls wir Reverse Iteration nutzen)
    for (auto it = s_modFolders.rbegin(); it != s_modFolders.rend(); ++it) {
        for (const auto& p : paths) {
            std::filesystem::path modFile = *it / p;
            std::error_code ec;
            if (std::filesystem::exists(modFile, ec)) {
                std::string relPath = std::filesystem::relative(modFile, *it).string();
                std::string modName = it->filename().string();
                
                // Prüfe, ob die spezifische Datei in DIESER Mod aktiviert ist
                if (IsFileEnabled(modName, relPath)) {
                    std::string log = fmt::format("[mDoDvdThd] Using file from {}: {}", modName, relPath);
                    dusk::AddModLog(log);
                    DuskLog.info("{}", log);
                    return modFile;
                } else {
                    dusk::AddModLog(fmt::format("[mDoDvdThd] Skipping disabled file in {}: {}", modName, relPath));
                }
            }
        }
    }
    return std::nullopt;
}

size_t GetModCount() { return s_modFolders.size(); }
const std::vector<std::string>& GetModLogs() { return s_modLogs; }
const std::vector<std::filesystem::path>& GetModFolders() { return s_modFolders; }
const std::map<std::filesystem::path, std::shared_ptr<ArcDirectory>>& GetParsedArchives() { return s_parsedArchives; }
void AddModLog(const std::string& msg) { s_modLogs.push_back(msg); }
void LogArchiveLoad(const char* arcName) { DuskLog.info("[JKRArchive] Loading: {}.arc", arcName); }
void LogFileLoad(const char* arcName, const char* fileName) { DuskLog.info("[JKRFile] {}/{}", arcName, fileName); }
}
