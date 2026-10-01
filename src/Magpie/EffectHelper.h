#pragma once
#include "EffectPickerModel.h"

namespace Magpie {

struct EffectHelper {
	static std::wstring_view GetDisplayName(std::wstring_view fullName) noexcept {
		const int rtxFamily = RTXVideoFamily(fullName);
		if (rtxFamily >= 0) return rtxFamily == 0 ? L"RTXVideo_Denoise" : L"RTXVideo_VSR";
		if (fullName == L"XeSSFG\\XeSS_FrameGeneration") return L"XeSS_FrameGeneration";
		size_t delimPos = fullName.find_last_of(L'\\');
		return delimPos != std::wstring::npos ? fullName.substr(delimPos + 1) : fullName;
	}
};

}
