#import "SCAccessibilityElement.h"

#import <AppKit/AppKit.h>

@implementation SCWindowData {

}
- (void)dealloc
{
    if (_focused) {
        CFRelease(_focused);
    }
    if (_main) {
        CFRelease(_main);
    }
    if (_minimized) {
        CFRelease(_minimized);
    }

    [super dealloc];
}

@end;
@implementation SCAccessibilityElementWindows {
    NSArray * _windowsList;
    NSString *_processTitle;
}

- (void)dealloc
{
    [self deallocWindowsList];
    if (_processTitle != NULL) {
        CFRelease(_processTitle);
    }
    [super dealloc];
}
- (void)deallocWindowsList
{
    if (_windowsList != NULL) {
        for (id elem in _windowsList) {
            CFRelease(elem);
        }
        CFRelease(_windowsList);
    }
}

- (NSArray*)getWindowsList {
    return _windowsList;
}

- (NSString*)getProcessTitle {
    return _processTitle;
}

- (void)setWindowsList:(NSArray*)windowsList
{
    if (_windowsList != NULL) {
        [self deallocWindowsList];
    }
    _windowsList = (AXUIElementRef)CFRetain(windowsList);
}

- (void)setProcessTitle:(NSString*)processTitle
{
    if (_processTitle != NULL) {
        CFRelease(_processTitle);
    }
    _processTitle = (NSString*)CFRetain(processTitle);
}


@end
@implementation SCAccessibilityElement {
    AXUIElementRef _underlyingElement;
}

- (instancetype)init
{
    if (self = [super init]) {
        _underlyingElement = NULL;
    }

    return self;
}

+ (SCAccessibilityElement *)frontmostApplicationElement
{
    NSRunningApplication *frontmostApplication = [NSWorkspace sharedWorkspace].frontmostApplication;
    pid_t pid = frontmostApplication.processIdentifier;
    return [self createApplicationElement:pid];
}

+ (SCAccessibilityElement *)frontmostWindowElement
{
    SCAccessibilityElement *frontmostApplicationElement = [SCAccessibilityElement frontmostApplicationElement];
    SCAccessibilityElement *frontmostWindowElement = nil;
    if (frontmostApplicationElement) {
        frontmostWindowElement = [frontmostApplicationElement elementWithAttribute:kAXFocusedWindowAttribute];
        if (!frontmostWindowElement) {
            NSLog(@"Invalid accessibility element provided, unable to determine the size and position of the window.");
        }
    }
    else {
        NSLog(@"Failed to find the application that currently has focus.");
    }
    return frontmostWindowElement;
}

+ (SCAccessibilityElement *)createApplicationElement:(pid_t)pid
{
    SCAccessibilityElement *applicationElement = [SCAccessibilityElement new];
    AXUIElementRef underlyingElement = AXUIElementCreateApplication(pid);
    applicationElement.underlyingElement = underlyingElement;
    CFRelease(underlyingElement);
    return applicationElement;
}

+ (SCAccessibilityElement *)createWindowElement:(pid_t)pid:(CFStringRef)attribute
{
    SCAccessibilityElement *frontmostApplicationElement = [SCAccessibilityElement createApplicationElement:pid];
    SCAccessibilityElement *frontmostWindowElement = nil;
    if (frontmostApplicationElement) {
        frontmostWindowElement = [frontmostApplicationElement elementWithAttribute:(attribute != NULL ? attribute : kAXFocusedWindowAttribute)];
        if (!frontmostWindowElement) {
            NSLog(@"Invalid accessibility element provided, unable to determine the size and position of the window.");
        }
    }
    else {
        NSLog(@"Failed to find the application that currently has focus.");
    }
    return frontmostWindowElement;
}

- (SCAccessibilityElement *)elementWithAttribute:(CFStringRef)attribute
{
    SCAccessibilityElement *newElement = nil;
    AXUIElementRef underlyingElement;
    AXError result = AXUIElementCopyAttributeValue(_underlyingElement, attribute, (CFTypeRef *)&underlyingElement);
    if (result == kAXErrorSuccess) {
        newElement = [SCAccessibilityElement new];
        newElement.underlyingElement = underlyingElement;
        CFRelease(underlyingElement);
    }
    else {
        NSLog(@"Unable to obtain the accessibility element with the specified attribute: %@ (error code %d)", attribute, result);
    }
    return newElement;
}

- (NSString *)stringValueOfAttribute:(CFStringRef)attribute
{
    if (CFGetTypeID(_underlyingElement) == AXUIElementGetTypeID()) {
        CFTypeRef value;
        AXError result;
        result = AXUIElementCopyAttributeValue(_underlyingElement, attribute, &value);
        if (result == kAXErrorSuccess) {
            return CFBridgingRelease(value);
        }
        else {
            NSLog(@"There was a problem getting the string value of the specified attribute: %@ (error code %d)", attribute, result);
        }
    }
    return nil;
}

- (CFBooleanRef)booleanValueOfAttribute:(CFStringRef)attribute
{
    if (CFGetTypeID(_underlyingElement) == AXUIElementGetTypeID()) {
        CFTypeRef value;
        AXError result;
        result = AXUIElementCopyAttributeValue(_underlyingElement, attribute, &value);
        if (result == kAXErrorSuccess) {
            return ((CFBooleanRef *)value);
        }
        else {
            NSLog(@"There was a problem getting the string value of the specified attribute: %@ (error code %d)", attribute, result);
        }
    }
    return nil;
}

