//
// Created by deniska on 12/1/2019.
//

#ifndef SIMPLE_SCREENSHOT_MAKER_GDIFRAMEPROCESSORSYNC_H
#define SIMPLE_SCREENSHOT_MAKER_GDIFRAMEPROCESSORSYNC_H
#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include <memory>
#include "GDIHelpers.h"

namespace SL {
    namespace Screen_Capture {

        class GDIFrameProcessorSync : public BaseFrameProcessorSync {
            HDCWrapper MonitorDC;
            HDCWrapper CaptureDC;
            HBITMAPWrapper CaptureBMP;
            Monitor SelectedMonitor;
            std::unique_ptr<unsigned char[]> NewImageBuffer;

            std::shared_ptr<Sync_Data> Data;
        public:
            DUPL_RETURN InitSync(std::shared_ptr<Sync_Data> data, const Monitor& monitor);
            DUPL_RETURN ProcessFrame(const Monitor& currentmonitorinfo);
        };
    }
}

#endif //SIMPLE_SCREENSHOT_MAKER_GDIFRAMEPROCESSORSYNC_H
