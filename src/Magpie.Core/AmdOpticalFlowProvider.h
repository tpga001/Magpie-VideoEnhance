#pragma once
#include "FrameGuidanceProvider.h"
#include "GroupBEffectProtocol.h"

namespace Magpie {

// Cross-vendor FidelityFX Optical Flow provider. "AMD OF" identifies the
// algorithm, not a vendor restriction: any D3D12 adapter satisfying the
// shader-model, wave-op and format requirements may create it.
class AmdOpticalFlowProvider final : public IMotionVectorProvider {
public:
	struct Impl;

	explicit AmdOpticalFlowProvider(
		AmdOpticalFlowMode mode = AmdOpticalFlowMode::Quality);
	AmdOpticalFlowProvider(const AmdOpticalFlowProvider&) = delete;
	AmdOpticalFlowProvider& operator=(const AmdOpticalFlowProvider&) = delete;
	~AmdOpticalFlowProvider() override;

	bool Initialize(
		DeviceResources& resources,
		FrameGuidanceExtent sourceExtent
	) noexcept override;
	bool BeginFrame(
		const FrameGuidanceFrame& frame,
		MotionVectorProviderOutput& output
	) noexcept override;
	void Reset(FrameGuidanceResetReason reason) noexcept override;
	bool Resize(FrameGuidanceExtent sourceExtent) noexcept override;
	OpticalFlowInitializationError InitializationError() const noexcept override;
	void SetHdrProtocol(const AmdOpticalFlowHdrProtocol& protocol) noexcept { _hdrProtocol = protocol; }

private:
	// 仅在 MP_ENABLE_AMD_OPTICAL_FLOW 构建中使用；无 SDK 的 CI 构建里 ClangCL -Werror 会报未使用
	[[maybe_unused]] AmdOpticalFlowMode _mode;
	std::unique_ptr<Impl> _impl;
	AmdOpticalFlowHdrProtocol _hdrProtocol{};
};

}
