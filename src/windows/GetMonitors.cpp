#include "internal/SCCommon.h"
#include "ScreenCapture.h"
#include <DXGI.h>

namespace SL {
namespace Screen_Capture {

    float scaleFromDpi(int xdpi)
    {
        switch (xdpi) {
        case 96:
            return 1.0f;
            break;
        case 120:
            return 1.25f;
            break;
        case 144:
            return 1.5f;
            break;
        case 192:
            return 2.0f;
            break;
        }

        return 1.0f;
    }

    RECT frameToRect(const Rect& frame) {
        RECT rect;
        rect.left = frame.X;
        rect.top = frame.Y;
        rect.bottom = frame.Y  + frame.Height;
        rect.right = frame.X  + frame.Width;
        return rect;
    }

    Rect rectToFrame(const RECT & rect) {
        Rect frame;
        frame.X = rect.left;
        frame.Y = rect.top;
        frame.Width = rect.right - rect.left;
        frame.Height = rect.bottom - rect.top;
        return frame;
    }

    std::shared_ptr<std::vector<Monitor>> GetMonitors()
    {
        std::shared_ptr<std::vector<Monitor>> ret = std::make_shared<std::vector<Monitor>>();

        IDXGIAdapter *pAdapter = nullptr;
        IDXGIFactory *pFactory = nullptr;

        // Get the handle to the primary display
        HMONITOR primaryMonitor = MonitorFromPoint(POINT{0,0}, MONITOR_DEFAULTTOPRIMARY);

        // Create a DXGIFactory object.
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&pFactory))) {
            for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
                IDXGIOutput *pOutput;

                for (UINT j = 0; pAdapter->EnumOutputs(j, &pOutput) != DXGI_ERROR_NOT_FOUND; ++j) {
                    DXGI_OUTPUT_DESC desc;
                    pOutput->GetDesc(&desc);
                    pOutput->Release();
                    std::wstring wname = desc.DeviceName;
                    std::string name(wname.begin(), wname.end());
                    DEVMODEW devMode;
                    EnumDisplaySettingsW(desc.DeviceName, ENUM_CURRENT_SETTINGS, &devMode);

                    auto mon = CreateDCW(desc.DeviceName, NULL, NULL, NULL);
                    auto xdpi = GetDeviceCaps(mon, LOGPIXELSX);
                    DeleteDC(mon);
                    auto scale = scaleFromDpi(xdpi);

                    bool flipSides = desc.Rotation == DXGI_MODE_ROTATION_ROTATE90 || desc.Rotation == DXGI_MODE_ROTATION_ROTATE270;
                    Monitor monitor = CreateMonitor(static_cast<int>(ret->size()), j, i,
                                                        flipSides ? devMode.dmPelsWidth : devMode.dmPelsHeight,
                                                        flipSides ? devMode.dmPelsHeight : devMode.dmPelsWidth,
                                                        devMode.dmPosition.x, devMode.dmPosition.y,
                                                        name, scale);
                    monitor.Main = desc.Monitor == primaryMonitor;
                    // Get the monitor information
                    MONITORINFO monitorInfo = { sizeof(monitorInfo) };
                    GetMonitorInfo(desc.Monitor, &monitorInfo);
                    monitor.VisibleFrame = rectToFrame(monitorInfo.rcWork);

                    ret->emplace_back(monitor);
                }
                pAdapter->Release();
            }
            pFactory->Release();
        }
        return ret;
    }
} // namespace Screen_Capture
} // namespace SL