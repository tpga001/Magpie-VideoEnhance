#pragma once
#include "DLSSNRFilter.h"
#include "DLSSNRTemporal.h"
#include "DLSSNRParameters.h"
#include "DeviceResources.h"
#include "Logger.h"

namespace Magpie {

// One renderer effect owns the serial chain. Each filter owns its feature,
// command queue, interop fences and temporal history.
class DLSSNRMultiPass final : public NativeEffectBackend {
public:
	bool Initialize(DeviceResources& resources, NgxD3D12Core& core,
		ID3D11Texture2D* input, ID3D11Texture2D* output,
		const EffectOption& option, bool hdr) noexcept {
		if (!Drain()) return false;
		_filters.clear();
		_intermediates.clear();
		_temporal.reset();
		_rawOutput = {};
		_core = &core;
		_option = option;
		_hdr = hdr;
		_revisions = {};
		const int count = Count(option);
		const int antiFlicker = DLSSNRAntiFlickerMode([&](std::string_view name, float fallback) {
			const auto it = option.parameters.find(std::string(name));
			return it == option.parameters.end() ? fallback : it->second;
		});
		D3D11_TEXTURE2D_DESC desc{};
		output->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		for (int i = 1; i < count; ++i) {
			winrt::com_ptr<ID3D11Texture2D> texture;
			const HRESULT hr = resources.GetD3DDevice()->CreateTexture2D(&desc, nullptr, texture.put());
			if (FAILED(hr)) {
				Logger::Get().ComError("Create DLSSNR Multi Pass intermediate failed", hr);
				return false;
			}
			_intermediates.push_back(std::move(texture));
		}
		if (antiFlicker && FAILED(resources.GetD3DDevice()->CreateTexture2D(&desc, nullptr, _rawOutput.put()))) {
			Logger::Get().Error("Create DLSSNR anti-flicker raw output failed");
			return false;
		}
		for (int i = 0; i < count; ++i) {
			auto filter = std::make_unique<DLSSNRFilter>();
			if (!filter->Initialize(resources, core,
				i == 0 ? input : _intermediates[i - 1].get(),
				i == count - 1 ? (antiFlicker ? _rawOutput.get() : output) : _intermediates[i].get(),
				ParseDLSSNRSettings(DLSSNRPassOption(option, i + 1), hdr))) {
				Logger::Get().Error(fmt::format("DLSSNR Multi Pass initialization failed at pass {}/{}", i + 1, count));
				return false;
			}
			filter->SetHdrBoundary(_hdrBoundary);
			_filters.push_back(std::move(filter));
		}
		if (antiFlicker) {
			_temporal = std::make_unique<DLSSNRTemporal>();
			if (!_temporal->Initialize(resources, input, count > 1 ? _intermediates[0].get() : input,
				_rawOutput.get(), output, antiFlicker, hdr)) {
				Logger::Get().Error("DLSSNR anti-flicker initialization failed");
				return false;
			}
		}
		Logger::Get().Info(fmt::format("DLSSNR Multi Pass: {} independent passes initialized", count));
		return true;
	}

