"""Exercise production StepTimer with a controlled clock and OS wait boundary."""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1])
header = (repo / 'src/Magpie.Core/StepTimer.h').read_text(encoding='utf-8-sig')
body = (repo / 'src/Magpie.Core/StepTimer.cpp').read_text(encoding='utf-8-sig')
body = re.sub(r'^#include[^\n]*\n', '', body, flags=re.M)
header = header.replace('#pragma once', '').replace('std::chrono::steady_clock', 'TestClock')
body = body.replace('steady_clock', 'TestClock')
prefix = r'''
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <optional>
#include <utility>
using namespace std::chrono_literals;
struct TestClock {
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<TestClock>;
    static constexpr bool is_steady = true;
    static inline time_point current{};
    static time_point now() { return current; }
};
static unsigned waits=0, yields=0;
namespace wil {
class unique_handle {
    HANDLE value=nullptr;
public:
    void reset(HANDLE handle) { value=handle; }
    HANDLE get() const { return value; }
    explicit operator bool() const { return value!=nullptr; }
};
using unique_event_nothrow=unique_handle;
}
template<class... T> HANDLE MockCreate(T...) { return reinterpret_cast<HANDLE>(1); }
template<class... T> BOOL MockSet(T...) { return TRUE; }
template<class... T> DWORD MockWait(T...) { ++waits; return WAIT_OBJECT_0; }
template<class... T> void MockSleep(T...) { ++yields; }
#undef CreateWaitableTimerEx
#define CreateWaitableTimerEx MockCreate
#define CreateWaitableTimerExW MockCreate
#define SetWaitableTimerEx MockSet
#define MsgWaitForMultipleObjectsEx MockWait
#define WaitForSingleObject MockWait
#define Sleep MockSleep
namespace Magpie::FrameTrace {
enum class Event { CaptureWake };
template<class... T> void Mark(T...) {}
}
#include "FramePacingWait.h"
'''
suffix = r'''
int main() {
    using namespace Magpie;
    for (bool strict : {false,true}) {
        StepTimer timer;
        timer.Initialize(0,80.0f,strict);
        bool fps=false;
        TestClock::current=TestClock::time_point(100ms);
        timer.WaitForNextFrame(false,fps);
        timer.CaptureStarting();
        timer.PrepareForRender();
        TestClock::current=TestClock::time_point(119ms);
        assert(timer.WaitForNextFrame(false,fps)==StepTimerStatus::WaitForNewFrame);
        timer.CaptureStarting();
        timer.PrepareForRender();
        TestClock::current=TestClock::time_point(125ms);
        assert(timer.WaitForNextFrame(false,fps)==(strict ? StepTimerStatus::WaitForFPSLimiter : StepTimerStatus::WaitForNewFrame));
        TestClock::current=TestClock::time_point(131ms);
        timer.WaitForNextFrame(false,fps);
        if (strict) assert(yields==0);
        TestClock::current=TestClock::time_point(131500us);
        assert(timer.WaitForNextFrame(false,fps)==StepTimerStatus::WaitForNewFrame);
        timer.CaptureStarting();
        timer.PrepareForRender();
        // A stall must not trigger a burst. Refresh after late message work too.
        TestClock::current=TestClock::time_point(800ms);
        timer.WaitForNextFrame(false,fps);
        TestClock::current=TestClock::time_point(810ms);
        timer.CaptureStarting();
        timer.PrepareForRender();
        TestClock::current=TestClock::time_point(820ms);
        if (strict) assert(timer.WaitForNextFrame(false,fps)==StepTimerStatus::WaitForFPSLimiter);
        TestClock::current=TestClock::time_point(822500us);
        assert(timer.WaitForNextFrame(false,fps)==StepTimerStatus::WaitForNewFrame);
    }
    assert(waits>0);
    std::cout << "PASS: production StepTimer strict 80 FPS starts, legacy phase behavior, late capture, long stalls and passive waits\n";
}
'''
(output / 'frame_sync_timer.cpp').write_text(prefix + header + body + suffix, encoding='utf-8')
