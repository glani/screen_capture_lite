#include "NSWorkspaceWrapperm.h"
#import "SCAccessibilityElement.h"
#include "NSWorkspaceWrapperShared.h"
#include "Utils.h"
#import <AppKit/AppKit.h>
#include <map>
#include <vector>
#include <iostream>

extern const CFStringRef kWindowPid = CFSTR("kWindowPid");
extern const CFStringRef kWindowAbsoluteUrlString = CFSTR("kWindowAbsoluteUrlString");
extern const CFStringRef kWindowIconString = CFSTR("kWindowIconString");
extern const CFStringRef kWindowLocalizedName = CFSTR("kWindowLocalizedName");
extern const CFStringRef kWindowRectangle = CFSTR("kWindowRectangle");


@implementation NSWorkspaceWrapper
- (void)Init {
    self = [super init];
}

- (void)releaseFrontmostApplication:(CFDictionaryRef)query {
    CFRelease(query);
}

- (void)releaseApplicationValues:(NSArray *)values {
    [values release];
}

- (NSArray *)determineApplicationValues {
    @autoreleasepool {
        auto pids = [[NSMutableArray alloc] init];
#if 0
        NSArray *appsArray = [[NSWorkspace sharedWorkspace] runningApplications];
        for (NSRunningApplication *a in appsArray) {
            auto pid = a.processIdentifier;
            [pids addObject:[NSNumber numberWithInteger:pid]];
        }
        [appsArray release];
#else
        NSMutableSet *pidSet = [[NSMutableSet alloc] init];
//        auto windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
        auto windowList = CGWindowListCopyWindowInfo(kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        CFIndex numWindows = CFArrayGetCount(windowList);
        for (int i = 0; i < (int) numWindows; i++) {
            auto dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, i));
            auto windowOwnerPID = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowOwnerPID));
            int windowPid;
            if (windowOwnerPID != NULL && CFNumberGetValue(windowOwnerPID, kCFNumberIntType, &windowPid)) {
                [pidSet addObject:[NSNumber numberWithInteger:windowPid]];
            }
        }
        for (NSNumber *pId in pidSet) {
            [pids addObject:pId];
        }
        CFRelease(pidSet);
        CFRelease(windowList);
#endif
        return pids;
    }
}

- (NSArray *)determineWindowsByProcessId:(pid_t)pid {
    auto windows = [[NSMutableArray alloc] init];

    auto accessibilityElementWindows = [SCAccessibilityElement createApplicationElement:pid];
    if (accessibilityElementWindows) {

        auto accessibilityElementAllWindows = [accessibilityElementWindows retrieveAllWindows];
        for (SCAccessibilityElement *window in [accessibilityElementAllWindows getWindowsList]) {
            SCWindowData *windowSCData = [self createWindowFromSCAccessibilityElement:pid :accessibilityElementAllWindows :window];
            if (windowSCData) {
                [windows addObject:windowSCData];
            }
            // no [window release]: the elements belong to the windows list (-deallocWindowsList releases them)
        }
        [accessibilityElementAllWindows release];
    }

    [accessibilityElementWindows release];

    return windows;
}

- (SCWindowData *)createWindowFromSCAccessibilityElement:(pid_t)pid :(SCAccessibilityElementWindows *)mainProcess :(SCAccessibilityElement *)element {
    auto window = [[SCWindowData alloc] init];
    window.pid = pid;

    NSString *processTitle = [mainProcess getProcessTitle];
    if (processTitle) {
        window.processTitle = (CFStringRef) CFRetain((__bridge CFStringRef) processTitle);
    } else {
        window.processTitle = nil;
    }
    [element enrich:window];
    return window;
}

- (Boolean)requestAccessibilityPermissions {
    @autoreleasepool {
        SCAccessibilityElement *accessibilityElement = nullptr;
        accessibilityElement = [SCAccessibilityElement frontmostApplicationElement];
        if (!accessibilityElement) {
            auto pids = [self determineApplicationValues];
            for (id number in pids) {
                pid_t pid;
                CFNumberGetValue((CFNumberRef) number, kCFNumberIntType, &pid);
                accessibilityElement = [SCAccessibilityElement createApplicationElement:pid];
                if (accessibilityElement) {
                    break;
                }
            }
        }

        if (accessibilityElement) {
            auto res = [accessibilityElement permissionsRequest];
            CFRelease(accessibilityElement);
            return res;
        }
    }

    return FALSE;
}

