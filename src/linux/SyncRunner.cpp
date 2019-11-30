//
// Created by denis on 11/30/19.
//
#include "ScreenCapture.h"
#include "internal/ThreadManager.h"
#include "X11FrameProcessorSync.h"

namespace SL {
    namespace Screen_Capture {
        void RunCaptureMonitor(std::shared_ptr<Sync_Data> data, Monitor monitor) {
            TryCaptureMonitorSync<X11FrameProcessorSync>(data, monitor);
        }
    }
}

