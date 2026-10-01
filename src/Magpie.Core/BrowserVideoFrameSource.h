#pragma once
#include "FrameSourceBase.h"

namespace Magpie {
// Decoded, unscaled SDR RGBA/NV12/I420 pixels from HTMLVideoElement. Source window
// geometry controls only presentation; it never determines texture dimensions.
class BrowserVideoFrameSource final : public FrameSourceBase {
public:
    const char* Name() const noexcept override { return "BrowserDecodedFrame"; }
    FrameSourceWaitType WaitType() const noexcept override { return FrameSourceWaitType::NoWait; }
protected:
    bool _Initialize() noexcept override;
    FrameSourceState _Update() noexcept override;
private:
    wil::unique_handle _mapping;
    wil::unique_handle _mutex;
    wil::unique_mapview_ptr<uint8_t> _view;
    SIZE _size{};
    SIZE _planeCapacity{};
    uint64_t _sequence = 0;
    int64_t _lastPts = -1;
    uint64_t _frames = 0;
    winrt::com_ptr<ID3D11Texture2D> _planes[4];
    ID3D11ShaderResourceView* _planeViews[4]{};
    ID3D11UnorderedAccessView* _outputView = nullptr;
    winrt::com_ptr<ID3D11ComputeShader> _convertShader;
    winrt::com_ptr<ID3D11Buffer> _convertParams;
    std::vector<uint8_t> _frameBytes;
    std::chrono::steady_clock::time_point _lastFrameAt{};
    bool _watchdogStarted = false;
    bool _reportedGeometryNormalization = false;
};
}
