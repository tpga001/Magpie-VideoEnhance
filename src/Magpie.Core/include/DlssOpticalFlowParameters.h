#pragma once
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace Magpie {

struct DlssOpticalFlowChoices {
	int method = 0;
	int amdQuality = 1;
	int nvidiaQuality = 2;
};

// New provider choices take precedence over legacy NVIDIA-only settings.
// Validate before converting floats, including values imported from configs.
template<class GetValue>
DlssOpticalFlowChoices ReadDlssOpticalFlowChoices(GetValue&& getValue) noexcept {
	auto choice = [&](std::string_view name, int minimum, int maximum, int fallback) {
		const auto value = getValue(name);
		return value && std::isfinite(*value) && *value >= float(minimum) &&
			*value <= float(maximum) && std::round(*value) == *value
			? static_cast<int>(*value) : fallback;
	};
	const auto legacyEnabled = getValue("useMotionVectors");
	const int legacyQuality = choice("motionVectorQuality", 0, 5,
		!getValue("motionVectorQuality") && legacyEnabled &&
		std::isfinite(*legacyEnabled) && *legacyEnabled >= 0.5f ? 2 : 0);
	const int method = getValue("opticalFlowMethod")
		? choice("opticalFlowMethod", 0, 2, 0) : (legacyQuality == 0 ? 0 : 2);
	return { method, choice("amdOpticalFlowMode", 0, 1, 1),
		choice("nvidiaOpticalFlowQuality", 1, 5,
			!getValue("nvidiaOpticalFlowQuality") && legacyQuality > 0 ? legacyQuality : 2) };
}

template<class Effect>
bool MigrateDlssOpticalFlowParameters(Effect& effect) {
	if (effect.name != L"DLSSNR\\DLSSNR_AI_Filter" &&
		effect.name != L"DLSSFG\\DLSS_FrameGeneration") return false;
	const auto choices = ReadDlssOpticalFlowChoices([&](std::string_view name) -> std::optional<float> {
		const auto it = effect.parameters.find(std::wstring(name.begin(), name.end()));
		return it == effect.parameters.end() ? std::nullopt : std::optional<float>(it->second);
	});
	bool changed = false;
	auto store = [&](const wchar_t* name, int value) {
		auto [it, inserted] = effect.parameters.try_emplace(name, float(value));
		if (inserted || it->second != float(value)) {
			it->second = float(value);
			changed = true;
		}
	};
	store(L"opticalFlowMethod", choices.method);
	store(L"amdOpticalFlowMode", choices.amdQuality);
	store(L"nvidiaOpticalFlowQuality", choices.nvidiaQuality);
	changed |= effect.parameters.erase(L"motionVectorQuality") != 0;
	changed |= effect.parameters.erase(L"useMotionVectors") != 0;
	return changed;
}

inline bool IsOpticalFlowParameter(std::string_view name) noexcept {
	return name == "opticalFlowMethod" || name == "amdOpticalFlowMode" ||
		name == "nvidiaOpticalFlowQuality";
}

}
