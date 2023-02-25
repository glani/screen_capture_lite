#pragma once

#import "SCAccessibilityElement.h"
#import <Foundation/Foundation.h>

@interface NSWorkspaceWrapper : NSObject
- (void)Init;
- (CFDictionaryRef)determineFrontmostApplication;
- (Boolean)requestAccessibilityPermissions;
- (void)releaseFrontmostApplication:(CFDictionaryRef)query;
- (NSArray *)determineApplicationValues;
- (NSArray *)determineWindowsByProcessId:(pid_t)pid;
- (void)releaseApplicationValues:(NSArray *)values;
- (SCWindowData *)minimizeWindow:(NSWindowDataRequest *)request:(bool)minimize;
- (SCWindowData *)fullscreenWindow:(NSWindowDataRequest *)request:(int)display:(bool)fullscreen;
- (SCWindowData *)setRectWindow:(NSWindowDataRequest *)request:(CGRect)rect;

- (bool)closeWindow:(NSWindowDataRequest *)request:(bool)force;
- (SCWindowData *)topMostWindow:(NSWindowDataRequest *)request;
- (SCWindowData *)getWindow:(NSWindowDataRequest *)request;
@end
