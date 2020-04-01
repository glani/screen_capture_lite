//
// Created by denis on 11/30/19.
//
#include "ScreenCapture.h"
#include "internal/ThreadManager.h"
#include "X11FrameProcessorSync.h"

namespace SL {
    namespace Screen_Capture {
        bool RunCaptureMonitor(std::shared_ptr<Sync_Data> data, Monitor monitor) {
            return TryCaptureMonitorSync<X11FrameProcessorSync>(data, monitor);
        }
    }
}

