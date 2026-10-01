#pragma once
#include "NativeEffectBackend.h"
#include <memory>

namespace Magpie {
class DeviceResources;

// Post-chain residual history. Never feeds an accumulated image back into NGX.
class DLSSNRTemporal {
public:
	DLSSNRTemporal() noexcept;
	~DLSSNRTemporal();
	bool Initialize(DeviceResources& resources, ID3D11Texture2D* input,
		ID3D11Texture2D* base, ID3D11Texture2D* raw, ID3D11Texture2D* output,
		int mode, bool hdr) noexcept;
	void Reset() noexcept;
	bool Draw(const NativeEffectDrawContext& context) noexcept;
private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};
}
