#include "dusk/imgui/ImGuiModLoader.hpp"
#include "imgui.h"
#include "dusk/mod_loader.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "fmt/format.h"
#include "dusk/arc_loader.hpp"
#include <map>

namespace dusk {
    static std::filesystem::file_time_type s_lastModsDirTime;
    static bool s_enableFileWatcher = true;
    static bool s_showSettings = false;
    static bool s_showMods = true;

    void SetAllFilesEnabled(const std::shared_ptr<ArcDirectory>& dir, const std::string& arcName, const std::string& modName, bool enabled) {
        for (const auto& file : dir->files) {
            if (file.name.ends_with(".bmd")) {
                dusk::SetFileEnabled(modName, arcName + "/" + file.name, enabled);
            }
        }
        for (const auto& subdir : dir->subdirs) {
            SetAllFilesEnabled(subdir, arcName, modName, enabled);
        }
    }

    void DrawArcNode(const std::shared_ptr<ArcDirectory>& dir, const std::string& arcName, const std::string& modName) {
        for (const auto& file : dir->files) {
            if (file.name.ends_with(".bmd")) {
                std::string fullRelPath = arcName + "/" + file.name;
                bool isEnabled = dusk::IsFileEnabled(modName, fullRelPath);
                if (ImGui::Checkbox(file.name.c_str(), &isEnabled)) {
                    dusk::SetFileEnabled(modName, fullRelPath, isEnabled);
                }
            }
        }
        for (const auto& subdir : dir->subdirs) {
            if (ImGui::TreeNode(subdir->name.c_str())) {
                DrawArcNode(subdir, arcName, modName);
                ImGui::TreePop();
            }
        }
    }

    void ImGuiModLoader::drawMenu() {
        if (ImGui::BeginMenu("ModLoader")) {
            ImGui::MenuItem("Log", nullptr, &m_showLog);
            ImGui::MenuItem("Mods", nullptr, &s_showMods);
            ImGui::MenuItem("Settings", nullptr, &s_showSettings);
            if (ImGui::MenuItem("Reload Mods")) {
                dusk::TriggerReload();
            }
            ImGui::EndMenu();
        }
    }

    void ImGuiModLoader::drawWindows() {
        if (s_enableFileWatcher) {
            auto modsDir = dusk::ConfigPath / "mods";
            if (std::filesystem::exists(modsDir)) {
                auto currentTime = std::filesystem::last_write_time(modsDir);
                if (currentTime != s_lastModsDirTime) {
                    s_lastModsDirTime = currentTime;
                    dusk::TriggerReload();
                }
            }
        }

        if (s_showSettings) {
            if (ImGui::Begin("Settings", &s_showSettings)) {
                ImGui::Checkbox("Enable Auto-Reload (FileWatcher)", &s_enableFileWatcher);
            }
            ImGui::End();
        }

        if (s_showMods) {
            if (ImGui::Begin("Mods", &s_showMods)) {
                if (ImGui::Button("Reload Mods")) {
                    dusk::TriggerReload();
                }
                auto modsPath = dusk::ConfigPath / "mods";
                if (std::filesystem::exists(modsPath)) {
                    for (const auto& entry : std::filesystem::directory_iterator(modsPath)) {
                        if (entry.is_directory()) {
                            std::string modName = entry.path().filename().string();
                            bool enabled = dusk::IsModEnabled(modName);
                            if (ImGui::Checkbox(("##" + modName).c_str(), &enabled)) {
                                dusk::SetModEnabled(modName, enabled);
                            }
                            ImGui::SameLine();
                            if (ImGui::TreeNode(modName.c_str())) {
                                ImGui::Indent();
                                for (const auto& [arcPath, arc] : dusk::GetParsedArchives()) {
                                    if (arcPath.string().find(entry.path().string()) != std::string::npos) {
                                        if (ImGui::TreeNode(arcPath.filename().string().c_str())) {
                                            ImGui::SameLine();
                                            if (ImGui::Button("All")) {
                                                SetAllFilesEnabled(arc, arcPath.filename().string(), modName, true);
                                            }
                                            ImGui::SameLine();
                                            if (ImGui::Button("None")) {
                                                SetAllFilesEnabled(arc, arcPath.filename().string(), modName, false);
                                            }
                                            if (arc) DrawArcNode(arc, arcPath.filename().string(), modName);
                                            ImGui::TreePop();
                                        }
                                    }
                                }
                                ImGui::Unindent();
                                ImGui::TreePop();
                            }
                        }
                    }
                }
            }
            ImGui::End();
        }

        if (m_showLog) {
            if (ImGui::Begin("Log", &m_showLog)) {
                for (const auto& log : GetModLogs()) {
                    ImGui::TextUnformatted(log.c_str());
                }
            }
            ImGui::End();
        }
    }
}
