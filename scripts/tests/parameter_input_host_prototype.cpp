#define UNICODE
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
#include <stdexcept>
#undef assert
#define assert(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")

static HWND game, host;
static int gameDown, gameUp, hostDown, hostUp;
static bool injectedLeftDown = false;
static bool previewMode = false, resumeGesture = false;
static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTCLIENT;
    if (msg == WM_MOUSEACTIVATE) return MA_ACTIVATE;
    if (msg == WM_LBUTTONDOWN) {
        if (hwnd == host) {
            if (previewMode) {
                previewMode=false; resumeGesture=true;
                SetWindowPos(host,HWND_TOPMOST,80,80,320,240,SWP_NOACTIVATE | SWP_SHOWWINDOW);
                SetForegroundWindow(host); SetFocus(host);
            }
            ++hostDown; SetCapture(host);
        }
        else ++gameDown;
        return 0;
    }
    if (msg == WM_LBUTTONUP) {
        if (hwnd == host) {
            if (hostDown == hostUp) return 0;
            ++hostUp;
            ReleaseCapture();
            if (resumeGesture) { resumeGesture=false; return 0; }
            SetForegroundWindow(game);
            ShowWindow(host, SW_HIDE);
        } else ++gameUp;
        return 0;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps);
        if (hwnd == game) {
            HBRUSH brush = CreateSolidBrush(RGB(48, 96, 144));
            FillRect(dc, &ps.rcPaint, brush); DeleteObject(brush);
        }
        EndPaint(hwnd, &ps); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
