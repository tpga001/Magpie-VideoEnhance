#pragma once
#include "MotionVectorRequest.h"
#include "ScalingOptions.h"
#include "DlssOpticalFlowParameters.h"
#include <cmath>

namespace Magpie {

inline MotionVectorRequest ParseOpticalFlowRequest(const EffectOption& effect,
	OpticalFlowMethod defaultMethod = OpticalFlowMethod::None) noexcept {
	auto choice = [&](const char* name, int minimum, int maximum, int fallback) {
		const auto it = effect.parameters.find(name);
		if (it == effect.parameters.end() || !std::isfinite(it->second) ||
			it->second < float(minimum) || it->second > float(maximum) ||
			std::round(it->second) != it->second) return fallback;
		return static_cast<int>(it->second);
	};
	const int method = choice("opticalFlowMethod", 0, 2, static_cast<int>(defaultMethod));
	if (method == 1) return MotionVectorRequest::Amd(static_cast<AmdOpticalFlowMode>(
		choice("amdOpticalFlowMode", 0, 1, 1)));
	if (method == 2) return MotionVectorRequest::Nvidia(static_cast<NvidiaOpticalFlowQuality>(
		choice("nvidiaOpticalFlowQuality", 1, NVIDIA_OPTICAL_FLOW_MAX_QUALITY, 2)));
	return {};
}

inline MotionVectorRequest ParseDlssOpticalFlowRequest(const EffectOption& effect) noexcept {
	const auto choices = ReadDlssOpticalFlowChoices([&](std::string_view name) -> std::optional<float> {
		const auto it = effect.parameters.find(std::string(name));
		return it == effect.parameters.end() ? std::nullopt : std::optional<float>(it->second);
	});
	if (choices.method == 1) return MotionVectorRequest::Amd(
		static_cast<AmdOpticalFlowMode>(choices.amdQuality));
	if (choices.method == 2) return MotionVectorRequest::Nvidia(
		static_cast<NvidiaOpticalFlowQuality>(choices.nvidiaQuality));
	return {};
}

}
