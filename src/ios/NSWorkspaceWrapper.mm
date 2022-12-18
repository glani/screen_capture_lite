#import <AppKit/AppKit.h>
#include <map>
#include "NSWorkspaceWrapperm.h"

extern const CFStringRef kTopMostPID = CFSTR( "kTopMostPID" );
extern const CFStringRef kAbsoluteUrlString = CFSTR( "kAbsoluteUrlString" );
extern const CFStringRef kTopMostLocalizedName = CFSTR( "kTopMostLocalizedName" );

@implementation NSWorkspaceWrapper
- (void)Init {
    self = [super init];
}

- (void) releaseFrontmostApplication:(CFDictionaryRef) query {
    CFRelease( query );
}

- (void) releaseApplicationValues:(NSArray*) values {
    [values release];
}

- (NSArray*) determineApplicationValues {
    auto pids = [[NSMutableArray alloc] init];
    NSArray *appsArray = [[NSWorkspace sharedWorkspace] runningApplications];
    for (NSRunningApplication *a in appsArray) {
        auto pid = a.processIdentifier;
        [pids addObject:[NSNumber numberWithInteger:pid]];
    }
    [appsArray release];
    return pids;
}

- (CFDictionaryRef) determineFrontmostApplication {
    CFMutableDictionaryRef query =
            CFDictionaryCreateMutable(NULL,
                                      0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
    @autoreleasepool {
        auto frontmostApplication= [[NSWorkspace sharedWorkspace] frontmostApplication];

        if (frontmostApplication) {
            auto pid = frontmostApplication.processIdentifier;
            auto executableURL = [frontmostApplication executableURL];
            if (executableURL) {
                CFStringRef absoluteString = CFStringCreateWithCString
                    (
                            ( CFAllocatorRef )NULL,
                            [executableURL absoluteString].UTF8String,
                            kCFStringEncodingUTF8
                    );
                CFDictionarySetValue(query, kAbsoluteUrlString,
                                 absoluteString);

                CFRelease( absoluteString );
            }
            CFStringRef localizedName = CFStringCreateWithCString
                    (
                            ( CFAllocatorRef )NULL,
                            [frontmostApplication localizedName].UTF8String,
                            kCFStringEncodingUTF8
                    );
            CFNumberRef processIdentifier = CFNumberCreate(
                    ( CFAllocatorRef )NULL,
                    kCFNumberIntType,
                    &pid
                    );
            CFDictionarySetValue(query, kTopMostPID,
                                 processIdentifier);
            CFDictionarySetValue(query, kTopMostLocalizedName,
                                 localizedName);

            CFRelease( localizedName );
            CFRelease( processIdentifier );
        }

    }

    return query;
}

- (void)dealloc {
    [super dealloc];
}
@end

namespace SL {
    namespace Screen_Capture {
        struct ApplicationData {
            int pid;
        };

        struct NSWorkspaceWrapperImpl {
            NSWorkspaceWrapper *ptr = nullptr;

            NSWorkspaceWrapperImpl() {
                ptr = [[NSWorkspaceWrapper alloc] init];
            }

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

            CFDictionaryRef determineFrontmostApplication() {
                return [ptr determineFrontmostApplication];
            }

            std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValues() {
                std::shared_ptr<std::map<int, struct ApplicationData>> data = std::make_shared<std::map<int, struct ApplicationData>>();
                NSArray* values = [ptr determineApplicationValues];
                for (NSNumber *value in values) {
                    struct ApplicationData ad;
                    ad.pid = value.intValue;
                    data->emplace(std::make_pair(ad.pid, ad));
                }

                [ptr releaseApplicationValues: values];
                return data;
            };

            void releaseFrontmostApplication(CFDictionaryRef query) {
                [ptr releaseFrontmostApplication: query];
            }

            void releaseApplicationValues(NSArray* values) {
                [ptr releaseApplicationValues: values];
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

        CFDictionaryRef determineFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p) {
            return p->determineFrontmostApplication();
        }

        std::shared_ptr<std::map<int, struct ApplicationData>> determineApplicationValuesWrap(NSWorkspaceWrapperImpl *p) {
            return p->determineApplicationValues();
        }

        void releaseFrontmostApplicationWrap(NSWorkspaceWrapperImpl *p, CFDictionaryRef query) {
            p->releaseFrontmostApplication(query);
        }
    }
}