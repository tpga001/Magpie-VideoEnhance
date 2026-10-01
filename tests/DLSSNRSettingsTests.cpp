#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include "DLSSNRParameters.h"
#include "EffectParameterRestart.h"

namespace Magpie {
struct MotionVectorRequest { int method = 0; };
struct DlssnrExperimentProtocol { bool enabled = false; float scale = 1; };
struct EffectOption { std::map<std::string, float> parameters; };
inline MotionVectorRequest ParseDlssOpticalFlowRequest(const EffectOption& option) {
	const auto it = option.parameters.find("opticalFlowMethod");
	return {it == option.parameters.end() ? 0 : static_cast<int>(it->second)};
}
#include "DLSSNRSettingsUnderTest.h"
}

int main() {
	using namespace Magpie;
	EffectOption option{{{"style", 2.f}, {"intensity", 0.5f}, {"localToneStrength", 0.25f},
		{"localStructureStrength", 0.75f}, {"skinStructureStrength", 2.f},
		{"useAutoMask", 1.f}, {"uiCorrection", 1.f}, {"opticalFlowMethod", 2.f},
		{"enableInputResolutionScaling", 1.f}, {"inputResolutionPercent", 75.f}}};
	const DLSSNRSettings defaults;
	for (int pass : {2, 3}) {
		const auto fresh = ParseDLSSNRSettings(DLSSNRPassOption(option, pass), true);
		assert(fresh.style == defaults.style && fresh.intensity == defaults.intensity);
		assert(fresh.localToneStrength == defaults.localToneStrength);
		assert(fresh.localStructureStrength == defaults.localStructureStrength);
		assert(fresh.skinStructureStrength == defaults.skinStructureStrength);
		assert(fresh.useAutoMask == defaults.useAutoMask && fresh.uiCorrection == defaults.uiCorrection);
		assert(fresh.motionRequest.method == 2 && fresh.experimentalHdr.enabled);
		assert(fresh.enableInputResolutionScaling && fresh.inputResolutionPercent == 75);
	}
	option.parameters["pass2_style"] = 1;
	option.parameters["pass2_intensity"] = 1.75f;
	option.parameters["pass3_skinStructureStrength"] = 1.5f;
	const auto first = ParseDLSSNRSettings(DLSSNRPassOption(option, 1));
	const auto second = ParseDLSSNRSettings(DLSSNRPassOption(option, 2));
	const auto third = ParseDLSSNRSettings(DLSSNRPassOption(option, 3));
	assert(first.style == 2 && first.intensity == 0.5f && first.skinStructureStrength == 2);
	assert(second.style == 1 && second.intensity == 1.75f && second.skinStructureStrength == 0);
	assert(third.style == 0 && third.intensity == 1 && third.skinStructureStrength == 1.5f);
	// Restoring one parameter cannot change the other two passes.
	option.parameters["pass2_intensity"] = defaults.intensity;
	assert(ParseDLSSNRSettings(DLSSNRPassOption(option, 2)).intensity == 1);
	assert(ParseDLSSNRSettings(DLSSNRPassOption(option, 1)).intensity == 0.5f);
	assert(ParseDLSSNRSettings(DLSSNRPassOption(option, 3)).skinStructureStrength == 1.5f);
	EffectParameterRestartQueue queue;
	const auto now = EffectParameterRestartQueue::Clock::now();
	assert(queue.Update(0, "multiPass", 2, 1, now));
	assert(!queue.ReadyToStop(now, false));
	assert(queue.ReadyToStop(now + std::chrono::milliseconds(300), false));
	assert(!queue.ReadyToStop(now + std::chrono::milliseconds(300), true));
	const auto changes = queue.TakeChanges();
	assert(changes.size() == 1 && changes.front().parameter == "multiPass");
	queue.WaitAfterStop(now);
	assert(!queue.ReadyToStart(now + std::chrono::milliseconds(499)));
	assert(queue.ReadyToStart(now + std::chrono::milliseconds(500)));
	std::cout << "DLSSNR settings: actual parser defaults, independent pass values, shared settings, HDR and restart debounce passed.\n";
}
