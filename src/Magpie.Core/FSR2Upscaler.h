#pragma once
#include "NativeEffectBackend.h"
#include "GroupBEffectProtocol.h"

namespace Magpie {

class DeviceResources;

class FSR2Upscaler final : public NativeEffectBackend {
public:
	FSR2Upscaler() = default;
	FSR2Upscaler(const FSR2Upscaler&) = delete;
	FSR2Upscaler& operator=(const FSR2Upscaler&) = delete;
	~FSR2Upscaler() override;

	bool Initialize(DeviceResources& resources, ID3D11Texture2D* input, ID3D11Texture2D* output,
		MotionVectorRequest motionRequest = {}) noexcept;
	bool Resize(DeviceResources& resources, ID3D11Texture2D* input, ID3D11Texture2D* output) noexcept override;
	bool Draw(const NativeEffectDrawContext& context) noexcept override;
	void SetFsrHdrProtocol(const FsrHdrProtocol& protocol) noexcept {
		_hdrProtocol = protocol;
	}
	FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept override {
		FrameGuidanceRequirements result{ .zero = true };
		result.Add(_motionRequest);
		return result;
	}

	EffectParameterRestartReason GetParameterRestartReason(std::string_view) const noexcept override {
		return EffectParameterRestartReason::FrameGuidance;
	}

private:
	MotionVectorRequest _motionRequest{};
	void _Reset() noexcept;

	// These fields are active only in the MP_ENABLE_FSR2_ZEROMV build.
	[[maybe_unused]] ID3D11Device* _device = nullptr;
	[[maybe_unused]] ID3D11DeviceContext4* _d3dDC = nullptr;
	[[maybe_unused]] HMODULE _coreModule = nullptr;
	[[maybe_unused]] HMODULE _backendModule = nullptr;
	[[maybe_unused]] void* _context = nullptr;
	[[maybe_unused]] void* _scratch = nullptr;
	[[maybe_unused]] size_t _scratchSize = 0;
	[[maybe_unused]] void* _contextCreate = nullptr;
	[[maybe_unused]] void* _contextDestroy = nullptr;
	[[maybe_unused]] void* _contextDispatch = nullptr;
	[[maybe_unused]] void* _getInterface = nullptr;
	[[maybe_unused]] void* _getScratchSize = nullptr;
	[[maybe_unused]] void* _getDevice = nullptr;
	[[maybe_unused]] void* _getResource = nullptr;
	[[maybe_unused]] winrt::com_ptr<ID3D11Texture2D> _zeroMotion;
	[[maybe_unused]] winrt::com_ptr<ID3D11UnorderedAccessView> _zeroMotionUav;
	[[maybe_unused]] winrt::com_ptr<ID3D11Texture2D> _zeroDepth;
	[[maybe_unused]] winrt::com_ptr<ID3D11UnorderedAccessView> _zeroDepthUav;
	[[maybe_unused]] winrt::com_ptr<ID3D11Texture2D> _reactive;
	[[maybe_unused]] winrt::com_ptr<ID3D11UnorderedAccessView> _reactiveUav;
	[[maybe_unused]] winrt::com_ptr<ID3D11Texture2D> _exposure;
	[[maybe_unused]] winrt::com_ptr<ID3D11UnorderedAccessView> _exposureUav;
	[[maybe_unused]] bool _resetHistory = true;
	[[maybe_unused]] FrameGuidanceFrameId _lastGuidanceResetFrameId = std::numeric_limits<FrameGuidanceFrameId>::max();
	[[maybe_unused]] bool _enableOpticalFlow = false;
	FsrHdrProtocol _hdrProtocol{};
};

}
