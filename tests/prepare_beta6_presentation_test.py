"""Exercise the production DLSS FIFO renderer with a deterministic clock, no UI/GPU."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
core = repo / 'src/Magpie.Core'

def method(source, signature):
    start = source.index(signature)
    left = source.index('{', start)
    end, depth = left + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

production = method((core / 'Renderer.cpp').read_text(encoding='utf-8-sig'),
                    'DLSSFGFrameRenderResult Renderer::RenderDLSSFGFrame(')
production = production.replace('std::chrono::steady_clock::now()', 'TestClock::now()')
pressure = method((core / 'Renderer.h').read_text(encoding='utf-8-sig'), 'bool _IsDLSSFGQueueFull()')

prefix = r'''
#include "FramePresentationTiming.h"
#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <optional>
using namespace std::chrono_literals;
namespace wil {
template<class F> struct scope_exit { F function; ~scope_exit() { function(); } };
}
struct TestClock {
    static inline std::chrono::steady_clock::time_point value{};
    static auto now() { return value; }
    static void Set(std::chrono::milliseconds time) { value=std::chrono::steady_clock::time_point(time); }
};
struct Event { int signals=0; Event* get() { return this; } explicit operator bool() const { return true; } };
static void SetEvent(Event* event) { ++event->signals; }
namespace Magpie {
namespace FrameTrace {
enum class Event { FgDequeued };
inline constexpr bool Enabled() { return false; }
inline int64_t Tick() { return 0; }
inline void Record(Event,int64_t,int64_t,uint64_t,int64_t,int64_t) {}
}
enum class DLSSFGFrameRenderResult { Presented,Retry,Dropped };
struct Presenter { bool UsesFrameLatencyWaitableObject() { return true; } };
struct Renderer {
    enum class Next { Present,Capacity,Resource,Drop } next=Next::Present;
    struct FrontendRenderTimings {
        std::chrono::nanoseconds beginFrame{},draw{},endFrame{};
        bool capacityBusy=false;
    };
    bool _dlssFrameGenerator=true;
    std::atomic<uint32_t> _sharedTextureGeneration=2, _pendingDLSSFGFrontendFrames=1;
    uint32_t _sharedTextureSlotCount=4;
    std::atomic<bool> _synchronousFramePresentationEnabled=true;
    std::array<std::atomic<int64_t>,4> _sharedPresentIntervalNs{};
    std::array<std::atomic<uint64_t>,4> _sharedTextureFrameIds{};
    std::array<Event,4> _sharedTextureAvailableEvents;
    Event _frameSyncConsumedEvent;
    std::optional<std::chrono::steady_clock::time_point> _frontendPacingDeadline;
    FramePresentationClock _presentationClock;
    Presenter presenter; Presenter* _presenter=&presenter;
    PresentationJobTiming recorded;
    int records=0;
    bool lastDropped=false;
    void _RecordDLSSFGFrontendTimings(bool,const PresentationJobTiming& timing,bool dropped) {
        recorded=timing;lastDropped=dropped;++records;
    }
    bool _FrontendRender(bool,uint32_t,FrontendRenderTimings* timing,bool,bool* dropped) {
        TestClock::value+=1ms;
        timing->beginFrame=1ms;
        timing->capacityBusy=next==Next::Capacity;
        *dropped=next==Next::Drop;
        return next==Next::Present;
    }
    DLSSFGFrameRenderResult RenderDLSSFGFrame(uint32_t,uint32_t,PresentationJobTiming&) noexcept;
'''

suffix = r'''
}
using namespace Magpie;
int main() {
    // One notification spans several deadlines, capacity/resource retries, and
    // completion. The fake clock makes duplicate accounting observable exactly.
    Renderer renderer;
    PresentationJobTiming job; TestClock::Set(0ms);job.enqueued=TestClock::now();
    renderer._sharedPresentIntervalNs[0]=10'000'000;
    renderer._presentationClock.Presented(TestClock::now(),TestClock::now(),10ms);
    for (int i=0;i<10;++i) {
        TestClock::Set(std::chrono::milliseconds(i));
        assert(renderer.RenderDLSSFGFrame(0,2,job)==DLSSFGFrameRenderResult::Retry);
        assert(renderer._frontendPacingDeadline==std::chrono::steady_clock::time_point(10ms));
        assert(renderer.records==0 && renderer._pendingDLSSFGFrontendFrames==1);
    }
    TestClock::Set(11ms);renderer.next=Renderer::Next::Capacity;
    assert(renderer.RenderDLSSFGFrame(0,2,job)==DLSSFGFrameRenderResult::Retry);
    TestClock::Set(15ms);renderer.next=Renderer::Next::Resource;
    assert(renderer.RenderDLSSFGFrame(0,2,job)==DLSSFGFrameRenderResult::Retry);
    TestClock::Set(20ms);renderer.next=Renderer::Next::Present;
    assert(renderer.RenderDLSSFGFrame(0,2,job)==DLSSFGFrameRenderResult::Presented);
    assert(renderer.recorded.deadline==11ms && renderer.recorded.capacity==3ms && renderer.recorded.resource==4ms);
    assert(renderer.recorded.cpu==3ms && renderer.recorded.beginFrame==3ms);
    assert(renderer.records==1 && !renderer.lastDropped && renderer._pendingDLSSFGFrontendFrames==0);
    assert(renderer._sharedTextureAvailableEvents[0].signals==1 && renderer._frameSyncConsumedEvent.signals==1);
    assert(!renderer._frontendPacingDeadline);

    // Stale generations never decrement or release the current ring. A dropped
    // current image does release it; both terminate their own timing sample.
    Renderer resized; PresentationJobTiming stale;
    stale.Retry(PresentationJobTiming::Wait::Deadline,TestClock::now());TestClock::value+=7ms;
    assert(resized.RenderDLSSFGFrame(0,1,stale)==DLSSFGFrameRenderResult::Dropped);
    assert(resized._pendingDLSSFGFrontendFrames==1 && resized._sharedTextureAvailableEvents[0].signals==0);
    assert(resized.recorded.deadline==7ms && resized.lastDropped);
    PresentationJobTiming fresh;resized.next=Renderer::Next::Drop;
    assert(resized.RenderDLSSFGFrame(0,2,fresh)==DLSSFGFrameRenderResult::Dropped);
    assert(resized._pendingDLSSFGFrontendFrames==0 && resized._sharedTextureAvailableEvents[0].signals==1);
    assert(resized.recorded.deadline==0ns && resized.lastDropped);
    Renderer stopping;stopping._synchronousFramePresentationEnabled=false;
    assert(stopping.RenderDLSSFGFrame(0,2,fresh)==DLSSFGFrameRenderResult::Dropped);
    assert(stopping._pendingDLSSFGFrontendFrames==0 && stopping._sharedTextureAvailableEvents[0].signals==1);

    // Multipliers retain their bounded ring. Full means wait before capture;
    // freeing one slot permits a new input without claiming a resource permit.
    for (uint32_t multiplier=2;multiplier<=4;++multiplier) {
        Renderer pressure;pressure._sharedTextureSlotCount=multiplier;
        pressure._pendingDLSSFGFrontendFrames=multiplier;
        assert(pressure._IsDLSSFGQueueFull());
        --pressure._pendingDLSSFGFrontendFrames;assert(!pressure._IsDLSSFGQueueFull());
        pressure._pendingDLSSFGFrontendFrames=multiplier;
        pressure._synchronousFramePresentationEnabled=false;assert(!pressure._IsDLSSFGQueueFull());
        pressure._synchronousFramePresentationEnabled=true;
        pressure._dlssFrameGenerator=false;assert(!pressure._IsDLSSFGQueueFull());
    }
    std::cout<<"PASS: production FIFO retry accounting, deadline wake, capacity/resource distinction, stale generations, drops, slot release and x2/x3/x4 capture backpressure\n";
}
'''
out = Path(sys.argv[1]) / 'beta6_presentation.cpp'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(prefix + pressure + '\n};\n' + production + suffix, encoding='utf-8')
print(out)
