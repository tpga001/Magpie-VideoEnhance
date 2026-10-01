"""Exercise the production coroutine continuation and deferred toolbar stop with fake Win32 objects."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
core = root / 'src/Magpie.Core'
cursor = (core / 'CursorManager.cpp').read_text(encoding='utf-8-sig')
window = (core / 'ScalingWindow.h').read_text(encoding='utf-8-sig')
window_cpp = (core / 'ScalingWindow.cpp').read_text(encoding='utf-8-sig')
overlay = (core / 'OverlayDrawer.cpp').read_text(encoding='utf-8-sig')
runtime = (core / 'ScalingRuntime.cpp').read_text(encoding='utf-8-sig')
continuation = cursor.split('// Check the retained token before touching this;', 1)[1]
continuation = continuation[continuation.index('\n\tif (!lifetime->IsCurrent'):].split('\n}\n', 1)[0]
continuation = continuation.replace('co_return', 'return')
def method(source, name):
    start = source.index(name)
    end = source.index('\n\t}', start) + 3
    return source[start:end]
assert 'if (_lifetime->IsStopping()) return;' in cursor.split('void CursorManager::_UpdateCursorState()', 1)[1].split('\n}', 1)[0]
assert cursor.index('BeginShutdown();', cursor.index('CursorManager::~CursorManager')) < cursor.index('_ShowSystemCursor(true, true)')
assert 'const auto lifetime = _lifetime;' in cursor
assert 'RequestStop(ScalingWindow::RunId())' in overlay.split('OverlayHelper::SegoeIcons::Cancel, closeStr', 1)[1].split('ImGui::EndDisabled', 1)[0]
destroy = window_cpp.split('void ScalingWindow::Destroy()', 1)[1].split('\n}', 1)[0]
assert destroy.index('_cursorManager->BeginShutdown()') < destroy.index('base_type::Destroy()')
handler = window_cpp.split('LRESULT ScalingWindow::_MessageHandler', 1)[1]
assert handler.index('_cursorManager->BeginShutdown()') < handler.index('_renderer->MessageHandler')
assert handler.index('_stopRequested = false') < handler.index('_renderer->MessageHandler')
assert runtime.index('ProcessPendingStop()') < runtime.index('scalingWindow.ProcessPendingParameterRestart()')
harness = r'''
#include "ScalingSessionLifetime.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
using namespace Magpie;
struct POINT { int x, y; };
struct FrameTrace {
    enum class Event { HitTestComplete };
    static int Tick() { return 0; } static int Frame() { return 0; }
    template<class... T> static void Record(T...) {}
};
struct CursorManager;
struct ScalingWindow {
    static inline uint32_t runId = 10;
    static uint32_t RunId() { return runId; }
    static ScalingWindow& Get() { static ScalingWindow w; return w; }
    bool open = true, renderer = true, _isDestroying = false, _stopRequested = false;
    CursorManager* cursor = nullptr;
    int stops = 0;
    explicit operator bool() const { return open; }
    bool Handle() const { return open; }
    bool TryGetRenderer() const { return renderer; }
    CursorManager* TryGetCursorManager() const { return cursor; }
    void Stop() { ++stops; open = false; renderer = false; ++runId; }
    REQUEST_STOP
    PROCESS_STOP
};
struct CursorManager {
    std::shared_ptr<ScalingSessionLifetime> _lifetime = std::make_shared<ScalingSessionLifetime>(ScalingWindow::RunId());
    int _lastCompletedHitTestId = 0, _lastCompletedHitTestResult = 0, updates = 0;
    POINT _lastCompletedHitTestPos{};
    ~CursorManager() { _lifetime->RequestStop(); }
    void _UpdateCursorState() { ++updates; assert(ScalingWindow::Get().renderer); }
    std::function<void()> Completion(int id, int area) {
        const auto lifetime = _lifetime;
        return [this, lifetime, id, area, screenPos = POINT{5,6}, traceRequest = 0] {
            CONTINUATION
        };
    }
};
int main() {
    auto& w = ScalingWindow::Get();
    CursorManager current; w.cursor = &current;
    current.Completion(2, 1)(); assert(current.updates == 1);
    current.Completion(1, 2)(); assert(current.updates == 1); // out of order
    current.Completion(3, 1)(); assert(current.updates == 1); // unchanged hit
    w.renderer = false; current.Completion(4, 2)(); assert(current.updates == 1);
    w.renderer = true; w.open = false; current.Completion(4, 2)(); assert(current.updates == 1);
    w.open = true;
    current._lifetime->RequestStop(); current.Completion(5, 2)(); assert(current.updates == 1);
    // Cleanup-created callbacks must retain the old token even after RunId changes.
    ++w.runId; current.Completion(6, 2)(); assert(current.updates == 1);
    for (int i = 0; i < 1000; ++i) {
        auto old = std::make_unique<CursorManager>(); w.cursor = old.get();
        auto late = old->Completion(1, 1); old.reset();
        CursorManager successor; w.cursor = &successor;
        late(); assert(successor.updates == 0); // same ID/address reuse still cancelled
        successor.Completion(1, 1)(); assert(successor.updates == 1);
        ++w.runId;
    }
    w = {}; w.RequestStop(w.runId - 1); assert(!w.ProcessPendingStop());
    w.RequestStop(w.runId); w.RequestStop(w.runId); assert(w.stops == 0);
    assert(w.ProcessPendingStop() && w.stops == 1);
    assert(!w.ProcessPendingStop() && w.stops == 1);
    w = {}; w._isDestroying = true; w.RequestStop(w.runId); assert(!w.ProcessPendingStop());
    std::cout << "Cursor shutdown: real continuation guards, late/reordered callbacks, 1000 successor sessions, "
        "and deferred/idempotent toolbar stop passed.\n";
}
'''.replace('CONTINUATION', continuation).replace('REQUEST_STOP', method(window, 'void RequestStop(')).replace('PROCESS_STOP', method(window, 'bool ProcessPendingStop('))
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
(out / 'cursor_shutdown.cpp').write_text(harness, encoding='utf-8')
print(out / 'cursor_shutdown.cpp')
