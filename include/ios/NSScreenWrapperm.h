#pragma once

#import <Foundation/Foundation.h>

@interface NSScreenData : NSObject
@property(nonatomic) CFStringRef screenName;
@property(nonatomic) CGRect frame;
@property(nonatomic) CGRect visibleFrame;
@property(nonatomic) uint32_t displayId;
@property(nonatomic) uint32_t num;
@property(nonatomic) bool main;
@end

@interface NSScreenWrapper : NSObject
- (NSArray*)retrieveScreens;
- (void)releaseScreens:(NSArray*)screens;
@end
