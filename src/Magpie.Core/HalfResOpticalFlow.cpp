#include "pch.h"
#include "HalfResOpticalFlow.h"
#include "DirectXHelper.h"
#include "Logger.h"

namespace Magpie {

static constexpr char FLOW_HLSL[] = R"(
cbuffer FlowConstants : register(b0) {
    uint2 FullSize;
    uint2 HalfSize;
};
Texture2D<float4> CurrentColor : register(t0);
Texture2D<float4> PreviousColor : register(t1);
RWTexture2D<float2> HalfFlowOut : register(u0);

float Luma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }
int2 ClampPixel(int2 p) { return clamp(p, int2(0, 0), int2(FullSize) - 1); }
float SampleLuma(Texture2D<float4> tex, int2 p) { return Luma(tex.Load(int3(ClampPixel(p), 0)).rgb); }

[numthreads(8, 8, 1)]
void EstimateHalf(uint3 tid : SV_DispatchThreadID) {
    if (any(tid.xy >= HalfSize)) return;
    int2 p = min(int2(tid.xy * 2 + 1), int2(FullSize) - 1);
    static const int2 taps[5] = {
        int2(0,0), int2(-2,0), int2(2,0), int2(0,-2), int2(0,2)
    };
    float bestError = 3.402823e+38;
    int2 bestOffset = int2(0, 0);
    [unroll] for (int y = -2; y <= 2; ++y) {
        [unroll] for (int x = -2; x <= 2; ++x) {
            int2 candidate = int2(x, y) * 2;
            float error = 0.0;
            [unroll] for (int i = 0; i < 5; ++i) {
                float a = SampleLuma(CurrentColor, p + taps[i]);
                float b = SampleLuma(PreviousColor, p + candidate + taps[i]);
                float d = a - b;
                error += d * d;
            }
            // Prefer smaller motion when candidates are nearly equivalent.
            error += dot(float2(candidate), float2(candidate)) * 0.000002;
            if (error < bestError) { bestError = error; bestOffset = candidate; }
        }
    }
    HalfFlowOut[tid.xy] = float2(bestOffset);
}

Texture2D<float2> HalfFlowIn : register(t0);
RWTexture2D<float2> FullFlowOut : register(u0);

[numthreads(8, 8, 1)]
void UpsampleFlow(uint3 tid : SV_DispatchThreadID) {
    if (any(tid.xy >= FullSize)) return;
    float2 hp = (float2(tid.xy) + 0.5) * 0.5 - 0.5;
    int2 p0 = int2(floor(hp));
    float2 f = frac(hp);
    int2 hi = int2(HalfSize) - 1;
    float2 a = HalfFlowIn.Load(int3(clamp(p0, int2(0,0), hi), 0));
    float2 b = HalfFlowIn.Load(int3(clamp(p0 + int2(1,0), int2(0,0), hi), 0));
    float2 c = HalfFlowIn.Load(int3(clamp(p0 + int2(0,1), int2(0,0), hi), 0));
    float2 d = HalfFlowIn.Load(int3(clamp(p0 + int2(1,1), int2(0,0), hi), 0));
    FullFlowOut[tid.xy] = lerp(lerp(a,b,f.x), lerp(c,d,f.x), f.y);
}
)";

