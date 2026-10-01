"""Analyze saved browser quality evidence without changing runtime settings."""
import argparse
import collections
import csv
import datetime as dt
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
parser.add_argument("--presentmon", type=Path)
parser.add_argument("--csv-clock-offset-hours", type=float, default=0)
args = parser.parse_args()
result = json.loads((args.directory / "result.json").read_text(encoding="utf-8-sig"))
summary = {"run_status": result["status"], "build_commit": result["buildCommit"], "stages": []}
present_rows = []
if args.presentmon:
    with args.presentmon.open(encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            # Windows date output observed a constant offset relative to both
            # first-present log anchors. Preserve raw CSV; make correction explicit.
            text = row["CPUStartDateTime"]
            date, time = text.split(" ")
            parts = [int(part) for part in date.split("-")]
            clock, fraction = time.split(".")
            hour, minute, second = [int(part) for part in clock.split(":")]
            row["cpu_clock"] = dt.datetime(*parts, hour, minute, second, int(fraction[:6])) + dt.timedelta(hours=args.csv_clock_offset_hours)
            present_rows.append(row)
    summary["presentmon_source"] = str(args.presentmon)
    summary["csv_clock_offset_hours"] = args.csv_clock_offset_hours
    summary["capture_stderr"] = Path(str(args.presentmon) + ".stderr.txt").read_text(encoding="utf-8-sig")
for stage in result["stages"]:
    item = {"name": stage["name"], "observation": stage.get("observation")}
    frames = stage.get("after", {}).get("frames", [])
    if len(frames) > 1:
        item["source_callback_fps"] = (len(frames) - 1) * 1000 / (frames[-1]["time"] - frames[0]["time"])
        item["source_samples"] = len(frames)
    before, after = stage.get("relayBefore"), stage.get("relayAfter")
    if before and after:
        frames_received = after["frames"] - before["frames"]
        item["received_source_frames"] = frames_received
        item["transport_drops"] = after["dropped"] - before["dropped"]
        if frames_received:
            item["average_copy_ms"] = (after["copyMs"] - before["copyMs"]) / frames_received
            item["average_ack_ms"] = (after["ackMs"] - before["ackMs"]) / frames_received
    rect = stage.get("final", {}).get("overlayRect")
    if rect:
        item["overlay_pixels"] = [rect["Right"] - rect["Left"], rect["Bottom"] - rect["Top"]]
    item["display_measurement"] = stage.get("displayMeasurement", stage.get("presentmon"))
    if present_rows and stage.get("captureEndedAt"):
        import numpy as np

        start = dt.datetime.fromisoformat(stage["captureStartedAt"].replace("Z", "+00:00")).astimezone().replace(tzinfo=None) + dt.timedelta(seconds=2)
        end = dt.datetime.fromisoformat(stage["captureEndedAt"].replace("Z", "+00:00")).astimezone().replace(tzinfo=None) - dt.timedelta(seconds=1)
        chains = collections.defaultdict(list)
        for row in present_rows:
            if int(row["ProcessID"]) == stage["pid"] and int(row["SwapChainAddress"], 16) and start <= row["cpu_clock"] <= end:
                chains[row["SwapChainAddress"]].append(row)
        item["display_streams"] = []
        for chain, rows in chains.items():
            displayed = [row for row in rows if row["DisplayedTime"] != "NA" and float(row["DisplayedTime"]) > 0 and row["DisplayLatency"] != "NA"]
            durations = np.array([float(row["DisplayedTime"]) for row in displayed])
            if not len(durations):
                continue
            item["display_streams"].append({
                "swapchain": chain,
                "window_start_local": start.isoformat(), "window_end_local": end.isoformat(),
                "presented_records": len(rows), "displayed_records": len(displayed),
                "not_displayed_records": sum(row["DisplayedTime"] == "NA" for row in rows),
                "displayed_fps": float(len(durations) * 1000 / durations.sum()),
                "summed_display_seconds": float(durations.sum() / 1000),
                "interval_ms": {"p50": float(np.percentile(durations, 50)), "p95": float(np.percentile(durations, 95)), "p99": float(np.percentile(durations, 99)), "max": float(durations.max())},
                "over_25ms_percent": float((durations > 25).mean() * 100),
                "displayed_types": dict(collections.Counter(row["FrameType"] for row in displayed)),
                "present_modes": dict(collections.Counter(row["PresentMode"] for row in displayed)),
                "display_latency_ms_p50": float(np.median([float(row["DisplayLatency"]) for row in displayed])),
            })
    burst = args.directory / (stage["name"] + "-text")
    images = sorted(burst.glob("text-*.png"))
    if images:
        import numpy as np
        from PIL import Image

        arrays = np.stack([np.array(Image.open(image).convert("RGB")) for image in images])
        span = arrays.max(axis=0).astype(np.int16) - arrays.min(axis=0).astype(np.int16)
        item["static_text_sample"] = {
            "frames": len(images),
            "mean_temporal_range_8bit": float(span.mean()),
            "pixels_with_channel_range_above_8_percent": float((span.max(axis=2) > 8).mean() * 100),
            "limitation": "A short synthetic static-text sample cannot certify real-video motion quality.",
        }
    summary["stages"].append(item)
log_path = args.directory / "runtime.log"
if log_path.exists():
    logs = log_path.read_text(encoding="utf-8-sig")
    summary["processing_routes"] = sorted(set(re.findall(r"RTX Video VSR (?:initialized|bypassed):[^\r\n]+", logs)))
    samples = []
    pattern = r"^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d+)\|.*XeSSFG SDK output: requested=\d+x frames=(\d+) submissions=(\d+)"
    for match in re.finditer(pattern, logs, re.MULTILINE):
        samples.append((dt.datetime.fromisoformat(match[1]), int(match[2]), int(match[3])))
    # Split whenever the native presenter was rebuilt and counters reset.
    streams = []
    for sample in samples:
        if not streams or sample[2] <= streams[-1][-1][2]:
            streams.append([])
        streams[-1].append(sample)
    summary["sdk_counter_rates_not_display_fps"] = []
    for stream in streams:
        if len(stream) < 2:
            continue
        first, last = stream[0], stream[-1]
        seconds = (last[0] - first[0]).total_seconds()
        summary["sdk_counter_rates_not_display_fps"].append({
            "seconds": seconds,
            "sdk_outputs_per_second": (last[1] - first[1]) / seconds,
            "submissions_per_second": (last[2] - first[2]) / seconds,
        })
(args.directory / "analysis.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(summary, ensure_ascii=False, indent=2))