- (CFDictionaryRef)determineFrontmostApplication {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    @autoreleasepool {
        auto application = [[NSWorkspace sharedWorkspace] frontmostApplication];

        if (application) {
            auto pid = application.processIdentifier;

            auto executableURL = [application executableURL];
            if (executableURL) {
                CFStringRef absoluteString =
                        CFStringCreateWithCString((CFAllocatorRef) NULL, [executableURL absoluteString].UTF8String,
                                                  kCFStringEncodingUTF8);
                CFDictionarySetValue(query, kWindowAbsoluteUrlString, absoluteString);

                CFRelease(absoluteString);
            }
            CFStringRef localizedName =
                    CFStringCreateWithCString((CFAllocatorRef) NULL, [application localizedName].UTF8String,
                                              kCFStringEncodingUTF8);
            CFNumberRef processIdentifier = CFNumberCreate((CFAllocatorRef) NULL, kCFNumberIntType, &pid);
            CFDictionarySetValue(query, kWindowPid, processIdentifier);
            CFDictionarySetValue(query, kWindowLocalizedName, localizedName);

            CFRelease(localizedName);
            CFRelease(processIdentifier);
        }
    }

    return query;
}

- (void)dealloc {
    [super dealloc];
}

- (SCWindowData *)minimizeWindow:(NSWindowDataRequest *)request :(bool)minimize {
    SCWindowData *res = nil;
    int pid = [request pid];
    auto appElement = [SCAccessibilityElement createApplicationElement:pid];
    if (appElement) {
        SCAccessibilityElementWindows *windowsElement = [appElement retrieveAllWindows];
        SCAccessibilityElement *windowRes = nil;
        NSArray *windowsList = [windowsElement getWindowsList];
        for (SCAccessibilityElement *window in windowsList) {
            if ([window determineRequiredWindow:request :[windowsList count] == 1]) {
                windowRes = window;
                break;
            }
        }
        if (windowRes) {
            [windowRes minimizeWindow:minimize];
            res = [self createWindowFromSCAccessibilityElement:pid :windowsElement :windowRes];
        }
        [windowsElement release];
        [appElement release]; // +1 from createApplicationElement
    }
    return res;
}

- (SCWindowData *)fullscreenWindow:(NSWindowDataRequest *)request :(int)display :(bool)fullscreen {
    SCWindowData *res = nil;
    int pid = [request pid];
    auto appElement = [SCAccessibilityElement createApplicationElement:pid];
    if (appElement) {
        SCAccessibilityElementWindows *windowsElement = [appElement retrieveAllWindows];
        SCAccessibilityElement *windowRes = nil;
        NSArray *windowsList = [windowsElement getWindowsList];
        for (SCAccessibilityElement *window in windowsList) {
            if ([window determineRequiredWindow:request :[windowsList count] == 1]) {
                windowRes = window;
                break;
            }
        }
        if (windowRes) {
            [windowRes fullscreenWindow:display :fullscreen];
            res = [self createWindowFromSCAccessibilityElement:pid :windowsElement :windowRes];
        }
        [windowsElement release];
        [appElement release]; // +1 from createApplicationElement
    }
    return res;
}

- (SCWindowData *)setRectWindow:(NSWindowDataRequest *)request :(CGRect)rect {
    SCWindowData *res = nil;
    int pid = [request pid];
    auto appElement = [SCAccessibilityElement createApplicationElement:pid];
    if (appElement) {
        SCAccessibilityElementWindows *windowsElement = [appElement retrieveAllWindows];
        SCAccessibilityElement *windowRes = nil;
        NSArray *windowsList = [windowsElement getWindowsList];
        for (SCAccessibilityElement *window in windowsList) {
            if ([window determineRequiredWindow:request :[windowsList count] == 1]) {
                windowRes = window;
                break;
            }
        }
        if (windowRes) {
            [windowRes setRectOfElement:rect];
            res = [self createWindowFromSCAccessibilityElement:pid :windowsElement :windowRes];
        }
        [windowsElement release];
        [appElement release]; // +1 from createApplicationElement
    }
    return res;
}

