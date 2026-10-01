#pragma once

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace Magpie {

inline constexpr LONG BROWSER_VIDEO_GEOMETRY_JITTER_PX = 2;

struct BrowserVideoGeometrySample {
	SIZE overlaySize{};
	SIZE frameSize{};
	SIZE processingSize{};
	std::array<float, 4> cropping{};
};

inline bool IsBrowserVideoSizeWithinJitter(SIZE current, SIZE candidate) noexcept {
	return std::abs(current.cx - candidate.cx) <= BROWSER_VIDEO_GEOMETRY_JITTER_PX &&
		std::abs(current.cy - candidate.cy) <= BROWSER_VIDEO_GEOMETRY_JITTER_PX;
}

inline bool IsBrowserVideoProcessingSizeInSameTier(
	SIZE current,
	SIZE candidate,
	SIZE currentFrame,
	SIZE candidateFrame) noexcept {
	if (current.cx <= 0 || current.cy <= 0 || candidate.cx <= 0 || candidate.cy <= 0 ||
		currentFrame.cx <= 0 || currentFrame.cy <= 0 || candidateFrame.cx <= 0 || candidateFrame.cy <= 0) {
		return false;
	}

	const auto projectedTolerance = [](LONG currentOutput, LONG candidateOutput,
		LONG currentInput, LONG candidateInput) noexcept {
		const double scale = (std::max)(
			static_cast<double>(currentOutput) / currentInput,
			static_cast<double>(candidateOutput) / candidateInput);
		return (std::max)(BROWSER_VIDEO_GEOMETRY_JITTER_PX,
			static_cast<LONG>(std::ceil(scale * BROWSER_VIDEO_GEOMETRY_JITTER_PX)));
	};

	return std::abs(current.cx - candidate.cx) <= projectedTolerance(
		current.cx, candidate.cx, currentFrame.cx, candidateFrame.cx) &&
		std::abs(current.cy - candidate.cy) <= projectedTolerance(
		current.cy, candidate.cy, currentFrame.cy, candidateFrame.cy);
}

inline bool CanReuseBrowserVideoGeometry(
	const BrowserVideoGeometrySample& current,
	const BrowserVideoGeometrySample& candidate) noexcept {
	if (!IsBrowserVideoSizeWithinJitter(current.overlaySize, candidate.overlaySize) ||
		!IsBrowserVideoSizeWithinJitter(current.frameSize, candidate.frameSize) ||
		!IsBrowserVideoProcessingSizeInSameTier(
			current.processingSize, candidate.processingSize, current.frameSize, candidate.frameSize)) {
		return false;
	}

	for (size_t i = 0; i < current.cropping.size(); ++i) {
		if (std::abs(current.cropping[i] - candidate.cropping[i]) > BROWSER_VIDEO_GEOMETRY_JITTER_PX) {
			return false;
		}
	}
	return true;
}

}
