//
// Created by deniska on 12/1/2019.
//
#include "GDIFrameProcessorSync.h"
#include <Dwmapi.h>

namespace SL {
    namespace Screen_Capture {

        DUPL_RETURN GDIFrameProcessorSync::InitSync(std::shared_ptr<Sync_Data> data, const Monitor &monitor)
        {
            SelectedMonitor = monitor;
            auto Ret = DUPL_RETURN_SUCCESS;

            MonitorDC.DC = CreateDCA(Name(SelectedMonitor), NULL, NULL, NULL);
            CaptureDC.DC = CreateCompatibleDC(MonitorDC.DC);
            CaptureBMP.Bitmap = CreateCompatibleBitmap(MonitorDC.DC, Width(SelectedMonitor), Height(SelectedMonitor));
            NewImageBuffer = std::make_unique<unsigned char[]>(ImageBufferSize);
            if (!MonitorDC.DC || !CaptureDC.DC || !CaptureBMP.Bitmap) {
                return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED;
            }

            Data = data;
            return Ret;
        }

        DUPL_RETURN GDIFrameProcessorSync::ProcessFrame(const Monitor &currentmonitorinfo)
        {

            auto Ret = DUPL_RETURN_SUCCESS;

            ImageRect ret;
            ret.left = ret.top = 0;
            ret.bottom = Height(SelectedMonitor);
            ret.right = Width(SelectedMonitor);

            // Selecting an object into the specified DC
            auto originalBmp = SelectObject(CaptureDC.DC, CaptureBMP.Bitmap);

            if (BitBlt(CaptureDC.DC, 0, 0, ret.right, ret.bottom, MonitorDC.DC, 0, 0, SRCCOPY | CAPTUREBLT) == FALSE) {
                // if the screen cannot be captured, return
                SelectObject(CaptureDC.DC, originalBmp);
                return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED; // likely a permission issue
            }
            else {

                BITMAPINFOHEADER bi;
                memset(&bi, 0, sizeof(bi));

                bi.biSize = sizeof(BITMAPINFOHEADER);

                bi.biWidth = ret.right;
                bi.biHeight = -ret.bottom;
                bi.biPlanes = 1;
                bi.biBitCount = sizeof(ImageBGRA) * 8; // always 32 bits damnit!!!
                bi.biCompression = BI_RGB;
                bi.biSizeImage = ((ret.right * bi.biBitCount + 31) / (sizeof(ImageBGRA) * 8)) * sizeof(ImageBGRA)  * ret.bottom;
                GetDIBits(MonitorDC.DC, CaptureBMP.Bitmap, 0, (UINT)ret.bottom, NewImageBuffer.get(), (BITMAPINFO *)&bi, DIB_RGB_COLORS);
                SelectObject(CaptureDC.DC, originalBmp);
                ProcessCapture(Data->ScreenCaptureData, *this, currentmonitorinfo, NewImageBuffer.get(), Width(SelectedMonitor)* sizeof(ImageBGRA));
            }

            return Ret;
        }

    } // namespace Screen_Capture
} // namespace SL

