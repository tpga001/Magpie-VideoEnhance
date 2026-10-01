// Captured-frame approximation of SMAA 4x for Magpie.
// It cycles the official four net-jitter positions and subsample indices over
// four captured frames. A true SMAA 4x integration also requires 2x MSAA scene
// subsamples, which are unavailable from a captured desktop image.

//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME SMAA_5_4X_EXPERIMENTAL
//!USE _DYNAMIC

//!PARAMETER
//!LABEL History weight
//!DEFAULT 0.75
//!MIN 0.0
//!MAX 0.95
//!STEP 0.05
float historyWeight;

//!PARAMETER
//!LABEL History rejection
//!DEFAULT 8.0
//!MIN 0.0
//!MAX 32.0
//!STEP 0.5
float historyRejection;

//!TEXTURE
Texture2D INPUT;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D OUTPUT;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
//!FORMAT R8G8B8A8_UNORM
Texture2D jitteredTex;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
//!FORMAT R8G8_UNORM
Texture2D edgesTex;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
//!FORMAT R8G8B8A8_UNORM
Texture2D blendTex;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
//!FORMAT R8G8B8A8_UNORM
Texture2D currentTex;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
//!FORMAT R8G8B8A8_UNORM
Texture2D historyTex;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
//!FORMAT R8G8B8A8_UNORM
Texture2D historyNext;

//!TEXTURE
//!SOURCE AreaTex.dds
//!FORMAT R8G8B8A8_UNORM
Texture2D areaTex;

//!TEXTURE
//!SOURCE SearchTex.dds
//!FORMAT R8_UNORM
Texture2D searchTex;

//!SAMPLER
//!FILTER POINT
SamplerState PointSampler;

//!SAMPLER
//!FILTER LINEAR
SamplerState LinearSampler;

//!COMMON

#define SMAA_RT_METRICS float4(GetInputPt(), GetInputSize())
#define SMAA_LINEAR_SAMPLER LinearSampler
#define SMAA_POINT_SAMPLER PointSampler
#define SMAA_PRESET_ULTRA
#include "SMAA.hlsli"

float2 TemporalJitterPixels() {
	// Official SMAA 4x net-jitter table converted to top-to-bottom texture Y.
	switch (GetFrameCount() & 3) {
	case 0: return float2(0.375, 0.125);
	case 1: return float2(-0.125, -0.375);
	case 2: return float2(0.125, 0.375);
	default: return float2(-0.375, -0.125);
	}
}

float4 TemporalSubsampleIndices() {
	switch (GetFrameCount() & 3) {
	case 0: return float4(5, 3, 1, 3);
	case 1: return float4(4, 6, 2, 3);
	case 2: return float4(3, 5, 1, 4);
	default: return float4(6, 4, 2, 4);
	}
}

//!PASS 1
//!DESC Captured-frame subpixel shift
//!STYLE PS
//!IN INPUT
//!OUT jitteredTex

float4 Pass1(float2 pos) {
	return INPUT.SampleLevel(LinearSampler, pos + TemporalJitterPixels() * GetInputPt(), 0);
}

//!PASS 2
//!DESC Luma edge detection
//!STYLE PS
//!IN jitteredTex
//!OUT edgesTex

float2 Pass2(float2 pos) {
	return SMAALumaEdgeDetectionPS(pos, jitteredTex);
}

//!PASS 3
//!DESC Four-phase blending weights
//!STYLE PS
//!IN edgesTex, areaTex, searchTex
//!OUT blendTex

float4 Pass3(float2 pos) {
	return SMAABlendingWeightCalculationPS(
		pos, edgesTex, areaTex, searchTex, TemporalSubsampleIndices());
}

//!PASS 4
//!DESC Neighborhood blending
//!STYLE PS
//!IN jitteredTex, blendTex
//!OUT currentTex

float4 Pass4(float2 pos) {
	return SMAANeighborhoodBlendingPS(pos, jitteredTex, blendTex);
}

//!PASS 5
//!DESC Clamped temporal resolve
//!STYLE PS
//!IN currentTex, historyTex
//!OUT historyNext

float4 Pass5(float2 pos) {
	const float2 pt = GetInputPt();
	float4 current = currentTex.SampleLevel(PointSampler, pos, 0);
	float3 neighborhoodMin = current.rgb;
	float3 neighborhoodMax = current.rgb;

	[unroll]
	for (int y = -1; y <= 1; ++y) {
		[unroll]
		for (int x = -1; x <= 1; ++x) {
			float3 sampleColor = currentTex.SampleLevel(
				PointSampler, pos + float2(x, y) * pt, 0).rgb;
			neighborhoodMin = min(neighborhoodMin, sampleColor);
			neighborhoodMax = max(neighborhoodMax, sampleColor);
		}
	}

	float4 history = historyTex.SampleLevel(PointSampler, pos, 0);
	history.rgb = clamp(history.rgb, neighborhoodMin, neighborhoodMax);
	float3 difference = abs(history.rgb - current.rgb);
	float maxDifference = max(difference.r, max(difference.g, difference.b));
	float confidence = saturate(1.0 - maxDifference * historyRejection);
	float validHistory = step(0.5, history.a);
	float weight = historyWeight * confidence * validHistory;
	float3 resolved = lerp(current.rgb, history.rgb, weight);

	return float4(resolved, 1.0);
}

//!PASS 6
//!DESC Store history
//!STYLE PS
//!IN historyNext
//!OUT historyTex

float4 Pass6(float2 pos) {
	return historyNext.SampleLevel(PointSampler, pos, 0);
}

//!PASS 7
//!DESC Output
//!STYLE PS
//!IN historyNext
//!OUT OUTPUT

float4 Pass7(float2 pos) {
	return historyNext.SampleLevel(PointSampler, pos, 0);
}
