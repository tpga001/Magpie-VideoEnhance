#include "NgxRuntimeGuard.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <string>

using Magpie::NgxRuntimeGuard;

// Model the observed SDK failure: SEH exits while its critical section is held.
static CRITICAL_SECTION sdkLock;
static int sdkCalls = 0;
static int ShutdownWithFault() {
    EnterCriticalSection(&sdkLock);
    ++sdkCalls;
    RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    LeaveCriticalSection(&sdkLock);
    return 1;
}

int main(int argc, char** argv) {
    DWORD seh = 99;
    // A normal SDK failure is retryable; healthy calls preserve their results.
    assert(NgxRuntimeGuard::Invoke([] { return -2; }, -1, &seh) == -2);
    assert(seh == 0 && !NgxRuntimeGuard::IsFaulted());
    assert(NgxRuntimeGuard::Invoke([] { return 7; }, -1, &seh) == 7);

    if (argc == 2 && std::string(argv[1]) == "shutdown-failure") {
        NgxRuntimeGuard::MarkShutdownFailed();
        assert(NgxRuntimeGuard::FaultCode() == ERROR_INVALID_STATE);
    } else {
        InitializeCriticalSection(&sdkLock);
        assert(NgxRuntimeGuard::Invoke(ShutdownWithFault, -1, &seh) == -1);
        assert(seh == EXCEPTION_ACCESS_VIOLATION);
        assert(NgxRuntimeGuard::FaultAddress() != 0);
        assert(NgxRuntimeGuard::FaultThread() == GetCurrentThreadId());
        assert(sdkLock.RecursionCount == 1);
        assert(reinterpret_cast<uintptr_t>(sdkLock.OwningThread) == GetCurrentThreadId());
    }

    const DWORD originalFault = NgxRuntimeGuard::FaultCode();
    std::vector<std::thread> newSessions;
    for (int i = 0; i != 16; ++i) newSessions.emplace_back([] {
        DWORD blockedSeh = 99;
        const int result = NgxRuntimeGuard::Invoke([] {
            // Executing this in a new backend would deadlock on the retained lock.
            EnterCriticalSection(&sdkLock);
            ++sdkCalls;
            LeaveCriticalSection(&sdkLock);
            return 1;
        }, -1, &blockedSeh);
        assert(result == -1 && blockedSeh == 0);
    });
    for (auto& session : newSessions) session.join();
    assert(NgxRuntimeGuard::FaultCode() == originalFault);
    NgxRuntimeGuard::MarkShutdownFailed();
    assert(NgxRuntimeGuard::FaultCode() == originalFault);
    if (argc == 1) {
        assert(sdkCalls == 1);
        // Only the test owns this fake SDK lock. Production never forces unlocks.
        LeaveCriticalSection(&sdkLock);
        DeleteCriticalSection(&sdkLock);
    } else assert(sdkCalls == 0);
    std::cout << "PASS: fault retained across 16 new backend sessions; no SDK re-entry\n";
}
