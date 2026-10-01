#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <vector>
#include "DLSSNRParameters.h"
#include "EffectParameterRules.h"
#include "MotionVectorRequest.h"

// Execute the production chain with deterministic SDK and texture doubles.
// This checks routing/lifetime/failure behavior without invoking NGX or a GPU.
using HRESULT = int;
constexpr int D3D11_USAGE_DEFAULT = 0;
constexpr int D3D11_BIND_SHADER_RESOURCE = 1;
constexpr int D3D11_BIND_UNORDERED_ACCESS = 2;
inline bool FAILED(HRESULT value) { return value < 0; }
struct D3D11_TEXTURE2D_DESC {
	int Width = 1280, Height = 720, Usage = 0, BindFlags = 0, CPUAccessFlags = 0, MiscFlags = 0;
};
struct ID3D11Texture2D {
	D3D11_TEXTURE2D_DESC desc;
	int value = 0;
	void GetDesc(D3D11_TEXTURE2D_DESC* result) { *result = desc; }
};
namespace winrt {
template<class T> struct com_ptr {
	std::shared_ptr<T> value;
	T* raw = nullptr;
	com_ptr() = default;
	com_ptr(com_ptr&& other) noexcept : value(std::move(other.value)), raw(other.raw) { other.raw = nullptr; }
	com_ptr& operator=(com_ptr&&) = default;
	~com_ptr() { if (!value) delete raw; }
	T** put() { return &raw; }
	T* get() const { return value ? value.get() : raw; }
};
}
namespace fmt {
template<class... Args> std::string format(const char* value, Args&&...) { return value; }
}
namespace Magpie {
struct EffectOption { std::map<std::string, float> parameters; };
enum class EffectParameterApplyMode { Live, RestartRequired, Unavailable };
enum class EffectParameterRestartReason { None, ResourceRecreation, FrameGuidance };
struct HdrEffectBoundaryContext {};
struct NativeEffectDrawContext {
	ID3D11Texture2D* input;
	ID3D11Texture2D* output;
	uint64_t inputRevision = 0;
};
struct NativeEffectBackend {
	virtual ~NativeEffectBackend() = default;
	virtual void SetHdrBoundary(HdrEffectBoundaryContext value) noexcept { _hdrBoundary = value; }
	virtual FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept = 0;
	virtual bool Drain() noexcept = 0;
	virtual bool Resize(struct DeviceResources&, ID3D11Texture2D*, ID3D11Texture2D*) noexcept = 0;
	virtual bool Draw(const NativeEffectDrawContext&) noexcept = 0;
	virtual EffectParameterApplyMode GetParameterApplyMode(std::string_view) const noexcept = 0;
	virtual EffectParameterRestartReason GetParameterRestartReason(std::string_view) const noexcept = 0;
	virtual bool ApplyLiveParameters(const EffectOption&, std::span<const std::string>) noexcept = 0;
	HdrEffectBoundaryContext _hdrBoundary;
};
struct DeviceResources {
	int failTextureAt = -1, created = 0;
	DeviceResources* GetD3DDevice() { return this; }
	HRESULT CreateTexture2D(const D3D11_TEXTURE2D_DESC* desc, void*, ID3D11Texture2D** texture) {
		if (created++ == failTextureAt) return -1;
		*texture = new ID3D11Texture2D{*desc}; return 0;
	}
};
struct NgxD3D12Core {};
struct DLSSNRTemporal {
	inline static int resets = 0, draws = 0;
	inline static ID3D11Texture2D* lastBase = nullptr;
	ID3D11Texture2D* raw = nullptr;
	bool Initialize(DeviceResources&, ID3D11Texture2D*, ID3D11Texture2D* base,
		ID3D11Texture2D* value, ID3D11Texture2D*, int, bool) {
		raw = value; lastBase = base; return true;
	}
	void Reset() { ++resets; }
	bool Draw(const NativeEffectDrawContext& context) { ++draws; context.output->value = raw->value; return true; }
};
struct Logger {
	static Logger& Get() { static Logger value; return value; }
	void Info(std::string_view) {}
	void Error(std::string_view) {}
	void ComError(std::string_view, HRESULT) {}
};
inline EffectOption ParseDLSSNRSettings(const EffectOption& value, bool) { return value; }
struct DLSSNRFilter {
	inline static int liveInstances = 0, nextId = 0, failInitAt = -1, failApplyAt = -1, failDrawAt = -1;
	inline static std::vector<uint64_t> drawRevisions;
	inline static bool healthy = true;
	bool IsHealthy() const { return healthy; }
	int id = nextId++;
	EffectOption settings;
	DLSSNRFilter() { ++liveInstances; }
	~DLSSNRFilter() { --liveInstances; }
	bool Initialize(DeviceResources&, NgxD3D12Core&, ID3D11Texture2D* input,
		ID3D11Texture2D* output, const EffectOption& option) {
		assert(input != output);
		assert(input->desc.Width == output->desc.Width && input->desc.Height == output->desc.Height);
		settings = option; return id != failInitAt;
	}
	void SetHdrBoundary(HdrEffectBoundaryContext) {}
	FrameGuidanceRequirements GetFrameGuidanceRequirements() const { return {true}; }
	bool Drain() { return true; }
	EffectParameterApplyMode GetParameterApplyMode(std::string_view name) const {
		return name == "enableInputResolutionScaling" ? EffectParameterApplyMode::RestartRequired : EffectParameterApplyMode::Live;
	}
	EffectParameterRestartReason GetParameterRestartReason(std::string_view) const { return EffectParameterRestartReason::ResourceRecreation; }
	bool ApplyLiveParameters(const EffectOption& option, std::span<const std::string>) {
		if (id == failApplyAt) return false;
		settings = option; return true;
	}
	bool Draw(const NativeEffectDrawContext& context) {
		drawRevisions.push_back(context.inputRevision);
		if (id == failDrawAt) return false;
		const auto it = settings.parameters.find("intensity");
		context.output->value = context.input->value * 10 +
			(it == settings.parameters.end() ? 1 : static_cast<int>(it->second));
		return true;
	}
};
}
#include "DLSSNRMultiPassUnderTest.h"

