#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include "xcb/XcbInitializer.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <dlfcn.h>

#include <string>
#include <string_view>

namespace SL {
namespace Screen_Capture {
    enum WMWorkspace { COMMON, GTK };

    std::shared_ptr<std::vector<Monitor>> GetMonitors()
    {
        std::shared_ptr<std::vector<Monitor>> ret = std::make_shared<std::vector<Monitor>>();

        auto &xcbInstance = XcbInitializer::getInstance();
        auto display = xcbInstance.xlibDisplay();
        if (display == NULL) {
            return ret;
        }
        int nmonitors = 0;
        XineramaScreenInfo *queryScreens = XineramaQueryScreens(display, &nmonitors);
        if (queryScreens == NULL) {
            return ret;
        }
        ret->reserve(nmonitors);

        Screen *screen = XDefaultScreenOfDisplay(display);
        // retrieve current desktop
        uint32_t displayNumber = 0;
        auto currentDesktop = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, screen->root,
                                                  xcbInstance.atom(XcbAtom::_NET_CURRENT_DESKTOP), XCB_ATOM_CARDINAL, 0, 1024);
        if (currentDesktop && currentDesktop->type == XCB_ATOM_CARDINAL && currentDesktop->format == 32 && currentDesktop->value_len) {
            uint32_t *displayNumberPtr = (uint32_t *)xcb_get_property_value(currentDesktop.get());
            displayNumber = *displayNumberPtr;
        }

        // try to determine GTK workspace values
        const std::string &gtkAtomName = string_format("_GTK_WORKAREAS_D%d", displayNumber);
        auto gtkWorkAreas = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, screen->root,
                                                xcbInstance.internAtom(gtkAtomName.c_str()), XCB_ATOM_CARDINAL, 0, 1024);
        std::vector<Rect> workspaces;
        WMWorkspace wmWorkspace = COMMON;
        if (gtkWorkAreas && gtkWorkAreas->type == XCB_ATOM_CARDINAL && gtkWorkAreas->format == 32 && gtkWorkAreas->value_len &&
            gtkWorkAreas->value_len % 4 == 0) {
            uint32_t *gtkWorkAreasPtr = (uint32_t *)xcb_get_property_value(gtkWorkAreas.get());
            int i = 0;
            while (i < gtkWorkAreas->value_len) {
                const auto &workspace = Rect(static_cast<int>(gtkWorkAreasPtr[0]), static_cast<int>(gtkWorkAreasPtr[1]),
                                             static_cast<int>(gtkWorkAreasPtr[2]), static_cast<int>(gtkWorkAreasPtr[3]));
                workspaces.emplace_back(workspace);

                gtkWorkAreasPtr += 4;
                i += 4;
            }
            wmWorkspace = GTK;
        }
        else {
            auto workArea = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, screen->root,
                                                xcbInstance.atom(XcbAtom::_NET_WORKAREA), XCB_ATOM_CARDINAL, 0, 1024);
            if (workArea && workArea->type == XCB_ATOM_CARDINAL && workArea->format == 32 && workArea->value_len >= 4 &&
                workArea->value_len % 4 == 0) {
                uint32_t *geom = (uint32_t *)xcb_get_property_value(workArea.get());
                const auto &workspace =
                    Rect(static_cast<int>(geom[0]), static_cast<int>(geom[1]), static_cast<int>(geom[2]), static_cast<int>(geom[3]));
                workspaces.emplace_back(workspace);
            }
        }

        for (auto i = 0; i < nmonitors; i++) {
            auto name = std::string("Display ") + std::to_string(i);
            Monitor monitor = CreateMonitor(i, queryScreens[i].screen_number, queryScreens[i].height, queryScreens[i].width, queryScreens[i].x_org,
                                            queryScreens[i].y_org, name, 1.0f);
            if (wmWorkspace == GTK) {
                // try to apply based on intersection
                auto fit = std::find_if(workspaces.begin(), workspaces.end(), [&](const Rect &rect) {
                    auto intersection = monitor.Frame & rect;
                    if (intersection && intersection->Width * intersection->Height > monitor.Frame.Width * monitor.Frame.Height * 0.5) {
                        return true;
                    }
                    return false;
                });
                if (fit != workspaces.end()) {
                    monitor.VisibleFrame = *fit;
                }
            }
            else {
                // logic & for two rects. It means we retrieved data from _NET_WORKAREA
                if (!workspaces.empty()) {
                    const Rect& rect = workspaces.front();
                    auto intersection = rect & monitor.Frame;
                    if (intersection) {
                        monitor.VisibleFrame = *intersection;
                    }
                }
            }

            ret->push_back(monitor);
        }
        XFree(queryScreens);
        return ret;
    }
} // namespace Screen_Capture
} // namespace SL
