#include "FramePacingOptions.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace Magpie;

int main() {
	FrameSyncSettings defaults;
	assert(defaults.enabled && defaults.frameRate == 60 && defaults.mode == FrameSyncMode::FrontEdge);
	for (auto mode : { FrameSyncMode::FrontEdge, FrameSyncMode::Async, FrameSyncMode::Reflex }) {
		const FrameSyncSettings selected{ true, 80, mode };
		assert(ResolveFrameSyncBackend(selected, false, true, true, false) == FrameSyncBackend::XeLL);
		assert(ResolveFrameSyncBackend(selected, false, false, true, true) == FrameSyncBackend::None);
		assert(ResolveFrameSyncBackend({false,80,mode}, true, false, true, false) == FrameSyncBackend::None);
	}
	assert(ResolveFrameSyncBackend({true,80,FrameSyncMode::Async}, false,false,false,false) == FrameSyncBackend::Async);
	assert(ResolveFrameSyncBackend({true,80,FrameSyncMode::Reflex}, false,false,true,false) == FrameSyncBackend::Reflex);
	assert(ResolveFrameSyncBackend({true,80,FrameSyncMode::Reflex}, true,false,true,false) == FrameSyncBackend::Reflex);
	assert(ResolveFrameSyncBackend({true,80,FrameSyncMode::Async}, true,false,true,false) == FrameSyncBackend::Async);
	assert(ResolveFrameSyncBackend(defaults, false,false,false,false) == FrameSyncBackend::None);
	FrameSyncSettings current{ true, 90, FrameSyncMode::FrontEdge };
	const FrameSyncSettings old{ true, 80, FrameSyncMode::FrontEdge };
	assert(MergeFrameSyncSettings(current, old, {true,80,FrameSyncMode::Async}));
	assert(current.frameRate == 90 && current.mode == FrameSyncMode::Async);
	const auto preserved = current;
	assert(!MergeFrameSyncSettings(current, old, {false,100,FrameSyncMode::Reflex}));
	assert(current == preserved);
	assert(!MergeFrameSyncSettings(current, current, {true,90,static_cast<FrameSyncMode>(99)}));
	assert(current == preserved);
	assert(!MergeFrameSyncSettings(current, current, {true,std::numeric_limits<float>::quiet_NaN(),FrameSyncMode::Async}));
	assert(MergeFrameSyncSettings(current, current, {true,0,FrameSyncMode::Reflex}));
	assert(current.frameRate == 0 && current.mode == FrameSyncMode::Reflex);
	assert(FrameSyncIntervalUs(80) == 12500 && FrameSyncIntervalUs(60) == 16667);
	assert(FrameSyncIntervalUs(0) == 0 && FrameSyncIntervalUs(std::numeric_limits<double>::infinity()) == 0);
	assert(ResolvePresentationFrameRate(0,0,240,3) == 80);
	assert(ResolvePresentationFrameRate(90,60,240,3) == 60);
	assert(ResolveFrameRateFilterTarget(true,1,30,80,240,2) == 80);
	assert(ResolveFrameRateFilterTarget(false,1,30,80,240,2) == 30);
	std::cout << "PASS: pacing policy, FG ownership, old defaults, concurrent three-field merge, automatic targets and Reflex intervals\n";
}
