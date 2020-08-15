//
// Created by Denis Esakov on 04.07.20.
//

#ifndef SIMPLE_SCREENSHOT_MAKER_NSWORKSPACEWRAPPER_H
#define SIMPLE_SCREENSHOT_MAKER_NSWORKSPACEWRAPPER_H

#include <CoreFoundation/CoreFoundation.h>
#include <map>
extern const CFStringRef kTopMostPID;
extern const CFStringRef kTopMostLocalizedName;

namespace SL {
namespace Screen_Capture {
    struct NSWorkspaceWrapperImpl;
    struct ApplicationData {
        int pid;
    };

    NSWorkspaceWrapperImpl *CreateNSWorkspaceWrapperImpl();

    void DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl *p);

    CFDictionaryRef determineFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p);
    void releaseFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p, CFDictionaryRef query);
    std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValuesWrap(NSWorkspaceWrapperImpl *p);

    class NSWorkspaceWrapper {
        NSWorkspaceWrapperImpl *NSWorkspaceWrapperImpl_ = nullptr;

      public:
        NSWorkspaceWrapper() { NSWorkspaceWrapperImpl_ = CreateNSWorkspaceWrapperImpl(); }

        ~NSWorkspaceWrapper() { DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl_); }

        CFDictionaryRef determineFrontmostApplication();

        void releaseFrontmostApplication(CFDictionaryRef query);

        std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValues();
    };
} // namespace Screen_Capture
} // namespace SL

#endif // SIMPLE_SCREENSHOT_MAKER_NSWORKSPACEWRAPPER_H
