#pragma once
#include <windows.h>

namespace Magpie {

inline bool SetScalingWindowOwner(HWND window, HWND owner) noexcept {
	SetLastError(ERROR_SUCCESS);
	const LONG_PTR previous = SetWindowLongPtrW(window, GWLP_HWNDPARENT,
		reinterpret_cast<LONG_PTR>(owner));
	return previous != 0 || GetLastError() == ERROR_SUCCESS;
}

}
