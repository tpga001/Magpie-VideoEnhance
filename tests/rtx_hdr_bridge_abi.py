"""Check the private bridge ABI using invalid arguments only; never initialize NGX."""
import ctypes
from pathlib import Path
import sys
package = Path(sys.argv[1]).resolve()
dll = ctypes.WinDLL(str(package/'Magpie.RtxVideo.dll'))
create = dll.MagpieRtxHdrCreate
create.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint32)]
create.restype = ctypes.c_long
draw = dll.MagpieRtxHdrDraw
draw.argtypes = [ctypes.c_void_p] * 4 + [ctypes.POINTER(ctypes.c_uint32)]
draw.restype = ctypes.c_long
destroy = dll.MagpieRtxHdrDestroy
destroy.argtypes = [ctypes.c_void_p]
destroy.restype = ctypes.c_long
invalid = ctypes.c_long(0x80070057).value
for _ in range(4):
    instance = ctypes.c_void_p()
    status = ctypes.c_uint32()
    assert create(None, str(package), ctypes.byref(instance), ctypes.byref(status)) == invalid
    assert not instance.value
    assert draw(None, None, None, None, ctypes.byref(status)) == invalid
    assert destroy(None) == 0
print('PASS: private RTX Video bridge exports and null-argument guards; no NGX initialization or GPU inference')
