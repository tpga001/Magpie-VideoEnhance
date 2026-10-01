#include "pch.h"
#include "RTXVideoVsr.h"
#include "RTXVideoParameters.h"
#include "DeviceResources.h"
#include "Logger.h"
#include "NgxRuntimeGuard.h"
#include "Win32Helper.h"

namespace Magpie {

RTXVideoVsr::~RTXVideoVsr() {
	if (_instance && _destroy && FAILED(_destroy(_instance))) {
		NgxRuntimeGuard::MarkShutdownFailed();
	}
	if (NgxRuntimeGuard::IsFaulted()) (void)_module.release();
}

bool RTXVideoVsr::Initialize(DeviceResources& resources, ID3D11Texture2D* input,
	ID3D11Texture2D* output, uint32_t qualityLevel) noexcept {
	_initializationError = ScalingError::NoError;
	if (NgxRuntimeGuard::IsFaulted() || qualityLevel < 1 || qualityLevel > 4 ||
		!Resize(resources, input, output)) {
		return false;
	}

	const auto directory = Win32Helper::GetExePath().parent_path();
	_module.reset(LoadLibraryExW((directory / L"Magpie.RtxVideo.dll").c_str(), nullptr,
		LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
	if (!_module) {
		Logger::Get().Win32Error("Load RTX Video VSR bridge failed");
		_initializationError = ScalingError::NvidiaVsrPathUnsupported;
		return false;
	}
	const auto create = reinterpret_cast<RtxVsrCreateFn>(
		GetProcAddress(_module.get(), "MagpieRtxVsrCreate"));
	_draw = reinterpret_cast<RtxVsrDrawFn>(
		GetProcAddress(_module.get(), "MagpieRtxVsrDraw"));
	_destroy = reinterpret_cast<RtxVsrDestroyFn>(
		GetProcAddress(_module.get(), "MagpieRtxVsrDestroy"));
	if (!create || !_draw || !_destroy) {
		Logger::Get().Error("RTX Video VSR bridge exports are incomplete");
		_initializationError = ScalingError::NvidiaVsrPathUnsupported;
		return false;
	}

	_context = resources.GetD3DDC();
	uint32_t status = 0;
	const HRESULT result = create(
		resources.GetD3DDevice(), directory.c_str(), &_instance, &status);
	if (FAILED(result)) {
		Logger::Get().Error(fmt::format(
			"RTX Video VSR initialization failed: HRESULT={:#x} NGX/exception={:#x}",
			static_cast<uint32_t>(result), status));
		if (result == E_UNEXPECTED) NgxRuntimeGuard::MarkShutdownFailed();
		_initializationError = ScalingError::NvidiaVsrPathUnsupported;
		return false;
	}

	_qualityLevel = qualityLevel;
	D3D11_TEXTURE2D_DESC in{}, out{};
	input->GetDesc(&in);
	output->GetDesc(&out);
	Logger::Get().Info(fmt::format(
		"RTX Video VSR initialized: quality={}, {}x{} -> {}x{}, SDK=1.1 D3D11",
		_qualityLevel, in.Width, in.Height, out.Width, out.Height));
	return true;
}

bool RTXVideoVsr::Resize(DeviceResources&, ID3D11Texture2D* input,
	ID3D11Texture2D* output) noexcept {
	if (!input || !output) return false;
	D3D11_TEXTURE2D_DESC in{}, out{};
	input->GetDesc(&in);
	output->GetDesc(&out);
	return MagpieRtxVsrEndpointsSupported(in, out);
}

bool RTXVideoVsr::Draw(const NativeEffectDrawContext& context) noexcept {
	if (!_instance || !_draw || !_context || NgxRuntimeGuard::IsFaulted()) return false;
	_context->ClearState();
	uint32_t status = 0;
	const HRESULT result = _draw(
		_instance, context.input, context.output, _qualityLevel, &status);
	_context->ClearState();
	if (FAILED(result)) {
		Logger::Get().Error(fmt::format(
			"RTX Video VSR evaluation failed: HRESULT={:#x} NGX/exception={:#x}",
			static_cast<uint32_t>(result), status));
		if (result == E_UNEXPECTED) NgxRuntimeGuard::MarkShutdownFailed();
		return false;
	}
	if (++_drawCount <= 2) {
		Logger::Get().Info(fmt::format(
			"RTX Video VSR Draw succeeded: frame={} quality={}",
			_drawCount, _qualityLevel));
	}
	return true;
}

EffectParameterApplyMode RTXVideoVsr::GetParameterApplyMode(
	std::string_view name) const noexcept {
	return name == "strength" ? EffectParameterApplyMode::Live
		: EffectParameterApplyMode::Unavailable;
}

bool RTXVideoVsr::ApplyParameters(const EffectOption& option,
	std::span<const std::string> names) noexcept {
	if (std::ranges::any_of(names, [](const auto& name) { return name != "strength"; })) {
		return false;
	}
	const auto value = option.parameters.find("strength");
	if (value == option.parameters.end() ||
		value->second != float(NormalizeRTXVideoStrength(value->second))) {
		return false;
	}
	_qualityLevel = RTXVideoQualityLevel(1, NormalizeRTXVideoStrength(value->second));
	Logger::Get().Info(fmt::format("RTX Video VSR quality applied: {}", _qualityLevel));
	return true;
}

}
