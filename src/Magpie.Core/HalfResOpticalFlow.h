#pragma once

namespace Magpie {

// Lightweight colour-only block matching. Motion is estimated on a half-size
// grid and expanded to render resolution for temporal upscalers.
class HalfResOpticalFlow {
public:
	HalfResOpticalFlow() = default;
	HalfResOpticalFlow(const HalfResOpticalFlow&) = delete;
	HalfResOpticalFlow& operator=(const HalfResOpticalFlow&) = delete;

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
		ID3D11Texture2D* input) noexcept;
	bool Estimate(ID3D11Texture2D* input) noexcept;
	ID3D11Texture2D* GetMotionTexture() const noexcept { return _fullFlow.get(); }
	void ResetHistory() noexcept { _hasHistory = false; }

private:
	bool _CreateInputSrv(ID3D11Texture2D* input) noexcept;

	ID3D11Device* _device = nullptr;
	ID3D11DeviceContext* _context = nullptr;
	ID3D11Texture2D* _input = nullptr;
	UINT _width = 0;
	UINT _height = 0;
	UINT _halfWidth = 0;
	UINT _halfHeight = 0;
	bool _hasHistory = false;
	winrt::com_ptr<ID3D11Texture2D> _previous;
	winrt::com_ptr<ID3D11ShaderResourceView> _previousSrv;
	winrt::com_ptr<ID3D11ShaderResourceView> _inputSrv;
	winrt::com_ptr<ID3D11Texture2D> _halfFlow;
	winrt::com_ptr<ID3D11ShaderResourceView> _halfFlowSrv;
	winrt::com_ptr<ID3D11UnorderedAccessView> _halfFlowUav;
	winrt::com_ptr<ID3D11Texture2D> _fullFlow;
	winrt::com_ptr<ID3D11UnorderedAccessView> _fullFlowUav;
	winrt::com_ptr<ID3D11ComputeShader> _estimateShader;
	winrt::com_ptr<ID3D11ComputeShader> _upsampleShader;
	winrt::com_ptr<ID3D11Buffer> _constantBuffer;
};

}
