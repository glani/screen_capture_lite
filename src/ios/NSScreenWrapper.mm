#import "NSScreenWrapperm.h"
#include "NSWorkspaceWrapperShared.h"
#import <AppKit/AppKit.h>
#include "Utils.h"
#include <vector>

const CFStringRef vNSScreenNumber = CFSTR("NSScreenNumber");
const CFStringRef vNSDeviceIsScreen = CFSTR("NSDeviceIsScreen");
const CFStringRef vYes = CFSTR("YES");

@implementation NSScreenData

- (void)dealloc
{
    [super dealloc];
}
@end

@implementation NSScreenWrapper
- (NSArray *)retrieveScreens
{
    NSMutableArray *res = [[NSMutableArray alloc] init];
    @autoreleasepool {
        auto screens = [NSScreen screens];
        auto mainScreen = [NSScreen mainScreen];
        uint32_t mainDisplayId = ((NSNumber *)[[mainScreen deviceDescription] objectForKey:((__bridge NSString *)vNSScreenNumber)]).intValue;
        uint32_t num = 0;
        for (NSScreen *screen in screens) {
            auto screenName = [screen localizedName];
            auto description = [screen deviceDescription];
            if ([[description objectForKey:((__bridge NSString *)vNSDeviceIsScreen)] isEqualToString:(__bridge NSString *)vYes]) {
                uint32_t displayId = ((NSNumber *)[description objectForKey:((__bridge NSString *)vNSScreenNumber)]).intValue;
                auto frame = [screen frame];
                auto visibleFrame = [screen visibleFrame];
                auto window = [[NSScreenData alloc] init];
                window.screenName = (__bridge CFStringRef)screenName;
                window.frame = frame;
                window.visibleFrame = visibleFrame;
                window.displayId = displayId;
                window.num = num;
                window.main = mainDisplayId == displayId;
                num++;
                [res addObject:window];
            }
        }
    }
    return res;
}

- (void)releaseScreens:(NSArray *)screens
{
    if (screens) {
        for (id value in screens) {
            [value release];
        }
        [screens release];
    }
}

@end

namespace SL {
namespace Screen_Capture {
    struct NSScreenWrapperImpl {
        NSScreenWrapper *ptr = nullptr;

        NSScreenWrapperImpl() { ptr = [[NSScreenWrapper alloc] init]; }

        ~NSScreenWrapperImpl()
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

        std::shared_ptr<std::vector<struct ScreenData>> retrieveScreens() {
            std::shared_ptr<std::vector<struct ScreenData>> data = std::make_shared<std::vector<struct ScreenData>>();
            auto screens = [ptr retrieveScreens];
            for (NSScreenData *screenData in screens) {
                struct ScreenData screen;
                screen.frame = screenData.frame;
                screen.visibleFrame = screenData.visibleFrame;
                screen.displayId = screenData.displayId;
                screen.num = screenData.num;
                screen.main = screenData.main;
                screen.screenName = getString(screenData.screenName);
                data->push_back(screen);
            }
            [ptr releaseScreens:screens];
            return data;
        }
    };

    NSScreenWrapperImpl *CreateNSScreenWrapperImpl() { return new NSScreenWrapperImpl(); }

    void DestroyNSScreenWrapperImpl(NSScreenWrapperImpl *p)
    {
        if (p) {
            delete p;
        }
    }

    std::shared_ptr<std::vector<struct ScreenData>> retrieveScreensWrap(NSScreenWrapperImpl *p) { return p->retrieveScreens(); }
} // namespace Screen_Capture
} // namespace SL