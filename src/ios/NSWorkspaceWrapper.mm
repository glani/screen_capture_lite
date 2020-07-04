#import <AppKit/AppKit.h>
#include "NSWorkspaceWrapperm.h"

@implementation NSWorkspaceWrapper
- (void)Init {
    self = [super init];
}

- (size_t)determineFrontmostApplicationPID {
    NSRunningApplication *frontmostApplication = [NSWorkspace sharedWorkspace].frontmostApplication;
    pid_t res = frontmostApplication.processIdentifier;
    [frontmostApplication release];
    return res;
}

- (void)dealloc {
    [super dealloc];
}
@end

namespace SL {
    namespace Screen_Capture {

        struct NSWorkspaceWrapperImpl {
            NSWorkspaceWrapper *ptr = nullptr;

            NSWorkspaceWrapperImpl() {
                ptr = [[NSWorkspaceWrapper alloc] init];
            }

            ~NSWorkspaceWrapperImpl() {
                if (ptr) {
                    [ptr release];
                    auto r = CFGetRetainCount(ptr);
                    while (r != 1) {
                        r = CFGetRetainCount(ptr);
                    }
                    ptr = nullptr;
                }
            }

            size_t determineFrontmostApplicationPID() {
                return [ptr determineFrontmostApplicationPID];
            }
        };

        NSWorkspaceWrapperImpl *CreateNSWorkspaceWrapperImpl() {
            return new NSWorkspaceWrapperImpl();
        }

        void DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl *p) {
            if (p) {
                delete p;
            }
        }

        size_t determineFrontmostApplicationPIDWrap(NSWorkspaceWrapperImpl *p) {
            return p->determineFrontmostApplicationPID();
        }
    }
}