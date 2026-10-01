#pragma once
#include <cstdint>

namespace Magpie {

struct VideoExtent {
	uint32_t width = 0;
	uint32_t height = 0;
};

// Video super resolution is useful only when this stage increases at least one
// output dimension. Equal-size and downscale routes keep the source pixels and
// let later stages, such as frame generation, run without loading a VSR model.
constexpr bool ShouldRunVideoSuperResolution(
	VideoExtent input,
	VideoExtent output
) noexcept {
	return output.width > input.width || output.height > input.height;
}

}
