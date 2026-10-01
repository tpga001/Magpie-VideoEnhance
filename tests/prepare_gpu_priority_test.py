"""Extract the production priority guard; replace only OS calls and its clock."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
s = (repo / 'src/Magpie.Core/Renderer.cpp').read_text(encoding='utf-8-sig')
start = s.index('void Renderer::_EnsureGpuPriority(')
brace = s.index('{', start)
end, depth = brace+1, 1
while depth:
    depth += (s[end]=='{') - (s[end]=='}')
    end += 1
method = s[start:end].replace('std::chrono::steady_clock::now()', 'testNow')

prefix = r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include <cassert>
#include <chrono>
#include <deque>
#include <iostream>
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0)
#endif
static std::chrono::steady_clock::time_point testNow{};
static D3DKMT_SCHEDULINGPRIORITYCLASS priority = D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL;
static int reads=0, writes=0, infos=0, errors=0;
static bool writeApplies=true;
static NTSTATUS writeStatus=0;
static std::deque<NTSTATUS> readStatuses;
NTSTATUS TestRead(HANDLE, D3DKMT_SCHEDULINGPRIORITYCLASS* out) {
  ++reads;
  NTSTATUS status=0;
  if (!readStatuses.empty()) { status=readStatuses.front(); readStatuses.pop_front(); }
  if (!status) *out=priority;
  return status;
}
NTSTATUS TestWrite(HANDLE, D3DKMT_SCHEDULINGPRIORITYCLASS value) {
  ++writes;
  assert(value == D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME);
  if (!writeStatus && writeApplies) priority=value;
  return writeStatus;
}
#define D3DKMTGetProcessSchedulingPriorityClass TestRead
#define D3DKMTSetProcessSchedulingPriorityClass TestWrite
namespace fmt { template<class... T> int format(T...) { return 0; } }
struct Logger {
  static Logger& Get() { static Logger l; return l; }
  template<class T> void Info(T) { ++infos; }
  template<class T> void Error(T) { ++errors; }
  void NTError(const char*, NTSTATUS) { ++errors; }
};
struct Renderer {
  std::chrono::steady_clock::time_point _nextGpuPriorityCheck{};
  bool _gpuPriorityVerified=false, _gpuPriorityFailureLogged=false;
  void _EnsureGpuPriority(bool force=false) noexcept;
};
'''
tests = r'''
int main() {
  using namespace std::chrono_literals;
  Renderer r;
  r._EnsureGpuPriority(true);
  assert(priority==D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME && reads==2 && writes==1 && r._gpuPriorityVerified);
  // Normal frame loops do no kernel work until the next one-second check.
  for(int i=0;i<1000;++i) r._EnsureGpuPriority();
  assert(reads==2 && writes==1);
  priority=D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH;
  testNow+=1s; r._EnsureGpuPriority();
  assert(priority==D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME && writes==2 && r._gpuPriorityVerified);
  // Resource recreation checks immediately, even within the same interval.
  priority=D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL; r._EnsureGpuPriority(true);
  assert(writes==3 && r._gpuPriorityVerified);
  // A denied request is not reported as verified and does not spam errors.
  priority=D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL; writeStatus=-1;
  r._EnsureGpuPriority(true); assert(!r._gpuPriorityVerified && errors==1);
  testNow+=1s; r._EnsureGpuPriority(); assert(errors==1);
  writeStatus=0; testNow+=1s; r._EnsureGpuPriority();
  assert(r._gpuPriorityVerified && !r._gpuPriorityFailureLogged);
  // An OS success without the requested readback is also a failure.
  priority=D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL; writeApplies=false;
  r._EnsureGpuPriority(true); assert(!r._gpuPriorityVerified && errors==2);
  writeApplies=true; r._EnsureGpuPriority(true); assert(r._gpuPriorityVerified);
  // Failed initial query can be repaired; failed verification remains unknown.
  readStatuses={-1,0}; r._EnsureGpuPriority(true); assert(r._gpuPriorityVerified);
  readStatuses={-1,-1}; r._EnsureGpuPriority(true); assert(!r._gpuPriorityVerified && errors==3);
  r._EnsureGpuPriority(true); assert(r._gpuPriorityVerified);
  const int written=writes; r._EnsureGpuPriority(true); assert(writes==written);
  std::cout << "PASS: REALTIME-only writes, readback, periodic drift repair, forced rebuild checks, denied/mismatched/unknown status and recovery\n";
}
'''
(out/'gpu_priority.cpp').write_text(prefix+method+tests, encoding='utf-8')
print('Extracted production GPU priority guard')
