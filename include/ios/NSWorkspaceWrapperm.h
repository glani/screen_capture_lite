#pragma once

#import <Foundation/Foundation.h>

@interface NSWorkspaceWrapper : NSObject
- (void)Init;
- (CFDictionaryRef)determineFrontmostApplication;
- (Boolean)requestAccessibilityPermissions;
- (void)releaseFrontmostApplication:(CFDictionaryRef)query;
- (NSArray *)determineApplicationValues;
- (NSArray *)determineWindowsByProcessId:(pid_t)pid;
- (void)releaseApplicationValues:(NSArray *)values;
@end
