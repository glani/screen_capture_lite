//
// Created by deniska on 12/1/2019.
//
#pragma once
#ifndef SIMPLE_SCREENSHOT_MAKER_DXUTILSHOLDER_H
#define SIMPLE_SCREENSHOT_MAKER_DXUTILSHOLDER_H

#include "internal/SCCommon.h"
#include <DXGI.h>
#include <iostream>
#include <memory>
#include <wrl.h>

#include <d3d11.h>
#include <dxgi1_2.h>

namespace SL {
    namespace Screen_Capture {
        struct DX_RESOURCES {
            Microsoft::WRL::ComPtr<ID3D11Device> Device;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
        };
        struct DUPLE_RESOURCES {
            Microsoft::WRL::ComPtr<IDXGIOutputDuplication> OutputDuplication;
            DXGI_OUTPUT_DESC OutputDesc;
            UINT Output;
        };

        // These are the errors we expect from general Dxgi API due to a transition
        extern HRESULT SystemTransitionsExpectedErrors[];

        // These are the errors we expect from IDXGIOutput1::DuplicateOutput due to a transition
        extern HRESULT CreateDuplicationExpectedErrors[];

        // These are the errors we expect from IDXGIOutputDuplication methods due to a transition
        extern HRESULT FrameInfoExpectedErrors[];

        // These are the errors we expect from IDXGIAdapter::EnumOutputs methods due to outputs becoming stale during a transition
        extern HRESULT EnumOutputsExpectedErrors[];

        DUPL_RETURN
        ProcessFailure(ID3D11Device *Device, LPCWSTR Str, LPCWSTR Title, HRESULT hr, HRESULT *ExpectedErrors);

        DUPL_RETURN Initialize(DUPLE_RESOURCES &r, ID3D11Device *device, const UINT adapter, const UINT output);

        RECT ConvertRect(RECT Dirty, const DXGI_OUTPUT_DESC &DeskDesc);

        DUPL_RETURN Initialize(DX_RESOURCES &data);

        class AquireFrameRAII {

            IDXGIOutputDuplication *_DuplLock;
            bool AquiredLock;

            void TryRelease() {
                if (AquiredLock) {
                    auto hr = _DuplLock->ReleaseFrame();
                    if (FAILED(hr) && hr != DXGI_ERROR_WAIT_TIMEOUT) {
                        ProcessFailure(nullptr, L"Failed to release frame in DUPLICATIONMANAGER", L"Error", hr,
                                       FrameInfoExpectedErrors);
                    }
                }
                AquiredLock = false;
            }

        public:
            AquireFrameRAII(IDXGIOutputDuplication *dupl) : _DuplLock(dupl), AquiredLock(false) {}

            ~AquireFrameRAII() { TryRelease(); }

            HRESULT AcquireNextFrame(UINT TimeoutInMilliseconds, DXGI_OUTDUPL_FRAME_INFO *pFrameInfo,
                                     IDXGIResource **ppDesktopResource) {
                auto hr = _DuplLock->AcquireNextFrame(TimeoutInMilliseconds, pFrameInfo, ppDesktopResource);
                TryRelease();
                AquiredLock = SUCCEEDED(hr);
                return hr;
            }
        };

        class MAPPED_SUBRESOURCERAII {
            ID3D11DeviceContext *_Context;
            ID3D11Resource *_Resource;
            UINT _Subresource;

        public:
            MAPPED_SUBRESOURCERAII(ID3D11DeviceContext *context) : _Context(context), _Resource(nullptr),
                                                                   _Subresource(0) {}

            ~MAPPED_SUBRESOURCERAII() { _Context->Unmap(_Resource, _Subresource); }

            HRESULT Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags,
                        D3D11_MAPPED_SUBRESOURCE *pMappedResource) {
                if (_Resource != nullptr) {
                    _Context->Unmap(_Resource, _Subresource);
                }
                _Resource = pResource;
                _Subresource = Subresource;
                return _Context->Map(_Resource, _Subresource, MapType, MapFlags, pMappedResource);
            }
        };

    }
}


#endif //SIMPLE_SCREENSHOT_MAKER_DXUTILSHOLDER_H
