#pragma once

#include <atomic>
#include <cstdint>

namespace Magpie {

struct DlssnrGpuTimingSnapshot {
	double latestMs = 0.0;
	double emaMs = 0.0;
	uint64_t sampleCount = 0;
};

// Renderer-thread telemetry shared with asynchronous Frame Guidance scheduling.
// It intentionally contains no GPU objects and never blocks either producer.
class FrameGuidancePerformance {
public:
	static void ResetDlssnrGpuTiming() noexcept {
		_latestDlssnrGpuMs.store(0.0, std::memory_order_relaxed);
		_emaDlssnrGpuMs.store(0.0, std::memory_order_relaxed);
		_dlssnrGpuSampleCount.store(0, std::memory_order_release);
	}

	static void PublishDlssnrGpuTiming(double milliseconds) noexcept {
		if (!(milliseconds > 0.0) || milliseconds > 1000.0) return;
		const uint64_t count = _dlssnrGpuSampleCount.load(std::memory_order_relaxed);
		const double previous = _emaDlssnrGpuMs.load(std::memory_order_relaxed);
		constexpr double ALPHA = 0.1;
		const double ema = count ? previous + (milliseconds - previous) * ALPHA :
			milliseconds;
		_latestDlssnrGpuMs.store(milliseconds, std::memory_order_relaxed);
		_emaDlssnrGpuMs.store(ema, std::memory_order_relaxed);
		_dlssnrGpuSampleCount.store(count + 1, std::memory_order_release);
	}

	static DlssnrGpuTimingSnapshot GetDlssnrGpuTiming() noexcept {
		DlssnrGpuTimingSnapshot result;
		result.sampleCount = _dlssnrGpuSampleCount.load(std::memory_order_acquire);
		result.latestMs = _latestDlssnrGpuMs.load(std::memory_order_relaxed);
		result.emaMs = _emaDlssnrGpuMs.load(std::memory_order_relaxed);
		return result;
	}

private:
	inline static std::atomic<double> _latestDlssnrGpuMs = 0.0;
	inline static std::atomic<double> _emaDlssnrGpuMs = 0.0;
	inline static std::atomic<uint64_t> _dlssnrGpuSampleCount = 0;
};

}
