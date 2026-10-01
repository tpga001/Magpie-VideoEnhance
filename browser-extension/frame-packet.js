// VideoFrame cannot cross the page/extension agent-cluster boundary.
// Read pixels in its own renderer and transfer the resulting ArrayBuffer.
globalThis.MagpieReadFrame = async (frame, crop, timestamp, reusable) => {
  if (["pq", "hlg", "smpte2084", "arib-std-b67"].includes(frame.colorSpace?.transfer)) throw new Error("当前原始帧模式仅支持 SDR 视频");
  if (frame.visibleRect.width !== frame.displayWidth || frame.visibleRect.height !== frame.displayHeight) throw new Error("暂不支持非方形像素视频");
  const matrices = { bt709: 1, smpte170m: 2, bt470bg: 2, "bt2020-ncl": 3 };
  const matrix = matrices[frame.colorSpace?.matrix];
  const yuv = matrix && [frame.visibleRect.x + crop.x, frame.visibleRect.y + crop.y, crop.width, crop.height].every(x => x % 2 === 0) && ["NV12", "I420"].includes(frame.format);
  const format = yuv ? frame.format : "RGBA", pixels = crop.width * crop.height;
  if (crop.width < 64 || crop.height < 64 || crop.width > 4096 || crop.height > 4096 || pixels > 3840 * 2160) throw new Error("视频帧尺寸超出支持范围");
  const size = yuv ? pixels * 3 / 2 : pixels * 4;
  const packet = reusable?.byteLength === 64 + size ? reusable : new ArrayBuffer(64 + size);
  const header = new DataView(packet);
  [0x31465642, 1, crop.width, crop.height, crop.width * (yuv ? 1 : 4), size].forEach((x, i) => header.setUint32(i * 4, x, true));
  header.setBigInt64(40, BigInt(timestamp), true);
  header.setUint32(48, format === "NV12" ? 1 : format === "I420" ? 2 : 0, true);
  header.setUint32(52, matrix || 0, true); header.setUint32(56, frame.colorSpace?.fullRange ? 1 : 0, true);
  const layout = yuv ? (format === "NV12" ? [{ offset: 0, stride: crop.width }, { offset: pixels, stride: crop.width }] : [
    { offset: 0, stride: crop.width }, { offset: pixels, stride: crop.width / 2 }, { offset: pixels * 5 / 4, stride: crop.width / 2 }
  ]) : [{ offset: 0, stride: crop.width * 4 }];
  await frame.copyTo(new Uint8Array(packet, 64), { ...(yuv ? {} : { format: "RGBA", colorSpace: "srgb" }),
    rect: { x: frame.visibleRect.x + crop.x, y: frame.visibleRect.y + crop.y, width: crop.width, height: crop.height }, layout });
  return { packet, format, width: crop.width, height: crop.height };
};
