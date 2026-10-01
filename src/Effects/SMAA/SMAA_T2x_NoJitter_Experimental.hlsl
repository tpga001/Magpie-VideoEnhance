// Captured-frame, no-jitter approximation of SMAA T2x for Magpie.
// It retains the official temporal subsample indices and history accumulation,
// but does not shift the captured image. This is not native SMAA T2x.

//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME SMAA_6_T2X_NO_JITTER_EXPERIMENTAL
//!USE _DYNAMIC

//!PARAMETER
//!LABEL History weight
//!DEFAULT 0.5
//!MIN 0.0
//!MAX 0.9
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

float4 TemporalSubsampleIndices() {
	return (GetFrameCount() & 1) == 0
		? float4(1, 1, 1, 0)
		: float4(2, 2, 2, 0);
}

//!PASS 1
//!DESC Luma edge detection without jitter
//!STYLE PS
//!IN INPUT
//!OUT edgesTex

float2 Pass1(float2 pos) {
	return SMAALumaEdgeDetectionPS(pos, INPUT);
}

//!PASS 2
//!DESC Temporal blending weights
//!STYLE PS
//!IN edgesTex, areaTex, searchTex
//!OUT blendTex

float4 Pass2(float2 pos) {
	return SMAABlendingWeightCalculationPS(
		pos, edgesTex, areaTex, searchTex, TemporalSubsampleIndices());
}

//!PASS 3
//!DESC Neighborhood blending
//!STYLE PS
//!IN INPUT, blendTex
//!OUT currentTex

float4 Pass3(float2 pos) {
	return SMAANeighborhoodBlendingPS(pos, INPUT, blendTex);
}

//!PASS 4
//!DESC Clamped temporal resolve
//!STYLE PS
//!IN currentTex, historyTex
//!OUT historyNext

float4 Pass4(float2 pos) {
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

	return float4(lerp(current.rgb, history.rgb, weight), 1.0);
}

//!PASS 5
//!DESC Store history
//!STYLE PS
//!IN historyNext
//!OUT historyTex

float4 Pass5(float2 pos) {
	return historyNext.SampleLevel(PointSampler, pos, 0);
}

//!PASS 6
//!DESC Output
//!STYLE PS
//!IN historyNext
//!OUT OUTPUT

float4 Pass6(float2 pos) {
	return historyNext.SampleLevel(PointSampler, pos, 0);
}
