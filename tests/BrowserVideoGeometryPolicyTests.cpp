#include "BrowserVideoGeometryPolicy.h"

#include <cassert>
#include <iostream>

using namespace Magpie;

namespace {

BrowserVideoGeometrySample Sample(
	SIZE overlay,
	SIZE frame,
	SIZE processing,
	std::array<float, 4> cropping = {}) {
	return {
		.overlaySize = overlay,
		.frameSize = frame,
		.processingSize = processing,
		.cropping = cropping
	};
}

}

int main() {
	const auto landscape = Sample(
		{ 3519, 1975 }, { 1920, 1080 }, { 3840, 2160 }, { 114, 181, 114, 4 });

	// DOM/DWM rounding by one pixel must not rebuild an otherwise unchanged run.
	assert(CanReuseBrowserVideoGeometry(landscape, Sample(
		{ 3518, 1975 }, { 1920, 1080 }, { 3840, 2160 }, { 114, 181, 113, 4 })));

	// A two-pixel decoded crop can change the fitted output by four pixels on a
	// 2x display. Both still belong to the same processing-size tier.
	assert(CanReuseBrowserVideoGeometry(landscape, Sample(
		{ 3518, 1975 }, { 1920, 1078 }, { 3840, 2156 }, { 114, 181, 113, 4 })));

	// Portrait video exercises the other fitted dimension.
	assert(CanReuseBrowserVideoGeometry(
		Sample({ 1113, 1977 }, { 1080, 1920 }, { 1214, 2160 }, { 1317, 181, 1317, 2 }),
		Sample({ 1113, 1976 }, { 1080, 1918 }, { 1216, 2160 }, { 1317, 181, 1317, 3 })));

	// Real window/video resizes must still rebuild even when the display-native
	// processing output happens to remain 4K.
	assert(!CanReuseBrowserVideoGeometry(landscape, Sample(
		{ 3168, 1782 }, { 1920, 1080 }, { 3840, 2160 }, { 579, 286, 0, 92 })));
	assert(!CanReuseBrowserVideoGeometry(landscape, Sample(
		{ 3519, 1975 }, { 1280, 720 }, { 3840, 2160 }, { 114, 181, 114, 4 })));

	// A monitor/output tier change and a crop movement outside the jitter window
	// are both observable geometry changes.
	assert(!CanReuseBrowserVideoGeometry(landscape, Sample(
		{ 3519, 1975 }, { 1920, 1080 }, { 2560, 1440 }, { 114, 181, 114, 4 })));
	assert(!CanReuseBrowserVideoGeometry(landscape, Sample(
		{ 3519, 1975 }, { 1920, 1080 }, { 3840, 2160 }, { 117, 181, 114, 4 })));

	// The decoded-frame reader uses the same boundary as the service policy.
	assert(IsBrowserVideoSizeWithinJitter({ 1920, 1080 }, { 1920, 1078 }));
	assert(!IsBrowserVideoSizeWithinJitter({ 1920, 1080 }, { 1920, 1077 }));

	std::cout << "PASS: browser video geometry jitter reuse and real resize boundaries\n";
}
