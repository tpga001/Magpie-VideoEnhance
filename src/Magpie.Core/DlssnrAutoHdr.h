#pragma once
#include "HdrFrame.h"

namespace Magpie {

// Canonical pixels have already been decoded to linear scRGB, including SDR
// embedded at the display's SDR white. Do not infer this from FP16 storage.
inline bool UseDlssnrAutoHdr(bool hdrEnabled, bool fp16Supported, const HdrFrameMetadata &input) noexcept {
	return hdrEnabled && fp16Supported && input.IsValid() && input.stage == HdrFrameStage::CanonicalInput;
}

} // namespace Magpie
