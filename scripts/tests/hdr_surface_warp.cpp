#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXPackedVector.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "HdrColorTransform.h"
#include "HdrEffectBoundary.h"
#include "EffectProtocolCatalogC.h"
#include "hdr_surface_shader.h"

using Microsoft::WRL::ComPtr;
using namespace Magpie;
using Pixel = std::array<float, 4>;
constexpr UINT Width = 1025;

void Check(bool value, const char* message) {
	if (!value) throw std::runtime_error(message);
}
void Hr(HRESULT value) { Check(SUCCEEDED(value), "D3D11 operation failed"); }

struct Harness {
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D11ComputeShader> shader;
	ComPtr<ID3D11Buffer> constants;

	Harness() {
		D3D_FEATURE_LEVEL level;
		Hr(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
			D3D11_SDK_VERSION, &device, &level, &context));
		ComPtr<ID3DBlob> blob, errors;
		auto result = D3DCompile(HLSL, sizeof(HLSL), "ProductionHdrSurfaceAdapter", nullptr,
			nullptr, "Main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &errors);
		if (errors) std::cerr << static_cast<const char*>(errors->GetBufferPointer());
		Hr(result);
		Hr(device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader));
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(AdapterConstants);
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		Hr(device->CreateBuffer(&desc, nullptr, &constants));
	}

	ComPtr<ID3D11Texture2D> Texture(DXGI_FORMAT format, const Pixel* data = nullptr) {
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = Width; desc.Height = 1; desc.MipLevels = 1; desc.ArraySize = 1;
		desc.Format = format; desc.SampleDesc.Count = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		D3D11_SUBRESOURCE_DATA initial{data, Width * sizeof(Pixel), 0};
		ComPtr<ID3D11Texture2D> texture;
		Hr(device->CreateTexture2D(&desc, data ? &initial : nullptr, &texture));
		return texture;
	}

	void Dispatch(ID3D11Texture2D* input, ID3D11Texture2D* output, const AdapterConstants& values) {
		ComPtr<ID3D11ShaderResourceView> srv;
		ComPtr<ID3D11UnorderedAccessView> uav;
		Hr(device->CreateShaderResourceView(input, nullptr, &srv));
		Hr(device->CreateUnorderedAccessView(output, nullptr, &uav));
		context->UpdateSubresource(constants.Get(), 0, nullptr, &values, 0, 0);
		context->CSSetShader(shader.Get(), nullptr, 0);
		context->CSSetConstantBuffers(0, 1, constants.GetAddressOf());
		context->CSSetShaderResources(0, 1, srv.GetAddressOf());
		context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
		context->Dispatch((Width + 7) / 8, 1, 1);
		ID3D11ShaderResourceView* emptySrv = nullptr;
		ID3D11UnorderedAccessView* emptyUav = nullptr;
		context->CSSetShaderResources(0, 1, &emptySrv);
		context->CSSetUnorderedAccessViews(0, 1, &emptyUav, nullptr);
	}

	std::vector<Pixel> Read(ID3D11Texture2D* texture) {
		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc(&desc);
		Check(desc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT || desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM,
			"Readback format");
		desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ComPtr<ID3D11Texture2D> staging;
		Hr(device->CreateTexture2D(&desc, nullptr, &staging));
		context->CopyResource(staging.Get(), texture);
		D3D11_MAPPED_SUBRESOURCE mapped;
		Hr(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
		std::vector<Pixel> result(Width);
		if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
			const auto* bytes = static_cast<const unsigned char*>(mapped.pData);
			for (UINT i = 0; i < Width; ++i)
				for (int channel = 0; channel < 4; ++channel) result[i][channel] = bytes[i * 4 + channel] / 255.0f;
		} else if (desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
			const auto* halves = static_cast<const DirectX::PackedVector::HALF*>(mapped.pData);
			for (UINT i = 0; i < Width; ++i)
				for (int channel = 0; channel < 4; ++channel)
					result[i][channel] = DirectX::PackedVector::XMConvertHalfToFloat(halves[i * 4 + channel]);
		} else {
			const auto* begin = static_cast<const Pixel*>(mapped.pData);
			std::copy(begin, begin + Width, result.begin());
		}
		context->Unmap(staging.Get(), 0);
		return result;
	}

	void Test(float white, float peak, float exposure, bool quantized) {
		HdrTransformParameters parameters;
		parameters.sdrWhiteNits = white; parameters.hdrPeakNits = peak; parameters.exposure = exposure;
		const float maxInput = peak / 80.0f;
		std::vector<Pixel> pixels(Width);
		for (UINT i = 0; i < Width; ++i) {
			const float value = maxInput * i / (Width - 1);
			pixels[i] = { value, value * 0.5f, maxInput - value, 0.375f };
		}
		auto input = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, pixels.data());
		auto proxy = Texture(quantized ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R32G32B32A32_FLOAT);
		auto output = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT);
		AdapterConstants cb{exposure, 1 / exposure, white / peak, peak, parameters.shoulder,
			80, white, 1, 2, 0, 1, 1};
		Dispatch(input.Get(), proxy.Get(), cb);
		const auto encodedPixels = Read(proxy.Get());
		cb.mode = 1; cb.inputTransfer = 2; cb.outputTransfer = 1;
		Dispatch(proxy.Get(), output.Get(), cb);
		const auto restored = Read(output.Get());
		float previous = -1;
		for (UINT i = 0; i < Width; ++i) {
			Check(restored[i][0] + 1e-5f >= previous, "HDR luminance reversed");
			previous = restored[i][0];
			for (int channel = 0; channel < 3; ++channel) {
				float encoded = HdrColorTransform::EncodeTransfer(
					HdrColorTransform::MapHdrToSdr(pixels[i][channel], parameters), HdrTransferFunction::SRGB);
				Check(std::abs(encodedPixels[i][channel] - encoded) < (quantized ? 0.501f / 255 : 1e-4f),
					"CPU/HLSL forward mapping mismatch");
				if (quantized) encoded = encodedPixels[i][channel];
				const float expected = HdrColorTransform::MapSdrToHdr(
					HdrColorTransform::DecodeTransfer(encoded, HdrTransferFunction::SRGB), parameters);
				Check(std::isfinite(restored[i][channel]), "Nonfinite HDR output");
				Check(std::abs(restored[i][channel] - expected) < maxInput * 0.003f + 1e-4f,
					"CPU/HLSL mapping mismatch");
				if (!quantized) Check(std::abs(restored[i][channel] - pixels[i][channel]) < maxInput * 0.001f,
					"Floating HDR round trip lost highlights");
				else Check(std::abs(restored[i][channel] - pixels[i][channel]) < maxInput * 0.04f,
					"R8 HDR quantization exceeded four percent of peak");
			}
			const float expectedAlpha = quantized ? 96.0f / 255 : 0.375f;
			Check(std::abs(restored[i][3] - expectedAlpha) < 1e-5f, "Alpha was not preserved");
		}
		Check(restored.back()[0] > maxInput * 0.99f, "HDR peak was lost");
		// The canonical marker must preserve all four channels without an SDR conversion.
		cb.mode = 4;
		Dispatch(input.Get(), output.Get(), cb);
		Check(Read(output.Get()) == pixels, "Canonical pass-through changed pixels");
	}

	void TestExplicitComponents(float white, float peak) {
		std::vector<Pixel> pixels(Width);
		for (UINT i = 0; i < Width; ++i) {
			const float value = peak / 80.0f * i / (Width - 1);
			pixels[i] = { value, value * 0.5f, value * 0.25f, 0.375f };
		}
		auto source32 = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, pixels.data());
		auto source16 = Texture(DXGI_FORMAT_R16G16B16A16_FLOAT);
		auto proxy = Texture(DXGI_FORMAT_R8G8B8A8_UNORM);
		auto hdr = Texture(DXGI_FORMAT_R16G16B16A16_FLOAT);
		AdapterConstants cb{1, 1, white / peak, peak, 1, 80, white, 1, 2, 4, 1, 1};
		Dispatch(source32.Get(), source16.Get(), cb);
		cb.mode = 0;
		Dispatch(source16.Get(), proxy.Get(), cb);
		cb.mode = 1; cb.inputTransfer = 2; cb.outputTransfer = 1;
		Dispatch(proxy.Get(), hdr.Get(), cb);
		const auto paired = Read(hdr.Get());
		for (UINT i = 0; i < Width; ++i) {
			for (int c = 0; c < 3; ++c)
				Check(std::abs(paired[i][c] - pixels[i][c]) < peak / 80.0f * 0.04f + 0.002f,
					"FP16/RGBA8 compatibility pair exceeded quantization budget");
			Check(std::abs(paired[i][3] - 96.0f / 255) < 0.001f, "FP16 pair alpha changed");
		}
		// The same SDR proxy must map to SDR white, not to the old HDR peak.
		const auto sdr = Read(proxy.Get());
		cb.mode = 7;
		Dispatch(proxy.Get(), hdr.Get(), cb);
		const auto native = Read(hdr.Get());
		for (UINT i = 0; i < Width; ++i) for (int c = 0; c < 3; ++c) {
			const float expected = HdrColorTransform::DecodeTransfer(sdr[i][c], HdrTransferFunction::SRGB) * white / 80.0f;
			Check(std::abs(native[i][c] - expected) < 0.005f, "Native SDR white mapping used the pair inverse");
		}
		Check(std::abs(native.back()[0] - white / 80.0f) < 0.005f, "SDR white did not map to requested nits");
		cb.mode = 8; cb.inputTransfer = 1; cb.outputTransfer = 2;
		Dispatch(source16.Get(), proxy.Get(), cb);
		const auto display = Read(proxy.Get());
		for (UINT i = 1; i < Width; ++i)
			Check(display[i][0] >= display[i - 1][0] && display[i][0] <= 1, "Display tone map reversed or overflowed");
		// Negative scene values and highlights must stay finite on the SDR boundary.
		pixels[0] = {-1, 100, 0, 0.375f};
		auto extremes = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, pixels.data());
		Dispatch(extremes.Get(), proxy.Get(), cb);
		const auto bounded = Read(proxy.Get());
		for (int c = 0; c < 4; ++c) Check(std::isfinite(bounded[0][c]) && bounded[0][c] >= 0 && bounded[0][c] <= 1,
			"Display boundary emitted invalid values");
	}

	void TestAutomaticDlssnrBoundary(float white) {
		std::vector<Pixel> pixels(Width);
		for (UINT i = 0; i < Width; ++i) {
			const float value = 12.5f * i / (Width - 1);
			pixels[i] = { value, value * 0.5f, value * 0.25f, 1.0f };
		}
		auto input = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, pixels.data());
		auto proxy = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT);
		auto output = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT);
		AdapterConstants cb{};
		cb.exposure = cb.inverseExposure = cb.normalizationScale = 1.0f;
		cb.sdrWhiteNits = white;
		cb.mode = 2;
		Dispatch(input.Get(), proxy.Get(), cb);
		const auto normalized = Read(proxy.Get());
		cb.mode = 3;
		Dispatch(proxy.Get(), output.Get(), cb);
		const auto restored = Read(output.Get());
		for (UINT i = 0; i < Width; ++i) for (int channel = 0; channel < 3; ++channel) {
			Check(std::abs(normalized[i][channel] - pixels[i][channel] / (white / 80.0f)) < 1e-4f,
				"Automatic DLSSNR input white normalization mismatch");
			Check(std::abs(restored[i][channel] - pixels[i][channel]) < 1e-4f,
				"Automatic DLSSNR boundary round trip changed brightness");
		}
	}

	void TestResizedBoundaries() {
		for (const auto size : {std::array<UINT, 2>{640, 360}, {1280, 720}}) {
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = size[0]; desc.Height = size[1]; desc.MipLevels = 1; desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; desc.SampleDesc.Count = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			ComPtr<ID3D11Texture2D> texture;
			Hr(device->CreateTexture2D(&desc, nullptr, &texture));
			HdrFrame frame;
			frame.texture = texture.Get();
			frame.metadata.width = desc.Width; frame.metadata.height = desc.Height;
			frame.metadata.frameId = 1; frame.metadata.valid = true;
			frame.metadata.stage = HdrFrameStage::CanonicalInput;
			frame.metadata.sourceFormat = desc.Format;
			frame.metadata.color.primaries = HdrColorPrimaries::Rec709;
			frame.metadata.color.transfer = HdrTransferFunction::Linear;
			frame.metadata.color.range = HdrColorRange::SceneLinear;
			for (const auto& routes : {EffectProtocolC::Bicubic(),
				EffectProtocolC::FrameGenerationMarker("DLSSFG"), EffectProtocolC::FrameGenerationMarker("XeSSFG")}) {
				const auto boundary = HdrEffectBoundary::Prepare(true, frame, routes, frame.metadata.color);
				Check(boundary.prepared && boundary.plan.profile == HdrAdapterProfile::DirectFP16,
					"Bicubic/FG resized canonical boundary was not prepared");
				Check(!boundary.plan.requiresSdrMapping && !boundary.plan.isPresentationTerminal,
					"Bicubic/FG marker introduced a color conversion");
			}
		}
	}
};

int main() {
	try {
		Harness harness;
		for (float white : {80.0f, 203.0f, 360.0f})
			for (float peak : {400.0f, 1000.0f, 4000.0f})
				for (float exposure : {0.5f, 1.0f, 2.0f})
					for (bool quantized : {false, true}) harness.Test(white, peak, exposure, quantized);
		harness.TestResizedBoundaries();
		for (float white : {80.0f, 203.0f, 360.0f})
			for (float peak : {400.0f, 1000.0f, 4000.0f}) harness.TestExplicitComponents(white, peak);
		for (float white : {80.0f, 203.0f, 360.0f}) harness.TestAutomaticDlssnrBoundary(white);
		std::cout << "PASS: production HDR shader on D3D11 WARP; 9 explicit FP16/RGBA8 component pipelines, native white mapping and SDR display maps; 54 ramp/round-trip cases, "
			"R8 quantization, CPU/HLSL parity, alpha, resized boundaries and three automatic DLSSNR white-point round trips\n";
	} catch (const std::exception& error) {
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
