#pragma once

#import <Carbon/Carbon.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * The pid of the frontmost application, read fresh on every call (0 if unknown). NSWorkspace.frontmostApplication is
 * NOT fresh off the main run loop: in SMBAct's engine and renderer it stays at the first value it read, so the "active
 * window" (and every layout hotkey) followed a stale app (smpostio-utils tests/macos/activation, probe `front`).
 */
pid_t SCCurrentFrontmostPid(void);
#ifdef __cplusplus
}
#endif

@interface NSWindowDataRequest : NSObject
@property(nonatomic) int pid;
@property(nonatomic) uint32_t handle;
@property(nonatomic) bool minimized;
@property(nonatomic) bool fullscreen;
@property(nonatomic) CGRect rect;

@end
@interface SCWindowData : NSObject
@property(nonatomic) CFStringRef processTitle;
@property(nonatomic) int pid;
@property(nonatomic) CFStringRef title;
@property(nonatomic) CGRect rect;
@property(nonatomic) uint32_t handle;
@property(nonatomic) CFBooleanRef focused;
@property(nonatomic) CFBooleanRef main;
@property(nonatomic) CFBooleanRef minimized;
@property(nonatomic) CFBooleanRef fullscreen;
@end

@interface SCAccessibilityElementWindows : NSObject
@end

@interface SCAccessibilityElement : NSObject

+ (SCAccessibilityElement *)frontmostApplicationElement;
+ (SCAccessibilityElement *)frontmostWindowElement;

+ (SCAccessibilityElement *)createApplicationElement:(pid_t)pid;
+ (SCAccessibilityElement *)createWindowElement:(pid_t)pid:(CFStringRef)attribute;

- (SCAccessibilityElementWindows *)retrieveAllWindows;

- (SCAccessibilityElement *)elementWithAttribute:(CFStringRef)attribute;

- (NSString *)stringValueOfAttribute:(CFStringRef)attribute;
- (CFBooleanRef)booleanValueOfAttribute:(CFStringRef)attribute:(bool)silent;

- (AXValueRef)valueOfAttribute:(CFStringRef)attribute type:(AXValueType)type;
- (BOOL)setValue:(AXValueRef)value forAttribute:(CFStringRef)attribute;

- (CGRect)rectOfElement;
- (void)setRectOfElement:(CGRect)rect;

+ (CGRect)normalizeCoordinatesOfRect:(CGRect)rect frameOfScreen:(CGRect)frameOfScreen;

- (BOOL)isSheet;
- (BOOL)isSystemDialog;

- (BOOL)permissionsRequest;

- (BOOL)determineRequiredWindow:(NSWindowDataRequest *)request:(bool)force;
- (void)enrich:(SCWindowData *)window;
- (void)fullscreenWindow:(bool)fullscreen;
- (void)minimizeWindow:(bool)minimize;
- (BOOL)closeWindow:(bool)force;
- (void)topMostWindow;
@end
