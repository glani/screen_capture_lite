#pragma once
#import "internal/SCCommon.h"
#import "NSFrameProcessorSync.h"
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

@interface FrameProcessorSync: NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
    @property(nonatomic, assign) SL::Screen_Capture::NSFrameProcessorSync* nsframeprocessor;
    @property(nonatomic, retain) AVCaptureSession *avcapturesession;
    @property(nonatomic, retain) AVCaptureVideoDataOutput *output;
    @property(nonatomic, retain) AVCaptureScreenInput* avinput;
    -(SL::Screen_Capture::DUPL_RETURN) Init:(SL::Screen_Capture::NSFrameProcessorSync*) parent;
@end


