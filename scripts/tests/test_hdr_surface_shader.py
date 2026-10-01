"""Extract the production HDR compute shader for the D3D11 WARP test.

Run with an output directory. Compile hdr_surface_warp.cpp with that directory
and Magpie.Core on the include path, HdrFrame.cpp/HdrColorTransform.cpp (without
the application PCH), and d3d11.lib/d3dcompiler.lib. No window or vendor SDK is used.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
source = (root / 'src/Magpie.Core/HdrSurfaceAdapter.cpp').read_text(encoding='utf-8-sig')
shader = re.search(r'constexpr char HLSL\[\] = R"\(.*?\)";', source, re.S)
constants = re.search(r'struct AdapterConstants \{.*?\n\};', source, re.S)
assert shader and constants, 'Production shader or constant layout was not found'
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
(output / 'hdr_surface_shader.h').write_text(
    '#pragma once\n#include <cstdint>\n' + shader[0] + '\n' + constants[0] + '\n',
    encoding='utf-8')
print(output / 'hdr_surface_shader.h')
