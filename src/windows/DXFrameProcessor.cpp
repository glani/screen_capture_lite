#include "DXUtilsHolder.h"
#include "DXFrameProcessor.h"
#include "internal/SCCommon.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <string>


#if (_MSC_VER >= 1700) && defined(_USING_V110_SDK71_)
namespace SL {
namespace Screen_Capture {

    DUPL_RETURN DXFrameProcessor::Init(std::shared_ptr<Thread_Data> data) { return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED; }
    DUPL_RETURN DXFrameProcessor::ProcessFrame() { return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED; }

} // namespace Screen_Capture
} // namespace SL
#else

namespace SL {
    namespace Screen_Capture {

        DUPL_RETURN DXFrameProcessor::Init(std::shared_ptr<Thread_Data> data, Monitor &monitor) {
            SelectedMonitor = monitor;
            DX_RESOURCES res;
            auto ret = Initialize(res);
            if (ret != DUPL_RETURN_SUCCESS) {
                return ret;
            }
            DUPLE_RESOURCES dupl;
            ret = Initialize(dupl, res.Device.Get(), Adapter(SelectedMonitor), Id(SelectedMonitor));
            if (ret != DUPL_RETURN_SUCCESS) {
                return ret;
            }
            Device = res.Device;
            DeviceContext = res.DeviceContext;
            OutputDuplication = dupl.OutputDuplication;
            OutputDesc = dupl.OutputDesc;
            Output = dupl.Output;

            Data = data;

            return ret;
        }

        //
        // Process a given frame and its metadata
        //

        DUPL_RETURN DXFrameProcessor::ProcessFrame(const Monitor &currentmonitorinfo) {
            Microsoft::WRL::ComPtr<IDXGIResource> DesktopResource;
            DXGI_OUTDUPL_FRAME_INFO FrameInfo = {0};
            AquireFrameRAII frame(OutputDuplication.Get());

            // Get new frame
            auto hr = frame.AcquireNextFrame(100, &FrameInfo, DesktopResource.GetAddressOf());
            if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
                return DUPL_RETURN_SUCCESS;
            } else if (FAILED(hr)) {
                return ProcessFailure(Device.Get(), L"Failed to acquire next frame in DUPLICATIONMANAGER", L"Error", hr,
                                      FrameInfoExpectedErrors);
            }
            if (FrameInfo.AccumulatedFrames == 0) {
                return DUPL_RETURN_SUCCESS;
            }
            Microsoft::WRL::ComPtr<ID3D11Texture2D> aquireddesktopimage;
            // QI for IDXGIResource
            hr = DesktopResource.Get()->QueryInterface(__uuidof(ID3D11Texture2D),
                                                       reinterpret_cast<void **>(aquireddesktopimage.GetAddressOf()));
            if (FAILED(hr)) {
                return ProcessFailure(nullptr,
                                      L"Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DUPLICATIONMANAGER",
                                      L"Error", hr,
                                      nullptr);
            }

            if (!StagingSurf) {
                D3D11_TEXTURE2D_DESC ThisDesc = {0};
                aquireddesktopimage->GetDesc(&ThisDesc);
                D3D11_TEXTURE2D_DESC StagingDesc;
                StagingDesc = ThisDesc;
                StagingDesc.BindFlags = 0;
                StagingDesc.Usage = D3D11_USAGE_STAGING;
                StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                StagingDesc.MiscFlags = 0;
                StagingDesc.Height = Height(SelectedMonitor);
                StagingDesc.Width = Width(SelectedMonitor);

                hr = Device->CreateTexture2D(&StagingDesc, nullptr, StagingSurf.GetAddressOf());
                if (FAILED(hr)) {
                    return ProcessFailure(Device.Get(), L"Failed to create staging texture for move rects", L"Error",
                                          hr,
                                          SystemTransitionsExpectedErrors);
                }
            }
            if (Width(currentmonitorinfo) == Width(SelectedMonitor) &&
                Height(currentmonitorinfo) == Height(SelectedMonitor)) {
                DeviceContext->CopyResource(StagingSurf.Get(), aquireddesktopimage.Get());
            } else {
                D3D11_BOX sourceRegion;
                sourceRegion.left = OffsetX(SelectedMonitor) - OutputDesc.DesktopCoordinates.left;
                sourceRegion.right = sourceRegion.left + Width(SelectedMonitor);
                sourceRegion.top = OffsetY(SelectedMonitor) + OutputDesc.DesktopCoordinates.top;
                sourceRegion.bottom = sourceRegion.top + Height(SelectedMonitor);
                sourceRegion.front = 0;
                sourceRegion.back = 1;
                DeviceContext->CopySubresourceRegion(StagingSurf.Get(), 0, 0, 0, 0, aquireddesktopimage.Get(), 0,
                                                     &sourceRegion);
            }

            D3D11_MAPPED_SUBRESOURCE MappingDesc = {0};
            MAPPED_SUBRESOURCERAII mappedresrouce(DeviceContext.Get());
            hr = mappedresrouce.Map(StagingSurf.Get(), 0, D3D11_MAP_READ, 0, &MappingDesc);
            // Get the data
            if (MappingDesc.pData == NULL) {
                return ProcessFailure(Device.Get(),
                                      L"DrawSurface_GetPixelColor: Could not read the pixel color because the mapped subresource returned NULL",
                                      L"Error",
                                      hr, SystemTransitionsExpectedErrors);
            }
            auto startsrc = reinterpret_cast<unsigned char *>(MappingDesc.pData);
            ProcessCapture(Data->ScreenCaptureData, *this, SelectedMonitor, startsrc, MappingDesc.RowPitch);
            return DUPL_RETURN_SUCCESS;
        }
    } // namespace Screen_Capture
} // namespace SL

#endif