- (bool)closeWindow:(NSWindowDataRequest *)request:(bool)force {
    int pid = [request pid];
    auto appElement = [SCAccessibilityElement createApplicationElement:pid];
    if (appElement) {
        SCAccessibilityElementWindows *windowsElement = [appElement retrieveAllWindows];
        SCAccessibilityElement *windowRes = nil;
        NSArray *windowsList = [windowsElement getWindowsList];
        for (SCAccessibilityElement *window in windowsList) {
            if ([window determineRequiredWindow:request :[windowsList count] == 1]) {
                windowRes = window;
                break;
            }
        }
        bool res = false;
        if (windowRes) {
            res = [windowRes closeWindow:force];
        }
        [windowsElement release];
        [appElement release]; // +1 from createApplicationElement
        return res;
    }
    return false;
}

- (SCWindowData *)topMostWindow:(NSWindowDataRequest *)request {
    SCWindowData *res = nil;
    int pid = [request pid];
    auto appElement = [SCAccessibilityElement createApplicationElement:pid];
    if (appElement) {
        SCAccessibilityElementWindows *windowsElement = [appElement retrieveAllWindows];
        SCAccessibilityElement *windowRes = nil;
        NSArray *windowsList = [windowsElement getWindowsList];
        for (SCAccessibilityElement *window in windowsList) {
            if ([window determineRequiredWindow:request :[windowsList count] == 1]) {
                windowRes = window;
                break;
            }
        }
        if (windowRes) {
            [windowRes topMostWindow];
            res = [self createWindowFromSCAccessibilityElement:pid :windowsElement :windowRes];
        }
        [windowsElement release];
        [appElement release]; // +1 from createApplicationElement
    }
    return res;
}

- (SCWindowData *)getWindow:(NSWindowDataRequest *)request {
    SCWindowData *res = nil;
    int pid = [request pid];
    auto appElement = [SCAccessibilityElement createApplicationElement:pid];
    if (appElement) {
        SCAccessibilityElementWindows *windowsElement = [appElement retrieveAllWindows];
        SCAccessibilityElement *windowRes = nil;
        NSArray *windowsList = [windowsElement getWindowsList];
        for (SCAccessibilityElement *window in windowsList) {
            if ([window determineRequiredWindow:request :[windowsList count] == 1]) {
                windowRes = window;
                break;
            }
        }
        if (windowRes) {
            res = [self createWindowFromSCAccessibilityElement:pid :windowsElement :windowRes];
        }
        [windowsElement release];
        [appElement release]; // +1 from createApplicationElement
    }
    return res;
}
@end

namespace SL {
    namespace Screen_Capture {

        struct NSWorkspaceWrapperImpl {
            NSWorkspaceWrapper *ptr = nullptr;

            NSWorkspaceWrapperImpl() { ptr = [[NSWorkspaceWrapper alloc] init]; }

            ~NSWorkspaceWrapperImpl() {
                if (ptr) {
                    auto r = CFGetRetainCount(ptr);
                    while (r != 1) {
                        r = CFGetRetainCount(ptr);
                    }
                    [ptr release];
                    ptr = nullptr;
                }
            }

            CFDictionaryRef determineFrontmostApplication() { return [ptr determineFrontmostApplication]; }

            Boolean requestAccessibilityPermissions() { return [ptr requestAccessibilityPermissions]; }

            std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValues() {
                std::shared_ptr<std::map<int, struct ApplicationData>> data = std::make_shared<std::map<int, struct ApplicationData>>();
                NSArray *values = [ptr determineApplicationValues];
                for (NSNumber *value in values) {
                    struct ApplicationData ad;
                    ad.pid = value.intValue;
                    data->emplace(std::make_pair(ad.pid, ad));
                }

                [ptr releaseApplicationValues:values];
                return data;
            };

            void releaseFrontmostApplication(CFDictionaryRef query) { [ptr releaseFrontmostApplication:query]; }

            void releaseApplicationValues(NSArray *values) { [ptr releaseApplicationValues:values]; }

