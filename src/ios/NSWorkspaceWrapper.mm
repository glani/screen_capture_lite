#include "NSWorkspaceWrapperm.h"
#import "SCAccessibilityElement.h"
#include "NSWorkspaceWrapperShared.h"
#include "Utils.h"
#import <AppKit/AppKit.h>
#include <map>
#include <vector>

extern const CFStringRef kWindowPid = CFSTR("kWindowPid");
extern const CFStringRef kWindowAbsoluteUrlString = CFSTR("kWindowAbsoluteUrlString");
extern const CFStringRef kWindowIconString = CFSTR("kWindowIconString");
extern const CFStringRef kWindowLocalizedName = CFSTR("kWindowLocalizedName");
extern const CFStringRef kWindowRectangle = CFSTR("kWindowRectangle");

@implementation NSWorkspaceWrapper
- (void)Init
{
    self = [super init];
}

- (void)releaseFrontmostApplication:(CFDictionaryRef)query
{
    CFRelease(query);
}

- (void)releaseApplicationValues:(NSArray *)values
{
    [values release];
}

- (NSArray *)determineApplicationValues
{
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
        auto windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
        CFIndex numWindows = CFArrayGetCount(windowList);
        for (int i = 0; i < (int)numWindows; i++) {
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

- (NSArray *)determineWindowsByProcessId:(pid_t)pid
{
    auto windows = [[NSMutableArray alloc] init];

    auto accessibilityElementWindows = [SCAccessibilityElement createApplicationElement:pid];
    if (accessibilityElementWindows) {

        auto accessibilityElementAllWindows = [accessibilityElementWindows retrieveAllWindows];
        for (SCAccessibilityElement *window in [accessibilityElementAllWindows getWindowsList]) {
            SCWindowData *windowSCData = [self createWindowFromSCAccessibilityElement:pid:accessibilityElementAllWindows:window];
            if (windowSCData) {
                [windows addObject:windowSCData];
            }
            [window release];
        }
        [accessibilityElementAllWindows release];
    }

    [accessibilityElementWindows release];

    return windows;
}
- (SCWindowData *)createWindowFromSCAccessibilityElement:(pid_t)pid:(SCAccessibilityElementWindows *)mainProcess:(SCAccessibilityElement *)element
{

    auto window = [[SCWindowData alloc] init];
    window.pid = pid;
    NSString *processTitle = [mainProcess getProcessTitle];
    if (processTitle) {
        window.processTitle = (CFStringRef)CFRetain((__bridge CFStringRef)processTitle);
    }
    else {
        window.processTitle = nil;
    }
    [element enrich:window];
    return window;
}

- (Boolean)requestAccessibilityPermissions
{
    @autoreleasepool {
        SCAccessibilityElement *accessibilityElement = nullptr;
        accessibilityElement = [SCAccessibilityElement frontmostApplicationElement];
        if (!accessibilityElement) {
            auto pids = [self determineApplicationValues];
            for (id number in pids) {
                pid_t pid;
                CFNumberGetValue((CFNumberRef)number, kCFNumberIntType, &pid);
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

- (CFDictionaryRef)determineFrontmostApplication
{
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    @autoreleasepool {
        auto application = [[NSWorkspace sharedWorkspace] frontmostApplication];

        if (application) {
            auto pid = application.processIdentifier;

            auto executableURL = [application executableURL];
            if (executableURL) {
                CFStringRef absoluteString =
                    CFStringCreateWithCString((CFAllocatorRef)NULL, [executableURL absoluteString].UTF8String, kCFStringEncodingUTF8);
                CFDictionarySetValue(query, kWindowAbsoluteUrlString, absoluteString);

                CFRelease(absoluteString);
            }
            CFStringRef localizedName =
                CFStringCreateWithCString((CFAllocatorRef)NULL, [application localizedName].UTF8String, kCFStringEncodingUTF8);
            CFNumberRef processIdentifier = CFNumberCreate((CFAllocatorRef)NULL, kCFNumberIntType, &pid);
            CFDictionarySetValue(query, kWindowPid, processIdentifier);
            CFDictionarySetValue(query, kWindowLocalizedName, localizedName);

            CFRelease(localizedName);
            CFRelease(processIdentifier);
        }
    }

    return query;
}

- (void)dealloc
{
    [super dealloc];
}
@end

namespace SL {
namespace Screen_Capture {

    struct NSWorkspaceWrapperImpl {
        NSWorkspaceWrapper *ptr = nullptr;

        NSWorkspaceWrapperImpl() { ptr = [[NSWorkspaceWrapper alloc] init]; }

        ~NSWorkspaceWrapperImpl()
        {
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

        std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValues()
        {
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

        std::shared_ptr<std::vector<struct WindowData>> determineWindows()
        {
            std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
            NSArray *values = [ptr determineApplicationValues];
            for (NSNumber *value in values) {
                int pid = value.intValue;
                auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                std::shared_ptr<std::string> execute;
                if (application) {
                    std::shared_ptr<std::string> execute;
                    std::shared_ptr<std::string> icon;
                    icon = retrieveIcon(application);
                    execute = retrieveExecutableURL(application);
                    NSArray *windows = [ptr determineWindowsByProcessId:pid];
                    this->processWindows(windows, pid, data.get(), execute, icon);
                    CFRelease(windows);
                }
            }

            [ptr releaseApplicationValues:values];
            return data;
        }

        std::shared_ptr<std::vector<struct WindowData>> determineWindows(int pid)
        {
            std::shared_ptr<std::vector<struct WindowData>> data = std::make_shared<std::vector<struct WindowData>>();
            NSArray *windows = [ptr determineWindowsByProcessId:pid];
            auto application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            if (application) {
                std::shared_ptr<std::string> execute;
                std::shared_ptr<std::string> icon;
                icon = retrieveIcon(application);
                execute = retrieveExecutableURL(application);
                this->processWindows(windows, pid, data.get(), execute, icon);
            }
            CFRelease(windows);
            return data;
        }

        std::shared_ptr<std::string> retrieveExecutableURL(NSRunningApplication *application)
        {
            std::shared_ptr<std::string> execute;
            auto executableURL = [application executableURL];
            if (executableURL) {
                CFStringRef absoluteString =
                    CFStringCreateWithCString((CFAllocatorRef)NULL, [executableURL absoluteString].UTF8String, kCFStringEncodingUTF8);
                execute = getString(absoluteString);
                CFRelease(absoluteString);
            }
            return execute;
        }

        std::shared_ptr<std::string> retrieveIcon(NSRunningApplication *application)
        {
            std::shared_ptr<std::string> icon;
            auto image = [application icon];
            if (image) {
                CGImageRef cgRef = [image CGImageForProposedRect:NULL context:nil hints:nil];
                NSBitmapImageRep *newRep = [[NSBitmapImageRep alloc] initWithCGImage:cgRef];
                [newRep setSize:[image size]];
                NSData *pngData = [newRep representationUsingType:NSPNGFileType properties:nil];
                auto encodedString = [pngData base64Encoding];
                CFStringRef value = (__bridge CFStringRef)[NSString stringWithFormat:@"data:image/png;base64,%@", encodedString];
                icon = getString(value);
                CFRelease(value);
                [newRep autorelease];
            }
            return icon;
        }

        void processWindows(NSArray *windows, int pid, std::vector<struct WindowData> *data, std::shared_ptr<std::string> &execute, std::shared_ptr<std::string> &icon)
        {
            int i = 0;
            for (SCWindowData *window in windows) {
                struct WindowData wd;
                wd.num = i;
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

                if (execute) {
                    wd.execute = execute;
                }

                wd.icon = icon;

                CFRelease(window);
                data->push_back(wd);
            }
        }
    };

    NSWorkspaceWrapperImpl *CreateNSWorkspaceWrapperImpl() { return new NSWorkspaceWrapperImpl(); }

    void DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl *p)
    {
        if (p) {
            delete p;
        }
    }

    CFDictionaryRef determineFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p) { return p->determineFrontmostApplication(); }

    Boolean requestAccessibilityPermissionsWrap(NSWorkspaceWrapperImpl *p) { return p->requestAccessibilityPermissions(); }

    std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValuesWrap(NSWorkspaceWrapperImpl *p)
    {
        return p->determineApplicationValues();
    }

    std::shared_ptr<std::vector<struct WindowData>> determineWindowsWrap(NSWorkspaceWrapperImpl *p, int pid) { return p->determineWindows(pid); }
    std::shared_ptr<std::vector<struct WindowData>> determineWindowsWrap(NSWorkspaceWrapperImpl *p) { return p->determineWindows(); }

    void releaseFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p, CFDictionaryRef query) { p->releaseFrontmostApplication(query); }
} // namespace Screen_Capture
} // namespace SL