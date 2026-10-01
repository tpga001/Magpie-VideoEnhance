// Coordinates of image pixels, excluding object-fit letterboxing. Cropping
// changes which decoded pixels are copied, never their resolution or density.
globalThis.MagpieVideoGeometry = function(video, viewport, style) {
  const box = video.getBoundingClientRect();
  const width = video.videoWidth, height = video.videoHeight;
  if (!(width >= 64 && height >= 64 && box.width > 0 && box.height > 0)) return null;
  if (style.transform && style.transform !== "none") {
    const match = /^matrix\(([^)]+)\)$/.exec(style.transform);
    const matrix = match?.[1].split(',').map(Number);
    // Translation and uniform positive scaling are already reflected in the
    // bounding rectangle. Rotation/mirroring require a separate image transform.
    if (!matrix || matrix.length !== 6 || !matrix.every(Number.isFinite) ||
        matrix[0] <= 0 || matrix[0] !== matrix[3] || matrix[1] !== 0 || matrix[2] !== 0) return null;
  }
  let sx = box.width / width, sy = box.height / height;
  const fit = style.objectFit || "contain";
  if (fit !== "fill") {
    let scale = fit === "cover" ? Math.max(sx, sy) : Math.min(sx, sy);
    if (fit === "none") scale = 1;
    if (fit === "scale-down") scale = Math.min(1, scale);
    sx = sy = scale;
  }
  const position = (style.objectPosition || "50% 50%").split(/\s+/);
  if (position.length !== 2 || position.some(x => !/^\d+(\.\d+)?%$/.test(x))) return null;
  const left = box.left + (box.width - width * sx) * parseFloat(position[0]) / 100;
  const top = box.top + (box.height - height * sy) * parseFloat(position[1]) / 100;
  const x0 = Math.max(0, box.left, left), y0 = Math.max(0, box.top, top);
  const x1 = Math.min(viewport.innerWidth, box.right, left + width * sx);
  const y1 = Math.min(viewport.innerHeight, box.bottom, top + height * sy);
  if (x1 <= x0 || y1 <= y0) return null;
  const crop = {
    x: Math.max(0, Math.ceil((x0 - left) / sx - 1e-6)), y: Math.max(0, Math.ceil((y0 - top) / sy - 1e-6)),
    width: 0, height: 0
  };
  crop.width = Math.min(width, Math.floor((x1 - left) / sx + 1e-6)) - crop.x;
  crop.height = Math.min(height, Math.floor((y1 - top) / sy + 1e-6)) - crop.y;
  // A one-pixel viewport crop must not force the whole frame through RGBA
  // conversion/readback. Keep 4:2:0 chroma boundaries even and move only inward:
  // at most one more source pixel per edge remains the browser's original.
  // Recompute the destination from the same crop so pixels are never stretched
  // or shifted, and the overlay never covers controls outside the video.
  const right = crop.x + crop.width, bottom = crop.y + crop.height;
  crop.x = Math.ceil(crop.x / 2) * 2;
  crop.y = Math.ceil(crop.y / 2) * 2;
  crop.width = Math.floor(right / 2) * 2 - crop.x;
  crop.height = Math.floor(bottom / 2) * 2 - crop.y;
  if (crop.width < 64 || crop.height < 64) return null;
  return { crop, rect: { left: left + crop.x * sx, top: top + crop.y * sy,
    width: crop.width * sx, height: crop.height * sy } };
};
