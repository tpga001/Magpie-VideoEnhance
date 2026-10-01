#include "pch.h"
#include "NgxRuntimeGuard.h"
#include "NgxD3D12Core.h"
#include "DeviceResources.h"
#include "Logger.h"
#include "Win32Helper.h"

#if defined(MP_ENABLE_DLSSNR) || defined(MP_ENABLE_DLSS_FRAME_GENERATION)
#include <nvsdk_ngx.h>
#endif

namespace Magpie {

#if defined(MP_ENABLE_DLSSNR) || defined(MP_ENABLE_DLSS_FRAME_GENERATION)

namespace {

NVSDK_NGX_Result InitCoreSafely(
	const wchar_t* applicationDirectory,
	ID3D12Device* device,
	const NVSDK_NGX_FeatureCommonInfo* featureInfo,
	DWORD* sehCode
) noexcept {
	return NgxRuntimeGuard::Invoke([&]() {
		return NVSDK_NGX_D3D12_Init_with_ProjectID(
			"7c134ab9-9677-4af5-a2b2-bca943350861",
			NVSDK_NGX_ENGINE_TYPE_CUSTOM,
			"Magpie-Experimental-0.5.7",
			applicationDirectory,
			device,
			featureInfo,
			NVSDK_NGX_Version_API);
	}, NVSDK_NGX_Result_FAIL_PlatformError, sehCode);
}

NVSDK_NGX_Result AllocateParametersSafely(
	NVSDK_NGX_Parameter** parameters,
	DWORD* sehCode
) noexcept {
	return NgxRuntimeGuard::Invoke([&]() {
		return NVSDK_NGX_D3D12_AllocateParameters(parameters);
	}, NVSDK_NGX_Result_FAIL_PlatformError, sehCode);
}

NVSDK_NGX_Result GetCapabilityParametersSafely(
	NVSDK_NGX_Parameter** parameters,
	DWORD* sehCode
) noexcept {
	return NgxRuntimeGuard::Invoke([&]() {
		return NVSDK_NGX_D3D12_GetCapabilityParameters(parameters);
	}, NVSDK_NGX_Result_FAIL_PlatformError, sehCode);
}

NVSDK_NGX_Result DestroyParametersSafely(
	NVSDK_NGX_Parameter* parameters,
	DWORD* sehCode
) noexcept {
	return NgxRuntimeGuard::Invoke([&]() {
		return NVSDK_NGX_D3D12_DestroyParameters(parameters);
	}, NVSDK_NGX_Result_FAIL_PlatformError, sehCode);
}

NVSDK_NGX_Result ShutdownCoreSafely(
	ID3D12Device* device,
	DWORD* sehCode
) noexcept {
	return NgxRuntimeGuard::Invoke([&]() {
		return NVSDK_NGX_D3D12_Shutdown1(device);
	}, NVSDK_NGX_Result_FAIL_PlatformError, sehCode);
}

bool LogNgxResult(
	std::string_view operation,
	std::string_view consumer,
	NVSDK_NGX_Result result,
	DWORD sehCode
) noexcept {
	if (sehCode) {
		Logger::Get().Error(fmt::format(
			"NGX D3D12 Core {} for {} raised SEH {:#x} at {:#x}, thread={}; "
			"NGX disabled until Magpie is restarted",
			operation, consumer, sehCode, NgxRuntimeGuard::FaultAddress(),
			NgxRuntimeGuard::FaultThread()));
		return false;
	}
	if (!NVSDK_NGX_SUCCEED(result)) {
		Logger::Get().Error(fmt::format(
			"NGX D3D12 Core {} for {} failed ({:#x})",
			operation, consumer, static_cast<uint32_t>(result)));
		return false;
	}
	return true;
}

}

NgxD3D12Core::~NgxD3D12Core() {
	_Shutdown();
}

bool NgxD3D12Core::Acquire(
	DeviceResources& resources,
	std::string_view consumer
) noexcept {
	if (NgxRuntimeGuard::IsFaulted()) {
		Logger::Get().Error("NGX initialization blocked after an earlier fault; restart Magpie");
		return false;
	}
	if (!_device) {
		const HRESULT hr = D3D12CreateDevice(
			resources.GetGraphicsAdapter(), D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(_device.put()));
		if (FAILED(hr)) {
			Logger::Get().ComError("Create shared NGX D3D12 device failed", hr);
			return false;
		}
	}
	if (!_initialized) {
		const std::filesystem::path applicationDirectory =
			Win32Helper::GetExePath().parent_path();
		const std::wstring featurePath = applicationDirectory.wstring();
		const wchar_t* featurePaths[]{ featurePath.c_str() };
		NVSDK_NGX_FeatureCommonInfo featureInfo{};
		featureInfo.PathListInfo.Path = featurePaths;
		featureInfo.PathListInfo.Length = 1;
		DWORD sehCode = 0;
		const NVSDK_NGX_Result result = InitCoreSafely(
			applicationDirectory.c_str(), _device.get(), &featureInfo, &sehCode);
		if (!LogNgxResult("Init", consumer, result, sehCode)) return false;
		_initialized = true;
		Logger::Get().Info("NGX D3D12 Core initialized once for Renderer session");
	}
	++_activeConsumers;
	Logger::Get().Info(fmt::format(
		"NGX D3D12 Core consumer acquired: name={} active={}",
		consumer, _activeConsumers));
	return true;
}

void NgxD3D12Core::Release(std::string_view consumer) noexcept {
	if (!_activeConsumers) {
		Logger::Get().Warn(fmt::format(
			"NGX D3D12 Core consumer release underflow: name={}", consumer));
		return;
	}
	--_activeConsumers;
	Logger::Get().Info(fmt::format(
		"NGX D3D12 Core consumer released: name={} active={}",
		consumer, _activeConsumers));
}

bool NgxD3D12Core::AllocateParameters(
	NVSDK_NGX_Parameter** parameters,
	std::string_view consumer
) noexcept {
	if (!_initialized || !parameters) return false;
	DWORD sehCode = 0;
	const NVSDK_NGX_Result result =
		AllocateParametersSafely(parameters, &sehCode);
	if (!LogNgxResult("AllocateParameters", consumer, result, sehCode) ||
		!*parameters) return false;
	++_activeParameterBlocks;
	return true;
}

bool NgxD3D12Core::GetCapabilityParameters(
	NVSDK_NGX_Parameter** parameters,
	std::string_view consumer
) noexcept {
	if (!_initialized || !parameters) return false;
	DWORD sehCode = 0;
	const NVSDK_NGX_Result result =
		GetCapabilityParametersSafely(parameters, &sehCode);
	if (!LogNgxResult("GetCapabilityParameters", consumer, result, sehCode) ||
		!*parameters) return false;
	++_activeParameterBlocks;
	return true;
}

bool NgxD3D12Core::DestroyParameters(
	NVSDK_NGX_Parameter* parameters,
	std::string_view consumer
) noexcept {
	if (!parameters) return true;
	DWORD sehCode = 0;
	const NVSDK_NGX_Result result =
		DestroyParametersSafely(parameters, &sehCode);
	const bool succeeded =
		LogNgxResult("DestroyParameters", consumer, result, sehCode);
	if (succeeded && _activeParameterBlocks) --_activeParameterBlocks;
	return succeeded;
}

void NgxD3D12Core::_Shutdown() noexcept {
	if (NgxRuntimeGuard::IsFaulted()) {
		// Keep the device alive for SDK state whose teardown was interrupted.
		(void)_device.detach();
		_initialized = false;
		return;
	}
	if (!_initialized) return;
	if (_activeConsumers || _activeParameterBlocks) {
		Logger::Get().Warn(fmt::format(
			"NGX D3D12 Core final shutdown with live state: consumers={} parameters={}",
			_activeConsumers, _activeParameterBlocks));
	}
	DWORD sehCode = 0;
	const NVSDK_NGX_Result result = ShutdownCoreSafely(_device.get(), &sehCode);
	if (LogNgxResult("final Shutdown1", "Renderer", result, sehCode)) {
		Logger::Get().Info("NGX D3D12 Core final Shutdown1 completed");
	} else {
		NgxRuntimeGuard::MarkShutdownFailed();
		(void)_device.detach();
	}
	_initialized = false;
}

#else

NgxD3D12Core::~NgxD3D12Core() = default;
bool NgxD3D12Core::Acquire(DeviceResources&, std::string_view) noexcept { return false; }
void NgxD3D12Core::Release(std::string_view) noexcept {}
bool NgxD3D12Core::AllocateParameters(
	NVSDK_NGX_Parameter**, std::string_view) noexcept { return false; }
bool NgxD3D12Core::GetCapabilityParameters(
	NVSDK_NGX_Parameter**, std::string_view) noexcept { return false; }
bool NgxD3D12Core::DestroyParameters(
	NVSDK_NGX_Parameter*, std::string_view) noexcept { return false; }
void NgxD3D12Core::_Shutdown() noexcept {}

#endif

}