            std::shared_ptr<struct WindowData> minimizeWindow(const WindowDataRequest &windowRequest, bool minimize) {
                int pid = windowRequest.pid;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                std::shared_ptr<std::string> execute;
                if (application) {
                    std::shared_ptr<std::string> icon;
                    if (windowRequest.retrieveIcon) {
                        icon = retrieveIconFromApplication(application);
                    }
                    std::shared_ptr<std::string> execute = retrieveExecutableURL(application);
                    NSWindowDataRequest *windowDataRequest = createNSWindowDataRequest(windowRequest);
                    SCWindowData *window = [ptr minimizeWindow:windowDataRequest :minimize];
                    [windowDataRequest release];
                    if (window) {
                        auto windows = [[NSMutableArray alloc] init];
                        [windows addObject:window];
                        std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
                        this->processWindows(windows, pid, data.get(), execute, icon);
                        CFRelease(windows);
                        if (!data->empty()) {
                            return std::make_shared<struct WindowData>(data->at(0));
                        }
                    }
                }
                return nullptr;
            }

            std::shared_ptr<struct WindowData>
            fullscreenWindow(const WindowDataRequest &windowRequest, int display, bool fullscreen) {
                int pid = windowRequest.pid;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                std::shared_ptr<std::string> execute;
                if (application) {
                    std::shared_ptr<std::string> icon;
                    if (windowRequest.retrieveIcon) {
                        icon = retrieveIconFromApplication(application);
                    }
                    std::shared_ptr<std::string> execute = retrieveExecutableURL(application);

                    NSWindowDataRequest *windowDataRequest = createNSWindowDataRequest(windowRequest);
                    SCWindowData *window = [ptr fullscreenWindow:windowDataRequest :display :fullscreen];
                    [windowDataRequest release];
                    if (window) {
                        auto windows = [[NSMutableArray alloc] init];
                        [windows addObject:window];
                        std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
                        this->processWindows(windows, pid, data.get(), execute, icon);
                        CFRelease(windows);
                        if (!data->empty()) {
                            return std::make_shared<struct WindowData>(data->at(0));
                        }
                    }
                }
                return nullptr;
            }


            std::shared_ptr<struct NativeWindowData> nativeWindowInfo(char* data, size_t sz) {
                auto res = std::make_shared<struct NativeWindowData>((NativeWindowData){});
                NSView* view = *reinterpret_cast<NSView**>(data);
                NSWindow *window = [view window];
                if (window) {
                    res->handle = [window windowNumber];
                } else {
                    res->handle = 0;
                }
                res->pid = 0;
                return res;
            }

            std::shared_ptr<struct WindowData> getWindow(const WindowDataRequest &windowRequest) {
                int pid = windowRequest.pid;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                std::shared_ptr<std::string> execute;
                if (application) {
                    std::shared_ptr<std::string> icon;
                    if (windowRequest.retrieveIcon) {
                        icon = retrieveIconFromApplication(application);
                    }
                    std::shared_ptr<std::string> execute = retrieveExecutableURL(application);
                    NSWindowDataRequest *windowDataRequest = createNSWindowDataRequest(windowRequest);
                    SCWindowData *window = [ptr getWindow:windowDataRequest];
                    [windowDataRequest release];
                    if (window) {
                        auto windows = [[NSMutableArray alloc] init];
                        [windows addObject:window];
                        std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
                        this->processWindows(windows, pid, data.get(), execute, icon);
                        CFRelease(windows);
                        if (!data->empty()) {
                            return std::make_shared<struct WindowData>(data->at(0));
                        }
                    }
                }
                return nullptr;
            }

            bool closeWindow(const WindowDataRequest& windowRequest, bool force) {
                int pid = windowRequest.pid;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                if (application) {
                    NSWindowDataRequest *windowDataRequest = createNSWindowDataRequest(windowRequest);
                    bool res = [ptr closeWindow:windowDataRequest:force];
                    [windowDataRequest release];
                    return res;
                }
                return false;
            }

            bool closeAllWindows(const WindowDataRequest& windowRequest, bool force) {
                int pid = windowRequest.pid;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                if (application) {
                    if (force) {
                        return [application forceTerminate];
                    } else {
                        return [application terminate];
                    }
                }
                return false;
            }