- (AXValueRef)valueOfAttribute:(CFStringRef)attribute type:(AXValueType)type
{
    if (CFGetTypeID(_underlyingElement) == AXUIElementGetTypeID()) {
        CFTypeRef value;
        AXError result;
        result = AXUIElementCopyAttributeValue(_underlyingElement, attribute, (CFTypeRef *)&value);
        if ((result == kAXErrorSuccess) && (AXValueGetType((AXValueRef)value) == type)) {
            return (AXValueRef)value;
        }
        else {
            NSLog(@"There was a problem getting the value of the specified attribute: %@ (error code %d)", attribute, result);
        }
    }
    return NULL;
}

- (void)setValue:(AXValueRef)value forAttribute:(CFStringRef)attribute
{
    AXError result = AXUIElementSetAttributeValue(_underlyingElement, attribute, (CFTypeRef *)value);
    if (result != kAXErrorSuccess) {
        NSLog(@"There was a problem setting the value of the specified attribute: %@ (error code %d)", attribute, result);
    }
}

- (CGRect)rectOfElement
{
    CGRect result = CGRectNull;
    CFTypeRef positionValue = [self valueOfAttribute:kAXPositionAttribute type:kAXValueTypeCGPoint];
    CFTypeRef sizeValue = [self valueOfAttribute:kAXSizeAttribute type:kAXValueTypeCGSize];
    CGPoint position;
    CGSize size;

    AXValueGetValue((AXValueRef)positionValue, kAXValueTypeCGPoint, (void *)&position);
    AXValueGetValue((AXValueRef)sizeValue, kAXValueTypeCGSize, (void *)&size);
    if ((positionValue != NULL) && (sizeValue != NULL)) {
        CFRelease(positionValue);
        CFRelease(sizeValue);
        result = CGRectMake(position.x, position.y, size.width, size.height);
    }
    return result;
}

- (void)setRectOfElement:(CGRect)rect
{
    AXValueRef positionRef;
    AXValueRef sizeRef;
    positionRef = AXValueCreate(kAXValueTypeCGPoint, (const void *)&rect.origin);
    sizeRef = AXValueCreate(kAXValueTypeCGSize, (const void *)&rect.size);
    [self setValue:sizeRef forAttribute:kAXSizeAttribute];
    [self setValue:positionRef forAttribute:kAXPositionAttribute];
    [self setValue:sizeRef forAttribute:kAXSizeAttribute];
    CFRelease(positionRef);
    CFRelease(sizeRef);
}

+ (CGRect)normalizeCoordinatesOfRect:(CGRect)rect frameOfScreen:(CGRect)frameOfScreen
{
    CGRect frameOfScreenWithMenuBar = [[[NSScreen screens] objectAtIndex:0] frame];
    rect.origin.y = frameOfScreen.size.height - NSMaxY(rect) + (frameOfScreenWithMenuBar.size.height - frameOfScreen.size.height);
    return rect;
}

- (BOOL)isSheet
{
    return [[self stringValueOfAttribute:kAXRoleAttribute] isEqualToString:(__bridge NSString *)kAXSheetRole];
}

- (BOOL)isSystemDialog
{
    return [[self stringValueOfAttribute:kAXSubroleAttribute] isEqualToString:(__bridge NSString *)kAXSystemDialogSubrole];
}

- (void)dealloc
{
    if (_underlyingElement != NULL) {
        CFRelease(_underlyingElement);
    }
}

- (void)setUnderlyingElement:(AXUIElementRef)underlyingElement
{
    if (_underlyingElement != NULL) {
        CFRelease(_underlyingElement);
    }
    _underlyingElement = (AXUIElementRef)CFRetain(underlyingElement);
}

- (BOOL)permissionsRequest
{
    BOOL trusted = AXIsProcessTrusted();
    NSArray *attrNames = NULL;
    AXError result = AXUIElementCopyAttributeNames(_underlyingElement, (CFArrayRef)&attrNames);
    if (attrNames) {
        CFRelease(attrNames);
    }
    if (result != kAXErrorSuccess) {
        return false;
    }
    return trusted;
}
- (void)enrich:(SCWindowData*)window
{
    CGRect rect = [self rectOfElement];
    NSString *title = [self stringValueOfAttribute:kAXTitleAttribute];
    if (title) {
        window.title = (CFStringRef)CFRetain((__bridge CFStringRef)title);
        CFRelease(title);
    } else {
        window.title = nil;
    }
    window.rect = rect;

    window.focused = nil;
    CFBooleanRef main = [self booleanValueOfAttribute:kAXMainAttribute];
    if (main != nil) {
        window.main = CFRetain(main);
        CFRelease(main);
    }
    else {
        window.main = nil;
    };
    CFBooleanRef minimized = [self booleanValueOfAttribute:kAXMinimizedAttribute];
    if (minimized != nil) {
        window.minimized = CFRetain(minimized);
        CFRelease(minimized);
    }
    else {
        window.minimized = nil;
    };
}

- (SCAccessibilityElementWindows* )retrieveAllWindows
{
    @autoreleasepool {
        CFTypeRef value;
        AXError result;
        result = AXUIElementCopyAttributeValue(_underlyingElement, kAXWindowsAttribute, &value);
        if (result == kAXErrorSuccess) {
            NSArray *windowsLst = (NSArray *)value;

            NSMutableArray *res = [[NSMutableArray alloc] init];
            for (id elem in windowsLst) {
                SCAccessibilityElement *accessibilityElement = [SCAccessibilityElement new];
                accessibilityElement.underlyingElement = elem;
                [res addObject:accessibilityElement];
            }
            [windowsLst release];

            SCAccessibilityElementWindows *accessibilityElementWindows = [SCAccessibilityElementWindows new];
            accessibilityElementWindows.windowsList = res;
            NSString *title = [self stringValueOfAttribute:kAXTitleAttribute];
            accessibilityElementWindows.processTitle = title;
            [res release];
            return accessibilityElementWindows;
        }
        else {
            return NULL;
        }
    }
}

@end