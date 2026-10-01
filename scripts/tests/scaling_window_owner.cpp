#include <windows.h>
#include "ScalingWindowOwner.h"
#include <cassert>
#include <wct.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

// Only windows created by this test are accessed. All remain hidden.
static DWORD ownerThread;
static bool detachInput;
static bool blockInDestroy;
static HANDLE destroyReady;
static HANDLE destroyRelease;
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_DESTROY && blockInDestroy) {
        assert(Magpie::SetScalingWindowOwner(hwnd, nullptr));
        assert(GetWindow(hwnd, GW_OWNER) == nullptr);
        SetEvent(destroyReady);
        WaitForSingleObject(destroyRelease, 8000);
    }
    if (message == WM_CREATE && detachInput) {
        SetLastError(0);
        const BOOL ok = AttachThreadInput(GetCurrentThreadId(), ownerThread, FALSE);
        const DWORD error = GetLastError();
        std::cout << "detach_result=" << ok << " error=" << error << std::endl;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

static void Pump() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

int wmain(int argc, wchar_t** argv) {
    WNDCLASSW windowClass{};
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"MagpieOwnedWindowDiagnostic";
    windowClass.lpfnWndProc = WindowProc;
    RegisterClassW(&windowClass);
    if (argc == 6) {
        const HWND owner = reinterpret_cast<HWND>(_wcstoui64(argv[1], nullptr, 16));
        ownerThread = GetWindowThreadProcessId(owner, nullptr);
        detachInput = wcstoul(argv[4], nullptr, 10) != 0;
        const unsigned mode = wcstoul(argv[5], nullptr, 10);
        const bool linked = mode == 1;
        HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, argv[2]);
        HANDLE release = OpenEventW(SYNCHRONIZE, FALSE, argv[3]);
        HWND popup = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
            windowClass.lpszClassName, L"Hidden diagnostic popup", WS_POPUP | WS_MAXIMIZE,
            0, 0, 32, 32, linked ? owner : nullptr, nullptr, windowClass.hInstance, nullptr);
        std::cout << "child_pid=" << GetCurrentProcessId() << " tid=" << GetCurrentThreadId()
            << " create=" << (popup != nullptr) << " linked=" << linked << std::endl;
        assert(popup);
        if (mode == 2) {
            // The production helper establishes ownership after initialization,
            // then removes it before a simulated blocking SDK teardown.
            assert(Magpie::SetScalingWindowOwner(popup, owner));
            assert(GetWindow(popup, GW_OWNER) == owner);
            blockInDestroy = true;
            destroyReady = ready;
            destroyRelease = release;
            assert(DestroyWindow(popup)); // Pause inside the real WM_DESTROY phase.
            blockInDestroy = false;
        }
        SetEvent(ready);
        WaitForSingleObject(release, 8000);
        // Pump until parent cleanup is complete so its operation can finish.
        const ULONGLONG end = GetTickCount64() + 2000;
        while (GetTickCount64() < end) { Pump(); Sleep(1); }
        if (IsWindow(popup)) DestroyWindow(popup);
        CloseHandle(ready); CloseHandle(release);
        return 0;
    }
    wchar_t executable[32768];
    GetModuleFileNameW(nullptr, executable, 32768);
    unsigned testIndex = 0;
    for (const auto op : {L"zorder"}) {
        if (argc == 2 && std::wstring(op) != L"zorder") continue;
        for (unsigned mode = 0; mode != 3; ++mode) {
            if (argc == 2 && mode == 1) continue;
            HWND owner = CreateWindowExW(0, windowClass.lpszClassName, L"Hidden diagnostic owner",
                WS_OVERLAPPEDWINDOW, 0, 0, 32, 32, nullptr, nullptr, windowClass.hInstance, nullptr);
            const auto prefix = L"Local\\MagpieWindowProbe-" + std::to_wstring(GetCurrentProcessId())
                + L"-" + std::to_wstring(++testIndex);
            const auto readyName = prefix + L"-ready";
            const auto releaseName = prefix + L"-release";
            HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
            HANDLE release = CreateEventW(nullptr, TRUE, FALSE, releaseName.c_str());
            wchar_t ownerHex[32];
            swprintf_s(ownerHex, L"%llx", reinterpret_cast<unsigned long long>(owner));
            std::wstring command = L"\"" + std::wstring(executable) + L"\" " + ownerHex
                + L" " + readyName + L" " + releaseName + L" 1 " + std::to_wstring(mode);
            STARTUPINFOW startup{}; startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
            startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            PROCESS_INFORMATION child{};
            std::wcout << L"test=" << testIndex << L" op=" << op << L" mode=" << mode << std::endl;
            if (!CreateProcessW(executable, command.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child)) return 2;
            const ULONGLONG readyDeadline = GetTickCount64() + 5000;
            while (WaitForSingleObject(ready, 0) != WAIT_OBJECT_0 && GetTickCount64() < readyDeadline) {
                Pump(); Sleep(1);
            }
            if (WaitForSingleObject(ready, 0) != WAIT_OBJECT_0) { SetEvent(release); return 3; }
            std::atomic<bool> completed{false};
            const DWORD operationThread = GetCurrentThreadId();
            std::thread watchdog([&] {
                for (unsigned i = 0; i < 200 && !completed.load(); ++i) Sleep(10);
                if (!completed.load()) {
                    std::cout << "operation_stalled_for_2s=true" << std::endl;
                    HWCT session = OpenThreadWaitChainSession(0, nullptr);
                    WAITCHAIN_NODE_INFO nodes[WCT_MAX_NODE_COUNT]{};
                    DWORD count = WCT_MAX_NODE_COUNT; BOOL cycle = FALSE;
                    if (GetThreadWaitChain(session, 0, WCTP_GETINFO_ALL_FLAGS,
                        operationThread, &count, nodes, &cycle)) {
                        for (DWORD n = 0; n < count; ++n) {
                            std::cout << "wct_node=" << n << " type=" << nodes[n].ObjectType
                                << " status=" << nodes[n].ObjectStatus;
                            if (nodes[n].ObjectType == WctThreadType)
                                std::cout << " pid=" << nodes[n].ThreadObject.ProcessId
                                    << " tid=" << nodes[n].ThreadObject.ThreadId;
                            std::cout << std::endl;
                        }
                    } else std::cout << "wct_error=" << GetLastError() << std::endl;
                    CloseThreadWaitChainSession(session);
                }
                SetEvent(release);
            });
            const ULONGLONG start = GetTickCount64();
            BOOL result = FALSE;
            SetLastError(0);
            if (std::wstring(op) == L"zorder") result = SetWindowPos(owner, HWND_TOP, 0, 0, 0, 0,
                SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);
            else if (std::wstring(op) == L"move") result = SetWindowPos(owner, nullptr, 10, 10, 32, 32,
                SWP_NOACTIVATE | SWP_NOZORDER);
            else result = DestroyWindow(owner);
            const DWORD error = GetLastError();
            std::cout << "result=" << result << " error=" << error
                << " duration_ms=" << GetTickCount64() - start << std::endl;
            const auto duration = GetTickCount64() - start;
            completed.store(true); watchdog.join();
            assert(result);
            if (mode == 1) assert(duration >= 1500); // Positive control reproduces the defect.
            else assert(duration < 1000); // Init and teardown must not wait on the popup.

            if (IsWindow(owner)) DestroyWindow(owner);
            const ULONGLONG childDeadline = GetTickCount64() + 4000;
            while (WaitForSingleObject(child.hProcess, 0) != WAIT_OBJECT_0 && GetTickCount64() < childDeadline) {
                Pump(); Sleep(1);
            }
            CloseHandle(child.hThread); CloseHandle(child.hProcess);
            CloseHandle(ready); CloseHandle(release);
        }
    }
    std::cout << "PASS: unowned initialization and detached teardown do not block owner Z-order changes" << std::endl;
    return 0;
}
