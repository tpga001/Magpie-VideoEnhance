"""Exercise extracted production task-switch routing without native input or windows."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
def method(text, signature):
    start=text.index(signature); end=text.index('{',start)+1; depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}'); end+=1
    return text[start:end]
def read(path): return (root/path).read_text(encoding='utf-8-sig')
hook=method(read('src/Magpie/ShortcutService.cpp'), 'if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && info->vkCode == VK_TAB')
service=method(read('src/Magpie/ScalingService.cpp'), 'void ScalingService::OnTaskSwitch()')
runtime=method(read('src/Magpie.Core/ScalingRuntime.cpp'), 'bool ScalingRuntime::StopForTaskSwitch()')
harness=r'''
#include <windows.h>
#include <array>
#include <cassert>
#include <iostream>
static std::array<bool,256> keys{};
static SHORT FakeKey(int key) { return keys[key] ? SHORT(0x8000) : 0; }
static LRESULT Next(HHOOK,int,WPARAM,LPARAM) { return 17; }
#define GetAsyncKeyState FakeKey
#define CallNextHookEx Next
struct AppSettings {
    bool enabled=false;
    static AppSettings& Get() { static AppSettings s; return s; }
    bool IsStopEffectsOnTaskSwitchEnabled() const { return enabled; }
};
enum class ScalingState { Idle, Starting, Scaling, Stopping };
struct ScalingWindow {
    static inline bool windowed=false;
    static bool SessionWindowedMode() { return windowed; }
};
struct ScalingRuntime {
    ScalingState state=ScalingState::Idle;
    int stops=0;
    ScalingState State() const { return state; }
    void Stop() { ++stops; state=ScalingState::Stopping; }
    bool StopForTaskSwitch();
};
struct Logger { static Logger& Get() { static Logger l; return l; } void Info(const char*) {} };
struct ScalingService {
    ScalingRuntime runtime;
    ScalingRuntime* _scalingRuntime=&runtime;
    bool _isAutoScaleSuspended=false;
    int timersStopped=0;
    static ScalingService& Get() { static ScalingService s; return s; }
    void StopTimer() { ++timersStopped; }
    void OnTaskSwitch();
};
RUNTIME
SERVICE
static LRESULT Key(WPARAM wParam,DWORD code,DWORD flags=0) {
    KBDLLHOOKSTRUCT event{};event.vkCode=code;event.flags=flags;
    const auto* info=&event;int nCode=HC_ACTION;LPARAM lParam=LPARAM(&event);
    struct { bool _keyboardHookShortcutActivated=true; } that;
    HOOK
    return 23;
}
static void Reset(bool enabled, bool windowed=false, ScalingState state=ScalingState::Scaling) {
    AppSettings::Get().enabled=enabled; ScalingWindow::windowed=windowed; keys.fill(false);
    auto& s=ScalingService::Get(); s.runtime={state,0};s._scalingRuntime=&s.runtime;
    s._isAutoScaleSuspended=false;s.timersStopped=0;
}
int main() {
    auto& s=ScalingService::Get();
    for(bool enabled : {false,true}) for(int modifier : {VK_MENU,VK_LWIN,VK_RWIN}) for(bool shift : {false,true}) {
        Reset(enabled); keys[modifier]=true;keys[VK_SHIFT]=shift;
        assert(Key(WM_KEYDOWN,VK_TAB)==17); // Windows always receives its shortcut.
        assert(s.runtime.stops==int(enabled) && s._isAutoScaleSuspended==enabled && s.timersStopped==int(enabled));
        if(enabled) { assert(Key(WM_KEYDOWN,VK_TAB)==17);assert(s.runtime.stops==1); }
    }
    Reset(true);assert(Key(WM_SYSKEYDOWN,VK_TAB,LLKHF_ALTDOWN)==17 && s.runtime.stops==1);
    for(auto state : {ScalingState::Idle,ScalingState::Stopping}) {
        Reset(true,false,state);s.OnTaskSwitch();assert(!s.runtime.stops && !s._isAutoScaleSuspended);
    }
    Reset(true,false,ScalingState::Starting);s.OnTaskSwitch();assert(s.runtime.stops==1);
    Reset(true,true);keys[VK_MENU]=true;assert(Key(WM_KEYDOWN,VK_TAB)==17);
    assert(!s.runtime.stops && !s._isAutoScaleSuspended); // Preserve the 0.6.6 fullscreen scope.
    Reset(true);s._scalingRuntime=nullptr;s.OnTaskSwitch();assert(!s.timersStopped);
    Reset(true);assert(Key(WM_KEYDOWN,VK_TAB)==23);assert(Key(WM_KEYDOWN,'A')==23);
    keys[VK_MENU]=true;assert(Key(WM_KEYUP,VK_TAB)==23);assert(Key(WM_KEYDOWN,VK_MENU)==23);
    assert(!s.runtime.stops && !s._isAutoScaleSuspended);
    // Changing the home switch is effective for the next task-switch event, even during a run.
    AppSettings::Get().enabled=false;assert(Key(WM_SYSKEYDOWN,VK_TAB,LLKHF_ALTDOWN)==17 && !s.runtime.stops);
    AppSettings::Get().enabled=true;assert(Key(WM_SYSKEYDOWN,VK_TAB,LLKHF_ALTDOWN)==17 && s.runtime.stops==1);
    std::cout << "PASS task-switch option: disabled bypass, Alt/Win/Shift-Tab passthrough, fullscreen starting/running only, one queued stop, auto-scale suspension, unrelated keys and live setting changes\n";
}
'''
output=Path(sys.argv[1])/'task_switch_option.cpp'
output.write_text(harness.replace('RUNTIME',runtime).replace('SERVICE',service).replace('    HOOK\n',hook+'\n'),encoding='utf-8')
print(output)
