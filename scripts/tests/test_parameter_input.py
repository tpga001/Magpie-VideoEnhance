"""Generate a no-GPU harness from the production input host and ImGui backend.

Win32 focus/capture are deterministic doubles; Dear ImGui and all input queue,
hit-test, host-message and transition methods come from the checked-out source.
Run parameter_input_host_prototype.cpp separately for actual USER32/DWM behavior.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
core = root / 'src/Magpie.Core'
def body(name):
    return '\n'.join(line for line in (core / name).read_text(encoding='utf-8-sig').splitlines()
                     if not line.startswith(('#include', '#pragma once')))

host_header = (core / 'OverlayDrawer.h').read_text(encoding='utf-8-sig')
host_members = host_header.split('private:', 1)[1].split('\tbool _BuildFonts()', 1)[0]
enum = re.search(r'enum class ParameterPanelState[^;]+;', (core / 'include/ScalingOptions.h').read_text(encoding='utf-8-sig')).group()

prefix = r'''
#define UNICODE
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <variant>
#include "OverlayWindowGeometry.h"
namespace phmap { template<class K, class V> using flat_hash_map = std::unordered_map<K,V>; }
namespace fmt { template<class... T> std::string format(const char* s, T&&...) { return s; } }
static HWND game = (HWND)1, scaling = (HWND)2, foreground = game, capture = nullptr, inputHost = nullptr;
static POINT cursor{100,100};
static POINT messagePoint{100,100};
static RECT hostRect{}, clipRect{};
static std::array<bool,256> keys{};
static WNDPROC hostProc;
static LONG_PTR hostUserData;
static bool denyFocus, visibleHost;
static int focusAttempts, gameEdges, updates;
static SHORT FakeAsync(int key) { return keys[key] ? SHORT(0x8000) : 0; }
static BOOL FakeCursor(POINT* p) { *p = cursor; return TRUE; }
static HWND FakeForeground() { return foreground; }
static BOOL FakeSetForeground(HWND hwnd) {
    ++focusAttempts;
    if (denyFocus) return FALSE;
    const HWND old = foreground; foreground = hwnd;
    if (old && old == inputHost && old != hwnd) hostProc(old, WM_KILLFOCUS, (WPARAM)hwnd, 0);
    return TRUE;
}
static HWND FakeCapture() { return capture; }
static HWND FakeSetCapture(HWND hwnd) {
    HWND old = capture; capture = hwnd;
    if (old && old == inputHost && old != hwnd) hostProc(old, WM_CAPTURECHANGED, 0, (LPARAM)hwnd);
    return old;
}
static BOOL FakeRelease() { FakeSetCapture(nullptr); return TRUE; }
static BOOL FakeClip(const RECT* r) { clipRect = r ? *r : RECT{}; return TRUE; }
static ATOM FakeRegister(const WNDCLASSEXW* wc) { hostProc = wc->lpfnWndProc; return 1; }
static HWND FakeCreate(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID user) {
    inputHost = (HWND)3; CREATESTRUCT cs{}; cs.lpCreateParams = user;
    hostProc(inputHost, WM_NCCREATE, 0, (LPARAM)&cs); return inputHost;
}
static LONG_PTR FakeSetLong(HWND, int, LONG_PTR data) { return std::exchange(hostUserData,data); }
static LONG_PTR FakeGetLong(HWND, int) { return hostUserData; }
static BOOL FakePosition(HWND, HWND, int x, int y, int w, int h, UINT flags) {
    hostRect = {x,y,x+w,y+h}; if (flags & SWP_SHOWWINDOW) visibleHost = true; return TRUE;
}
static BOOL FakeRect(HWND, RECT* r) { *r = hostRect; return TRUE; }
static BOOL FakeClientToScreen(HWND, POINT* p) { p->x+=hostRect.left; p->y+=hostRect.top; return TRUE; }
static BOOL FakeShow(HWND, int command) { visibleHost = command != SW_HIDE; return TRUE; }
static BOOL FakeVisible(HWND) { return visibleHost; }
static BOOL FakePost(HWND, UINT, WPARAM, LPARAM) { return TRUE; }
static BOOL FakeDestroy(HWND hwnd) { hostProc(hwnd,WM_NCDESTROY,0,0); inputHost=nullptr; visibleHost=false; return TRUE; }
static HWND FakeFocus(HWND hwnd) { return hwnd; }
static UINT_PTR FakeTimer(HWND, UINT_PTR id, UINT, TIMERPROC) { return id; }
static BOOL FakeKillTimer(HWND, UINT_PTR) { return TRUE; }
static LRESULT FakeDef(HWND, UINT, WPARAM, LPARAM) { return 0; }
static LONG FakeMessageTime() { static LONG time; return ++time; }
static DWORD FakeMessagePos() { return DWORD(MAKELPARAM(messagePoint.x,messagePoint.y)); }
#define GetAsyncKeyState FakeAsync
#define GetKeyState FakeAsync
#define GetCursorPos FakeCursor
#define GetForegroundWindow FakeForeground
#define SetForegroundWindow FakeSetForeground
#define GetCapture FakeCapture
#define SetCapture FakeSetCapture
#define ReleaseCapture FakeRelease
#define ClipCursor FakeClip
#define RegisterClassExW FakeRegister
#define CreateWindowExW FakeCreate
#define SetWindowLongPtrW FakeSetLong
#define GetWindowLongPtrW FakeGetLong
namespace ImGui { inline void FakePosition(ImGuiWindow* w, ImVec2 p) { SetWindowPos(w,p); } }
#define SetWindowPos FakePosition
#define GetWindowRect FakeRect
#define ClientToScreen FakeClientToScreen
#define ShowWindow FakeShow
#define IsWindowVisible FakeVisible
#define PostMessageW FakePost
#define DestroyWindow FakeDestroy
#define SetFocus FakeFocus
#define SetTimer FakeTimer
#define KillTimer FakeKillTimer
#define DefWindowProcW FakeDef
#define GetMessageTime FakeMessageTime
#define GetMessagePos FakeMessagePos
#define IsWindow(hwnd) ((hwnd) != nullptr)
namespace Magpie {
struct DeviceResources {};
struct ImGuiBackend {
    bool Initialize(DeviceResources&) { return true; }
    bool BuildFonts() { return true; }
    void RenderDrawData(ImDrawData&, POINT) {}
};
struct Logger {
    static Logger& Get() { static Logger l; return l; }
    template<class... T> void Error(T&&...) {}
    template<class... T> void Warn(T&&...) {}
    template<class... T> void Info(T&&...) {}
    template<class... T> void Win32Error(T&&...) {}
};
struct StrHelper { template<class... T> static std::string Concat(T&&... t) { std::string s; (s.append(t),...); return s; } };
struct Win32Helper { static SIZE GetSizeOfRect(const RECT& r) { return {r.right-r.left,r.bottom-r.top}; } };
}
'''

fixture = r'''
namespace Magpie {
ENUM
SESSION
enum class OverlayAction { Profiler };
class OverlayDrawer {
public:
    ~OverlayDrawer() noexcept;
    HOST_MEMBERS
    bool _isEffectParametersVisible = false, _overlayDirty = false;
    bool _isToolbarVisible=false, _isToolbarPinned=false, _isProfilerVisible=false;
    OverlaySessionState CaptureSessionState() const noexcept;
    void RestoreSessionState(const OverlaySessionState&) noexcept;
    void InvokeAction(OverlayAction) { _isProfilerVisible = !_isProfilerVisible; }
    void _ClearStatesIfNoVisibleWindow() { if (!_isEffectParametersVisible && !_isToolbarVisible && !_isProfilerVisible) ClearStates(); }
    ImGuiImpl _imguiImpl;
    bool IsEditingParameters() const noexcept { return _parameterFocusSwitchingEnabled && _parameterPanelState == ParameterPanelState::Edit; }
    void ClearStates() noexcept { _imguiImpl.ClearStates(); _overlayDirty=true; }
    void SuspendParameterInput() noexcept;
    void ReleaseParameterInput() noexcept;
    bool HasHeldParameterInput() const noexcept;
    void UpdateParameterInputHost() noexcept;
    bool HandleParameterPreviewEscape(WPARAM, const KBDLLHOOKSTRUCT&) noexcept;
    bool AnyVisibleWindow() const { return _isEffectParametersVisible || _isToolbarVisible || _isProfilerVisible; }
    bool MessageHandler(UINT, WPARAM, LPARAM) noexcept;
};
static OverlayDrawer* overlay;
class CursorManager {
public:
    bool onOverlay=false, captured=false;
    bool _isUnderCapture=false, _isCapturedOnForeground=false, _shouldDrawCursor=false, nativeCursorShown=false;
    bool _UpdateParameterCursor() noexcept;
    POINT SrcToScaling(POINT point, bool) const { return {point.x*2,point.y*2}; }
    bool _StopCapture(POINT& point, bool) { point=SrcToScaling(point,false); _isUnderCapture=false; return true; }
    void _RestoreClipCursor() { FakeClip(nullptr); }
    void _ReliableSetCursorPos(POINT point) { cursor=point; }
    void _ClearHitTestResult() {}
    void _ShowSystemCursor(bool show) { nativeCursorShown=show; }
    void Update() { ++updates; }
    POINT CursorPos() const { return cursor; }
    bool IsCursorCapturedOnForeground() const { return false; }
    void IsCursorOnOverlay(bool value) { onOverlay=value; }
    void IsCursorCapturedOnOverlay(bool value) { captured=value; }
};
struct TestRenderer {
    RECT rect{0,0,800,600};
    const RECT& DestRect() const { return rect; }
    HWND ParameterInputHandle() const { return overlay->_hwndParameterInput; }
    bool IsEditingParameters() const { return overlay->IsEditingParameters(); }
    bool IsParameterPreviewAt(POINT point) const { return overlay->_HasParameterForeground() && overlay->_imguiImpl.IsParameterPreviewAt(point); }
};
using Renderer = TestRenderer;
struct TestSource { HWND Handle() const { return game; } bool SetFocus() const { return FakeSetForeground(game); } };
class ScalingWindow {
public:
    static ScalingWindow& Get() { static ScalingWindow window; return window; }
    TestRenderer renderer;
    Magpie::CursorManager cursorManager;
    TestSource source;
    int toasts=0;
    bool alive=true, _stopRequested=false, _isDestroying=false;
    uint8_t _pendingSourceTransition=0;
    HWND Handle() const { return alive ? scaling : nullptr; }
    bool HasHeldParameterInput() const { return overlay->HasHeldParameterInput(); }
    void Stop() noexcept;
    STOP_PROCESS
    void Destroy() { overlay->ReleaseParameterInput(); alive=false; }
    void _CancelParameterRestart() {}
    void CleanAfterSrcRepositioned() {}
    TestRenderer& Renderer() { return renderer; }
    const RECT& RendererRect() const { return renderer.rect; }
    Magpie::CursorManager& CursorManager() { return cursorManager; }
    Magpie::CursorManager* TryGetCursorManager() { return &cursorManager; }
    TestSource& SrcTracker() { return source; }
    bool IsParameterInputWindow(HWND hwnd) { return hwnd && hwnd==overlay->_hwndParameterInput; }
    bool IsResizingOrMoving() const { return false; }
    std::wstring GetLocalizedString(std::wstring_view s) { return std::wstring(s); }
    void ShowToast(std::wstring_view) { ++toasts; }
};
}
'''.replace('ENUM', enum).replace('HOST_MEMBERS', host_members)

tests = r'''
using namespace Magpie;
enum class ShortcutAction { EffectParameters, COUNT_OR_NONE };
static LRESULT HotkeyNext(HHOOK, int, WPARAM, LPARAM) { return 17; }
#define CallNextHookEx HotkeyNext
struct ShortcutHelper { static const char* ToString(ShortcutAction) { return "EffectParameters"; } };
struct ShortcutProbe {
    int fires=0;
    void _FireShortcut(ShortcutAction) { ++fires; }
    LRESULT Hook() { auto action=ShortcutAction::EffectParameters; int nCode=HC_ACTION; WPARAM wParam=WM_KEYDOWN; LPARAM lParam=0; SHORTCUT_ROUTE return 0; }
    LRESULT Window(UINT message, WPARAM wParam) { HOTKEY_ROUTE return 0; }
};
static int slider = 25;
static bool actualParameter = false;
static EffectParameterDesc numericParameter;
static float numericValue = 0.5f;
static bool checkbox = false;
static POINT choicePoint{}, childPoint{}, checkboxPoint{};
static ImVec2 panelPos{20,20};
static bool drawToolbar=false;
static phmap::flat_hash_map<std::string,OverlayWindowOption> windows;
static void Frame(bool present=true) {
    overlay->_imguiImpl.NewFrame(windows,0,1);
    if (drawToolbar) {
        ImGui::SetNextWindowPos({500,10}); ImGui::SetNextWindowSize({200,70});
        ImGui::Begin("##toolbar",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
        if (ImGui::Button("Parameters",{150,30})) overlay->_ToggleParameterPanel();
        ImGui::End();
    }
    if (overlay->_isEffectParametersVisible) {
    ImGui::SetNextWindowPos(panelPos); ImGui::SetNextWindowSize({300,260});
    if (ImGui::Begin(overlay->_parameterFocusSwitchingEnabled ? "Parameters - mode###effectParameters" : "Parameters##effectParameters",
        overlay->_parameterFocusSwitchingEnabled ? nullptr : &overlay->_isEffectParametersVisible,
        !overlay->_parameterFocusSwitchingEnabled || overlay->IsEditingParameters() ? 0 : ImGuiWindowFlags_NoInputs)) {
        if (actualParameter) {
            int tick=0, maximum=0; assert(GetEffectParameterTicks(numericParameter,numericValue,tick,maximum));
            const auto display=std::to_string(numericValue);
            if (DrawEffectParameterSlider("Slider",numericParameter,numericValue,tick,maximum,display.c_str()))
                numericValue=NormalizeEffectParameterValue(numericParameter,numericValue);
        } else ImGui::SliderInt("Slider",&slider,0,100);
        ImGui::Checkbox("Checkbox",&checkbox);
        auto check=ImGui::GetItemRectMin(); checkboxPoint={LONG(check.x+8),LONG(check.y+8)};
        const bool comboOpen = ImGui::BeginCombo("Choice","Current");
        if (!comboOpen) { auto r = ImGui::GetItemRectMin(); choicePoint = {LONG(r.x+100),LONG(r.y+8)}; }
        if (comboOpen) {
            ImGui::Selectable("One"); ImGui::Selectable("Two"); ImGui::EndCombo();
        }
        ImGui::BeginChild("previewProbe",{200,70});
        auto cp=ImGui::GetWindowPos(); childPoint={LONG(cp.x+20),LONG(cp.y+20)};
        ImGui::TextUnformatted("Preview child region"); ImGui::EndChild();
    }
    ImGui::End();
    }
    overlay->_imguiImpl.Draw({});
    if (present) { overlay->_imguiImpl.OnPresentSucceeded(); overlay->_UpdateParameterPreviewHost(); }
}
static void Frames(int count=6) { for(int i=0;i<count;++i) Frame(); }
static void Mouse(UINT msg, int x, int y) {
    cursor={x,y};
    messagePoint=cursor;
    if(msg==WM_LBUTTONDOWN) keys[VK_LBUTTON]=true;
    if(msg==WM_LBUTTONUP) keys[VK_LBUTTON]=false;
    if (foreground==game && visibleHost && msg==WM_LBUTTONDOWN && PtInRect(&hostRect,cursor))
        FakeSetForeground(inputHost); // Native WM_MOUSEACTIVATE activates before delivering DOWN.
    if (foreground==game) { if (msg==WM_LBUTTONDOWN || msg==WM_LBUTTONUP) ++gameEdges; return; }
    OverlayDrawer::_ParameterInputWndProc(inputHost,msg,0,MAKELPARAM(x-hostRect.left,y-hostRect.top));
}
static void Key(UINT msg, int key) {
    keys[key]=msg==WM_KEYDOWN;
    OverlayDrawer::_ParameterInputWndProc(inputHost,msg,key,0);
}
static void ScalingMouse(UINT msg, POINT eventPoint, POINT currentPoint) {
    messagePoint=eventPoint; cursor=currentPoint;
    if(msg==WM_LBUTTONDOWN) keys[VK_LBUTTON]=true;
    if(msg==WM_LBUTTONUP) keys[VK_LBUTTON]=false;
    overlay->MessageHandler(msg,0,MAKELPARAM(eventPoint.x,eventPoint.y));
}
static bool PreviewEscape(UINT message, DWORD flags=0) {
    KBDLLHOOKSTRUCT key{}; key.vkCode=VK_ESCAPE; key.flags=flags;
    const bool claimed=overlay->HandleParameterPreviewEscape(message,key);
    // Low-level hooks see the asynchronous state from before this edge.
    if (!claimed) keys[VK_ESCAPE]=message==WM_KEYDOWN || message==WM_SYSKEYDOWN;
    return claimed;
}
int RunFocusMode() {
    ShortcutProbe shortcut;
    assert(shortcut.Hook()==17 && shortcut.fires==0);
    assert(shortcut.Window(WM_HOTKEY,0)==0 && shortcut.fires==1);
    OverlayDrawer panel; overlay=&panel;
    panel._parameterFocusSwitchingEnabled=true;
    panel._imguiImpl.ParameterFocusSwitchingEnabled(true);
    DeviceResources resources; assert(panel._imguiImpl.Initialize(resources));
    auto& io=ImGui::GetIO(); unsigned char* pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    io.DisplaySize={800,600}; io.DeltaTime=1.0f/60;
    // Open via a real ImGui toolbar Button during the production input frame.
    drawToolbar=panel._isToolbarVisible=true; Frames();
    ScalingMouse(WM_LBUTTONDOWN,{540,45},{540,45}); Frames();
    ScalingMouse(WM_LBUTTONUP,{540,45},{540,45}); Frames();
    assert(panel.IsEditingParameters() && foreground==inputHost);
    ScalingMouse(WM_LBUTTONDOWN,{600,450},{600,450});
    ScalingMouse(WM_LBUTTONUP,{600,450},{600,450}); Frames(); panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Preview);
    panel._SetParameterPanelState(ParameterPanelState::Closed); drawToolbar=panel._isToolbarVisible=false; Frames();
    cursor=messagePoint={100,100};
    // One shared toolbar/shortcut action opens edit and closes visible panels.
    panel._ToggleParameterPanel(); Frames(); assert(panel.IsEditingParameters());
    panel._ToggleParameterPanel(); Frames(); panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Closed && !panel._isEffectParametersVisible && foreground==game);
    panel._ToggleParameterPanel(); Frames();
    panel._SetParameterPanelState(ParameterPanelState::Preview); Frames(); panel.UpdateParameterInputHost();
    assert(panel._isEffectParametersVisible && !panel.IsEditingParameters());
    panel._ToggleParameterPanel(); Frames(); assert(!panel._isEffectParametersVisible);
    // Closing through the action still waits for held shortcut modifiers.
    panel._ToggleParameterPanel(); Frames(); Key(WM_KEYDOWN,VK_MENU);
    panel._ToggleParameterPanel(); Frames(); assert(panel.IsEditingParameters());
    Key(WM_KEYUP,VK_MENU); Frames(); panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Closed && foreground==game);
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frame();
    assert(panel._imguiImpl.OwnsPointerAtCursor()); Frames();
    assert(panel.IsEditingParameters() && foreground==inputHost && visibleHost);
    // Moving out never exits edit; the first outside click stays entirely in the host.
    Mouse(WM_MOUSEMOVE,600,450); Frames(); assert(panel.IsEditingParameters());
    Mouse(WM_LBUTTONDOWN,600,450); Frames(); panel.UpdateParameterInputHost();
    assert(panel.IsEditingParameters() && foreground==inputHost && gameEdges==0);
    Mouse(WM_MOUSEMOVE,610,455); // Queued noncritical movement must not postpone the release.
    Mouse(WM_LBUTTONUP,600,450);
    assert(!panel.IsEditingParameters()); Frames();
    Mouse(WM_MOUSEMOVE,610,455);
    panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Preview && visibleHost && foreground==game && gameEdges==0);
    assert(hostRect.left==20 && hostRect.top==20 && hostRect.right==320 && hostRect.bottom==280);
    // Preview remains read-only in ImGui; only its native panel rectangle claims activation.
    cursor={100,80}; Frames(); assert(!io.WantCaptureMouse);
    cursor=childPoint; Frames(); assert(!io.WantCaptureMouse && !panel._imguiImpl.OwnsPointerAtCursor());
    Mouse(WM_LBUTTONDOWN,600,450); Mouse(WM_LBUTTONUP,600,450); assert(gameEdges==2);
    // The original press activates editing AND operates the hit control.
    auto preview = [&] {
        Key(WM_KEYDOWN,VK_ESCAPE); Key(WM_KEYUP,VK_ESCAPE); Frames(); panel.UpdateParameterInputHost();
        assert(panel._parameterPanelState==ParameterPanelState::Preview && foreground==game);
    };
    Mouse(WM_LBUTTONDOWN,100,51); Frame();
    assert(panel.IsEditingParameters() && ImGui::IsAnyItemActive() && capture==inputHost);
    Mouse(WM_MOUSEMOVE,600,450); Frames(); Mouse(WM_LBUTTONUP,600,450); Frames();
    assert(slider==100 && !capture && panel.IsEditingParameters() && gameEdges==2);
    preview();
    const bool beforeCheck=checkbox;
    Mouse(WM_LBUTTONDOWN,checkboxPoint.x,checkboxPoint.y);
    Mouse(WM_LBUTTONUP,checkboxPoint.x,checkboxPoint.y); Frames();
    assert(panel.IsEditingParameters() && checkbox!=beforeCheck && gameEdges==2);
    preview();
    Mouse(WM_LBUTTONDOWN,choicePoint.x,choicePoint.y); Frames();
    Mouse(WM_LBUTTONUP,choicePoint.x,choicePoint.y); Frames();
    assert(panel.IsEditingParameters() && !ImGui::GetCurrentContext()->OpenPopupStack.empty());
    Key(WM_KEYDOWN,VK_ESCAPE); Key(WM_KEYUP,VK_ESCAPE); Frames();
    preview();
    Mouse(WM_LBUTTONDOWN,childPoint.x,childPoint.y); Frame();
    Mouse(WM_LBUTTONUP,childPoint.x,childPoint.y); Frames();
    assert(panel.IsEditingParameters() && gameEdges==2);
    preview();
    // A failed Present must not move the native hit target ahead of the visible panel.
    panelPos={450,400}; Frame(false); panel.UpdateParameterInputHost();
    assert(hostRect.left==20 && hostRect.top==20);
    Frame(); assert(hostRect.left==450 && hostRect.top==400 && hostRect.bottom==600);
    panelPos={20,20}; Frames(); assert(hostRect.left==20 && hostRect.bottom==280);
    // Production cursor handoff runs before the 3D capture branch. Test the
    // mapped visible point, native point after handoff, and leaving the panel.
    auto& pointer=ScalingWindow::Get().CursorManager();
    cursor={50,30}; pointer._isUnderCapture=true; pointer._shouldDrawCursor=true;
    assert(pointer._UpdateParameterCursor());
    assert(!pointer._isUnderCapture && cursor.x==100 && cursor.y==60 && pointer.nativeCursorShown);
    assert(pointer._UpdateParameterCursor() && cursor.x==100 && cursor.y==60);
    cursor={600,450}; assert(!pointer._UpdateParameterCursor());
    foreground=(HWND)9; cursor={50,30}; pointer._isUnderCapture=true;
    assert(!pointer._UpdateParameterCursor() && pointer._isUnderCapture);
    foreground=game; pointer._isUnderCapture=false; cursor=messagePoint={100,100};
    // The scaling HWND is a real second input route. Its preview press must
    // enter editing, and its outside press must request Preview just like the host.
    const bool scalingCheck=checkbox;
    ScalingMouse(WM_LBUTTONDOWN,checkboxPoint,checkboxPoint); Frame();
    assert(panel.IsEditingParameters() && foreground==inputHost);
    ScalingMouse(WM_LBUTTONUP,checkboxPoint,checkboxPoint); Frames();
    assert(checkbox!=scalingCheck);
    // A cursor already moved back over the panel must not change where the DOWN happened.
    ScalingMouse(WM_LBUTTONDOWN,{600,450},checkboxPoint); Frames();
    assert(panel._pendingParameterPanelState==ParameterPanelState::Preview && panel.IsEditingParameters());
    ScalingMouse(WM_LBUTTONUP,{600,450},checkboxPoint); Frames(); panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Preview && gameEdges==2);
    // Modifiers inherited at activation may release to the old game queue.
    // No host WM_KEYUP arrives: physical release must still permit leaving edit.
    keys[VK_MENU]=keys[VK_SHIFT]=true;
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    keys[VK_MENU]=keys[VK_SHIFT]=false;
    ScalingMouse(WM_LBUTTONDOWN,{600,450},{600,450});
    ScalingMouse(WM_LBUTTONUP,{600,450},{600,450}); Frames(); panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Preview && !panel.HasHeldParameterInput());
    assert(!io.KeyAlt && !io.KeyShift);
    // A delayed focus/cancel notification for the scaling HWND must not
    // cancel the newly focused input host or discard its pending control press.
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    Mouse(WM_LBUTTONDOWN,100,51); Frame();
    assert(ImGui::IsAnyItemActive());
    panel.MessageHandler(WM_KILLFOCUS,WPARAM(inputHost),0);
    panel.MessageHandler(WM_CANCELMODE,0,0); Frames();
    assert(panel.IsEditingParameters() && ImGui::IsAnyItemActive());
    Mouse(WM_LBUTTONUP,100,51); Frames(); preview();
    // Preview Esc owns a complete press/repeat/release and closes only in the
    // outer update, without activating a host or forwarding a key to the game.
    const int previewFocusAttempts=focusAttempts;
    assert(PreviewEscape(WM_KEYDOWN)); assert(panel.HasHeldParameterInput());
    assert(PreviewEscape(WM_KEYDOWN)); panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Preview);
    assert(PreviewEscape(WM_KEYUP)); assert(!panel.HasHeldParameterInput());
    assert(panel._parameterPanelState==ParameterPanelState::Preview);
    panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Closed && !visibleHost && foreground==game);
    assert(focusAttempts==previewFocusAttempts);
    assert(!PreviewEscape(WM_KEYDOWN)); assert(!PreviewEscape(WM_KEYUP));
    panel._SetParameterPanelState(ParameterPanelState::Preview); Frames();
    keys[VK_ESCAPE]=true; assert(!PreviewEscape(WM_KEYDOWN)); assert(!PreviewEscape(WM_KEYUP));
    keys[VK_MENU]=true; assert(!PreviewEscape(WM_SYSKEYDOWN,LLKHF_ALTDOWN)); assert(!PreviewEscape(WM_SYSKEYUP)); keys[VK_MENU]=false;
    assert(panel._parameterPanelState==ParameterPanelState::Preview);
    // Other apps keep their keys; losing foreground invalidates a pending close.
    foreground=(HWND)9; panel.UpdateParameterInputHost(); assert(!visibleHost);
    assert(!PreviewEscape(WM_KEYDOWN)); assert(!PreviewEscape(WM_KEYUP));
    foreground=game; panel.UpdateParameterInputHost(); assert(visibleHost);
    assert(PreviewEscape(WM_KEYDOWN)); foreground=(HWND)9; panel.UpdateParameterInputHost();
    foreground=game; assert(PreviewEscape(WM_KEYUP)); panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Preview);
    assert(PreviewEscape(WM_KEYDOWN)); panel._SetParameterPanelState(ParameterPanelState::Closed);
    panel._SetParameterPanelState(ParameterPanelState::Preview); assert(PreviewEscape(WM_KEYUP)); panel.UpdateParameterInputHost();
    assert(panel._parameterPanelState==ParameterPanelState::Preview);
    // A drag that starts on the panel stays captured after leaving the panel.
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    Mouse(WM_LBUTTONDOWN,100,51); Frames();
    assert(ImGui::IsAnyItemActive() && capture==inputHost);
    Mouse(WM_MOUSEMOVE,600,450); Frames();
    assert(panel.IsEditingParameters() && capture==inputHost);
    Mouse(WM_LBUTTONUP,600,450); Frames(); panel.UpdateParameterInputHost();
    assert(panel.IsEditingParameters() && !capture && !ImGui::IsAnyItemActive());
    // A game-area click returns after its pair even when a dropdown is open.
    Mouse(WM_LBUTTONDOWN,choicePoint.x,choicePoint.y); Frames(); Mouse(WM_LBUTTONUP,choicePoint.x,choicePoint.y); Frames();
    assert(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    Mouse(WM_LBUTTONDOWN,600,450); Frames(); Mouse(WM_LBUTTONUP,600,450); Frames();
    panel.UpdateParameterInputHost();
    assert(!panel.IsEditingParameters() && ImGui::GetCurrentContext()->OpenPopupStack.empty());
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    // Keyboard Escape first dismisses a popup, then exits on a complete key pair.
    Mouse(WM_LBUTTONDOWN,choicePoint.x,choicePoint.y); Frames(); Mouse(WM_LBUTTONUP,choicePoint.x,choicePoint.y); Frames();
    assert(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    Key(WM_KEYDOWN,VK_ESCAPE); Key(WM_KEYUP,VK_ESCAPE); Frames();
    assert(panel.IsEditingParameters() && ImGui::GetCurrentContext()->OpenPopupStack.empty());
    Key(WM_KEYDOWN,VK_ESCAPE); assert(panel.IsEditingParameters());
    Key(WM_KEYUP,VK_ESCAPE); Frames(); panel.UpdateParameterInputHost();
    assert(!panel.IsEditingParameters() && foreground==game);
    // Real ImGui numeric input receives modifier/key/character events in order.
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    Key(WM_KEYDOWN,VK_CONTROL); Frames();
    Mouse(WM_LBUTTONDOWN,100,51); Frames(); Mouse(WM_LBUTTONUP,100,51); Frames();
    assert(ImGui::GetCurrentContext()->InputTextState.ID == ImGui::GetCurrentContext()->ActiveId && ImGui::IsAnyItemActive());
    Key(WM_KEYDOWN,'A'); Frames(); Key(WM_KEYUP,'A'); Key(WM_KEYUP,VK_CONTROL); Frames();
    OverlayDrawer::_ParameterInputWndProc(inputHost,WM_CHAR,'4',0);
    OverlayDrawer::_ParameterInputWndProc(inputHost,WM_CHAR,'2',0); Frames();
    Key(WM_KEYDOWN,VK_RETURN); Frames(); Key(WM_KEYUP,VK_RETURN); Frames();
    assert(slider==42 && panel.IsEditingParameters() && gameEdges==2);
    // Text entry uses real parameter units; mouse dragging still uses STEP indices.
    actualParameter=true;
    numericParameter.constant=EffectConstant<float>{0.5f,0.0f,2.0f,0.05f}; numericValue=0.5f; Frames();
    auto typeParameter = [](const char* text) {
        Key(WM_KEYDOWN,VK_CONTROL); Frames();
        Mouse(WM_LBUTTONDOWN,100,51); Frames(); Mouse(WM_LBUTTONUP,100,51); Frames();
        assert(ImGui::TempInputIsActive(ImGui::GetCurrentContext()->ActiveId));
        Key(WM_KEYDOWN,'A'); Frames(); Key(WM_KEYUP,'A'); Key(WM_KEYUP,VK_CONTROL); Frames();
        for (const char* c=text;*c;++c) OverlayDrawer::_ParameterInputWndProc(inputHost,WM_CHAR,*c,0);
        Frames(12); Key(WM_KEYDOWN,VK_RETURN); Frames(); Key(WM_KEYUP,VK_RETURN); Frames();
    };
    typeParameter("0.75"); assert(std::abs(numericValue-0.75f)<1e-6f);
    typeParameter("0.73"); assert(std::abs(numericValue-0.75f)<1e-6f);
    typeParameter("99"); assert(numericValue==2.0f);
    numericParameter.constant=EffectConstant<float>{0.0f,-1.0f,1.0f,0.125f}; numericValue=0.0f; Frames();
    typeParameter("-0.375"); assert(std::abs(numericValue+0.375f)<1e-6f);
    numericParameter.constant=EffectConstant<int>{7,1,15,2}; numericValue=7; Frames();
    typeParameter("11"); assert(numericValue==11);
    typeParameter("10"); assert(numericValue==11);
    actualParameter=false; Frames();
    panel._SetParameterPanelState(ParameterPanelState::Preview); Frames(); panel.UpdateParameterInputHost();
    // Shortcut modifiers and queued releases delay a return to the game.
    keys[VK_CONTROL]=true; panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    panel._SetParameterPanelState(ParameterPanelState::Preview); assert(panel.IsEditingParameters());
    Key(WM_KEYUP,VK_CONTROL); Frames(); panel.UpdateParameterInputHost(); assert(!panel.IsEditingParameters());
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    Mouse(WM_LBUTTONDOWN,100,51); Frames();
    panel._SetParameterPanelState(ParameterPanelState::Preview); assert(panel.IsEditingParameters());
    Mouse(WM_LBUTTONUP,100,51); assert(panel.IsEditingParameters());
    Frames(); panel.UpdateParameterInputHost(); assert(!panel.IsEditingParameters());
    // Losing foreground cancels capture and ImGui activity without activating the game.
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    Mouse(WM_LBUTTONDOWN,100,51); Frames();
    const int attempts=focusAttempts; FakeSetForeground((HWND)9);
    assert(!panel.IsEditingParameters() && foreground==(HWND)9 && !capture && !visibleHost);
    assert(focusAttempts==attempts+1 && !ImGui::IsAnyItemActive());
    Mouse(WM_LBUTTONUP,600,450); keys.fill(false);
    // A rebuild restore over another app stays in preview. No focus attempt is made.
    panel._SetParameterPanelState(ParameterPanelState::Edit,false);
    assert(!panel.IsEditingParameters() && foreground==(HWND)9 && focusAttempts==attempts+1);
    // Focus failure makes one attempt, then leaves a visible preview and no capture.
    foreground=game; denyFocus=true; panel._SetParameterPanelState(ParameterPanelState::Edit);
    assert(!panel.IsEditingParameters() && panel._parameterFocusFailed && visibleHost && !capture);
    const int failedAttempts=focusAttempts; panel.UpdateParameterInputHost(); panel.UpdateParameterInputHost();
    assert(focusAttempts==failedAttempts); denyFocus=false;
    // Stopping while a panel press is down defers HWND destruction until its up.
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    Mouse(WM_LBUTTONDOWN,100,51); Key(WM_KEYDOWN,'Z'); Frames();
    auto& scalingWindow = ScalingWindow::Get(); scalingWindow.Stop();
    assert(scalingWindow.Handle() && scalingWindow._stopRequested && panel.IsEditingParameters());
    assert(!scalingWindow.ProcessPendingStop());
    Mouse(WM_LBUTTONUP,600,450); Key(WM_KEYUP,'Z'); Frames();
    assert(scalingWindow.ProcessPendingStop());
    assert(!scalingWindow.Handle() && !visibleHost && !capture && gameEdges==2);
    scalingWindow.alive=true;
    panel._SetParameterPanelState(ParameterPanelState::Edit); Frames();
    const auto editingState=panel.CaptureSessionState();
    Key(WM_KEYDOWN,'Z'); panel._SetParameterPanelState(ParameterPanelState::Preview);
    assert(panel.CaptureSessionState().parameterPanelState==ParameterPanelState::Preview);
    panel._SetParameterPanelState(ParameterPanelState::Closed);
    assert(!panel.CaptureSessionState().effectParametersVisible);
    Key(WM_KEYUP,'Z'); Frames(); panel.UpdateParameterInputHost();
    panel.RestoreSessionState(editingState); Frames(); assert(panel.IsEditingParameters());
    panel.ReleaseParameterInput(); foreground=(HWND)9;
    const int beforeRestore=focusAttempts; panel.RestoreSessionState(editingState); Frames();
    assert(!panel.IsEditingParameters() && foreground==(HWND)9 && focusAttempts==beforeRestore);
    foreground=game; panel.RestoreSessionState(editingState); Frames(); assert(panel.IsEditingParameters());
    panel._SetParameterPanelState(ParameterPanelState::Closed); Frames(); panel.UpdateParameterInputHost();
    assert(!panel._isEffectParametersVisible && !visibleHost && foreground==game);
    std::cout << "PASS focus-switching mode production input: real toolbar Button route, registered parameter hotkey route, both HWND mouse routes, event-coordinate outside click, inherited modifier release, delayed scaling focus/cancel messages, three states, first-click slider/checkbox/dropdown, last-present hit target, preview Esc guards, drag/popup/numeric input, deferred stop and restore\n";
    return 0;
}

int RunLegacyMode() {
    foreground=game; inputHost=capture=nullptr; visibleHost=false; focusAttempts=gameEdges=0;
    keys.fill(false); windows.clear(); panelPos={20,20}; slider=25; checkbox=false; drawToolbar=false;
    OverlayDrawer panel; overlay=&panel;
    assert(!panel._parameterFocusSwitchingEnabled);
    DeviceResources resources; assert(panel._imguiImpl.Initialize(resources));
    auto& io=ImGui::GetIO(); unsigned char* pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    io.DisplaySize={800,600}; io.DeltaTime=1.0f/60;
    auto mouse=[](UINT message,int x,int y) {
        cursor=messagePoint={x,y};
        if(message==WM_LBUTTONDOWN) keys[VK_LBUTTON]=true;
        if(message==WM_LBUTTONUP) keys[VK_LBUTTON]=false;
        return overlay->MessageHandler(message,0,MAKELPARAM(x,y));
    };
    // Actual toolbar Button and hotkey action open an ordinary editable overlay.
    drawToolbar=panel._isToolbarVisible=true; Frames();
    mouse(WM_LBUTTONDOWN,540,45); Frames(); mouse(WM_LBUTTONUP,540,45); Frames();
    assert(panel._isEffectParametersVisible && !inputHost && foreground==game && focusAttempts==0);
    assert(panel._parameterPanelState==ParameterPanelState::Closed);
    mouse(WM_LBUTTONDOWN,checkboxPoint.x,checkboxPoint.y); Frames();
    mouse(WM_LBUTTONUP,checkboxPoint.x,checkboxPoint.y); Frames(); assert(checkbox);
    mouse(WM_LBUTTONDOWN,100,51); Frames(); assert(ImGui::IsAnyItemActive() && capture==scaling);
    mouse(WM_MOUSEMOVE,600,450); Frames(); mouse(WM_LBUTTONUP,600,450); Frames();
    assert(slider==100 && !capture && panel._isEffectParametersVisible && foreground==game);
    mouse(WM_LBUTTONDOWN,choicePoint.x,choicePoint.y); Frames();
    mouse(WM_LBUTTONUP,choicePoint.x,choicePoint.y); Frames();
    assert(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    mouse(WM_LBUTTONDOWN,600,450); Frames(); mouse(WM_LBUTTONUP,600,450); Frames();
    assert(ImGui::GetCurrentContext()->OpenPopupStack.empty() && panel._isEffectParametersVisible);
    // Ordinary game-area clicks and Escape never turn the panel into a preview.
    assert(!mouse(WM_LBUTTONDOWN,600,450)); mouse(WM_LBUTTONUP,600,450); Frames();
    assert(!PreviewEscape(WM_KEYDOWN)); assert(!PreviewEscape(WM_KEYUP));
    assert(panel._imguiImpl.MessageHandler(WM_KEYDOWN,'A',0)==ImGuiInputResult::None);
    assert(panel._imguiImpl.MessageHandler(WM_CHAR,'4',0)==ImGuiInputResult::None);
    assert(panel._isEffectParametersVisible && !inputHost && focusAttempts==0);
    const auto state=panel.CaptureSessionState(); assert(state.effectParametersVisible);
    panel._ToggleParameterPanel(); Frames(); assert(!panel._isEffectParametersVisible);
    panel.RestoreSessionState(state); Frames(); assert(panel._isEffectParametersVisible);
    // Restored panel is immediately interactive, without an activation-only click.
    const bool previous=checkbox;
    mouse(WM_LBUTTONDOWN,checkboxPoint.x,checkboxPoint.y); Frames();
    mouse(WM_LBUTTONUP,checkboxPoint.x,checkboxPoint.y); Frames(); assert(checkbox!=previous);
    // Legacy title-bar close button remains available.
    mouse(WM_LBUTTONDOWN,307,29); Frames(); mouse(WM_LBUTTONUP,307,29); Frames();
    assert(!panel._isEffectParametersVisible && !inputHost && focusAttempts==0 && foreground==game);
    std::cout << "PASS default 0.6.6 input: toolbar open/close, direct first-click controls, drag/capture, popup dismissal, game-area continuity, no keyboard/Escape state interception, restore and title close, no input host or foreground activation\n";
    return 0;
}
int main() { RunFocusMode(); return RunLegacyMode(); }

'''

def method(text, signature):
    start = text.index(signature)
    left = text.index('{', start)
    depth = 1
    end = left + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]
scaling_header = (core / 'ScalingWindow.h').read_text(encoding='utf-8-sig')
scaling_cpp = (core / 'ScalingWindow.cpp').read_text(encoding='utf-8-sig')
fixture = fixture.replace('STOP_PROCESS', method(scaling_header, 'bool ProcessPendingStop() noexcept'))
session_header = (core / 'include/ScalingOptions.h').read_text(encoding='utf-8-sig')
fixture = fixture.replace('SESSION', method(session_header, 'struct OverlaySessionState') + ';')
drawer_cpp = (core / 'OverlayDrawer.cpp').read_text(encoding='utf-8-sig')
assert '"closeParameters"' not in drawer_cpp
assert 'ImGui::Begin(title.c_str(), _parameterFocusSwitchingEnabled ? nullptr : &_isEffectParametersVisible,' in drawer_cpp
parameter_action = drawer_cpp.split('case OverlayAction::EffectParameters:', 1)[1].split('break;', 1)[0]
assert '_ToggleParameterPanel();' in parameter_action
assert 'parametersVisible != _isEffectParametersVisible) InvokeAction(OverlayAction::EffectParameters)' in drawer_cpp
session_code = 'namespace Magpie {\n' + method(drawer_cpp, 'OverlaySessionState OverlayDrawer::CaptureSessionState()') + '\n' + method(drawer_cpp, 'void OverlayDrawer::RestoreSessionState(') + '\n' + method(drawer_cpp, 'bool OverlayDrawer::MessageHandler(') + '\n}\n'
cursor_cpp = (core / 'CursorManager.cpp').read_text(encoding='utf-8-sig')
cursor_state = method(cursor_cpp, 'void CursorManager::_UpdateCursorState()')
assert cursor_state.index('if (_UpdateParameterCursor()) return;') < cursor_state.index('if (options.Is3DGameMode())')
cursor_code = 'namespace Magpie {\n' + method(cursor_cpp, 'bool CursorManager::_UpdateParameterCursor()') + '\n}\n'
shortcut_cpp = (root / 'src/Magpie/ShortcutService.cpp').read_text(encoding='utf-8-sig')
route = re.search(r'if \(action == ShortcutAction::EffectParameters\)\s+return CallNextHookEx\([^;]+;', shortcut_cpp).group()
tests = tests.replace('SHORTCUT_ROUTE', route)
tests = tests.replace('HOTKEY_ROUTE', method(shortcut_cpp, 'if (message == WM_HOTKEY)'))
numeric_desc = (core / 'include/EffectDesc.h').read_text(encoding='utf-8-sig')
numeric_types = numeric_desc[numeric_desc.index('template <typename T>'):numeric_desc.index('struct EffectPassFlags')]
numeric_code = 'namespace Magpie {\n' + numeric_types + '\n}\n' + body('include/EffectParameterValue.h') + '\nnamespace Magpie {\n' + method(drawer_cpp, 'static bool DrawEffectParameterSlider(') + '\n}\n'
stop_code = 'namespace Magpie {\n' + method(scaling_cpp, 'void ScalingWindow::Stop() noexcept') + '\n}\n'

output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
(output / 'parameter_input.cpp').write_text(prefix + body('ImGuiImpl.h') + fixture + body('ImGuiImpl.cpp') + body('ParameterInputHost.cpp') + cursor_code + stop_code + session_code + numeric_code + tests, encoding='utf-8')
print(output / 'parameter_input.cpp')
