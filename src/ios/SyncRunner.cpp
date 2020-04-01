//
// Created by Denis Esakov on 01/12/2019.
//
#include "ScreenCapture.h"
#include "internal/ThreadManager.h"
#include "ios/NSFrameProcessorSync.h"

namespace SL {
    namespace Screen_Capture {
        bool RunCaptureMonitor(std::shared_ptr<Sync_Data> data, Monitor monitor) {
            return TryCaptureMonitorSync<NSFrameProcessorSync>(data, monitor);
        }

        bool RequestPermissionsSync() {
            return RequestPermissionsSync<NSFrameProcessorSync>();
        }
    }
}


