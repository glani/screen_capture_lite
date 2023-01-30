#pragma once

#include <memory>
#include <string>
#include <CoreGraphics/CGGeometry.h>

namespace SL {
namespace Screen_Capture {
    struct ApplicationData {
        int pid;
    };
    struct WindowData {
        int pid;
        uint32_t handle;
        std::shared_ptr<std::string> processTitle;
        std::shared_ptr<std::string> title;
        CGRect rect;
        std::shared_ptr<bool> focused;
        std::shared_ptr<bool> main;
        std::shared_ptr<bool> minimized;
        std::shared_ptr<bool> fullscreen;
        std::shared_ptr<std::string> execute;
        std::shared_ptr<std::string> icon;
    };
    struct ScreenData {
        CGRect frame;
        CGRect visibleFrame;
        uint32_t displayId;
        uint32_t num;
        bool main;
        std::shared_ptr<std::string> screenName;
    };

    struct WindowDataRequest {
        CGRect rect;
        int pid;
        int handle;
        bool minimized;
        bool fullscreen;
        bool retrieveIcon;
    };
} // namespace Screen_Capture
} // namespace SL
