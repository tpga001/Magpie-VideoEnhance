#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>

namespace Magpie::XeSSFGCompatibility {
struct Patch {
    uint32_t rva = 0;
    uint32_t size = 0;
    std::array<uint8_t, 16> original{};
    std::array<uint8_t, 16> replacement{};
    uint32_t protection = 0;
};
enum class PatchResult { Success, Rejected, RolledBack, Poisoned };

// Backend owns address/protection checks. Roll back the failed write as well:
// a write can change memory before failing to restore protection or flush code.
template<class Backend>
PatchResult InstallPatches(std::span<Patch> patches, Backend& backend) noexcept {
    for (auto& patch : patches) if (!backend.Preflight(patch)) return PatchResult::Rejected;
    for (size_t i = 0; i < patches.size(); ++i) {
        if (backend.Write(patches[i], false)) continue;
        bool restored = true;
        for (size_t j = i + 1; j > 0; --j) restored &= backend.Write(patches[j - 1], true);
        return restored ? PatchResult::RolledBack : PatchResult::Poisoned;
    }
    return PatchResult::Success;
}
template<class Backend>
bool RestorePatches(std::span<Patch> patches, Backend& backend) noexcept {
    bool restored = true;
    for (size_t j = patches.size(); j > 0; --j) restored &= backend.Write(patches[j - 1], true);
    return restored;
}
}
