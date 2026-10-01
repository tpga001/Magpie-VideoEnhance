#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>

namespace Magpie {
inline uint32_t XeSSFGSleepIntervalUs(double baseFPS, uint32_t multiplier,
    bool frameGenerationEnabled) noexcept {
    if (!std::isfinite(baseFPS) || baseFPS <= 0) return 0;
    const double outputFPS = baseFPS * (frameGenerationEnabled ? multiplier : 1u);
    if (!std::isfinite(outputFPS) || outputFPS <= 0) return 0;
    return static_cast<uint32_t>(std::max(1.0, std::round(1'000'000.0 / outputFPS)));
}

struct XeSSFGSourceSample {
    // sequence identifies a capture session, not an individual frame. It stays
    // constant until capture is interrupted; frameId advances within a session.
    uint64_t frameId = 0, sequence = 0, generation = 0;
    int64_t timestamp100ns = 0;
};
class XeSSFGTiming {
public:
    struct Estimate { double sourceMs = 0, submitMs = 0, fedMs = 0; bool reset = false, captured = false; };
    Estimate Submit(XeSSFGSourceSample current, double nowMs, double previousExtraWaitMs) noexcept {
        Estimate result;
        result.submitMs = _lastSubmit > 0 ? nowMs - _lastSubmit : 0;
        result.reset = !_previous.frameId || current.generation != _previous.generation ||
            current.sequence != _previous.sequence || current.frameId <= _previous.frameId ||
            result.submitMs >= 500 || result.submitMs < 0 ||
            (current.timestamp100ns > 0 && _previous.timestamp100ns > 0 &&
                current.timestamp100ns <= _previous.timestamp100ns);
        if (result.reset) _count = _position = 0;
        if (!result.reset && current.timestamp100ns > 0 && _previous.timestamp100ns > 0) {
            result.sourceMs = static_cast<double>(current.timestamp100ns - _previous.timestamp100ns) / 10000.0;
            // Reject known skipped submissions. Consecutive accepted frame IDs
            // do NOT prove consecutive produced frames: WGC can drain its pool
            // and keep only the newest. This measures accepted capture cadence,
            // not a backpressure-free game-frame render time.
            result.captured = current.frameId == _previous.frameId + 1 &&
                result.sourceMs >= 0.125 && result.sourceMs < 500;
        }
        // Only our measured extra waits inside the previous synchronous Present
        // belong wholly to this submit-to-submit interval. Never subtract the
        // whole Present or any time from the independent capture timestamp.
        const double sample = result.captured ? result.sourceMs :
            result.submitMs - std::clamp(previousExtraWaitMs, 0.0, std::max(0.0, result.submitMs));
        if (!result.reset && std::isfinite(sample) && sample >= 0.125 && sample < 500) {
            if (_count && _captured != result.captured) _count = _position = 0;
            _captured = result.captured;
            _samples[_position] = sample;
            _position = (_position + 1) % _samples.size();
            _count = std::min(_count + 1, _samples.size());
        }
        auto sorted = _samples;
        std::sort(sorted.begin(), sorted.begin() + _count);
        // During bootstrap use the shortest of the first two observations, so
        // one slow startup interval does not dominate an even-sized median.
        // From three observations onward use the rolling median. No fixed FPS
        // cap: sustained low frame rates and later rate changes remain valid.
        result.fedMs = _count ? sorted[_count < 3 ? 0 : _count / 2] : 0;
        _previous = current;
        _lastSubmit = nowMs;
        return result;
    }
    void Reset() noexcept { *this = {}; }
private:
    XeSSFGSourceSample _previous{};
    double _lastSubmit = 0;
    std::array<double, 9> _samples{};
    size_t _position = 0, _count = 0;
    bool _captured = false;
};
}