            std::shared_ptr<struct WindowData> activateWindow(const WindowDataRequest &windowRequest) {
                int pid = windowRequest.pid;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                std::shared_ptr<std::string> execute;
                if (application) {
                    [application activateWithOptions:NSApplicationActivateIgnoringOtherApps];
                    std::shared_ptr<std::string> icon;
                    if (windowRequest.retrieveIcon) {
                        icon = retrieveIconFromApplication(application);
                    }
                    std::shared_ptr<std::string> execute = retrieveExecutableURL(application);
                    NSWindowDataRequest *windowDataRequest = createNSWindowDataRequest(windowRequest);
                    SCWindowData *window = [ptr topMostWindow:windowDataRequest];
                    [windowDataRequest release];
                    if (window) {
                        auto windows = [[NSMutableArray alloc] init];
                        [windows addObject:window];
                        std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
                        this->processWindows(windows, pid, data.get(), execute, icon);
                        CFRelease(windows);
                        if (!data->empty()) {
                            return std::make_shared<struct WindowData>(data->at(0));
                        }
                    }
                }
                return nullptr;
            }

            std::shared_ptr<struct WindowData> topMostWindow(const WindowDataRequest &windowRequest) {
                int pid = windowRequest.pid;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                std::shared_ptr<std::string> execute;
                if (application) {
                    [application activateWithOptions:NSApplicationActivateIgnoringOtherApps];
                    std::shared_ptr<std::string> icon;
                    if (windowRequest.retrieveIcon) {
                        icon = retrieveIconFromApplication(application);
                    }
                    std::shared_ptr<std::string> execute = retrieveExecutableURL(application);
                    NSWindowDataRequest *windowDataRequest = createNSWindowDataRequest(windowRequest);
                    SCWindowData *window = [ptr topMostWindow:windowDataRequest];
                    [windowDataRequest release];
                    if (window) {
                        auto windows = [[NSMutableArray alloc] init];
                        [windows addObject:window];
                        std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
                        this->processWindows(windows, pid, data.get(), execute, icon);
                        CFRelease(windows);
                        if (!data->empty()) {
                            return std::make_shared<struct WindowData>(data->at(0));
                        }
                    }
                }
                return nullptr;
            }

            std::shared_ptr<struct WindowData> rectWindow(const WindowDataRequest &windowRequest, const CGRect &rect) {
                int pid = windowRequest.pid;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                std::shared_ptr<std::string> execute;
                if (application) {
                    std::shared_ptr<std::string> icon;
                    if (windowRequest.retrieveIcon) {
                        icon = retrieveIconFromApplication(application);
                    }
                    std::shared_ptr<std::string> execute = retrieveExecutableURL(application);
                    NSWindowDataRequest *windowDataRequest = createNSWindowDataRequest(windowRequest);
                    SCWindowData *window = [ptr setRectWindow:windowDataRequest :rect];
                    [windowDataRequest release];
                    if (window) {
                        auto windows = [[NSMutableArray alloc] init];
                        [windows addObject:window];
                        std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
                        this->processWindows(windows, pid, data.get(), execute, icon);
                        CFRelease(windows);
                        if (!data->empty()) {
                            return std::make_shared<struct WindowData>(data->at(0));
                        }
                    }
                }
                return nullptr;
            }

            std::shared_ptr<std::vector<struct WindowData>> determineWindows(bool retrieveIcon) {
                std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
                NSArray *values = [ptr determineApplicationValues];
                for (NSNumber *value in values) {
                    int pid = value.intValue;
                    auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                    std::shared_ptr<std::string> execute;
                    if (application) {
                        std::shared_ptr<std::string> execute;
                        std::shared_ptr<std::string> icon;
                        if (retrieveIcon) {
                            icon = retrieveIconFromApplication(application);
                        }
                        execute = retrieveExecutableURL(application);
                        NSArray *windows = [ptr determineWindowsByProcessId:pid];
                        this->processWindows(windows, pid, data.get(), execute, icon);
                        CFRelease(windows);
                    }
                }

                [ptr releaseApplicationValues:values];
                return data;
            }

