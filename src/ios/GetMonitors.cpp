#include "NSScreenWrapper.h"
#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include <ApplicationServices/ApplicationServices.h>

namespace SL {
namespace Screen_Capture {
    static NSScreenWrapper ptrWrapper;

    std::shared_ptr<std::vector<Monitor>> GetMonitors()
    {
        auto screens = ptrWrapper.retrieveScreens();
        std::shared_ptr<std::vector<Monitor>> ret = std::make_shared<std::vector<Monitor>>();
        std::vector<CGDirectDisplayID> displays;
        CGDisplayCount count = 0;
        // get count
        CGGetActiveDisplayList(0, 0, &count);
        displays.resize(count);

        CGGetActiveDisplayList(count, displays.data(), &count);
        for (auto i = 0; i < count; i++) {
            // only include non-mirrored displays
            unsigned int display = displays[i];
            if (CGDisplayMirrorsDisplay(display) == kCGNullDirectDisplay) {
                auto fit = std::find_if(screens->begin(), screens->end(), [&](const ScreenData &elem) { return elem.displayId == display; });
                auto fitMain = std::find_if(screens->begin(), screens->end(), [&](const ScreenData &elem) { return elem.main; });
                if (fit != screens->end() && fitMain != screens->end()) {
                    auto dismode = CGDisplayCopyDisplayMode(display);
                    auto width = CGDisplayModeGetPixelWidth(dismode);
                    auto height = CGDisplayModeGetPixelHeight(dismode);
                    CGDisplayModeRelease(dismode);
                    auto r = CGDisplayBounds(display);
                    auto scale = static_cast<float>(width) / static_cast<float>(r.size.width);
                    auto name = std::string("Monitor ") + std::to_string(display);

                    Monitor monitor =
                        CreateMonitor(static_cast<int>(ret->size()), display, height, width, int(r.origin.x), int(r.origin.y), name, scale);
                    monitor.DisplayName = fit->screenName ? *fit->screenName : std::string();
                    monitor.Frame = {};
                    monitor.VisibleFrame = {};

                    monitor.VisibleFrame.X = int(fit->visibleFrame.origin.x);
                    monitor.VisibleFrame.Y = int(fit->visibleFrame.origin.y);
                    monitor.VisibleFrame.Width = int(fit->visibleFrame.size.width);
                    monitor.VisibleFrame.Height = int(fit->visibleFrame.size.height);

                    monitor.Frame.X = int(fit->frame.origin.x);
                    monitor.Frame.Y = int(fit->frame.origin.y);
                    monitor.Frame.Width = int(fit->frame.size.width);
                    monitor.Frame.Height = int(fit->frame.size.height);
                    monitor.Main = fit->main;
                    if (fit->main) {
                        monitor.VisibleFrame.Y = std::max(monitor.Frame.Height - monitor.VisibleFrame.Height - monitor.VisibleFrame.Y, 0); // no top place for dock
                    } else {
                        monitor.Frame.X = monitor.OffsetX;
                        monitor.Frame.Y = monitor.OffsetY;
                        monitor.VisibleFrame.X = monitor.OffsetX;
                        monitor.VisibleFrame.Y = monitor.OffsetY + std::max(int(fitMain->frame.size.height - fitMain->visibleFrame.size.height - fitMain->visibleFrame.origin.y), 0);
                        monitor.VisibleFrame.Height -= monitor.VisibleFrame.Y;
                    }

                    ret->emplace_back(monitor);
                }
            }
        }

        return ret;
    }
} // namespace Screen_Capture
} // namespace SL
