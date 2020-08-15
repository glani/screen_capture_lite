#pragma once

#import <Foundation/Foundation.h>

@interface NSWorkspaceWrapper : NSObject
- (void)Init;
- (CFDictionaryRef)determineFrontmostApplication;
- (void) releaseFrontmostApplication:(CFDictionaryRef) query;
- (NSArray*) determineApplicationValues;
- (void) releaseApplicationValues:(NSArray*) values;
@end
