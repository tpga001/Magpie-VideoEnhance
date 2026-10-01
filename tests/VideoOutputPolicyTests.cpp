#include "VideoOutputPolicy.h"
#include <cassert>
#include <iostream>

using namespace Magpie;

int main() {
	// 1080p video on a 4K display: run VSR.
	assert(ShouldRunVideoSuperResolution({ 1920, 1080 }, { 3840, 2160 }));

	// 1080p video on a 1080p display: keep native resolution and continue to FG.
	assert(!ShouldRunVideoSuperResolution({ 1920, 1080 }, { 1920, 1080 }));

	// A landscape video fitted to a portrait 1080-wide display is a downscale.
	assert(!ShouldRunVideoSuperResolution({ 1920, 1080 }, { 1080, 608 }));

	// Rounding can increase only one dimension; that is still an upscale route.
	assert(ShouldRunVideoSuperResolution({ 1920, 1080 }, { 1921, 1080 }));

	std::cout << "PASS: display-native video output policy\n";
}
