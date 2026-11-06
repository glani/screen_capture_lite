#pragma once
#include "internal/SCCommon.h"
#include <memory>
#include <X11/X.h>
#include <X11/extensions/Xfixes.h>

namespace SL {
    namespace Screen_Capture {
        
        class X11MouseProcessor: public BaseFrameProcessor {
            Display* SelectedDisplay=nullptr;
            std::unique_ptr<unsigned char[]> OldImageBuffer;
            XID RootWindow;
            int Last_x = 0;
            int Last_y =0;
            int LastWidth = -1; // size of the image sent last (-1: none from the current buffers)
            int LastHeight = -1;
            
        public:
            const int MaxCursurorSize =32;
            X11MouseProcessor();
            ~X11MouseProcessor();
            DUPL_RETURN Init(std::shared_ptr<Thread_Data> data);
            DUPL_RETURN ProcessFrame();

        };

    }
}