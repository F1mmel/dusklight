#pragma once
#include "window.hpp"

namespace dusk::ui {

class SettingsWindow : public Window {
public:
    SettingsWindow(bool prelaunch = false);

    void update() override;

protected:
    bool mPrelaunch;
    std::map<std::string, std::unique_ptr<dusk::config::ConfigVar<bool>>> mModConfigs;
};

}  // namespace dusk::ui