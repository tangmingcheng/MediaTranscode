# RKMPP Zero-Copy Design

## Goal

Add a strict FFmpeg RKMPP path for local and realtime video transcoding without changing the default Windows planner or runtime behavior. The path must cover H.264 and HEVC decode/encode combinations, preserve DRM PRIME frames through the DAG, and use RGA for planner-requested resizing.

## Architecture

- `--hardware-backend auto|rkmpp` becomes a typed planner constraint. `rkmpp` and `--disable-hw` conflict.
- The planner owns codec, filter, frame-domain, pixel-format, transfer, and zero-copy decisions. Builders only map the selected plan; runtime nodes validate and execute it.
- RKMPP decode produces DRM PRIME frames. No-resize plans omit a filter and forward the same FFmpeg frame references. Resize plans use the exact RGA filter exposed by the target FFmpeg and require DRM PRIME on both sides.
- RKMPP encode consumes DRM PRIME directly. RKMPP must not be forced through a generic hardware device or frames-context path when its FFmpeg codec owns those resources internally.
- Missing codecs, filters, DRM descriptors, formats, dimensions, or buffer ownership fail before DAG start where discoverable and fail immediately at runtime otherwise. There is no software transfer or backend fallback in strict mode.

## Runtime Evidence

Diagnostics record the selected chain, stage frame contracts, frame format and dimensions, backing-buffer identity, RGA use, software-frame observations, transfers, queue/drop state, memory, A/V drift, and exit reason. Passing RKMPP runs require zero software frames, downloads, and uploads. A no-resize run preserves decoder-to-encoder buffer identity; an RGA run may replace the buffer but remains DRM PRIME.

## Compatibility

The existing automatic backend selection, CUDA/QSV/D3D11VA/software paths, Windows FFmpeg linkage, canonical scheduling, lifecycle, flush, abort, and lineage behavior remain unchanged. No CI or tracked automated test infrastructure is added.

## Acceptance

- Windows: clean VS2026 Debug rebuild plus existing local and realtime A/V media chains using the fixed 120-second source.
- RK local: four H.264/HEVC decode/encode combinations at source size, plus H.264 and HEVC RGA-resize cases.
- RK realtime: MPEG-TS/UDP input to separate RTP output for all four codec combinations, plus one complete H.264 A/V route.
- The target source is `/home/firefly/Downloads/test16s.mp4`; all target commands run after `ffenv on`.
