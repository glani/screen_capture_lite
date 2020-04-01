#pragma once
#include "internal/SCCommon.h"

namespace SL {
    namespace Screen_Capture {
        class NSFrameProcessorSync;
        struct NSFrameProcessorSyncImpl;
        NSFrameProcessorSyncImpl* CreateNSFrameProcessorSyncImpl();
        void DestroyNSFrameProcessorSyncImpl(NSFrameProcessorSyncImpl*);
        void setMinFrameDurationSync(NSFrameProcessorSyncImpl*, const std::chrono::microseconds& );
        
        DUPL_RETURN Init(NSFrameProcessorSyncImpl* createdimpl, NSFrameProcessorSync* parent);
        DUPL_RETURN RequestPermissions();
        
        class NSFrameProcessorSync : public BaseFrameProcessorSync {
            NSFrameProcessorSyncImpl* NSFrameProcessorSyncImpl_ = nullptr;
        public:
            NSFrameProcessorSync();
            ~NSFrameProcessorSync();
            Monitor SelectedMonitor;
            std::mutex okMutex;
            std::condition_variable okCondition;
            bool ok;
            DUPL_RETURN InitSync(std::shared_ptr<Sync_Data> data, Monitor& monitor);
            DUPL_RETURN ProcessFrame(const Monitor& curentmonitorinfo);
            DUPL_RETURN RequestPermissions();
        };
    }
}
