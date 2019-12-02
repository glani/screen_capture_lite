//
// Created by deniska on 12/1/2019.
//

#ifndef SIMPLE_SCREENSHOT_MAKER_DXFrameProcessorSyncSYNC_H
#define SIMPLE_SCREENSHOT_MAKER_DXFrameProcessorSyncSYNC_H

#pragma once
#include "internal/SCCommon.h"
#include <DXGI.h>
#include <memory>
#include <wrl.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <mutex>
#include <condition_variable>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

namespace SL {
    namespace Screen_Capture {
        class DXFrameProcessorSync : public BaseFrameProcessorSync {
            Microsoft::WRL::ComPtr<ID3D11Device> Device;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> StagingSurf;

            Microsoft::WRL::ComPtr<IDXGIOutputDuplication> OutputDuplication;
            DXGI_OUTPUT_DESC OutputDesc;
            UINT Output;
            std::vector<BYTE> MetaDataBuffer;
            Monitor SelectedMonitor;

        public:
            DXFrameProcessorSync();

            DUPL_RETURN InitSync(std::shared_ptr<Sync_Data> data, Monitor &monitor);
            DUPL_RETURN ProcessFrame(const Monitor &currentmonitorinfo);
            std::mutex okMutex;
            std::condition_variable okCondition;
            bool ok;
        };

    } // namespace Screen_Capture
} // namespace SL

#endif //SIMPLE_SCREENSHOT_MAKER_DXFrameProcessorSyncSYNC_H
