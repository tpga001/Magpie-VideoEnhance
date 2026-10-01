"""Generate a CPU regression from the production capture duplicate-filter branch.

Usage: python scripts/tests/test_fg_duplicate_filter.py OUTPUT
Compile OUTPUT/fg_duplicate.cpp with cl /std:c++20 /EHsc, then run it.
GPU comparisons are faked; filter selection and sequence handling are production code.
"""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
source = (root / 'src/Magpie.Core/FrameSourceBase.cpp').read_text(encoding='utf-8-sig')
branch = source[source.index('\tconst bool newSequence ='):source.index('\nColorDescription FrameSourceBase::')]
branch = branch[:branch.rindex('\n}')]

for family, effect in [('DLSSFG', 'DLSS_FrameGeneration'), ('XeSSFG', 'XeSS_FrameGeneration')]:
    shader = (root / f'src/Effects/{family}/{effect}.hlsl').read_text(encoding='utf-8-sig')
    names = re.findall(r'^int (\w+);', shader, re.M)
    assert names[:3] == ['multiplier', 'duplicateFrameFiltering', 'opticalFlowMethod']
    block = next(part for part in shader.split('//!PARAMETER') if 'int duplicateFrameFiltering;' in part)
    assert all(f'//!{key} {value}' in block for key, value in [('DEFAULT', 1), ('MIN', 0), ('MAX', 1), ('STEP', 1)])
    for language in ['en-US', 'zh-Hans', 'zh-Hant']:
        resource = ET.parse(root / f'src/Magpie/Resources.language-{language}.resw')
        key = f'EffectParam_{family}_{effect}_duplicateFrameFiltering_Label'
        assert len(resource.findall(f'./data[@name="{key}"]')) == 1

harness = r'''
#include <optional>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <cstdlib>
enum class FrameSourceState { NewFrame, Waiting, Error };
enum class DuplicateFrameDetectionMode { Always, Dynamic, Never };
struct ScalingOptions {
    DuplicateFrameDetectionMode duplicateFrameDetectionMode = DuplicateFrameDetectionMode::Dynamic;
    bool game = false;
    bool Is3DGameMode() const { return game; }
    bool IsStatisticsForDynamicDetectionEnabled() const { return false; }
};
struct ScalingWindow {
    ScalingOptions options;
    static ScalingWindow& Get() { static ScalingWindow window; return window; }
    const ScalingOptions& Options() { return options; }
};
struct Resource {
    bool valid = false;
    void* get() { return nullptr; }
    explicit operator bool() const { return valid; }
    void operator=(std::nullptr_t) { valid = false; }
};
struct ID3D11DeviceContext4 {
    void CopyResource(void*, void*) { ++copies; }
    int copies = 0;
};
struct Device {
    ID3D11DeviceContext4 context;
    ID3D11DeviceContext4* GetD3DDC() { return &context; }
};
struct Logger {
    static Logger& Get() { static Logger logger; return logger; }
    void Error(const char*) {}
};
constexpr uint16_t INITIAL_CHECK_COUNT=16, INITIAL_SKIP_COUNT=1, MAX_SKIP_COUNT=16;
struct Capture {
    uint64_t _duplicateCaptureSequence=0, _captureSequence=1;
    bool _isCheckingForDuplicateFrame=true;
    uint32_t _framesLeft=16, _nextSkipCount=1;
    std::optional<bool> _duplicateFrameDetectionOverride;
    Resource _prevFrame, _prevFrameSrv, _output;
    Device device;
    Device* _deviceResources=&device;
    std::atomic<std::pair<uint32_t,uint32_t>> _statistics{{0,0}};
    int checks=0, initializations=0;
    bool duplicate=true;
    bool _InitCheckingForDuplicateFrame() { ++initializations; _prevFrame.valid=true; return true; }
    bool _IsDuplicateFrame() { ++checks; return duplicate; }
    FrameSourceState Feed(FrameSourceState state=FrameSourceState::NewFrame) {
BRANCH
    }
};
static void Check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    using State = FrameSourceState;
    auto& options=ScalingWindow::Get().options;
    for (auto mode : {DuplicateFrameDetectionMode::Always, DuplicateFrameDetectionMode::Dynamic, DuplicateFrameDetectionMode::Never}) {
        for (bool game : {false,true}) {
            options.duplicateFrameDetectionMode=mode; options.game=game;
            Capture disabled; disabled._duplicateFrameDetectionOverride=false;
            for (int i=0;i<40;++i) Check(disabled.Feed()==State::NewFrame,"FG off dropped an input frame");
            Check(disabled.checks==0 && disabled.initializations==0,"FG off still accessed GPU duplicate detection");
            Capture enabled; enabled._duplicateFrameDetectionOverride=true;
            Check(enabled.Feed()==State::NewFrame,"first input was lost");
            for (int i=0;i<40;++i) Check(enabled.Feed()==State::Waiting,"FG on did not filter duplicates");
            enabled.duplicate=false;
            Check(enabled.Feed()==State::NewFrame,"FG on dropped changed pixels");
            enabled.duplicate=true; ++enabled._captureSequence;
            Check(enabled.Feed()==State::NewFrame,"capture restart failed to publish first frame");
            Check(enabled.Feed()==State::Waiting,"filter did not resume after restart");
            Capture normal;
            Check(normal.Feed()==State::NewFrame,"non-FG first input was lost");
            const bool globalFilters=!game && mode!=DuplicateFrameDetectionMode::Never;
            Check(normal.Feed()==(globalFilters ? State::Waiting : State::NewFrame),"non-FG global behavior changed");
            Check(enabled.Feed(State::Waiting)==State::Waiting && enabled.Feed(State::Error)==State::Error,
                "capture waiting/error state changed");
        }
    }
    std::cout << "FG duplicate filter: on/off overrides all global modes and game mode; non-FG policy, changed pixels and capture restart passed.\n";
}
'''.replace('BRANCH', branch)
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
(output / 'fg_duplicate.cpp').write_text(harness, encoding='utf-8')
print(output / 'fg_duplicate.cpp')
