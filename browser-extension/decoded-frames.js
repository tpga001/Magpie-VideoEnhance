// The extension uses a single supported path: decoded pixels -> extension
// worker -> authenticated loopback socket -> native shared memory.
globalThis.MagpieDecodedFrames = globalThis.MagpieFrameRelay;
