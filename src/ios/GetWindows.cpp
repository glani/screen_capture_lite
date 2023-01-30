#include "ScreenCapture.h"
#include "Utils.h"
#include "internal/SCShared.h"
#include "ios/NSWorkspaceWrapper.h"
#include "NSMouseProcessor.h"
#include <algorithm>

namespace SL {
    namespace Screen_Capture {
        static NSWorkspaceWrapper ptrWrapper;


        std::shared_ptr<Window> getWindow(const WindowData *window);

        std::shared_ptr<WindowDataRequest> windowDataRequest(const SingleWindowRequest &singleWindowRequest);

        bool RequestAccessibilityPermissions() { return ptrWrapper.requestAccessibilityPermissions(); }

        // InitWindows
        void InitWindows() {}

        std::shared_ptr<ShowingDesktopResult> ShowingDesktop(bool show) {
            // TODO
            return nullptr;
        }

        // Make window maximize
        std::shared_ptr<Window> Maximize(const SingleWindowRequest &windowRequest, bool maximize) {
            // not supported
            // should be calculated as a  fake fullscreen
            return nullptr;
        }

        // Make window minimize
        std::shared_ptr<Window> Visible(const SingleWindowRequest &windowRequest, bool visible) {
            std::shared_ptr<Window> windowPtr;
            std::shared_ptr<WindowData> window = ptrWrapper.minimize(*windowDataRequest(windowRequest), visible);
            if (window) {
                windowPtr = getWindow(window.get());
            }

            return windowPtr;
        }

        std::shared_ptr<Window> GetWindow(const SingleWindowRequest &windowRequests) {
            std::shared_ptr<Window> windowPtr;
            std::shared_ptr<WindowData> window = ptrWrapper.getWindow(*windowDataRequest(windowRequests));
            if (window) {
                windowPtr = getWindow(window.get());
            }
            return windowPtr;
        }

        int getTopmostProcess();

        std::shared_ptr<Window> GetActiveWindow(const SharedWindowsRequest &request) {
            std::shared_ptr<Window> windowPtr;
            int topMostPid = getTopmostProcess();
            if (topMostPid == 0) {
                return nullptr;
            }

            auto windows = ptrWrapper.determineWindows(topMostPid, request.RetrieveIcons);
            for (const auto window: *windows) {
                if (window.main && *window.main) {
                    windowPtr = getWindow(&window);
                    break;
                }
            }

            return windowPtr;
        }

        std::shared_ptr<std::vector<Window>> GetWindows(const SearchAllWindowsRequest &request) {
            auto windows = ptrWrapper.determineWindows(request.RetrieveIcons);
//            int option = kCGWindowListExcludeDesktopElements | kCGWindowListOptionOnScreenOnly;
//            auto windowList = CGWindowListCopyWindowInfo(option, kCGNullWindowID);
//            std::map<uint32_t, std::vector<uint32_t>> processToIndex;
//            CFIndex numWindows = CFArrayGetCount(windowList);
//            for (uint32_t i = 0; i < numWindows; i++) {
//                auto dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, i));
//                if (dict != NULL) {
//                    auto windowOwnerPID = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowOwnerPID));
//                    uint32_t windowPid;
//
//                    if (windowOwnerPID != NULL && CFNumberGetValue(windowOwnerPID, kCFNumberIntType, &windowPid)) {
//                        auto ownerHandle = static_cast<size_t>( windowPid);
//                        auto it = processToIndex.find(ownerHandle);
//                        if (it == processToIndex.end()) {
//                            auto iit = processToIndex.emplace(ownerHandle, std::vector<uint32_t>());
//                            it = iit.first;
//                        }
//                        it->second.emplace_back(i);
//                    }
//                }
//            }

            std::shared_ptr<std::vector<Window>> ret = std::make_shared<std::vector<Window>>();

            for (const auto window: *windows) {
                auto w = getWindow(&window);
//                auto it = processToIndex.find(w->OwnerHandle);
//                if (it != processToIndex.end()) {
//                    auto index = it->second;
//                    for (const auto &item: index) {
//                        auto dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, item));
//
//                        auto dims = static_cast<CFDictionaryRef>(CFDictionaryGetValue(dict, kCGWindowBounds));
//                        CGRect rect;
//                        CGRectMakeWithDictionaryRepresentation(dims, &rect);
//                        if (CGRectEqualToRect(window.rect, rect)) {
//                            uint32_t windowid = 0;
//                            CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowNumber)),
//                                             kCFNumberIntType, &windowid);
//                            w->Handle = windowid;
//                            break;
//                        }
//                    }
//                }

                ret->emplace_back(*w);
            }
//            CFRelease(windowList);
            return ret;
        }

        std::shared_ptr<Window> Topmost(const SingleWindowRequest &windowRequests) {
            std::shared_ptr<Window> windowPtr;
            std::shared_ptr<WindowData> window = ptrWrapper.topMost(*windowDataRequest(windowRequests));
            if (window) {
                windowPtr = getWindow(window.get());
            }

            return windowPtr;
        }