	void SetHdrBoundary(HdrEffectBoundaryContext context) noexcept override {
		NativeEffectBackend::SetHdrBoundary(std::move(context));
		for (const auto& filter : _filters) filter->SetHdrBoundary(_hdrBoundary);
	}
	FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept override {
		FrameGuidanceRequirements result;
		for (const auto& filter : _filters) result.Merge(filter->GetFrameGuidanceRequirements());
		const auto it = _option.parameters.find("antiFlicker");
		if (it != _option.parameters.end() && it->second >= 2 && it->second <= 4 &&
			it->second == std::floor(it->second) && !result.HasMotion())
			result.Add(MotionVectorRequest::Amd(AmdOpticalFlowMode::Quality));
		return result;
	}
	bool Drain() noexcept override {
		bool success = true;
		for (const auto& filter : _filters) success = filter->Drain() && success;
		return success;
	}
	bool Resize(DeviceResources& resources, ID3D11Texture2D* input,
		ID3D11Texture2D* output) noexcept override {
		const EffectOption option = _option;
		return _core && Initialize(resources, *_core, input, output, option, _hdr);
	}
	bool Draw(const NativeEffectDrawContext& context) noexcept override {
		for (size_t i = 0; i < _filters.size(); ++i) {
			NativeEffectDrawContext pass = context;
			pass.input = i == 0 ? context.input : _intermediates[i - 1].get();
			pass.output = i + 1 == _filters.size() ? (_temporal ? _rawOutput.get() : context.output) : _intermediates[i].get();
			pass.inputRevision += _revisions[i];
			if (!_filters[i]->Draw(pass)) return false;
			if (_temporal && !_filters[i]->IsHealthy()) _temporal->Reset();
			if (_filters.size() > 1 && !_filters[i]->IsHealthy()) {
				Logger::Get().Error("DLSSNR Multi Pass evaluation failed; rejecting the complete chain");
				return false;
			}
		}
		return !_filters.empty() && (!_temporal || _temporal->Draw(context));
	}
	EffectParameterApplyMode GetParameterApplyMode(std::string_view name) const noexcept override {
		if (name == "multiPass" || name == "antiFlicker" || _filters.empty()) return EffectParameterApplyMode::RestartRequired;
		const size_t index = DLSSNRParameterPass(name) - 1;
		// Hidden settings can be saved without constructing a disabled pass.
		if (index >= _filters.size()) return EffectParameterApplyMode::Live;
		return _filters[index]->GetParameterApplyMode(DLSSNRBaseParameter(name));
	}
	EffectParameterRestartReason GetParameterRestartReason(std::string_view name) const noexcept override {
		if (name == "antiFlicker") return EffectParameterRestartReason::FrameGuidance;
		if (name == "multiPass" || _filters.empty()) return EffectParameterRestartReason::ResourceRecreation;
		return _filters.front()->GetParameterRestartReason(DLSSNRBaseParameter(name));
	}
	bool ApplyLiveParameters(const EffectOption& option,
		std::span<const std::string> names) noexcept override {
		if (Count(option) != static_cast<int>(_filters.size())) return false;
		std::array<std::vector<std::string>, 3> routed;
		for (const auto& name : names) {
			if (GetParameterApplyMode(name) != EffectParameterApplyMode::Live) return false;
			const auto base = DLSSNRBaseParameter(name);
			const bool independent = std::ranges::find(DLSSNR_PASS_PARAMETERS, base) != DLSSNR_PASS_PARAMETERS.end();
			for (size_t i = 0; i < _filters.size(); ++i) {
				if (!independent || i + 1 == static_cast<size_t>(DLSSNRParameterPass(name)))
					routed[i].emplace_back(base);
			}
		}
		for (size_t i = 0; i < _filters.size(); ++i) {
			if (routed[i].empty()) continue;
			if (!_filters[i]->ApplyLiveParameters(DLSSNRPassOption(option, static_cast<int>(i + 1)), routed[i])) {
				for (size_t j = 0; j < i; ++j) {
					if (!routed[j].empty()) _filters[j]->ApplyLiveParameters(
						DLSSNRPassOption(_option, static_cast<int>(j + 1)), routed[j]);
				}
				return false;
			}
		}
		for (size_t i = 0; i < _filters.size(); ++i) {
			if (!routed[i].empty()) {
				// Reevaluate downstream passes when upstream settings alter the
				// output of an otherwise duplicate captured frame.
				for (size_t j = i + 1; j < _filters.size(); ++j) ++_revisions[j];
			}
		}
		_option = option;
		if (_temporal && !names.empty()) _temporal->Reset();
		return true;
	}

private:
	static int Count(const EffectOption& option) noexcept {
		return DLSSNRPassCount([&](std::string_view name, float fallback) {
			const auto it = option.parameters.find(std::string(name));
			return it == option.parameters.end() ? fallback : it->second;
		});
	}
	NgxD3D12Core* _core = nullptr;
	EffectOption _option;
	bool _hdr = false;
	std::array<uint64_t, 3> _revisions{};
	// Textures outlive all filters that refer to them.
	std::vector<winrt::com_ptr<ID3D11Texture2D>> _intermediates;
	winrt::com_ptr<ID3D11Texture2D> _rawOutput;
	std::unique_ptr<DLSSNRTemporal> _temporal;
	std::vector<std::unique_ptr<DLSSNRFilter>> _filters;
};

}
