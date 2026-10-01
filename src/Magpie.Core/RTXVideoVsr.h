#pragma once
#include "NativeEffectBackend.h"
#include "../RtxVideoBridge/RtxVideoBridge.h"

namespace Magpie {

class RTXVideoVsr final : public NativeEffectBackend {
public:
	~RTXVideoVsr();

	bool Initialize(DeviceResources& resources, ID3D11Texture2D* input,
		ID3D11Texture2D* output, uint32_t qualityLevel) noexcept;
	bool Resize(DeviceResources&, ID3D11Texture2D*, ID3D11Texture2D*) noexcept override;
	bool Draw(const NativeEffectDrawContext& context) noexcept override;
	EffectParameterApplyMode GetParameterApplyMode(std::string_view name) const noexcept override;
	bool ApplyParameters(const EffectOption& option,
		std::span<const std::string> names) noexcept override;
	ScalingError InitializationError() const noexcept { return _initializationError; }

private:
	wil::unique_hmodule _module;
	void* _instance = nullptr;
	RtxVsrDrawFn _draw = nullptr;
	RtxVsrDestroyFn _destroy = nullptr;
	ID3D11DeviceContext* _context = nullptr;
	uint32_t _qualityLevel = 2;
	uint64_t _drawCount = 0;
	ScalingError _initializationError = ScalingError::NoError;
};

}
