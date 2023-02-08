#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <map>
#include "NSWorkspaceWrapperShared.h"
#include "NSWorkspaceWrapper.inc.h"

namespace SL {
namespace Screen_Capture {
    struct NSWorkspaceWrapperImpl;
    struct ApplicationData;
    struct WindowData;

    NSWorkspaceWrapperImpl *CreateNSWorkspaceWrapperImpl();

    void DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl *p);

    CFDictionaryRef determineFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p);
    Boolean requestAccessibilityPermissionsWrap(NSWorkspaceWrapperImpl *p);
    void releaseFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p, CFDictionaryRef query);
    std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValuesWrap(NSWorkspaceWrapperImpl *p);
    std::shared_ptr<std::vector<struct WindowData>> determineWindowsWrap(NSWorkspaceWrapperImpl *p, int pid, bool retrieveIcon);
    std::shared_ptr<std::vector<struct WindowData>> determineWindowsWrap(NSWorkspaceWrapperImpl *p, bool retrieveIcon);

    std::shared_ptr<struct WindowData> minimizeWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest& windowRequest, bool minimize);
    std::shared_ptr<struct WindowData> fullscreenWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest& windowRequest, int display, bool fullscreen);
    std::shared_ptr<struct WindowData> rectWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest& windowRequest, const CGRect& rect);
    std::shared_ptr<struct WindowData> topMostWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest& windowRequest);
    std::shared_ptr<struct WindowData> activateWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest& windowRequest);
    std::shared_ptr<struct WindowData> getWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest& windowRequest);
    std::shared_ptr<struct NativeWindowData> nativeWindowInfoWrap(NSWorkspaceWrapperImpl *p, char* data, size_t sz);

    class NSWorkspaceWrapper {
        NSWorkspaceWrapperImpl *NSWorkspaceWrapperImpl_ = nullptr;

      public:
        NSWorkspaceWrapper() { NSWorkspaceWrapperImpl_ = CreateNSWorkspaceWrapperImpl(); }

        ~NSWorkspaceWrapper() { DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl_); }

        CFDictionaryRef determineFrontmostApplication();

        bool requestAccessibilityPermissions();

        void releaseFrontmostApplication(CFDictionaryRef query);

        std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValues();
        std::shared_ptr<std::vector<struct WindowData>> determineWindows(int pid, bool retrieveIcon);
        std::shared_ptr<std::vector<struct WindowData>> determineWindows(bool retrieveIcon);

        std::shared_ptr<struct WindowData> minimize(const WindowDataRequest& windowRequest, bool minimize);
        std::shared_ptr<struct WindowData> fullscreen(const WindowDataRequest& windowRequest, int display, bool fullscreen);
        std::shared_ptr<struct WindowData> rectWindow(const WindowDataRequest& windowRequest, const CGRect& rect);
        std::shared_ptr<struct WindowData> topMost(const WindowDataRequest& windowRequest);
        std::shared_ptr<struct WindowData> activate(const WindowDataRequest& windowRequest);
        std::shared_ptr<struct WindowData> getWindow(const WindowDataRequest& windowRequest);
        std::shared_ptr<struct NativeWindowData> nativeWindowInfo(char* data, size_t sz);
    };
} // namespace Screen_Capture
} // namespace SL

