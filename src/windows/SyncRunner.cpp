//
// Created by deniska on 12/1/2019.
//
#include "ScreenCapture.h"
#include "internal/ThreadManager.h"
#include "DXFrameProcessorSync.h"
#include "GDIFrameProcessorSync.h"

namespace SL {
    namespace Screen_Capture {

        template <class T> bool SwitchToInputDesktopSync(const std::shared_ptr<T> data)
        {
            HDESK CurrentDesktop = nullptr;
            CurrentDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
            if (!CurrentDesktop) {
                // We do not have access to the desktop so request a retry
                data->lastError = DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED;
                return false;
            }

            // Attach desktop to this thread
            bool DesktopAttached = SetThreadDesktop(CurrentDesktop) != 0;
            CloseDesktop(CurrentDesktop);
            CurrentDesktop = nullptr;
            if (!DesktopAttached) {
                // We do not have access to the desktop so request a retry
                data->lastError = DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED;
                return false;
            }
            return true;
        }

        bool RunCaptureMonitor(std::shared_ptr<Sync_Data> data, Monitor monitor)
        {
            // need to switch to the input desktop for capturing...
            if (!SwitchToInputDesktopSync(data))
                return false;
#if defined _DEBUG || !defined NDEBUG
            std::cout << "Starting to Capture on Monitor " << Name(monitor) << std::endl;
            std::cout << "Trying DirectX Desktop Duplication " << std::endl;
#endif
            if (!TryCaptureMonitorSync<DXFrameProcessorSync>(data, monitor)) { // if DX is not supported, fallback to GDI capture
#if defined _DEBUG || !defined NDEBUG
                std::cout << "DirectX Desktop Duplication not supported, falling back to GDI Capturing . . ." << std::endl;
#endif
                return TryCaptureMonitorSync<GDIFrameProcessorSync>(data, monitor);
            }

            return true;
        }

        bool RequestPermissionsSync() {
            return true;
        }

    }
}

