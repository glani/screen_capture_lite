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

        std::shared_ptr<std::vector<struct WindowData>> NSWorkspaceWrapper::determineWindows(bool retrieveIcon) {
            return determineWindowsWrap(this->NSWorkspaceWrapperImpl_, retrieveIcon);
        }

        std::shared_ptr<std::vector<struct WindowData>>
        NSWorkspaceWrapper::determineWindows(const int pid, bool retrieveIcon) {
            return determineWindowsWrap(this->NSWorkspaceWrapperImpl_, pid, retrieveIcon);
        }


        std::shared_ptr<struct WindowData>
        NSWorkspaceWrapper::minimize(const WindowDataRequest &windowRequest, bool minimize) {
            return minimizeWindowWrap(this->NSWorkspaceWrapperImpl_, windowRequest, minimize);
        }

        bool NSWorkspaceWrapper::closeWindow(const WindowDataRequest &windowRequest, bool force) {
            return closeWindowWrap(this->NSWorkspaceWrapperImpl_, windowRequest, force);
        }

        bool NSWorkspaceWrapper::closeAllWindows(const WindowDataRequest &windowRequest, bool force) {
            return closeAllWindowsWrap(this->NSWorkspaceWrapperImpl_, windowRequest, force);
        }

        std::shared_ptr<struct WindowData>
        NSWorkspaceWrapper::fullscreen(const WindowDataRequest &windowRequest, int display, bool fullscreen) {
            return fullscreenWindowWrap(this->NSWorkspaceWrapperImpl_, windowRequest, display, fullscreen);
        }

        std::shared_ptr<struct WindowData> NSWorkspaceWrapper::getWindow(const WindowDataRequest &windowRequest) {
            return getWindowWrap(this->NSWorkspaceWrapperImpl_, windowRequest);
        }

        std::shared_ptr<struct WindowData> NSWorkspaceWrapper::topMost(const WindowDataRequest &windowRequest) {
            return topMostWindowWrap(this->NSWorkspaceWrapperImpl_, windowRequest);
        }

        std::shared_ptr<struct WindowData> NSWorkspaceWrapper::activate(const WindowDataRequest &windowRequest) {
            return activateWindowWrap(this->NSWorkspaceWrapperImpl_, windowRequest);
        }

        std::shared_ptr<struct NativeWindowData> NSWorkspaceWrapper::nativeWindowInfo(char* data, size_t sz) {
            return nativeWindowInfoWrap(this->NSWorkspaceWrapperImpl_, data, sz);
        }

        bool NSWorkspaceWrapper::activateApplication(int pid) {
            return activateApplicationWrap(this->NSWorkspaceWrapperImpl_, pid);
        }

        std::shared_ptr<struct WindowData>
        NSWorkspaceWrapper::rectWindow(const WindowDataRequest &windowRequest, const CGRect &rect) {
            return rectWindowWrap(this->NSWorkspaceWrapperImpl_, windowRequest, rect);
        }

    } // namespace Screen_Capture
} // namespace SL