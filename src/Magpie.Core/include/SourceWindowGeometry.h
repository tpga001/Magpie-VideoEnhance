#pragma once
#include <windows.h>
#include <cmath>

namespace Magpie {

inline bool IsValidSourceCropping(double left, double top, double right, double bottom,
	double width, double height, double minSize) noexcept {
	return std::isfinite(left) && std::isfinite(top) && std::isfinite(right) && std::isfinite(bottom) &&
		left >= 0 && top >= 0 && right >= 0 && bottom >= 0 &&
		width - left - right >= minSize && height - top - bottom >= minSize;
}

// These are uncropped, physical screen coordinates. Use the monitor bounds,
// not its work area or the future scaling window's position and size.
inline bool SourceWindowCoversMonitor(
	const RECT& frameRect, const RECT& clientRect, const RECT& monitorRect
) noexcept {
	if (monitorRect.left >= monitorRect.right || monitorRect.top >= monitorRect.bottom) {
		return false;
	}
	const auto covers = [&monitorRect](const RECT& rect) noexcept {
		return rect.left <= monitorRect.left && rect.top <= monitorRect.top &&
			rect.right >= monitorRect.right && rect.bottom >= monitorRect.bottom;
	};
	return covers(frameRect) || covers(clientRect);
}

}
