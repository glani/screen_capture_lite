#pragma once

#import <Carbon/Carbon.h>

@interface SCWindowData : NSObject
@property(nonatomic) CFStringRef processTitle;
@property(nonatomic) int pid;
@property(nonatomic) CFStringRef title;
@property(nonatomic) CGRect rect;
@property(nonatomic) CFBooleanRef focused;
@property(nonatomic) CFBooleanRef main;
@property(nonatomic) CFBooleanRef minimized;
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
- (CFBooleanRef)booleanValueOfAttribute:(CFStringRef)attribute;

- (AXValueRef)valueOfAttribute:(CFStringRef)attribute type:(AXValueType)type;
- (BOOL)setValue:(AXValueRef)value forAttribute:(CFStringRef)attribute;

- (CGRect)rectOfElement;
- (void)setRectOfElement:(CGRect)rect;

+ (CGRect)normalizeCoordinatesOfRect:(CGRect)rect frameOfScreen:(CGRect)frameOfScreen;

- (BOOL)isSheet;
- (BOOL)isSystemDialog;

- (BOOL)permissionsRequest;

- (void)enrich:(SCWindowData *)window;
@end
