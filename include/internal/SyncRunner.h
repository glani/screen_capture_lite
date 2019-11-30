//
// Created by denis on 11/30/19.
//

#ifndef SIMPLE_SCREENSHOT_MAKER_SYNCRUNNER_H
#define SIMPLE_SCREENSHOT_MAKER_SYNCRUNNER_H

#include "internal/SCCommon.h"
#include "ScreenCapture.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace SL {
    namespace Screen_Capture {
        class SyncRunner {

        public:
            SyncRunner();

            ~SyncRunner();

            void Init(const std::shared_ptr <Sync_Data> &data);
        };

        void RunCaptureMonitor(std::shared_ptr<Sync_Data> data, Monitor monitor);
    }
}
#endif //SIMPLE_SCREENSHOT_MAKER_SYNCRUNNER_H
