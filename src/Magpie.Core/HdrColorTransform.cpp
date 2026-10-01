#include "pch.h"
#include "HdrColorTransform.h"

#include <algorithm>
#include <cmath>

namespace Magpie {

namespace {
constexpr float PQMaxNits = 10000.0f;
constexpr float PQM1 = 2610.0f / 16384.0f;
constexpr float PQM2 = 2523.0f / 32.0f;
constexpr float PQC1 = 3424.0f / 4096.0f;
constexpr float PQC2 = 2413.0f / 128.0f;
constexpr float PQC3 = 2392.0f / 128.0f;

float Clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }

float DecodePq(float value) noexcept {
    const float normalized = std::pow(Clamp01(value), 1.0f / PQM2);
    const float numerator = std::max(normalized - PQC1, 0.0f);
    const float denominator = PQC2 - PQC3 * normalized;
    return denominator > 0.0f ? std::pow(numerator / denominator, 1.0f / PQM1) * PQMaxNits : 0.0f;
}

float EncodePq(float value) noexcept {
    const float normalized = std::pow(std::max(value, 0.0f) / PQMaxNits, PQM1);
    return Clamp01(std::pow((PQC1 + PQC2 * normalized) / (1.0f + PQC3 * normalized), PQM2));
}

float DecodeHlg(float value) noexcept {
    constexpr float a = 0.17883277f;
    constexpr float b = 1.0f - 4.0f * a;
    const float c = 0.5f - a * std::log(4.0f * a);
    const float encoded = Clamp01(value);
    return encoded <= 0.5f ? (encoded * encoded) / 3.0f : (std::exp((encoded - c) / a) + b) / 12.0f;
}

float EncodeHlg(float value) noexcept {
    constexpr float a = 0.17883277f;
    constexpr float b = 1.0f - 4.0f * a;
    const float c = 0.5f - a * std::log(4.0f * a);
    const float linear = std::max(value, 0.0f);
    return Clamp01(linear <= 1.0f / 12.0f
        ? std::sqrt(3.0f * linear) : a * std::log(12.0f * linear - b) + c);
}
}

bool HdrTransformParameters::IsValid() const noexcept {
    return std::isfinite(exposure) && exposure > 0.0f &&
        std::isfinite(referenceWhiteNits) && referenceWhiteNits == 80.0f &&
        std::isfinite(sdrWhiteNits) && sdrWhiteNits > 0.0f &&
        std::isfinite(hdrPeakNits) && hdrPeakNits >= referenceWhiteNits &&
        std::isfinite(shoulder) && shoulder > 0.0f;
}

HdrTransformParameters HdrColorTransform::ForFrame(const ColorDescription& color) noexcept {
    HdrTransformParameters parameters;
    if (!color.IsValid()) return parameters;
    parameters.referenceWhiteNits = 80.0f;
    parameters.sdrWhiteNits = color.sdrWhiteNits;
    parameters.hdrPeakNits = std::max({
        color.displayPeakNits, 80.0f, color.sdrWhiteNits });
    parameters.exposure = color.isPreExposed ? color.preExposure : 1.0f;
    return parameters;
}

HdrTransformConstants HdrColorTransform::PrepareConstants(
    const HdrTransformParameters& parameters,
    HdrTransferFunction inputTransfer,
    HdrTransferFunction outputTransfer
) noexcept {
    const HdrTransformParameters valid = parameters.IsValid() ? parameters : HdrTransformParameters{};
    return { valid.exposure, 1.0f / valid.exposure, valid.sdrWhiteNits / valid.hdrPeakNits,
        valid.hdrPeakNits, valid.shoulder, static_cast<uint32_t>(inputTransfer),
        static_cast<uint32_t>(outputTransfer) };
}

float HdrColorTransform::DecodeTransfer(float value, HdrTransferFunction transfer) noexcept {
    switch (transfer) {
    case HdrTransferFunction::SRGB: return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
    case HdrTransferFunction::PQ: return DecodePq(value);
    case HdrTransferFunction::HLG: return DecodeHlg(value);
    case HdrTransferFunction::Linear:
    case HdrTransferFunction::Unknown:
    default: return value;
    }
}

float HdrColorTransform::EncodeTransfer(float value, HdrTransferFunction transfer) noexcept {
    switch (transfer) {
    case HdrTransferFunction::SRGB: return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(std::max(value, 0.0f), 1.0f / 2.4f) - 0.055f;
    case HdrTransferFunction::PQ: return EncodePq(value);
    case HdrTransferFunction::HLG: return EncodeHlg(value);
    case HdrTransferFunction::Linear:
    case HdrTransferFunction::Unknown:
    default: return value;
    }
}

float HdrColorTransform::MapHdrToSdr(float value, const HdrTransformParameters& parameters) noexcept {
    const HdrTransformParameters valid = parameters.IsValid() ? parameters : HdrTransformParameters{};
    const float whiteScale = valid.sdrWhiteNits / 80.0f;
    const float peak = std::max(valid.hdrPeakNits / 80.0f, whiteScale);
    const float normalizedPeak = peak * valid.exposure / whiteScale;
    const float normalized = std::clamp(value, 0.0f, peak) * valid.exposure / whiteScale;
    const float shoulder = std::max(valid.shoulder, 0.001f);
    // Peak-normalized logarithmic bridge. White must be below 1 when HDR
    // headroom exists; mapping white to 1 leaves no ordered SDR codes for
    // highlights. This is paired with MapSdrToHdr, not a display tone map.
    return std::clamp(std::log1p(normalized / shoulder) /
        std::log1p(normalizedPeak / shoulder), 0.0f, 1.0f);
}

float HdrColorTransform::MapSdrToHdr(float value, const HdrTransformParameters& parameters) noexcept {
    const HdrTransformParameters valid = parameters.IsValid() ? parameters : HdrTransformParameters{};
    const float whiteScale = valid.sdrWhiteNits / 80.0f;
    const float peak = std::max(valid.hdrPeakNits / 80.0f, whiteScale);
    const float normalizedPeak = peak * valid.exposure / whiteScale;
    const float mapped = std::clamp(value, 0.0f, 1.0f);
    const float shoulder = std::max(valid.shoulder, 0.001f);
    const float normalized = shoulder * std::expm1(mapped *
        std::log1p(normalizedPeak / shoulder));
    return std::min(normalized * whiteScale / valid.exposure, peak);
}

HdrColor HdrColorTransform::Transform(
    const HdrColor& color,
    HdrTransferFunction inputTransfer,
    HdrTransferFunction outputTransfer,
    const HdrTransformParameters& parameters
) noexcept {
    HdrColor result = color;
    for (size_t channel = 0; channel < 3; ++channel) {
        result[channel] = EncodeTransfer(DecodeTransfer(color[channel], inputTransfer), outputTransfer);
    }
    result[3] = parameters.preserveAlpha ? color[3] : 1.0f;
    return result;
}

}
