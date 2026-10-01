#pragma once
#include <chrono>

namespace Magpie::NativeBackendTiming {

// Detailed sampling is a validation build option. Disabled builds submit no
// timing queries and do not read the CPU clock for these measurements.
#ifdef MP_ENABLE_NATIVE_BACKEND_TIMING
inline constexpr bool Enabled = true;
#else
inline constexpr bool Enabled = false;
#endif

using Clock = std::chrono::steady_clock;

inline Clock::time_point Now() noexcept {
	if constexpr (Enabled) return Clock::now();
	return {};
}

inline double ElapsedMilliseconds(Clock::time_point start) noexcept {
	if constexpr (Enabled) {
		return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
	}
	return 0.0;
}

}
