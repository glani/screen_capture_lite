#include "NSWorkspaceWrapper.h"
#include <map>

namespace SL {
    namespace Screen_Capture {

        CFDictionaryRef NSWorkspaceWrapper::determineFrontmostApplication() {
            return determineFrontmostApplicationWrap(this->NSWorkspaceWrapperImpl_);
        }

        void NSWorkspaceWrapper::releaseFrontmostApplication(CFDictionaryRef query) {
            releaseFrontmostApplicationWrap(this->NSWorkspaceWrapperImpl_, query);
        }

        std::shared_ptr<std::map<int, struct ApplicationData>> NSWorkspaceWrapper::determineApplicationValues() {
            return determineApplicationValuesWrap(this->NSWorkspaceWrapperImpl_);
        }

        bool NSWorkspaceWrapper::requestAccessibilityPermissions() {
            return requestAccessibilityPermissionsWrap(this->NSWorkspaceWrapperImpl_);
        }

        std::shared_ptr<std::vector<struct WindowData>> NSWorkspaceWrapper::determineWindows() {
            return determineWindowsWrap(this->NSWorkspaceWrapperImpl_);
        }

        std::shared_ptr<std::vector<struct WindowData>> NSWorkspaceWrapper::determineWindows(const int pid) {
            return determineWindowsWrap(this->NSWorkspaceWrapperImpl_, pid);
        }

    } // namespace Screen_Capture
} // namespace SL