            std::shared_ptr<std::vector<struct WindowData>> determineWindows(int pid, bool retrieveIcon) {
                std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
                NSArray *windows = [ptr determineWindowsByProcessId:pid];
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                if (application) {
                    std::shared_ptr<std::string> execute;
                    execute = retrieveExecutableURL(application);
                    std::shared_ptr<std::string> icon;
                    if (retrieveIcon) {
                        icon = retrieveIconFromApplication(application);
                    }
                    this->processWindows(windows, pid, data.get(), execute, icon);
                } else {
                    // processWindows hands back the +1 of each SCWindowData; without it they are released here
                    for (SCWindowData *window in windows) {
                        CFRelease(window);
                    }
                }
                CFRelease(windows);
                return data;
            }

            std::shared_ptr<std::string> retrieveExecutableURL(NSRunningApplication *application) {
                std::shared_ptr<std::string> execute;
                auto executableURL = [application executableURL];
                if (executableURL) {
                    CFStringRef valueCSStr = (__bridge CFStringRef) [executableURL absoluteString];
                    execute = getString(valueCSStr);
                }
                return execute;
            }

            std::shared_ptr<std::string> retrieveIconFromApplication(NSRunningApplication *application) {
                std::shared_ptr<std::string> icon;
                auto image = [application icon];
                auto bundle = [application bundleIdentifier];

                if (image) {
                    @autoreleasepool {
                        const CGRect &rect = CGRectMake(0, 0, 512, 512);
                        NSBitmapImageRep *imageHighRes = [image bestRepresentationForRect:rect context:nil hints:nil];
                        if (imageHighRes) {
                            // imageHighRes is +0 (owned by image); img (+1) is ours. Before, img leaked and an extra
                            // [imageHighRes autorelease] was balanced only by that leak (leak report H2).
                            auto img = [[NSImage alloc] initWithSize:[imageHighRes size]];
                            [img autorelease];
                            [img addRepresentation:imageHighRes];

                            CGImageRef cgRef = [img CGImageForProposedRect:NULL context:nil hints:nil];
                            auto newRep = [[NSBitmapImageRep alloc] initWithCGImage:cgRef];
                            [newRep setSize:[img size]];

                            NSData *pngData = [newRep representationUsingType:NSPNGFileType properties:nil];
                            auto encodedString = [pngData base64Encoding];

                            NSString *value = [NSString stringWithFormat:@"data:image/png;base64,%@", encodedString];
                            CFStringRef valueCSStr = (__bridge CFStringRef) value;
                            icon = getString(valueCSStr);
                            [newRep autorelease];
                        } else {
                            CGImageRef cgRef = [image CGImageForProposedRect:NULL context:nil hints:nil];
                            NSBitmapImageRep *newRep = [[NSBitmapImageRep alloc] initWithCGImage:cgRef];
                            [newRep setSize:[image size]];
                            NSData *pngData = [newRep representationUsingType:NSPNGFileType properties:nil];
                            auto encodedString = [pngData base64Encoding];
                            NSString *value = [NSString stringWithFormat:@"data:image/png;base64,%@", encodedString];
                            CFStringRef valueCSStr = (__bridge CFStringRef) value;
                            icon = getString(valueCSStr);
                            [newRep autorelease];
                        }
                    }
                }
                return icon;
            }


            void processWindows(NSArray *windows, int pid, std::vector<struct WindowData> *data,
                                std::shared_ptr<std::string> &execute,
                                std::shared_ptr<std::string> &icon) {
                int i = 0;
                for (SCWindowData *window in windows) {
                    struct WindowData wd;
                    wd.handle = [window handle];
                    i++;
                    wd.pid = pid;
                    wd.rect = [window rect];

                    CFStringRef processTitle = [window processTitle];
                    if (processTitle != nullptr) {
                        wd.processTitle = getString(processTitle);
                    }

                    CFStringRef title = [window title];
                    if (title != nullptr) {
                        wd.title = getString(title);
                    }

                    CFBooleanRef focused = [window focused];
                    if (focused != nullptr) {
                        wd.focused = std::make_shared<bool>(CFBooleanGetValue(focused));
                    }

                    CFBooleanRef main = [window main];
                    if (main != nullptr) {
                        wd.main = std::make_shared<bool>(CFBooleanGetValue(main));
                    }

                    CFBooleanRef minimized = [window minimized];
                    if (minimized != nullptr) {
                        wd.minimized = std::make_shared<bool>(CFBooleanGetValue(minimized));
                    }

                    CFBooleanRef fullscreen = [window fullscreen];
                    if (fullscreen != nullptr) {
                        wd.fullscreen = std::make_shared<bool>(CFBooleanGetValue(fullscreen));
                    }

                    if (execute) {
                        wd.execute = execute;
                    }

                    wd.icon = icon;

                    CFRelease(window);
                    data->push_back(wd);
                }
            }

