//
// Created by denis on 11/30/19.
//

#ifndef SIMPLE_SCREENSHOT_MAKER_X11FRAMEPROCESSORSYNC_H
#define SIMPLE_SCREENSHOT_MAKER_X11FRAMEPROCESSORSYNC_H
#pragma once
#include "internal/SCCommon.h"
#include <memory>
#include <X11/Xlib.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
namespace SL {
    namespace Screen_Capture {
        class X11FrameProcessorSync : public BaseFrameProcessorSync {

            Display *SelectedDisplay = nullptr;
            XID SelectedWindow = 0;
            XImage *XImage_ = nullptr;
            std::unique_ptr<XShmSegmentInfo> ShmInfo;
            Monitor SelectedMonitor;

        public:
            X11FrameProcessorSync();

            ~X11FrameProcessorSync();

            DUPL_RETURN InitSync(std::shared_ptr<Sync_Data> data, Monitor &monitor);

            DUPL_RETURN ProcessFrame(const Monitor &currentmonitorinfo);
        };
    }
}
#endif //SIMPLE_SCREENSHOT_MAKER_X11FRAMEPROCESSORSYNC_H
