#import <iostream>
#include "NSFrameProcessorSyncm.h"

@implementation FrameProcessorSync

- (SL::Screen_Capture::DUPL_RETURN)Init:(SL::Screen_Capture::NSFrameProcessorSync *)parent {
    self = [super init];
    if (self) {
        self.Working = false;
        self.nsframeprocessor = parent;
        self.avcapturesession = [[AVCaptureSession alloc] init];

        self.avinput = [[[AVCaptureScreenInput alloc] initWithDisplayID:SL::Screen_Capture::Id(
                parent->SelectedMonitor)] autorelease];
        [self.avcapturesession addInput:self.avinput];

        self.output = [[AVCaptureVideoDataOutput alloc] init];
        NSDictionary *videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithUnsignedInt:kCVPixelFormatType_32BGRA], (id) kCVPixelBufferPixelFormatTypeKey, nil];

        [self.output setVideoSettings:videoSettings];
        [self.output setAlwaysDiscardsLateVideoFrames:true];
        //partial capture needed
        if (parent->SelectedMonitor.OffsetX != parent->SelectedMonitor.OriginalOffsetX ||
            parent->SelectedMonitor.OffsetY != parent->SelectedMonitor.OriginalOffsetY ||
            parent->SelectedMonitor.Height != parent->SelectedMonitor.OriginalHeight ||
            parent->SelectedMonitor.Width != parent->SelectedMonitor.OriginalWidth) {
            CGRect r;
            r.origin.x = parent->SelectedMonitor.OffsetX;
            //apple uses the opengl texture coords where the bottom left is 0,0
            r.origin.y = parent->SelectedMonitor.OriginalHeight -
                         ((parent->SelectedMonitor.OffsetY - parent->SelectedMonitor.OriginalOffsetY) +
                          parent->SelectedMonitor.Height);
            r.size.height = parent->SelectedMonitor.Height;
            r.size.width = parent->SelectedMonitor.Width;
            [self.avinput setCropRect:r];
        }
        CMTime interval = CMTimeMake(1, 1);
        [self.avinput setMinFrameDuration:interval];


        self.avinput.capturesCursor = false;
        self.avinput.capturesMouseClicks = false;

        [self.avcapturesession addOutput:self.output];
        [self.output setSampleBufferDelegate:self queue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0)];
        [self.avcapturesession startRunning];

        return SL::Screen_Capture::DUPL_RETURN::DUPL_RETURN_SUCCESS;
    }
    return SL::Screen_Capture::DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
}

- (void)dealloc {
    [self.avcapturesession stopRunning];
    while (self.avcapturesession.isRunning || self.Working) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    [self.output release];
    [self.avinput release];
    [self.avcapturesession release];
    [super dealloc];
}

- (void)setFrameRate:(CMTime)interval {
    [self.avinput setMinFrameDuration:interval];
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
    self.Working = true;
    if (!self.avcapturesession.isRunning) {
        self.Working = false;
        return;
    }

    [self.avcapturesession stopRunning];
    if (self.output) {
        self.output.connections[0].enabled = NO;
    }


    bool processed = false;
    std::unique_lock<std::mutex> __(self.nsframeprocessor->okMutex);
    processed = self.nsframeprocessor->ok;
    __.unlock();

    // std::cout << "Thread Id: " << std::this_thread::get_id() << std::endl;
    if (processed) {
        self.Working = false;
        return;
    }

    auto data = self.nsframeprocessor->Data;
    auto &selectedmonitor = self.nsframeprocessor->SelectedMonitor;

    auto imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    auto bytesperrow = CVPixelBufferGetBytesPerRow(imageBuffer);
    auto buf = static_cast<unsigned char *>(CVPixelBufferGetBaseAddress(imageBuffer));
    SL::Screen_Capture::ProcessCapture(data->ScreenCaptureData, *(self.nsframeprocessor), selectedmonitor, buf,
                                       bytesperrow);
    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    self.Working = false;

    std::unique_lock<std::mutex> _(self.nsframeprocessor->okMutex);
    self.nsframeprocessor->ok = true;
    self.nsframeprocessor->okCondition.notify_one();
    _.unlock();
}
@end

namespace SL {
    namespace Screen_Capture {

        struct NSFrameProcessorSyncImpl {
            FrameProcessorSync *ptr = nullptr;

            NSFrameProcessorSyncImpl() {
                ptr = [[FrameProcessorSync alloc] init];
            }

            ~NSFrameProcessorSyncImpl() {
                if (ptr) {
                    [ptr release];
                    auto r = CFGetRetainCount(ptr);
                    while (r != 1) {
                        r = CFGetRetainCount(ptr);
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    ptr = nullptr;
                }
            }

            void setMinFrameDurationSync(const std::chrono::microseconds &duration) {
                [ptr setFrameRate:CMTimeMake(1, 1)];
            }

            DUPL_RETURN Init(NSFrameProcessorSync *parent) {
                return [ptr Init:parent];
            }
        };

        NSFrameProcessorSyncImpl *CreateNSFrameProcessorSyncImpl() {
            return new NSFrameProcessorSyncImpl();
        }

        void DestroyNSFrameProcessorSyncImpl(NSFrameProcessorSyncImpl *p) {
            if (p) {
                delete p;
            }
        }

        void setMinFrameDurationSync(NSFrameProcessorSyncImpl *p, const std::chrono::microseconds &duration) {
            if (p) {
                p->setMinFrameDurationSync(duration);
            }
        }

        DUPL_RETURN Init(NSFrameProcessorSyncImpl *createdimpl, NSFrameProcessorSync *parent) {
            if (createdimpl) {
                return createdimpl->Init(parent);
            }
            return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
        }
    }
}
