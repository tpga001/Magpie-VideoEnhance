// Animated source for the real Magpie WGC + AMD optical-flow smoke test.
#include <windows.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include "../src/Shared/CommonSharedConstants.h"
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static LRESULT CALLBACK SourceProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_TIMER) { InvalidateRect(window, nullptr, FALSE); return 0; }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT area{}; GetClientRect(window, &area);
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, area.right, area.bottom);
        HGDIOBJ old = SelectObject(buffer, bitmap);
        FillRect(buffer, &area, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        const LONG x = static_cast<LONG>((GetTickCount64() / 4) % 800);
        RECT block{x, 100, x + 100, 300};
        FillRect(buffer, &block, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        BitBlt(dc, 0, 0, area.right, area.bottom, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old); DeleteObject(bitmap); DeleteDC(buffer);
        EndPaint(window, &paint); return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}

static void Pump() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    Sleep(1);
}

int main(int argc, char** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wchar_t secondsText[32]{};
    GetEnvironmentVariableW(L"MAGPIE_CAPTURE_SECONDS", secondsText, 32);
    const auto seconds = secondsText[0] ? std::clamp(_wtoi(secondsText), 1, 900) : 12;
    std::vector<int> modes{2, 3, 4, 2, 5};
    if (argc > 1) {
        modes.clear();
        for (int i = 1; i < argc; ++i) {
            const int mode = std::stoi(argv[i]);
            assert(mode >= 2 && mode <= 5);
            modes.push_back(mode);
        }
    }
    for (int mode : modes) {
        const auto className = L"MagpieXeSSCapture" + std::to_wstring(mode);
        WNDCLASSW wc{}; wc.lpfnWndProc = SourceProc; wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = className.c_str();
        const auto registered = RegisterClassW(&wc);
        assert(registered || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
        HWND window = CreateWindowExW(0, className.c_str(), L"Magpie XeSS capture regression (automatic)",
            WS_OVERLAPPEDWINDOW, 100, 100, 960, 580, nullptr, nullptr, wc.hInstance, nullptr);
        assert(window);
        ShowWindow(window, SW_SHOWNORMAL);
        const auto foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        const auto sourceThread = GetCurrentThreadId();
        const bool attached = foregroundThread != sourceThread &&
            AttachThreadInput(sourceThread, foregroundThread, TRUE) != FALSE;
        const bool focused = SetForegroundWindow(window) != FALSE;
        if (attached) AttachThreadInput(sourceThread, foregroundThread, FALSE);
        std::cout << "source mode=" << mode << " foreground=" << focused << std::endl;
        SetTimer(window, 1, 16, nullptr);
        const ULONGLONG deadline = GetTickCount64() + 20000;
        HWND scaling = nullptr;
        while (GetTickCount64() < deadline) {
            Pump();
            scaling = FindWindowW(CommonSharedConstants::SCALING_WINDOW_CLASS_NAME, nullptr);
            if (scaling && IsWindowVisible(scaling)) break;
        }
        if (!scaling || !IsWindowVisible(scaling)) {
            wchar_t foregroundClass[256]{};
            GetClassNameW(GetForegroundWindow(), foregroundClass, 256);
            std::wcerr << L"Foreground class: " << foregroundClass << std::endl;
            std::cerr << "Automatic scaling did not start: mode=" << mode << std::endl;
            DestroyWindow(window); return 1;
        }
        std::cout << "capturing mode=" << mode << std::endl;
        const auto end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
        while (GetTickCount64() < end) Pump();
        DestroyWindow(window);
        const auto stopDeadline = GetTickCount64() + 10000;
        while (FindWindowW(CommonSharedConstants::SCALING_WINDOW_CLASS_NAME, nullptr) &&
            GetTickCount64() < stopDeadline) Pump();
        if (FindWindowW(CommonSharedConstants::SCALING_WINDOW_CLASS_NAME, nullptr)) {
            std::cerr << "Scaling did not stop" << std::endl; return 1;
        }
    }
    std::cout << "Capture fixture completed; inspect SDK counts and pacing in Magpie logs." << std::endl;
}
