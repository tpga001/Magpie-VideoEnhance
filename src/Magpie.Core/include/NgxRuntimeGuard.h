#pragma once
#include <windows.h>
#include <atomic>
#include <mutex>

namespace Magpie {

class NgxRuntimeGuard {
public:
	static bool IsFaulted() noexcept { return _faultCode.load(std::memory_order_acquire) != 0; }
	static DWORD FaultCode() noexcept { return _faultCode.load(std::memory_order_acquire); }
	static uintptr_t FaultAddress() noexcept { return _faultAddress.load(std::memory_order_relaxed); }
	static DWORD FaultThread() noexcept { return _faultThread.load(std::memory_order_relaxed); }

	template<typename Function, typename Result>
	static Result Invoke(Function&& function, Result failure, DWORD* sehCode) noexcept {
		std::lock_guard lock(_callMutex);
		*sehCode = 0;
		if (IsFaulted()) return failure;
		return _InvokeSafely(function, failure, sehCode);
	}

	static void MarkShutdownFailed() noexcept {
		std::lock_guard lock(_callMutex);
		_RecordFault(ERROR_INVALID_STATE, 0);
	}

private:
	template<typename Function, typename Result>
	static Result _InvokeSafely(Function& function, Result failure, DWORD* sehCode) noexcept {
		__try {
			return function();
		} __except (_CaptureException(GetExceptionInformation(), sehCode)) {
			return failure;
		}
	}

	static LONG _CaptureException(EXCEPTION_POINTERS* exception, DWORD* sehCode) noexcept {
		*sehCode = exception->ExceptionRecord->ExceptionCode;
		_RecordFault(*sehCode, reinterpret_cast<uintptr_t>(exception->ExceptionRecord->ExceptionAddress));
		return EXCEPTION_EXECUTE_HANDLER;
	}

	static void _RecordFault(DWORD code, uintptr_t address) noexcept {
		if (IsFaulted()) return;
		_faultAddress.store(address, std::memory_order_relaxed);
		_faultThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
		_faultCode.store(code, std::memory_order_release);
	}

	static inline std::mutex _callMutex;
	static inline std::atomic<DWORD> _faultCode = 0;
	static inline std::atomic<uintptr_t> _faultAddress = 0;
	static inline std::atomic<DWORD> _faultThread = 0;
};

}
