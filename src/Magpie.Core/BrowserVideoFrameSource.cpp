#include "pch.h"
#include "BrowserVideoFrameSource.h"
#include "BackendDescriptorStore.h"
#include "BrowserVideoGeometryPolicy.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Logger.h"
#include "ScalingWindow.h"

namespace Magpie {
namespace {
struct FrameHeader {
    uint32_t magic, version, width, height, stride, bytes;
    uint64_t sequence;
    int64_t timestamp100ns, ptsMicroseconds;
    uint32_t format, matrix, fullRange, reserved;
};
static_assert(sizeof(FrameHeader) == 64);
constexpr const char* YUV_TO_RGB = R"(
Texture2D<float> Y : register(t0);
Texture2D<float2> UV : register(t1);
Texture2D<float> U : register(t2);
Texture2D<float> V : register(t3);
RWTexture2D<float4> Output : register(u0);
cbuffer Params : register(b0) {
    uint Width, Height, OutputWidth, OutputHeight;
    uint Format, Matrix, FullRange, Padding;
};
[numthreads(16,16,1)]
void Convert(uint3 id : SV_DispatchThreadID) {
    if (id.x >= OutputWidth || id.y >= OutputHeight) return;
    uint2 source = min(uint2(id.x * Width / OutputWidth, id.y * Height / OutputHeight),
        uint2(Width - 1, Height - 1));
    float y = Y[source];
    float2 uv = Format == 1 ? UV[source/2] : float2(U[source/2], V[source/2]);
    uv -= 128.0/255.0;
    if (FullRange == 0) { y = (y - 16.0/255.0) * (255.0/219.0); uv *= 255.0/224.0; }
    float4 k = Matrix == 2 ? float4(1.402, -.344136, -.714136, 1.772) :
        (Matrix == 3 ? float4(1.4746, -.164553, -.571353, 1.8814) : float4(1.5748, -.187324, -.468124, 1.8556));
    Output[id.xy] = float4(saturate(float3(y + k.x*uv.y, y + k.y*uv.x + k.z*uv.y, y + k.w*uv.x)), 1);
})";
}

bool BrowserVideoFrameSource::_Initialize() noexcept {
    const auto& options = ScalingWindow::Get().Options();
    const auto& name = options.browserFrameMapping;
    _size = options.browserFrameSize;
    if (!name.starts_with(L"Local\\MagpieBrowserFrame_") || _size.cx < 64 || _size.cy < 64 ||
        int64_t(_size.cx) * _size.cy > 3840 * 2160) {
        Logger::Get().Error("Browser decoded source options are invalid");
        return false;
    }
    _mapping.reset(OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str()));
    if (!_mapping) {
        Logger::Get().Win32Error("Open browser decoded frame mapping failed");
        return false;
    }
    _mutex.reset(OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (name + L"_Mutex").c_str()));
    if (!_mutex) {
        Logger::Get().Win32Error("Open browser decoded frame mutex failed");
        return false;
    }
    _view.reset(static_cast<uint8_t*>(MapViewOfFile(_mapping.get(), FILE_MAP_READ, 0, 0, 0)));
    if (!_view) {
        Logger::Get().Win32Error("Map browser decoded frame view failed");
        return false;
    }
    _output = DirectXHelper::CreateTexture2D(_deviceResources->GetD3DDevice(),
        DXGI_FORMAT_R8G8B8A8_UNORM, _size.cx, _size.cy, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
    if (!_output) return false;
    _outputView = _descriptorStore->GetUnorderedAccessView(_output.get());
    try {
        _planeCapacity = {
            _size.cx + BROWSER_VIDEO_GEOMETRY_JITTER_PX,
            _size.cy + BROWSER_VIDEO_GEOMETRY_JITTER_PX
        };
        _frameBytes.resize(size_t(_planeCapacity.cx) * _planeCapacity.cy * 4);
    } catch (...) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        _planes[i] = DirectXHelper::CreateTexture2D(_deviceResources->GetD3DDevice(),
            i == 1 ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8_UNORM,
            i == 0 ? _planeCapacity.cx : (_planeCapacity.cx + 1) / 2,
            i == 0 ? _planeCapacity.cy : (_planeCapacity.cy + 1) / 2,
            D3D11_BIND_SHADER_RESOURCE);
        if (!_planes[i]) return false;
        _planeViews[i] = _descriptorStore->GetShaderResourceView(_planes[i].get());
        if (!_planeViews[i]) return false;
    }
    winrt::com_ptr<ID3DBlob> code;
    if (!_outputView || !DirectXHelper::CompileComputeShader(YUV_TO_RGB, "Convert", code.put(), "BrowserVideo/YuvToRgb.hlsl")) return false;
    if (FAILED(_deviceResources->GetD3DDevice()->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, _convertShader.put()))) return false;
    D3D11_BUFFER_DESC constants{ .ByteWidth = 32, .Usage = D3D11_USAGE_DEFAULT, .BindFlags = D3D11_BIND_CONSTANT_BUFFER };
    if (FAILED(_deviceResources->GetD3DDevice()->CreateBuffer(&constants, nullptr, _convertParams.put()))) return false;
    _captureSequence = 1;
    Logger::Get().Info(fmt::format("Browser decoded source initialized: {}x{} (before CSS scaling)", _size.cx, _size.cy));
    return true;
}

