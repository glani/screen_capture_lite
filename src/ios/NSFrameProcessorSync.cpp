#include "NSFrameProcessorSync.h"

namespace SL {
    namespace Screen_Capture {

        NSFrameProcessorSync::NSFrameProcessorSync() {
            NSFrameProcessorSyncImpl_ = nullptr;
            ok = false;
        }

        NSFrameProcessorSync::~NSFrameProcessorSync() {
            DestroyNSFrameProcessorSyncImpl(NSFrameProcessorSyncImpl_);
        }

        DUPL_RETURN NSFrameProcessorSync::InitSync(std::shared_ptr<Sync_Data> data, Monitor &monitor) {
            Data = data;
            SelectedMonitor = monitor;
            NSFrameProcessorSyncImpl_ = CreateNSFrameProcessorSyncImpl();
            return Screen_Capture::Init(NSFrameProcessorSyncImpl_, this);
        }

        DUPL_RETURN NSFrameProcessorSync::RequestPermissions() {
            return Screen_Capture::RequestPermissions();
        }

        DUPL_RETURN NSFrameProcessorSync::ProcessFrame(const Monitor &curentmonitorinfo) {
            setMinFrameDurationSync(NSFrameProcessorSyncImpl_, std::chrono::seconds(100));
            std::unique_lock<std::mutex> lock(this->okMutex);
            this->okCondition.wait_for(lock, std::chrono::milliseconds(10000), [this] { return this->ok; });
            return this->ok ? DUPL_RETURN_SUCCESS : DUPL_RETURN_ERROR_EXPECTED;
        }
    }
}
