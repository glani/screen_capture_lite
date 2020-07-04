#pragma once

#import <Foundation/Foundation.h>

@interface NSWorkspaceWrapper : NSObject
- (void)Init;
- (size_t)determineFrontmostApplicationPID;
@end
