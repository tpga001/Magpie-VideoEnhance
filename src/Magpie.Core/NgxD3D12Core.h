#pragma once
#include <d3d12.h>

struct NVSDK_NGX_Parameter;

namespace Magpie {

class DeviceResources;

// Renderer-session owner for the process-global NGX D3D12 Core state. Feature
// consumers may own independent queues, but share this device and final shutdown.
class NgxD3D12Core {
public:
	NgxD3D12Core() = default;
	NgxD3D12Core(const NgxD3D12Core&) = delete;
	NgxD3D12Core& operator=(const NgxD3D12Core&) = delete;
	~NgxD3D12Core();

	bool Acquire(DeviceResources& resources, std::string_view consumer) noexcept;
	void Release(std::string_view consumer) noexcept;
	ID3D12Device* Device() const noexcept { return _device.get(); }

	bool AllocateParameters(
		NVSDK_NGX_Parameter** parameters,
		std::string_view consumer
	) noexcept;
	bool GetCapabilityParameters(
		NVSDK_NGX_Parameter** parameters,
		std::string_view consumer
	) noexcept;
	bool DestroyParameters(
		NVSDK_NGX_Parameter* parameters,
		std::string_view consumer
	) noexcept;

private:
	void _Shutdown() noexcept;

	winrt::com_ptr<ID3D12Device> _device;
	// 以下字段只在各 SDK 门控后端(同一 TU 之外)中读取，本 TU 不触达；
	// 无 SDK 的 CI 构建里 ClangCL -Werror、-Wunused-private-field 会报错
	[[maybe_unused]] uint32_t _activeConsumers = 0;
	[[maybe_unused]] uint32_t _activeParameterBlocks = 0;
	[[maybe_unused]] bool _initialized = false;
};

}
