#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <vector>
#include "NSWorkspaceWrapperShared.h"

namespace SL {
namespace Screen_Capture {
    struct NSScreenWrapperImpl;
    struct ScreenData;

    NSScreenWrapperImpl *CreateNSScreenWrapperImpl();
    void DestroyNSScreenWrapperImpl(NSScreenWrapperImpl *p);

    std::shared_ptr<std::vector<struct ScreenData>> retrieveScreensWrap(NSScreenWrapperImpl *p);

    class NSScreenWrapper {
        NSScreenWrapperImpl *NSScreenWrapperImpl_ = nullptr;

      public:
        NSScreenWrapper() { NSScreenWrapperImpl_ = CreateNSScreenWrapperImpl(); }

        ~NSScreenWrapper() { DestroyNSScreenWrapperImpl(NSScreenWrapperImpl_); }
        
        std::shared_ptr<std::vector<struct ScreenData>> retrieveScreens();

    };
} // namespace Screen_Capture
} // namespace SL