            NSWindowDataRequest *createNSWindowDataRequest(const WindowDataRequest &request) {
                auto res = [[NSWindowDataRequest alloc] init];
                [res setFullscreen:request.fullscreen];
                [res setMinimized:request.minimized];
                [res setHandle:request.handle];
                [res setPid:request.pid];
                [res setRect:request.rect];
                return res;
            }
        };

        NSWorkspaceWrapperImpl *CreateNSWorkspaceWrapperImpl() { return new NSWorkspaceWrapperImpl(); }

        void DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl *p) {
            if (p) {
                delete p;
            }
        }

        // The C++ entry points (NSWorkspaceWrapper.cpp). They run on libuv threadpool threads, which have no autorelease
        // pool: without one, everything autoreleased here (NSRunningApplication, executableURL strings, window titles,
        // icons) stayed until the thread exited, i.e. forever. A pool at this outermost boundary only: everything handed
        // back is C++ by then (getString copies), and nothing inside depends on a pool draining earlier (leak report H4).
        CFDictionaryRef
        determineFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p) { @autoreleasepool { return p->determineFrontmostApplication(); } }

        Boolean
        requestAccessibilityPermissionsWrap(NSWorkspaceWrapperImpl *p) { @autoreleasepool { return p->requestAccessibilityPermissions(); } }

        std::shared_ptr<std::map<int, struct ApplicationData>>
        determineApplicationValuesWrap(NSWorkspaceWrapperImpl *p) {
            @autoreleasepool { return p->determineApplicationValues(); }
        }

        std::shared_ptr<std::vector<struct WindowData>>
        determineWindowsWrap(NSWorkspaceWrapperImpl *p, int pid, bool retrieveIcon) {
            @autoreleasepool { return p->determineWindows(pid, retrieveIcon); }
        }

        std::shared_ptr<std::vector<struct WindowData>>
        determineWindowsWrap(NSWorkspaceWrapperImpl *p, bool retrieveIcon) { @autoreleasepool { return p->determineWindows(retrieveIcon); } }

        std::shared_ptr<struct WindowData>
        minimizeWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest &windowRequest, bool minimize) {
            @autoreleasepool { return p->minimizeWindow(windowRequest, minimize); }
        }

        std::shared_ptr<struct WindowData>
        fullscreenWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest &windowRequest, int display,
                             bool fullscreen) {
            @autoreleasepool { return p->fullscreenWindow(windowRequest, display, fullscreen); }
        }

        std::shared_ptr<struct WindowData>
        rectWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest &windowRequest, const CGRect &rect) {
            @autoreleasepool { return p->rectWindow(windowRequest, rect); }
        }

        bool closeWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest& windowRequest, bool force) {
            @autoreleasepool { return p->closeWindow(windowRequest, force); }
        }

        bool closeAllWindowsWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest& windowRequest, bool force) {
            @autoreleasepool { return p->closeAllWindows(windowRequest, force); }
        }


        std::shared_ptr<struct WindowData>
        topMostWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest &windowRequest) {
            @autoreleasepool { return p->topMostWindow(windowRequest); }
        }

        std::shared_ptr<struct WindowData>
        activateWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest &windowRequest) {
            @autoreleasepool { return p->activateWindow(windowRequest); }
        }

        std::shared_ptr<struct WindowData>
        getWindowWrap(NSWorkspaceWrapperImpl *p, const WindowDataRequest &windowRequest) {
            @autoreleasepool { return p->getWindow(windowRequest); }
        }


        std::shared_ptr<struct NativeWindowData>
        nativeWindowInfoWrap(NSWorkspaceWrapperImpl *p, char* data, size_t sz) {
            @autoreleasepool { return p->nativeWindowInfo(data, sz); }
        }


        void releaseFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p,
                                             CFDictionaryRef query) { p->releaseFrontmostApplication(query); }
    } // namespace Screen_Capture
} // namespace SL