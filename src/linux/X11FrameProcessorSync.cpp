//
// Created by denis on 11/30/19.
//
#include "X11FrameProcessorSync.h"
#include <X11/Xutil.h>
#include <assert.h>
#include <vector>

namespace SL {
    namespace Screen_Capture {
        X11FrameProcessorSync::X11FrameProcessorSync() {
        }

        X11FrameProcessorSync::~X11FrameProcessorSync() {

            if (ShmInfo) {
                shmdt(ShmInfo->shmaddr);
                shmctl(ShmInfo->shmid, IPC_RMID, 0);
                XShmDetach(SelectedDisplay, ShmInfo.get());
            }
            if (XImage_) {
                XDestroyImage(XImage_);
            }
            if (SelectedDisplay) {
                XCloseDisplay(SelectedDisplay);
            }
        }

        DUPL_RETURN X11FrameProcessorSync::InitSync(std::shared_ptr<Sync_Data> data, Monitor &monitor) {
            auto ret = DUPL_RETURN::DUPL_RETURN_SUCCESS;
            Data = data;
            SelectedMonitor = monitor;
            SelectedDisplay = XOpenDisplay(NULL);
            if (!SelectedDisplay) {
                return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED;
            }
            int scr = XDefaultScreen(SelectedDisplay);

            ShmInfo = std::make_unique<XShmSegmentInfo>();

            XImage_ = XShmCreateImage(SelectedDisplay,
                                      DefaultVisual(SelectedDisplay, scr),
                                      DefaultDepth(SelectedDisplay, scr),
                                      ZPixmap,
                                      NULL,
                                      ShmInfo.get(),
                                      Width(SelectedMonitor),
                                      Height(SelectedMonitor));
            ShmInfo->shmid = shmget(IPC_PRIVATE, XImage_->bytes_per_line * XImage_->height, IPC_CREAT | 0777);

            ShmInfo->readOnly = False;
            ShmInfo->shmaddr = XImage_->data = (char *) shmat(ShmInfo->shmid, 0, 0);

            XShmAttach(SelectedDisplay, ShmInfo.get());

            return ret;
        }

        DUPL_RETURN X11FrameProcessorSync::ProcessFrame(const Monitor &curentmonitorinfo) {
            auto Ret = DUPL_RETURN_SUCCESS;
            if (!XShmGetImage(SelectedDisplay,
                              RootWindow(SelectedDisplay, DefaultScreen(SelectedDisplay)),
                              XImage_,
                              OffsetX(SelectedMonitor),
                              OffsetY(SelectedMonitor),
                              AllPlanes)) {
                return DUPL_RETURN_ERROR_EXPECTED;
            }
            ProcessCapture(Data->ScreenCaptureData, *this, SelectedMonitor, (unsigned char *) XImage_->data,
                           XImage_->bytes_per_line);
            return Ret;
        }
    }
}

