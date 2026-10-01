"""Exercise the production renderer boundary traversal with CPU texture doubles."""
from pathlib import Path
import sys
repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1])
renderer = (repo/'src/Magpie.Core/Renderer.cpp').read_text(encoding='utf-8-sig')
def function(signature):
    start = renderer.index(signature)
    brace = renderer.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (renderer[end] == '{') - (renderer[end] == '}')
        end += 1
    return renderer[start:end]
fixture = (repo/'tests/prepare_hdr_bicubic_test.py').read_text(encoding='utf-8-sig')
texture = fixture[fixture.index('struct MockTexture :'):fixture.index('enum class ScalingType')]
prefix = r'''
#include "HdrComponentRuntime.h"
#include "HdrEffectBoundary.h"
#include "EffectProtocolCatalogC.h"
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
using namespace Magpie;
'''+texture+r'''
struct EffectOption { std::string name; std::map<std::string,float> parameters; };
struct Options {
    HdrComponentPlan hdrComponents;
    bool IsHdrCompatibilityEnabled() const { return hdrComponents.outputHdr; }
    bool IsHdrCaptureEnabled() const { return hdrComponents.captureHdr; }
    bool IsEffectHdrEnabled(size_t i) const { return i < hdrComponents.stages.size() ? hdrComponents.stages[i].inputHdr : hdrComponents.outputHdr; }
};
struct ScalingWindow {
    struct Options options;
    static ScalingWindow& Get() { static ScalingWindow w; return w; }
    const struct Options& Options() const { return options; }
};
struct Drawer {
    ID3D11Texture2D* output = nullptr;
    ID3D11Texture2D* source = nullptr;
    HdrEffectBoundaryContext boundary;
    void SetHdrBoundary(HdrEffectBoundaryContext c) { boundary=std::move(c); }
    void SetHdrInputSource(ID3D11Texture2D* p) { source=p; }
    ID3D11Texture2D* GetExternalOutputTexture() const { return output; }
};
struct Backend {
    HdrEffectBoundaryContext boundary;
    bool hdr=false;
    void SetHdrBoundary(HdrEffectBoundaryContext c) { boundary=std::move(c); }
};
static void ConfigureHdrBackendProtocol(const std::string&, Backend& b, const HdrFrameMetadata&, bool hdr) { b.hdr=hdr; }
static HdrFormatRoutes GetHdrRoutesForEffect(const EffectOption&, bool, bool) { return EffectProtocolC::Bicubic(); }
struct Source {
    ID3D11Texture2D* texture;
    ID3D11Texture2D* GetPipelineTexture() const { return texture; }
    HdrFrameMetadata GetHdrFrameMetadata() const { return {}; }
    uint64_t CaptureSequence() const { return 7; }
    uint64_t ResourceGeneration() const { return 11; }
    int64_t CaptureTimestamp100ns() const { return 123456; }
};
struct Renderer {
    Source* _frameSource;
    uint64_t _capturedFrameId=9009;
    bool _dlssnrAutoHdr=true;
    std::vector<EffectOption> _runtimeEffectOptions;
    std::vector<Drawer> _effectDrawers;
    std::vector<std::unique_ptr<Backend>> _nativeEffectBackends;
    HdrFrameMetadata _pipelineOutputMetadata;
    HdrComponentPlan _runtimeHdrComponents;
    void _UpdateHdrEffectBoundaryContexts() noexcept;
};
'''
suffix = r'''
int main() {
    auto& options=ScalingWindow::Get().options;
    std::vector<EffectOption> effects{{"Color\\HDR_to_SDR",{{"whiteNits",360.0f}}},
        {"DLSSNR\\DLSSNR_AI_Filter"},{"Color\\SDR_to_HDR"},{"DLSSNR\\DLSSNR_AI_Filter"}};
    options.hdrComponents=BuildHdrComponentPlan(effects);
    assert(options.hdrComponents.error==HdrComponentError::None);
    MockTexture capture{16,DXGI_FORMAT_R16G16B16A16_FLOAT};
    Source source{&capture};
    Renderer renderer{&source};
    renderer._runtimeEffectOptions=effects;
    renderer._runtimeHdrComponents=options.hdrComponents;
    std::vector<std::unique_ptr<MockTexture>> outputs;
    for (size_t i=0;i<effects.size();++i) {
        outputs.push_back(std::make_unique<MockTexture>(32, options.hdrComponents.stages[i].outputHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM));
        renderer._effectDrawers.push_back({outputs.back().get()});
        renderer._nativeEffectBackends.push_back(std::make_unique<Backend>());
    }
    for (UINT size : {32u,64u}) {
        for (auto& texture : outputs) texture->desc.Width=texture->desc.Height=size;
        renderer._UpdateHdrEffectBoundaryContexts();
        assert(!renderer._nativeEffectBackends[1]->hdr && renderer._nativeEffectBackends[3]->hdr);
        assert(renderer._effectDrawers[1].boundary.inputFrame.metadata.color.transfer==HdrTransferFunction::SRGB);
        const auto& hdr=renderer._effectDrawers[3].boundary;
        assert(hdr.prepared && hdr.inputFrame.texture==outputs[2].get());
        assert(hdr.inputFrame.metadata.width==size && hdr.inputFrame.metadata.color.sdrWhiteNits==360);
        assert(renderer._effectDrawers[2].source==outputs[1].get());
        const auto& meta=renderer._pipelineOutputMetadata;
        assert(meta.frameId==9009 && meta.captureSequence==7 && meta.resourceGeneration==11 && meta.timestamp100ns==123456);
        assert(meta.color.transfer==HdrTransferFunction::Linear && meta.width==size);
    }
    // HDR introduced after SDR capture must also prepare the next HDR backend.
    renderer._runtimeEffectOptions={{"Color\\SDR_to_HDR"},{"DLSSNR\\DLSSNR_AI_Filter"}};
    options.hdrComponents=BuildHdrComponentPlan(renderer._runtimeEffectOptions);
    renderer._runtimeHdrComponents=options.hdrComponents;
    capture.desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    renderer._effectDrawers.resize(2); renderer._nativeEffectBackends.resize(2);
    outputs[0]->desc.Format=outputs[1]->desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    renderer._UpdateHdrEffectBoundaryContexts();
    assert(renderer._nativeEffectBackends[1]->hdr && renderer._effectDrawers[1].boundary.prepared);
    assert(renderer._pipelineOutputMetadata.color.sdrWhiteNits==203);
    renderer._runtimeEffectOptions={{"RTXVideo\\RTXVideo_HDR"}};
    options.hdrComponents=BuildHdrComponentPlan(renderer._runtimeEffectOptions);
    renderer._runtimeEffectOptions[0].parameters["peakNits"]=1600;
    renderer._runtimeHdrComponents=BuildHdrComponentPlan(renderer._runtimeEffectOptions);
    renderer._effectDrawers.resize(1); renderer._nativeEffectBackends.resize(1);
    renderer._UpdateHdrEffectBoundaryContexts();
    assert(renderer._pipelineOutputMetadata.color.displayPeakNits==1600);
    assert(options.hdrComponents.stages[0].peakNits==1000);
    std::cout << "PASS: production renderer HDR boundary traversal, SDR/HDR NR selection, introduced HDR, resized upstream textures and frame identity\n";
}
'''
(output/'hdr_components_boundary.cpp').write_text(prefix + function('static HdrFrame MakePipelineInputFrame(') + '\n' +
    function('void Renderer::_UpdateHdrEffectBoundaryContexts()') + '\n' + suffix, encoding='utf-8')
