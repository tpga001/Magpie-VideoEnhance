<p align="center"><img src="./src/Magpie/Icons/SVG/Magpie Icon Full Disabled.svg" width="150" height="150" alt="Magpie"></p>
<h1 align="center">Magpie Video Enhance</h1>

🌍 **English** | [简体中文](./README_ZH.md)

Magpie Video Enhance is a private development snapshot focused on browser and local video enhancement. It is derived from [SAOG0721/Magpie](https://github.com/SAOG0721/Magpie) and, through that project, [Blinue/Magpie](https://github.com/Blinue/Magpie). It adds video-only capture and alignment to Magpie's window-scaling and effect system while retaining the upstream experimental effects.

The source tree contains both the original window-processing path and experimental browser/PotPlayer video paths. This repository does not yet publish its own installable release. See [current status](STATUS.md) before building or testing; this is not an official Magpie release.

## Main Features

### Image Processing and Frame Generation

| Feature | Example Effects | Purpose |
| --- | --- | --- |
| Spatial upscaling and sharpening | Lanczos, FSR and other MagpieFX effects | Enlarge window content and adjust clarity and detail |
| Experimental temporal super resolution | DLSS SR, FSR 2/3/4, XeSS SR | Explore image reconstruction from captured frames and estimated motion |
| AI image controls | DLSSNR | Same-resolution SDR processing with tone, structure, shadow, reflection and glow controls |
| Video enhancement | RTX Video | Video super resolution and denoising for low-resolution or compressed content |
| Frame generation | DLSSFG, XeSSFG | Generate intermediate images between real frames for smoother motion |

Experimental-effect availability depends on the build options, GPU, driver, runtime components and effect combination. The [current status](STATUS.md) describes what has been verified in this project.

### Effect Groups and Parameter Editing

An **effect group** combines effects in a chosen order, storing their parameters and scaling settings. Use built-in groups or create your own combinations for different applications.

The toolbar's parameter panel lets you adjust effects while viewing the result, with automatic saving. Controls indicate whether a change applies live or requires the group to restart; double-clicking a slider restores the effect's default value. Parameters support groups, drop-down choices and Chinese translations.

The same panel places the Front Edge Sync switch and target frame-rate slider (15–360 FPS, step 1) before the effect controls, so you can adjust frame pacing and image processing in one place. Follow each control's Live or Restart label to see how a change takes effect. The parameter window remembers its position and size and stays within the visible area when the capture area changes.

### Comparison, Performance Monitoring and Frame Sync

The toolbar offers original/processed comparison, per-effect timings and frame-rate monitoring, screenshots and parameter editing, with customizable shortcuts. When frame generation is enabled, separate output and real-frame readings help you assess the result and processing cost.

Front Edge Sync controls Magpie's frame submission pace, regulating real-frame input before FG when frame generation is active. Apply a matching frame-rate cap in the source application; synchronization waits may increase latency. See the [frame-sync guide](docs/FRAME_SYNC_GUIDE.md) for setup details.

## Download and Install

This repository has no published binary package yet. For local development, review the [build script](scripts/Build-Release.ps1), [optional dependency boundaries](docs/THIRD_PARTY_AND_REDISTRIBUTION.md), and [current status](STATUS.md). GitHub's automatically generated source archive is not a ready-to-run application package.

## Usage and Compatibility

- Local builds target Windows x64 and require a DirectX 11-capable GPU; individual AI effects may have higher requirements.
- The original Magpie path processes complete window images, while the experimental video paths select the video region. Neither path has the game engine's full native motion vectors, depth, exposure or separated UI. Estimated optical flow can assist some effects, but this is not equivalent to native in-game DLSS/FSR/XeSS integration.
- Image processing can affect text and UI along with the scene; temporal effects may also produce ghosting or other artifacts. Use Comparison to judge whether an effect suits the content.
- Use one frame-generation effect per group and avoid combining it with other frame-generation systems. Generated FPS is not the game's real rendering rate and does not imply a proportional improvement in input responsiveness.

## Reporting Problems

Start with the suggestions and details in Home's recent-issue card, or open the log directory. For an issue in this project, record the application version, GPU and driver, effect group, input/output resolution, reproduction steps and relevant logs.

The [upstream Magpie FAQ](https://github.com/Blinue/Magpie/wiki/FAQ) also covers general usage questions. Please discuss this fork's experimental effects and compatibility issues in this repository.

## Development and Documentation

The project includes the Magpie application, MagpieFX effects and experimental native effect backends. Optional proprietary backends are disabled by default in source builds and require separately supplied SDKs and runtimes. Configure local paths in the untracked `src/BuildOptions.props.user` file.

- [Third-party dependencies, licenses and build boundaries](docs/THIRD_PARTY_AND_REDISTRIBUTION.md)
- [Experimental feature designs and development records](docs/experimental/README.md)
- [Build and packaging script](scripts/Build-Release.ps1)
- [MagpieFX effect format](<docs/MagpieFX (EN).md>)

## Contributions and Acknowledgments

This project builds on [Blinue/Magpie](https://github.com/Blinue/Magpie) and its contributors' work. Thanks also to everyone contributing code, translations, design suggestions and testing feedback.

- [HexBen123](https://github.com/HexBen123): early depth-estimation performance optimization and TensorRT integration guidance, plus the [DLSSNR parameter-localization proposal](https://github.com/SAOG0721/Magpie/pull/16).
- [Kristijan1001](https://github.com/Kristijan1001): the [reference implementation for editing effect parameters while scaling](https://github.com/SAOG0721/Magpie/pull/4), which informed this project's parameter-panel design and implementation.
- [LCPD15](https://github.com/LCPD15): the [1.0× windowed-scaling fix](https://github.com/SAOG0721/Magpie/pull/3), allowing same-resolution effects to be displayed at the source size without forcing output enlargement.
- [TurnX-alt](https://github.com/TurnX-alt): [build, UI and repository consistency improvements](https://github.com/SAOG0721/Magpie/pull/23), including ClangCL build fixes without optional SDKs, preset parameter and feedback link corrections, narrow-window text wrapping, scaling-start notifications and a repository consistency check suite.
- [konodiodaaaaa1](https://github.com/konodiodaaaaa1): [HDR support and compatibility work](https://github.com/SAOG0721/Magpie/pull/24).

Contributions through Issues and Pull Requests are welcome. See [Contributors](https://github.com/SAOG0721/Magpie/graphs/contributors) for the code contribution history.

## License

Magpie-derived source code is licensed under [GPLv3](LICENSE). Third-party SDKs, models and runtimes have their own licenses; see the [third-party and redistribution notes](docs/THIRD_PARTY_AND_REDISTRIBUTION.md).
