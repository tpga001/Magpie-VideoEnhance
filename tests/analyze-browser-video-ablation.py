"""Compare fixed screen regions from real browser SR/FG ablation runs."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
args = parser.parse_args()
run = json.loads((args.directory / "result.json").read_text(encoding="utf-8-sig"))
summary = {"run_status": run["status"], "build_commit": run["buildCommit"], "stages": [],
           "limitation": "Temporal variation of a static synthetic region; not a perceptual score or displayed FPS."}
regions = []
for stage in run["stages"]:
    folder = args.directory / (stage["name"] + "-text")
    if not (folder / "samples.json").exists() or not stage.get("initial"):
        continue
    sampling = json.loads((folder / "samples.json").read_text(encoding="utf-8-sig"))
    regions.append(sampling["region"])
    images = sorted(folder.glob("text-*.png"))
    frames = np.stack([np.asarray(Image.open(image).convert("RGB")) for image in images])
    span = frames.max(axis=0).astype(np.int16) - frames.min(axis=0).astype(np.int16)
    span_max = span.max(axis=2)
    text_mask = frames.mean(axis=0).min(axis=2) > 140
    # Streaming differences avoid allocating a second full floating-point stack.
    adjacent = [float(np.abs(frames[i].astype(np.int16) - frames[i-1].astype(np.int16)).mean())
                for i in range(1, len(frames))]
    row = {
        "name": stage["name"], "route": stage.get("route"), "observation": stage.get("observation"),
        "frame_count": len(frames), "sample_seconds": stage.get("sampleSeconds"),
        "region": sampling["region"], "mean_temporal_range_8bit": float(span.mean()),
        "pixels_with_channel_range_above_8_percent": float((span_max > 8).mean() * 100),
        "text_core_mean_temporal_range_8bit": float(span[text_mask].mean()) if text_mask.any() else None,
        "adjacent_frame_mean_absolute_difference_8bit": float(np.mean(adjacent)),
        "largest_adjacent_difference_8bit": max(adjacent),
        "largest_step_pair": [images[int(np.argmax(adjacent))].name, images[int(np.argmax(adjacent)) + 1].name],
        "foreground_loss_samples": sum(item["foreground"] != stage["initial"]["browser"] for item in sampling["samples"]),
        "overlay_visible_samples": sum(item["overlayVisible"] for item in sampling["samples"]),
    }
    summary["stages"].append(row)
    del frames
summary["identical_sample_rectangles"] = bool(regions) and all(region == regions[0] for region in regions)
cleanup = args.directory / "cleanup.json"
if cleanup.exists():
    summary["cleanup"] = json.loads(cleanup.read_text(encoding="utf-8-sig"))
(args.directory / "analysis.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(summary, ensure_ascii=False, indent=2))
