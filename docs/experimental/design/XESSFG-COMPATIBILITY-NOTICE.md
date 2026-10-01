# XeSSFG compatibility source notice

The runtime compatibility restrictions and pacing logic are adapted from Coldwood1026/OptiScaler, pinned commit `70676c5f037c8c26f1ec355b250a72303cd268da` (GPL version 3):

- [XeFGUnlock.h](https://github.com/Coldwood1026/OptiScaler/blob/70676c5f037c8c26f1ec355b250a72303cd268da/OptiScaler/proxies/XeFGUnlock.h)
- [XeFGPacing.h](https://github.com/Coldwood1026/OptiScaler/blob/70676c5f037c8c26f1ec355b250a72303cd268da/OptiScaler/proxies/XeFGPacing.h)
- [License](https://github.com/Coldwood1026/OptiScaler/blob/70676c5f037c8c26f1ec355b250a72303cd268da/LICENSE)

Magpie modifications, 2026-09-14: strict loaded-module identity and native-prologue checks; transactional installation/restoration; one lease covering native and compatibility contexts; per-worker/context timing with reset epochs; bounded output records; source-metadata and attributed-submit period estimates; automatic 3×/4× selection. Hook callbacks perform no allocation or formatted logging. The copied historical global estimator and synchronous logger were removed. Source files are `src/Magpie.Core/XeSSFGCompatibility.h`, `XeSSFGPatchTransaction.h`, `XeSSFGPacing.h`, and `XeSSFGTiming.h` in the accompanying Magpie source tree.

The GPL-3.0 text is included in the repository `LICENSE` and runtime `LICENSE-Magpie.txt`. This notice covers the compatibility adaptation, not Intel's separately licensed runtime. The distribution contains the original, unmodified `libxess_fg.dll`; supported changes occur only in process memory. Intel runtime terms remain in `INTEL-XESS-LICENSE.txt` and `INTEL-XESS-THIRD-PARTY.txt`.
