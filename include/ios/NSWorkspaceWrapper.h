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
    std::shared_ptr<std::vector<struct WindowData>> determineWindowsWrap(NSWorkspaceWrapperImpl *p, int pid);
    std::shared_ptr<std::vector<struct WindowData>> determineWindowsWrap(NSWorkspaceWrapperImpl *p);


    class NSWorkspaceWrapper {
        NSWorkspaceWrapperImpl *NSWorkspaceWrapperImpl_ = nullptr;

      public:
        NSWorkspaceWrapper() { NSWorkspaceWrapperImpl_ = CreateNSWorkspaceWrapperImpl(); }

        ~NSWorkspaceWrapper() { DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl_); }

        CFDictionaryRef determineFrontmostApplication();

        bool requestAccessibilityPermissions();

        void releaseFrontmostApplication(CFDictionaryRef query);

        std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValues();
        std::shared_ptr<std::vector<struct WindowData>> determineWindows(int pid);
        std::shared_ptr<std::vector<struct WindowData>> determineWindows();
    };
} // namespace Screen_Capture
} // namespace SL