        std::shared_ptr<Window> Fullscreen(const SingleWindowRequest &windowRequests, int display, bool fullscreen) {
            std::shared_ptr<Window> windowPtr;
            std::shared_ptr<WindowData> window = ptrWrapper.fullscreen(*windowDataRequest(windowRequests), display,
                                                                       fullscreen);
            if (window) {
                windowPtr = getWindow(window.get());
            }

            return windowPtr;
        }

        std::shared_ptr<Window>
        Rectangle(const SingleWindowRequest &windowRequests, const Point &position, const SizeValues &size) {
            std::shared_ptr<Window> windowPtr;
            CGRect rect = {
                    {static_cast<float>(position.X), static_cast<float>(position.Y)},
                    {static_cast<float>(size.Width), static_cast<float>(size.Height)}
            };
            std::shared_ptr<WindowData> window = ptrWrapper.rectWindow(*windowDataRequest(windowRequests), rect);
            if (window) {
                windowPtr = getWindow(window.get());
            }

            return windowPtr;
        }

        std::shared_ptr<Window> Minimize(const SingleWindowRequest &singleWindowRequest, bool minimize) {
            std::shared_ptr<Window> windowPtr;
            std::shared_ptr<WindowData> window = ptrWrapper.minimize(*windowDataRequest(singleWindowRequest), minimize);
            if (window) {
                windowPtr = getWindow(window.get());
            }

            return windowPtr;
        }

        std::shared_ptr<Window> getWindow(const WindowData *window) {
            auto windowPtr = std::make_shared<Window>((Window) {});
            windowPtr->Position.X = static_cast<int>(window->rect.origin.x);
            windowPtr->Position.Y = static_cast<int>(window->rect.origin.y);

            windowPtr->Size.Width = static_cast<int>(window->rect.size.width);
            windowPtr->Size.Height = static_cast<int>(window->rect.size.height);
            windowPtr->OwnerHandle = static_cast<size_t>(window->pid);
            windowPtr->Handle = static_cast<size_t>(window->handle);

            if (window->title) {
                windowPtr->Name = *window->title;
            }
            auto ownerName = window->processTitle;
            if (ownerName != NULL) {
                windowPtr->ProcessName = *ownerName;
                WindowAttribute attributeClass;
                attributeClass.Code = std::string(S_RES_CLASS);
                attributeClass.Value = *ownerName;
                windowPtr->Attributes.emplace_back(attributeClass);

                WindowAttribute attributeName;
                attributeName.Code = std::string(S_RES_NAME);
                attributeName.Value = *ownerName;
                windowPtr->Attributes.emplace_back(attributeName);
            }

            if (window->minimized != NULL) {
                WindowAttribute attributeVisible;
                attributeVisible.Code = std::string(S_RES_VISIBLE);
                if (*window->minimized) {
                    attributeVisible.Value = std::string(S_FALSE);
                } else {
                    attributeVisible.Value = std::string(S_TRUE);
                }
                windowPtr->Attributes.emplace_back(attributeVisible);
            }
            if (window->execute != NULL) {
                WindowAttribute attribute;
                attribute.Code = std::string(S_RES_START_ABSOLUTE_URL);
                attribute.Value = *window->execute;
                windowPtr->Attributes.emplace_back(attribute);
            }

            if (window->icon != NULL) {
                WindowAttribute attribute;
                attribute.Code = std::string(S_RES_ICON_URL);
                attribute.Value = *window->icon;
                windowPtr->Attributes.emplace_back(attribute);
            }
            return windowPtr;
        }


        std::shared_ptr<WindowDataRequest> windowDataRequest(const SingleWindowRequest &singleWindowRequest) {
            auto windowRequestPtr = std::make_shared<WindowDataRequest>((WindowDataRequest) {});
            windowRequestPtr->handle = singleWindowRequest.Handle;
            windowRequestPtr->pid = singleWindowRequest.OwnerHandle;
            windowRequestPtr->rect = {
                    {static_cast<float>(singleWindowRequest.Position.X), static_cast<float>(singleWindowRequest.Position.Y)},
                    {static_cast<float>(singleWindowRequest.Size.Width), static_cast<float>(singleWindowRequest.Size.Height)}
            };
            windowRequestPtr->minimized = singleWindowRequest.Minimized;
            windowRequestPtr->fullscreen = singleWindowRequest.Fullscreen;

            windowRequestPtr->retrieveIcon = singleWindowRequest.RetrieveIcons;

            return windowRequestPtr;
        }


        int getTopmostProcess() {
            int topMostPid = 0;
            CFDictionaryRef dictTopMost = ptrWrapper.determineFrontmostApplication();
            CFNumberRef topMostPIDRef = static_cast<CFNumberRef>(CFDictionaryGetValue(dictTopMost, kWindowPid));
            if (topMostPIDRef == NULL) {
                ptrWrapper.releaseFrontmostApplication(dictTopMost);
                return 0;
            }
            CFNumberGetValue(topMostPIDRef, kCFNumberIntType, &topMostPid);
            ptrWrapper.releaseFrontmostApplication(dictTopMost);
            return topMostPid;
        }

    } // namespace Screen_Capture
} // namespace SL
