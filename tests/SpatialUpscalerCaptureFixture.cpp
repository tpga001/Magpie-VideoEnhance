// Animated source used by isolated spatial and native-SR capture smoke tests.
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../src/Shared/CommonSharedConstants.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static LRESULT CALLBACK SourceProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_TIMER) {
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT area{};
        GetClientRect(window, &area);
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, area.right, area.bottom);
        HGDIOBJ old = SelectObject(buffer, bitmap);
        FillRect(buffer, &area, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        const LONG x = static_cast<LONG>((GetTickCount64() / 4) % 800);
        RECT movingBlock{x, 100, x + 100, 300};
        FillRect(buffer, &movingBlock, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SetBkMode(buffer, TRANSPARENT);
        SetTextColor(buffer, RGB(64, 192, 255));
        const wchar_t* label = L"Magpie spatial upscaler smoke test";
        TextOutW(buffer, 48, 380, label, static_cast<int>(wcslen(label)));
        BitBlt(dc, 0, 0, area.right, area.bottom, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        EndPaint(window, &paint);
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}

static void Pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    Sleep(1);
}

int main(int argc, char** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wchar_t secondsText[32]{};
    GetEnvironmentVariableW(L"MAGPIE_CAPTURE_SECONDS", secondsText, 32);
    const int seconds = secondsText[0] ? std::clamp(_wtoi(secondsText), 1, 900) : 6;
    std::vector<int> modes{1, 2};
    if (argc > 1) {
        modes.clear();
        for (int i = 1; i < argc; ++i) {
            const int mode = std::stoi(argv[i]);
            assert(mode >= 1 && mode <= 3);
            modes.push_back(mode);
        }
    }

    for (int mode : modes) {
        const std::wstring className = L"MagpieSpatialCapture" + std::to_wstring(mode);
        WNDCLASSW wc{};
        wc.lpfnWndProc = SourceProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = className.c_str();
        const ATOM registered = RegisterClassW(&wc);
        assert(registered || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
        HWND window = CreateWindowExW(
            0, className.c_str(), L"Magpie spatial upscaler smoke test (automatic)",
            WS_OVERLAPPEDWINDOW, 700, 300, 960, 580,
            nullptr, nullptr, wc.hInstance, nullptr);
        assert(window);
        ShowWindow(window, SW_SHOWNORMAL);
        SetForegroundWindow(window);
        SetTimer(window, 1, 16, nullptr);

        const ULONGLONG startupDeadline = GetTickCount64() + 20000;
        HWND scaling = nullptr;
        while (GetTickCount64() < startupDeadline) {
            Pump();
            scaling = FindWindowW(CommonSharedConstants::SCALING_WINDOW_CLASS_NAME, nullptr);
            if (scaling && IsWindowVisible(scaling)) break;
        }
        if (!scaling || !IsWindowVisible(scaling)) {
            std::cerr << "Automatic scaling did not start: mode=" << mode << std::endl;
            DestroyWindow(window);
            return 1;
        }
        std::cout << "capturing mode=" << mode << std::endl;
        const ULONGLONG end = GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000;
        while (GetTickCount64() < end) Pump();

        DestroyWindow(window);
        const ULONGLONG stopDeadline = GetTickCount64() + 10000;
        while (FindWindowW(CommonSharedConstants::SCALING_WINDOW_CLASS_NAME, nullptr) &&
               GetTickCount64() < stopDeadline) {
            Pump();
        }
        if (FindWindowW(CommonSharedConstants::SCALING_WINDOW_CLASS_NAME, nullptr)) {
            std::cerr << "Scaling did not stop" << std::endl;
            return 1;
        }
    }
    std::cout << "Spatial upscaler capture fixture completed." << std::endl;
}
