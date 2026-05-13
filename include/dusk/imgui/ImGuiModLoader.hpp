#pragma once

namespace dusk {
    class ImGuiModLoader {
    public:
        void drawMenu();
        void drawWindows();

    private:
        bool m_showLog = false;
    };
}
