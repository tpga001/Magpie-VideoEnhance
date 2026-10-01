#pragma once
#include <cmath>
#include <string>
#include <string_view>

namespace Magpie {
inline constexpr std::wstring_view XESS_FG_EFFECT = L"XeSSFG\\XeSS_FrameGeneration";

// Shared by startup and import. Run before historical optical-flow migrations.
template<class ModeRange>
bool MigrateXeSSFGEffects(ModeRange& modes) {
    bool changed = false;
    for (auto& mode : modes) {
        bool found = false;
        for (auto it = mode.effects.begin(); it != mode.effects.end();) {
            auto& effect = *it;
            const bool legacy = effect.name == L"XeSSFG\\XeSS_FrameGeneration_x2_ZeroMV" ||
                effect.name == L"XeSSFG\\XeSS_MultiFrameGeneration_ZeroMV";
            if (!legacy && effect.name != XESS_FG_EFFECT) { ++it; continue; }
            if (found) { it = mode.effects.erase(it); changed = true; continue; }
            found = true;
            if (legacy) {
                effect.name = XESS_FG_EFFECT;
                effect.parameters[L"multiplier"] = 2.0f;
                changed = true;
            }
            const auto normalize = [&](const wchar_t* key, float lo, float hi, float fallback) {
                auto [param, inserted] = effect.parameters.try_emplace(key, fallback);
                changed |= inserted;
                const float v = param->second;
                if (!std::isfinite(v) || v < lo || v > hi || std::floor(v) != v) {
                    param->second = fallback;
                    changed = true;
                }
            };
            // Missing legacy flow means None; missing unified flow means AMD.
            normalize(L"opticalFlowMethod", 0, 2, legacy ? 0.0f : 1.0f);
            normalize(L"amdOpticalFlowMode", 0, 1, 1);
            normalize(L"nvidiaOpticalFlowQuality", 1, 5, 2);
            normalize(L"multiplier", 2, 4, 2);
            ++it;
        }
    }
    return changed;
}
}
