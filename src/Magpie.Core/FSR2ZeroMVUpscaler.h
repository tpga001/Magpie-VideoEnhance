#pragma once
#include "HalfResOpticalFlow.h"
#include "NativeEffectBackend.h"

namespace Magpie {

class DeviceResources;

class FSR2ZeroMVUpscaler final : public NativeEffectBackend {
public:
	FSR2ZeroMVUpscaler() = default;
	FSR2ZeroMVUpscaler(const FSR2ZeroMVUpscaler&) = delete;
	FSR2ZeroMVUpscaler& operator=(const FSR2ZeroMVUpscaler&) = delete;
	~FSR2ZeroMVUpscaler() override;

	bool Initialize(DeviceResources& resources, ID3D11Texture2D* input, ID3D11Texture2D* output,
		bool enableOpticalFlow = false, bool enableJitter = false) noexcept;
	bool Resize(DeviceResources& resources, ID3D11Texture2D* input, ID3D11Texture2D* output) noexcept override;
	bool Draw(const NativeEffectDrawContext& context) noexcept override;

private:
	void _Reset() noexcept;

	ID3D11Device* _device = nullptr;
	ID3D11DeviceContext* _d3dDC = nullptr;
	HMODULE _coreModule = nullptr;
	HMODULE _backendModule = nullptr;
	void* _context = nullptr;
	void* _scratch = nullptr;
	size_t _scratchSize = 0;
	void* _contextCreate = nullptr;
	void* _contextDestroy = nullptr;
	void* _contextDispatch = nullptr;
	void* _getInterface = nullptr;
	void* _getScratchSize = nullptr;
	void* _getDevice = nullptr;
	void* _getResource = nullptr;
	winrt::com_ptr<ID3D11Texture2D> _zeroMotion;
	winrt::com_ptr<ID3D11UnorderedAccessView> _zeroMotionUav;
	winrt::com_ptr<ID3D11Texture2D> _zeroDepth;
	winrt::com_ptr<ID3D11UnorderedAccessView> _zeroDepthUav;
	winrt::com_ptr<ID3D11Texture2D> _reactive;
	winrt::com_ptr<ID3D11UnorderedAccessView> _reactiveUav;
	bool _resetHistory = true;
	bool _enableOpticalFlow = false;
	bool _enableJitter = false;
	uint32_t _frameIndex = 0;
	std::unique_ptr<HalfResOpticalFlow> _opticalFlow;
};

}
