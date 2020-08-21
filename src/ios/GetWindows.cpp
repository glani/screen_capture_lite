#include "ScreenCapture.h"
#include <algorithm>
#include <ApplicationServices/ApplicationServices.h>
#include <map>
#include "ios/NSWorkspaceWrapper.h"

namespace SL {
    namespace Screen_Capture {
        static NSWorkspaceWrapper ptrWrapper;

        std::shared_ptr<std::string> getString(const CFStringRef &value);

        void determineScaleValues(float &xscale, float &yscale);

        std::shared_ptr<Window> getWindow(const __CFDictionary *dict, float xscale, float yscale);

        std::shared_ptr<Window> GetActiveWindow() {
            auto xscale = 1.0f;
            auto yscale = 1.0f;
            determineScaleValues(xscale, yscale);

            CFDictionaryRef dictTopMost = ptrWrapper.determineFrontmostApplication();
            CFNumberRef topMostPID = static_cast<CFNumberRef>(CFDictionaryGetValue(dictTopMost, kTopMostPID));
            int pid = 0;
            if (topMostPID != NULL) {
                CFNumberGetValue(topMostPID, kCFNumberIntType, &pid);
            }

            int option = kCGWindowListOptionOnScreenOnly;
            auto windowList = CGWindowListCopyWindowInfo(option, kCGNullWindowID);
            std::shared_ptr<Window> ret; // = std::make_shared<Window>();
            CFIndex numWindows = CFArrayGetCount(windowList);
            for (int i = 0; i < (int) numWindows; i++) {
                auto dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, i));
                auto windowOwnerPID = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowOwnerPID));
                int windowPid;
                if (windowOwnerPID != NULL && CFNumberGetValue(windowOwnerPID, kCFNumberIntType, &windowPid)) {
                    if (pid == windowPid) {
                        auto w = getWindow(dict, xscale, yscale);
                        if (w) {
                            ret = w;
                            CFStringRef localizedName = (CFStringRef) CFDictionaryGetValue(dictTopMost,
                                                                                           kTopMostLocalizedName);
                            if (localizedName != NULL) {
                                auto v = getString(localizedName);
                                if (v) {
                                    WindowAttribute attributeName;
                                    attributeName.Code = std::string("res_localized_name");
                                    attributeName.Value = *v;
                                    w->Attributes.emplace_back(attributeName);
                                }
                            }
                            break;
                        }
                    }
                }
            }
            CFRelease(windowList);
            ptrWrapper.releaseFrontmostApplication(dictTopMost);
            return ret;
        }

        std::shared_ptr<std::vector<Window>> GetWindows() {
            auto xscale = 1.0f;
            auto yscale = 1.0f;
            determineScaleValues(xscale, yscale);

            int option = kCGWindowListOptionAll;
            auto windowList = CGWindowListCopyWindowInfo(option, kCGNullWindowID);
            std::shared_ptr<std::vector<Window>> ret = std::make_shared<std::vector<Window>>();
            CFIndex numWindows = CFArrayGetCount(windowList);
            for (int i = 0; i < (int) numWindows; i++) {
                auto dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, i));
                if (dict != NULL) {
                    auto w = getWindow(dict, xscale, yscale);
                    if (w) {
                        ret->emplace_back(*w);
                    }
                }
            }
            CFRelease(windowList);

            return ret;
        }

        std::shared_ptr<Window> getWindow(
                const __CFDictionary *dict,
                float xscale,
                float yscale) {

            uint32_t windowid = 0;
            auto cfwindowname = static_cast<CFStringRef>(CFDictionaryGetValue(dict, kCGWindowName));
            if (cfwindowname == NULL) {
                return std::shared_ptr<Window>();
            }

            auto windowName = getString(cfwindowname);
            if (windowName == NULL) {
                return std::shared_ptr<Window>();
            }
            auto windowPtr = std::make_shared<Window>((Window) {});
            windowPtr->Name = *windowName;
            CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowNumber)),
                             kCFNumberIntType, &windowid);
            windowPtr->Handle = static_cast<size_t>(windowid);

            auto dims = static_cast<CFDictionaryRef>(CFDictionaryGetValue(dict, kCGWindowBounds));
            CGRect rect;
            CGRectMakeWithDictionaryRepresentation(dims, &rect);
            windowPtr->Position.x = static_cast<int>(rect.origin.x);
            windowPtr->Position.y = static_cast<int>(rect.origin.y);

            windowPtr->Size.x = static_cast<int>(rect.size.width * xscale);
            windowPtr->Size.y = static_cast<int>(rect.size.height * yscale);

            auto windowOwnerPID = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowOwnerPID));
            uint32_t windowPid;

            if (windowOwnerPID != NULL && CFNumberGetValue(windowOwnerPID, kCFNumberIntType, &windowPid)) {
                windowPtr->OwnerHandle = static_cast<size_t>( windowPid);
            } else {
                windowPtr->OwnerHandle = 0;
            }

            auto ownerName = static_cast<CFStringRef>(CFDictionaryGetValue(dict, kCGWindowOwnerName));
            if (ownerName != NULL) {
                auto ownerNameValue = getString(ownerName);
                if (ownerNameValue) {
                    WindowAttribute attributeClass;
                    attributeClass.Code = std::string("res_class");
                    attributeClass.Value = *ownerNameValue;
                    windowPtr->Attributes.emplace_back(attributeClass);

                    WindowAttribute attributeName;
                    attributeName.Code = std::string("res_name");
                    attributeName.Value = *ownerNameValue;
                    windowPtr->Attributes.emplace_back(attributeName);
                }
            }

            WindowAttribute attributeVisible;
            attributeVisible.Code = std::string("res_visible");

            auto isOnScreen = static_cast<CFBooleanRef>(CFDictionaryGetValue(dict, kCGWindowIsOnscreen));
            if (isOnScreen != NULL && CFBooleanGetValue(isOnScreen)) {
                attributeVisible.Value = std::string("TRUE");
            } else {
                attributeVisible.Value = std::string("FALSE");
            }
            windowPtr->Attributes.emplace_back(attributeVisible);
            return windowPtr;
        }

        void determineScaleValues(float &xscale, float &yscale) {
            CGDisplayCount count = 0;
            CGGetActiveDisplayList(0, 0, &count);
            std::vector<CGDirectDisplayID> displays;
            displays.resize(count);
            CGGetActiveDisplayList(count, displays.data(), &count);


            for (auto i = 0; i < count; i++) {
                //only include non-mirrored displays
                if (CGDisplayMirrorsDisplay(displays[i]) == kCGNullDirectDisplay) {

                    auto dismode = CGDisplayCopyDisplayMode(displays[i]);
                    auto scaledsize = CGDisplayBounds(displays[i]);

                    auto pixelwidth = CGDisplayModeGetPixelWidth(dismode);
                    auto pixelheight = CGDisplayModeGetPixelHeight(dismode);

                    CGDisplayModeRelease(dismode);

                    if (scaledsize.size.width != pixelwidth) {//scaling going on!
                        xscale = static_cast<float>(pixelwidth) / static_cast<float>(scaledsize.size.width);
                    }
                    if (scaledsize.size.height != pixelheight) {//scaling going on!
                        yscale = static_cast<float>(pixelheight) / static_cast<float>(scaledsize.size.height);
                    }
                    break;
                }
            }
        }

        std::shared_ptr<std::string> getString(const CFStringRef &value) {
            CFIndex bufferSize = 2 * (CFStringGetLength(value) + 1);
            char *buffer = new char[bufferSize];

            std::string result;
            if (CFStringGetCString(value, buffer, bufferSize, kCFStringEncodingUTF8)) {
                auto result = std::make_shared<std::string>(buffer);
                delete[] buffer;
                return result;
            }
            delete[] buffer;
            return std::shared_ptr<std::string>();
        }
    }
}