FrameSourceState BrowserVideoFrameSource::_Update() noexcept {
	const auto now = std::chrono::steady_clock::now();
	if (!_watchdogStarted) {
		// Effect initialization can take several seconds after this source is
		// constructed. Start the watchdog when rendering actually begins.
		_watchdogStarted = true;
		_lastFrameAt = now;
	} else if (now - _lastFrameAt > std::chrono::seconds(5)) {
        Logger::Get().Error("Browser decoded source timed out; remove stale video overlay");
        return FrameSourceState::Error;
    }
    const DWORD wait = WaitForSingleObject(_mutex.get(), 0);
    if (wait == WAIT_ABANDONED) { ReleaseMutex(_mutex.get()); return FrameSourceState::Error; }
    if (wait != WAIT_OBJECT_0) return FrameSourceState::Waiting;
    const auto& sharedHeader = *reinterpret_cast<const FrameHeader*>(_view.get());
    const FrameHeader header = sharedHeader;
    if (!header.sequence || header.sequence == _sequence) {
        // Paused video is stopped by the browser. A dead bridge must not leave
        // an indefinitely frozen overlay covering a still-playing page.
        ReleaseMutex(_mutex.get());
        return FrameSourceState::Waiting;
    }
    if (header.magic != 0x31465642 || header.version != 1 || header.format > 2 ||
        header.width < 64 || header.height < 64 ||
        header.width > static_cast<uint32_t>(_planeCapacity.cx) ||
        header.height > static_cast<uint32_t>(_planeCapacity.cy)) {
        ReleaseMutex(_mutex.get());
        return FrameSourceState::Waiting;
    }
    const SIZE headerSize{ static_cast<LONG>(header.width), static_cast<LONG>(header.height) };
    if (!IsBrowserVideoSizeWithinJitter(_size, headerSize)) {
        ReleaseMutex(_mutex.get());
        return FrameSourceState::Waiting;
    }
    const bool yuv = header.format != 0;
    if (header.stride != header.width * (yuv ? 1 : 4) ||
        header.bytes != uint64_t(header.width) * header.height * (yuv ? 3 : 8) / 2 ||
        header.bytes > _frameBytes.size() ||
        (yuv && (((header.width | header.height) & 1) || header.matrix < 1 || header.matrix > 3 || header.fullRange > 1))) {
        ReleaseMutex(_mutex.get());
        return FrameSourceState::Error;
    }
    const bool normalizedGeometry = header.width != uint32_t(_size.cx) || header.height != uint32_t(_size.cy);
    if (normalizedGeometry && !yuv) {
        // RGBA is uploaded directly into the fixed output texture. Rebuild for
        // the new dimensions instead of risking a partial-resource copy.
        ReleaseMutex(_mutex.get());
        return FrameSourceState::Error;
    }
    // Keep the cross-process mutex around the CPU copy only. UpdateSubresource
    // may wait for GPU resources and must not block the decoded-frame publisher.
    memcpy(_frameBytes.data(), _view.get() + sizeof(FrameHeader), header.bytes);
    ReleaseMutex(_mutex.get());
    if (_lastPts >= 0 && (header.ptsMicroseconds < _lastPts || header.ptsMicroseconds - _lastPts > 500000)) ++_captureSequence;
    _lastPts = header.ptsMicroseconds;
    _captureTimestamp100ns = header.timestamp100ns;
    _sequence = header.sequence;
    auto* dc = _deviceResources->GetD3DDC();
    const uint8_t* pixels = _frameBytes.data();
    if (!yuv) dc->UpdateSubresource(_output.get(), 0, nullptr, pixels, header.stride, 0);
    else {
        const uint32_t planeSize = header.width * header.height;
        const D3D11_BOX yBox{ 0, 0, 0, header.width, header.height, 1 };
        const D3D11_BOX uvBox{ 0, 0, 0, header.width / 2, header.height / 2, 1 };
        dc->UpdateSubresource(_planes[0].get(), 0, &yBox, pixels, header.width, 0);
        if (header.format == 1) dc->UpdateSubresource(
            _planes[1].get(), 0, &uvBox, pixels + planeSize, header.width, 0);
        else {
            dc->UpdateSubresource(
                _planes[2].get(), 0, &uvBox, pixels + planeSize, header.width / 2, 0);
            dc->UpdateSubresource(
                _planes[3].get(), 0, &uvBox, pixels + planeSize * 5 / 4, header.width / 2, 0);
        }
        const uint32_t parameters[8]{
            header.width, header.height, static_cast<uint32_t>(_size.cx), static_cast<uint32_t>(_size.cy),
            header.format, header.matrix, header.fullRange
        };
        dc->UpdateSubresource(_convertParams.get(), 0, nullptr, parameters, 0, 0);
        ID3D11Buffer* buffer = _convertParams.get();
        dc->CSSetConstantBuffers(0, 1, &buffer);
        dc->CSSetShaderResources(0, 4, _planeViews);
        dc->CSSetUnorderedAccessViews(0, 1, &_outputView, nullptr);
        dc->CSSetShader(_convertShader.get(), nullptr, 0);
        dc->Dispatch((_size.cx + 15) / 16, (_size.cy + 15) / 16, 1);
        ID3D11ShaderResourceView* emptySrvs[4]{};
        ID3D11UnorderedAccessView* emptyUav = nullptr;
        dc->CSSetShaderResources(0, 4, emptySrvs);
        dc->CSSetUnorderedAccessViews(0, 1, &emptyUav, nullptr);
        if (normalizedGeometry && !_reportedGeometryNormalization) {
            _reportedGeometryNormalization = true;
            Logger::Get().Info(fmt::format(
                "Browser decoded source normalized geometry jitter: {}x{} -> {}x{}",
                header.width, header.height, _size.cx, _size.cy));
        }
    }
    _lastFrameAt = std::chrono::steady_clock::now();
    if (++_frames == 1 || _frames % 300 == 0) Logger::Get().Info(fmt::format(
        "Browser decoded frames uploaded: frames={} source={}x{} ptsUs={} pixelFormat={}", _frames, _size.cx, _size.cy, _lastPts, header.format));
    return FrameSourceState::NewFrame;
}
}
