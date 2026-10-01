#pragma once
#include <windows.h>
#include <cstdint>
#include <cwctype>
#include <limits>
#include <string>
#include <string_view>

namespace Magpie::ScalingModeNames {

inline std::wstring_view Trim(std::wstring_view name) noexcept {
	while (!name.empty() && std::iswspace(name.front())) name.remove_prefix(1);
	while (!name.empty() && std::iswspace(name.back())) name.remove_suffix(1);
	return name;
}

inline bool Equal(std::wstring_view left, std::wstring_view right) noexcept {
	left = Trim(left);
	right = Trim(right);
	if (left.empty() || right.empty()) return left.empty() && right.empty();
	return CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
		right.data(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

template <typename Modes>
bool Contains(const Modes& modes, std::wstring_view name,
	size_t except = (std::numeric_limits<size_t>::max)()) noexcept {
	for (size_t i = 0; i < modes.size(); ++i) {
		if (i != except && Equal(modes[i].name, name)) return true;
	}
	return false;
}

template <typename Modes>
bool HasDuplicates(const Modes& modes) noexcept {
	for (size_t i = 0; i < modes.size(); ++i) {
		for (size_t j = 0; j < i; ++j) {
			if (Equal(modes[i].name, modes[j].name)) return true;
		}
	}
	return false;
}

template <typename Exists>
std::wstring Unique(std::wstring_view name, Exists&& exists) {
	const std::wstring base(Trim(name));
	std::wstring candidate = base;
	for (uint32_t suffix = 2; exists(candidate); ++suffix) {
		candidate = base + L" (" + std::to_wstring(suffix) + L")";
	}
	return candidate;
}

} // namespace Magpie::ScalingModeNames
