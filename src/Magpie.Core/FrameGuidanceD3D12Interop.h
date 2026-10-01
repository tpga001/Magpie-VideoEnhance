#pragma once
#include "FrameGuidanceTypes.h"
#include <d3d12.h>

namespace Magpie {

// Reusable ownership boundary for sharing Renderer-owned D3D11 guidance with
// D3D12 consumers. It validates coherence, opens NT handles, tracks the last
// consumer fence value and emits matching state transitions.
class FrameGuidanceD3D12Interop {
public:
	bool Initialize(ID3D12Device* device, ID3D12Fence* consumerFence) noexcept;
	bool Update(
		const FrameGuidanceView& view,
		FrameGuidanceFrameId frameId,
		FrameGuidanceExtent extent
	) noexcept;
	bool WaitForProducer(
		ID3D11DeviceContext4* context,
		const FrameGuidanceView& view
	) noexcept;
	void Transition(
		ID3D12GraphicsCommandList* commandList,
		D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after
	) noexcept;
	void MarkSubmitted(uint64_t fenceValue) noexcept {
		_lastConsumerFenceValue = fenceValue;
	}

	ID3D12Resource* Motion() const noexcept { return _motion12.get(); }
	ID3D12Resource* Depth() const noexcept { return _depth12.get(); }

private:
	bool _WaitForConsumer() noexcept;
	bool _OpenShared(
		ID3D11Texture2D* texture11,
		winrt::com_ptr<ID3D12Resource>& texture12
	) noexcept;

	ID3D12Device* _device = nullptr;
	ID3D12Fence* _consumerFence = nullptr;
	ID3D11Texture2D* _motion11 = nullptr;
	ID3D11Texture2D* _depth11 = nullptr;
	winrt::com_ptr<ID3D12Resource> _motion12;
	winrt::com_ptr<ID3D12Resource> _depth12;
	uint64_t _lastConsumerFenceValue = 0;
};

}