static void Pump() {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    do {
        MSG msg{};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while (std::chrono::steady_clock::now() < end);
    DwmFlush();
}
static INPUT MoveToTestPoint() {
    INPUT move{}; move.type=INPUT_MOUSE;
    move.mi.dwFlags=MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    move.mi.dx=MulDiv(200-GetSystemMetrics(SM_XVIRTUALSCREEN),65535,GetSystemMetrics(SM_CXVIRTUALSCREEN)-1);
    move.mi.dy=MulDiv(160-GetSystemMetrics(SM_YVIRTUALSCREEN),65535,GetSystemMetrics(SM_CYVIRTUALSCREEN)-1);
    return move;
}
static void Edge(DWORD flags) {
    // Move and edge are serialized in one SendInput batch. Never target another app.
    assert(WindowFromPoint({200,160}) == (hostUp ? game : host));
    const bool toHost=hostUp==0;
    const bool down=flags==MOUSEEVENTF_LEFTDOWN;
    const int expected=(toHost ? (down?hostDown:hostUp) : (down?gameDown:gameUp))+1;
    INPUT inputs[2]{MoveToTestPoint(),{}};
    inputs[1].type=INPUT_MOUSE; inputs[1].mi.dwFlags=flags;
    const UINT sent=SendInput(2,inputs,sizeof(INPUT));
    if (sent==2) injectedLeftDown = flags == MOUSEEVENTF_LEFTDOWN;
    assert(sent==2);
    for (int i=0;i<20 && (toHost?(down?hostDown:hostUp):(down?gameDown:gameUp))<expected;++i) Pump();
}
static int RunTest() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HWND prior = GetForegroundWindow(); POINT priorCursor{}; GetCursorPos(&priorCursor);
    WNDCLASS wc{}; wc.hInstance = GetModuleHandle(nullptr); wc.lpfnWndProc = Proc;
    wc.lpszClassName = L"MagpieParameterHostPrototype"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    assert(RegisterClass(&wc));
    game = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, wc.lpszClassName, L"Input host prototype",
        WS_POPUP, 80, 80, 320, 240, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(game, SW_SHOW); SetForegroundWindow(game); UpdateWindow(game); Pump();
    // A test launched from a background build process needs a real activation click.
    assert(WindowFromPoint({200, 160}) == game);
    SetCursorPos(200, 160);
    INPUT activate[3]{MoveToTestPoint(),{}, {}};
    activate[1].type = activate[2].type = INPUT_MOUSE;
    activate[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    activate[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    assert(SendInput(3, activate, sizeof(INPUT)) == 3); for (int i = 0; i < 20 && gameUp == 0; ++i) Pump(); assert(gameUp == 1);
    gameDown = gameUp = 0;
    assert(GetForegroundWindow() == game);
    HDC desktop = GetDC(nullptr); COLORREF before = GetPixel(desktop, 200, 160);
    host = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"Input host",
        WS_POPUP, 80, 80, 320, 240, game, nullptr, wc.hInstance, nullptr);
    assert(host);
    SetWindowPos(host, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(host); SetFocus(host); Pump();
    assert(GetForegroundWindow() == host && GetFocus() == host);
    assert(WindowFromPoint({200, 160}) == host);
    assert(GetPixel(desktop, 200, 160) == before); ReleaseDC(nullptr, desktop);
    RECT locked{200, 160, 201, 161}; assert(ClipCursor(&locked)); assert(ClipCursor(nullptr));
    RECT free{}; GetClipCursor(&free); assert(free.right - free.left > 1);
    SetCursorPos(200, 160); Pump();
    Edge(MOUSEEVENTF_LEFTDOWN);
    if (!(hostDown==1 && hostUp==0 && gameDown==0 && GetForegroundWindow()==host)) std::cerr << "Counts: " << hostDown << "," << hostUp << "," << gameDown << "," << gameUp << " foregroundHost=" << (GetForegroundWindow()==host) << "\n";
    assert(hostDown == 1 && hostUp == 0 && gameDown == 0 && GetForegroundWindow() == host);
    Edge(MOUSEEVENTF_LEFTUP);
    assert(hostUp == 1 && gameDown == 0 && gameUp == 0 && GetForegroundWindow() == game);
    assert(WindowFromPoint({200, 160}) == game && GetCapture() == nullptr);
    Edge(MOUSEEVENTF_LEFTDOWN); Edge(MOUSEEVENTF_LEFTUP);
    assert(gameDown == 1 && gameUp == 1);
    // Reuse the transparent host as a bounded, nonactivating preview target.
    // Outside it, real USER32 hit testing still reaches the game. An actual
    // first click activates and expands the host without leaking either edge.
    previewMode=true;
    hostDown=hostUp=gameDown=gameUp=0;
    SetWindowPos(host,HWND_TOPMOST,120,100,100,90,SWP_NOACTIVATE | SWP_SHOWWINDOW); Pump();
    assert(GetForegroundWindow()==game && WindowFromPoint({200,160})==host);
    assert(WindowFromPoint({300,260})==game);
    Edge(MOUSEEVENTF_LEFTDOWN);
    assert(GetForegroundWindow()==host && hostDown==1 && gameDown==0 && GetCapture()==host);
    Edge(MOUSEEVENTF_LEFTUP);
    assert(GetForegroundWindow()==host && hostUp==1 && gameDown==0 && gameUp==0 && !GetCapture());
    DestroyWindow(host); DestroyWindow(game);
    SetCursorPos(priorCursor.x, priorCursor.y);
    if (prior && IsWindow(prior)) SetForegroundWindow(prior);
    std::cout << "PASS: transparent pixels, rectangular hit test, activation, clip release, paired first click, subsequent game click, bounded preview without activation, preview first-click activation without game edges\n";
    return 0;
}

int main() {
    HWND prior=GetForegroundWindow(); POINT saved{}; GetCursorPos(&saved);
    try { return RunTest(); }
    catch (const std::exception& error) {
        const bool ownedFocus=GetForegroundWindow()==host || GetForegroundWindow()==game;
        if (injectedLeftDown) {
            INPUT release{}; release.type=INPUT_MOUSE; release.mi.dwFlags=MOUSEEVENTF_LEFTUP;
            SendInput(1,&release,sizeof(INPUT)); Pump();
        }
        if (GetCapture()==host || GetCapture()==game) ReleaseCapture();
        if (host && IsWindow(host)) DestroyWindow(host);
        if (game && IsWindow(game)) DestroyWindow(game);
        if (ownedFocus) {
            SetCursorPos(saved.x,saved.y);
            if (prior && IsWindow(prior)) SetForegroundWindow(prior);
        }
        std::cerr << "Native prototype failed/interrupted: " << error.what() << '\n';
        return 1;
    }
}