bool HalfResOpticalFlow::Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
	ID3D11Texture2D* input) noexcept {
	_device = device;
	_context = context;
	D3D11_TEXTURE2D_DESC desc{};
	input->GetDesc(&desc);
	_width = desc.Width;
	_height = desc.Height;
	_halfWidth = (_width + 1) / 2;
	_halfHeight = (_height + 1) / 2;

	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = 0;
	desc.CPUAccessFlags = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	HRESULT hr = _device->CreateTexture2D(&desc, nullptr, _previous.put());
	if (SUCCEEDED(hr)) hr = _device->CreateShaderResourceView(_previous.get(), nullptr, _previousSrv.put());
	if (FAILED(hr) || !_CreateInputSrv(input)) {
		Logger::Get().ComError("Create optical-flow history resources failed", hr);
		return false;
	}

	_halfFlow = DirectXHelper::CreateTexture2D(_device, DXGI_FORMAT_R16G16_FLOAT,
		_halfWidth, _halfHeight, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	_fullFlow = DirectXHelper::CreateTexture2D(_device, DXGI_FORMAT_R16G16_FLOAT,
		_width, _height, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	if (!_halfFlow || !_fullFlow) return false;
	hr = _device->CreateShaderResourceView(_halfFlow.get(), nullptr, _halfFlowSrv.put());
	if (SUCCEEDED(hr)) hr = _device->CreateUnorderedAccessView(_halfFlow.get(), nullptr, _halfFlowUav.put());
	if (SUCCEEDED(hr)) hr = _device->CreateUnorderedAccessView(_fullFlow.get(), nullptr, _fullFlowUav.put());
	if (FAILED(hr)) return false;

	winrt::com_ptr<ID3DBlob> blob;
	if (!DirectXHelper::CompileComputeShader(FLOW_HLSL, "EstimateHalf", blob.put(), "HalfResOpticalFlow")) return false;
	hr = _device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, _estimateShader.put());
	blob = nullptr;
	if (FAILED(hr) || !DirectXHelper::CompileComputeShader(FLOW_HLSL, "UpsampleFlow", blob.put(), "HalfResOpticalFlow")) return false;
	hr = _device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, _upsampleShader.put());
	if (FAILED(hr)) return false;

	struct Constants { UINT full[2]; UINT half[2]; } constants{
		{_width, _height}, {_halfWidth, _halfHeight}
	};
	D3D11_BUFFER_DESC cbd{ .ByteWidth = sizeof(Constants), .Usage = D3D11_USAGE_IMMUTABLE,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER };
	D3D11_SUBRESOURCE_DATA initial{ .pSysMem = &constants };
	hr = _device->CreateBuffer(&cbd, &initial, _constantBuffer.put());
	if (FAILED(hr)) return false;

	static constexpr float ZERO[4]{};
	_context->ClearUnorderedAccessViewFloat(_fullFlowUav.get(), ZERO);
	_context->CopyResource(_previous.get(), input);
	_hasHistory = false;
	Logger::Get().Info(fmt::format("Half-resolution optical flow initialized: {}x{} -> {}x{}",
		_width, _height, _halfWidth, _halfHeight));
	return true;
}

bool HalfResOpticalFlow::_CreateInputSrv(ID3D11Texture2D* input) noexcept {
	if (_input == input && _inputSrv) return true;
	_input = input;
	_inputSrv = nullptr;
	return SUCCEEDED(_device->CreateShaderResourceView(input, nullptr, _inputSrv.put()));
}

bool HalfResOpticalFlow::Estimate(ID3D11Texture2D* input) noexcept {
	if (!_CreateInputSrv(input)) return false;
	if (!_hasHistory) {
		static constexpr float ZERO[4]{};
		_context->ClearUnorderedAccessViewFloat(_fullFlowUav.get(), ZERO);
		_context->CopyResource(_previous.get(), input);
		_hasHistory = true;
		return true;
	}

	ID3D11Buffer* cb = _constantBuffer.get();
	_context->CSSetConstantBuffers(0, 1, &cb);
	ID3D11ShaderResourceView* estimateSrvs[2]{ _inputSrv.get(), _previousSrv.get() };
	ID3D11UnorderedAccessView* halfUav = _halfFlowUav.get();
	_context->CSSetShader(_estimateShader.get(), nullptr, 0);
	_context->CSSetShaderResources(0, 2, estimateSrvs);
	_context->CSSetUnorderedAccessViews(0, 1, &halfUav, nullptr);
	_context->Dispatch((_halfWidth + 7) / 8, (_halfHeight + 7) / 8, 1);

	ID3D11ShaderResourceView* nullSrvs[2]{};
	ID3D11UnorderedAccessView* nullUav = nullptr;
	_context->CSSetShaderResources(0, 2, nullSrvs);
	_context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
	ID3D11ShaderResourceView* halfSrv = _halfFlowSrv.get();
	ID3D11UnorderedAccessView* fullUav = _fullFlowUav.get();
	_context->CSSetShader(_upsampleShader.get(), nullptr, 0);
	_context->CSSetShaderResources(0, 1, &halfSrv);
	_context->CSSetUnorderedAccessViews(0, 1, &fullUav, nullptr);
	_context->Dispatch((_width + 7) / 8, (_height + 7) / 8, 1);
	_context->CSSetShaderResources(0, 2, nullSrvs);
	_context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
	_context->CSSetShaderResources(0, 2, nullSrvs);
	_context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
	_context->CSSetShader(nullptr, nullptr, 0);
	_context->CopyResource(_previous.get(), input);
	return true;
}

}