int main() {
	using namespace Magpie;
	EffectOption option{{{"intensity", 2.f}, {"style", 2.f}, {"residualMultiplier", 1.5f}}};
	auto get = [&](std::string_view name, float fallback) {
		const auto it = option.parameters.find(std::string(name));
		return it == option.parameters.end() ? fallback : it->second;
	};
	assert(DLSSNRPassCount(get) == 1);
	assert(DLSSNRAntiFlickerMode(get) == 0);
	for (float invalid : {-1.f, 5.f, 1.5f, std::numeric_limits<float>::quiet_NaN()}) {
		option.parameters["antiFlicker"] = invalid;
		assert(DLSSNRAntiFlickerMode(get) == 0);
	}
	option.parameters.erase("antiFlicker");
	assert(DLSSNRPassOption(option, 1).parameters.at("intensity") == 2);
	for (int pass : {2, 3}) {
		const auto fresh = DLSSNRPassOption(option, pass);
		assert(!fresh.parameters.contains("intensity") && !fresh.parameters.contains("style"));
		assert(fresh.parameters.at("residualMultiplier") == 1.5f);
	}
	for (float invalid : {-1.f, 4.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
		option.parameters["multiPass"] = invalid;
		assert(DLSSNRPassCount(get) == 1);
	}
	for (int count : {1, 2, 3, 2, 1}) {
		option.parameters["multiPass"] = static_cast<float>(count);
		assert(IsEffectParameterVisible("DLSSNR\\DLSSNR_AI_Filter", "pass2_intensity", get) == (count >= 2));
		assert(IsEffectParameterVisible("DLSSNR\\DLSSNR_AI_Filter", "pass3_intensity", get) == (count >= 3));
		assert(IsEffectParameterVisible("Custom", "pass3_intensity", get));
	}
	option.parameters["multiPass"] = 3;
	option.parameters["pass2_intensity"] = 2;
	option.parameters["pass3_intensity"] = 3;
	DeviceResources resources;
	NgxD3D12Core core;
	ID3D11Texture2D input, output;
	{
		DLSSNRMultiPass chain;
		assert(chain.Initialize(resources, core, &input, &output, option, false));
		assert(DLSSNRFilter::liveInstances == 3 && resources.created == 2);
		assert(chain.GetFrameGuidanceRequirements().zero);
		assert(chain.Draw({&input, &output, 7}));
		assert(output.value == 223); // Encodes the exact serial order and isolated settings.
		assert(chain.GetParameterApplyMode("multiPass") == EffectParameterApplyMode::RestartRequired);
		std::vector<std::string> edits{"intensity"};
		option.parameters["intensity"] = 1;
		assert(chain.ApplyLiveParameters(option, edits));
		DLSSNRFilter::drawRevisions.clear();
		assert(chain.Draw({&input, &output, 7}) && output.value == 123);
		assert((DLSSNRFilter::drawRevisions == std::vector<uint64_t>{7, 8, 8}));
		edits = {"pass2_intensity"}; option.parameters["pass2_intensity"] = 1;
		assert(chain.ApplyLiveParameters(option, edits));
		DLSSNRFilter::drawRevisions.clear();
		assert(chain.Draw({&input, &output, 7}) && output.value == 113);
		assert((DLSSNRFilter::drawRevisions == std::vector<uint64_t>{7, 8, 9}));
		// Reject a partial shared-setting transaction and restore prior filters.
		DLSSNRFilter::failApplyAt = 1;
		option.parameters["intensity"] = 2; edits = {"residualMultiplier"};
		assert(!chain.ApplyLiveParameters(option, edits));
		assert(chain.Draw({&input, &output, 7}) && output.value == 113);
		DLSSNRFilter::failApplyAt = -1;
		option.parameters["intensity"] = 1;
		assert(chain.Resize(resources, &input, &output));
		assert(DLSSNRFilter::liveInstances == 3);
		assert(chain.Draw({&input, &output, 7}) && output.value == 113);
		for (int count : {2, 1, 3}) {
			option.parameters["multiPass"] = static_cast<float>(count);
			assert(chain.Initialize(resources, core, &input, &output, option, false));
			assert(DLSSNRFilter::liveInstances == count);
		}
		assert(chain.Draw({&input, &output, 7}) && output.value == 113);
		DLSSNRFilter::failDrawAt = DLSSNRFilter::nextId - 2;
		assert(!chain.Draw({&input, &output, 7}));
		DLSSNRFilter::failDrawAt = -1;
		DLSSNRFilter::healthy = false;
		assert(!chain.Draw({&input, &output, 7}));
		DLSSNRFilter::healthy = true;
	}
	assert(DLSSNRFilter::liveInstances == 0);
	DLSSNRFilter::failInitAt = DLSSNRFilter::nextId + 1;
	{ DLSSNRMultiPass chain; assert(!chain.Initialize(resources, core, &input, &output, option, false)); }
	assert(DLSSNRFilter::liveInstances == 0);
	DLSSNRFilter::failInitAt = -1;
	resources.failTextureAt = resources.created;
	{ DLSSNRMultiPass chain; assert(!chain.Initialize(resources, core, &input, &output, option, false)); }
	assert(DLSSNRFilter::liveInstances == 0);
	resources.failTextureAt = -1;
	for (int mode : {1, 2, 3, 4}) {
		option.parameters["antiFlicker"] = static_cast<float>(mode);
		assert(DLSSNRAntiFlickerMode(get) == mode);
		for (int count : {1, 3}) {
			option.parameters["multiPass"] = static_cast<float>(count);
			DLSSNRMultiPass chain;
			assert(chain.Initialize(resources, core, &input, &output, option, false));
			assert(chain.GetFrameGuidanceRequirements().HasMotion() == (mode >= 2));
			assert(chain.GetParameterApplyMode("antiFlicker") == EffectParameterApplyMode::RestartRequired);
			assert(chain.GetParameterRestartReason("antiFlicker") == EffectParameterRestartReason::FrameGuidance);
			assert((DLSSNRTemporal::lastBase == &input) == (count == 1));
			assert(chain.Draw({&input, &output, 7}));
			assert(output.value == (count == 1 ? 1 : 113));
			const int resets = DLSSNRTemporal::resets;
			std::vector<std::string> edits{"intensity"};
			assert(chain.ApplyLiveParameters(option, edits));
			assert(DLSSNRTemporal::resets == resets + 1);
		}
	}
	std::cout << "DLSSNR Multi Pass: defaults, isolation, visibility, serial order, revisions, rollback, resize and failures passed.\n";
}
