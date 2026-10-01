"""Extract the production WGC frame validation/copy branch into a CPU harness.

Usage: python scripts/tests/test_capture_duplicate_timestamp.py OUTPUT [SOURCE]
Then compile OUTPUT/capture_timestamp.cpp with cl /std:c++20 /EHsc and run it.
GPU copy, frame handles and recovery notifications are fakes; the production
branch is copied unchanged. SOURCE optionally selects a pre-fix revision.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
source = Path(sys.argv[2]) if len(sys.argv) > 2 else root / 'src/Magpie.Core/GraphicsCaptureFrameSource.cpp'
capture = source.read_text(encoding='utf-8-sig')
start = capture.index('\t\t\tconst bool valid = content.Width')
end = capture.index('\n\t\t}\n\n\t\t// Only an explicitly interrupted sequence', start)
branch = capture[start:end]
recovery_start = capture.index('\t\t// Only an explicitly interrupted sequence', end)
recovery_end = capture.index('\n\t} catch (', recovery_start)
recovery = capture[recovery_start:recovery_end]

harness = r'''
#include <windows.h>
#include <d3d11.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
using namespace std::chrono_literals;
enum class FrameSourceState { Waiting, NewFrame, Error };
namespace FrameTrace {
enum class Event { WgcRejected };
template<class... T> void Mark(T...) {}
}
namespace fmt { template<class... T> int format(T...) { return 0; } }
struct Logger {
    static Logger& Get() { static Logger value; return value; }
    void Warn(int) {} void Info(int) {}
};
struct ScalingWindow {
    static ScalingWindow& Get() { static ScalingWindow value; return value; }
    HWND Handle() const { return nullptr; }
};
struct CommonSharedConstants { static constexpr UINT WM_FRONTEND_RENDER = WM_USER + 1; };
// No windows or notifications are created by this CPU test.
bool TestPostMessage(HWND, UINT, int, int) { return true; }
#undef PostMessage
#define PostMessage TestPostMessage
struct Resource { void* get() const { return nullptr; } };
struct Device {
    int copies = 0;
    Device* GetD3DDC() { return this; }
    template<class... T> void CopySubresourceRegion(T...) { ++copies; }
};
struct Source {
    D3D11_BOX _frameBox{0,0,0,2560,1440,1};
    uint64_t _captureSessionGeneration=1, _captureSequence=1, _rejectedFrames=0;
    int64_t _lastFrameTimestamp100ns=0, _captureTimestamp100ns=0;
    bool _captureInterrupted=false;
    bool _recoveryTimer=true;
    HRESULT _captureErrorCode=E_FAIL;
    int interruptions=0, recoveries=0, closed=0;
    std::chrono::steady_clock::time_point _recoveryStarted{}, _lastRecoveryGeometryCheck{};
    Device device;
    Device* _deviceResources=&device;
    Resource _output;
    void _InterruptCapture(const char*) {
        if (_captureInterrupted) return;
        _captureInterrupted=true; ++_captureSequence; ++interruptions; _rejectedFrames=0;
        _recoveryStarted=std::chrono::steady_clock::now();
    }
    void _FinishRecovery() { _captureInterrupted=false; ++recoveries; }
    FrameSourceState _FailCapture(const char*, HRESULT) { return FrameSourceState::Error; }
    FrameSourceState Feed(int64_t timestamp, int width=2560, int height=1440) {
        struct { int Width, Height; } content{width,height};
        D3D11_TEXTURE2D_DESC desc{}; desc.Width=2560; desc.Height=1440;
        struct Frame { int& closed; void Close() { ++closed; } } frame{closed};
        Resource texture;
BRANCH
RECOVERY
    }
};
static void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    using S=FrameSourceState;
    Source source;
    Check(source.Feed(1000000)==S::NewFrame, "first frame was not accepted");
    for (int i=0; i<629; ++i) {
        Check(source.Feed(1000000)==S::Waiting, "duplicate published as new input");
        Check(source.interruptions==0 && source._captureSequence==1,
            "duplicate timestamp reset capture history");
        Check(source.device.copies==1 && source._captureTimestamp100ns==1000000,
            "duplicate changed the pipeline texture or timestamp");
    }
    Check(source.closed==630, "duplicate capture frame handle leaked");
    Check(source.Feed(1166667)==S::NewFrame && source.interruptions==0,
        "new frame after duplicates failed to preserve history");
    Check(source.Feed(1100000)==S::Waiting && source.interruptions==1,
        "backwards timestamp no longer interrupts capture");
    Check(source.Feed(1166667)==S::Waiting && source._captureInterrupted && source.recoveries==0,
        "duplicate incorrectly completed an existing recovery");
    Check(source.Feed(1333334)==S::NewFrame && !source._captureInterrupted && source.recoveries==1,
        "advancing frame did not recover");
    Check(source.Feed(1333334,0)==S::Waiting && source.interruptions==2,
        "equal timestamp hid invalid content bounds");
    Check(source.Feed(1500001)==S::NewFrame && source.recoveries==2, "bounds recovery failed");
    Check(source.Feed(0)==S::Waiting && source.interruptions==3, "zero timestamp accepted");
    Check(source.Feed(1666668)==S::NewFrame && source.recoveries==3, "timestamp recovery failed");
    Check(source.Feed(60000000)==S::NewFrame && source.interruptions==4 && source.recoveries==4,
        "long-pause recovery changed");
    source._recoveryStarted=std::chrono::steady_clock::now()-6s;
    Check(source.Feed(60000000)==S::Waiting, "healthy static input incorrectly timed out");
    source._InterruptCapture("test restart");
    source._recoveryStarted=std::chrono::steady_clock::now()-6s;
    Check(source.Feed(60000000)==S::Error, "duplicate bypassed an active recovery timeout");
    source._recoveryStarted=std::chrono::steady_clock::now();
    source._recoveryTimer=false;
    Check(source.Feed(60000000)==S::Error, "duplicate hid recovery timer failure");
    std::cout << "WGC production branch: 629 duplicates preserve history; backwards/zero timestamps, invalid bounds and recovery passed.\n";
}
'''.replace('BRANCH', branch).replace('RECOVERY', recovery)
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
(output / 'capture_timestamp.cpp').write_text(harness, encoding='utf-8')
print(output / 'capture_timestamp.cpp')
