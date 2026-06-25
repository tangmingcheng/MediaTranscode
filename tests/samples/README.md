# MediaTranscode test samples

Integration and regression tests look for media samples in this directory by default.

Required integration smoke sample:

```text
sample_h264_aac_2560x1440.mp4
```

This sample is used by the sync, async and cancellation smoke subtests. The integration test transcodes it to `160x120` with H.264 video and audio disabled, then probes the output to verify codec, resolution, stream layout, duration, fps and frame count.

Optional strict cancellation sample:

```text
sample_h264_aac_320x240_25s.mp4
```

The strict cancellation subtest runs only when the optional 25-second sample exists. If it is missing, the subtest prints a skip message and the rest of the integration test still runs. Use a longer sample here so `stopLocalVideoTranscode()` is called before the job naturally completes.

Guidelines for committed samples:

- Keep normal smoke samples small enough for local and CI runs.
- Use a longer sample, around 20-30 seconds, only for strict cancellation behavior.
- Prefer stable H.264/AAC MP4 files with predictable stream metadata.
- Keep samples stable; do not replace existing files in-place unless all references are intentionally updated.
- Do not make default tests depend on hardware codecs.

You can override the directory at configure time:

```bash
cmake -S . -B build -DMEDIA_TRANSCODE_TEST_SAMPLES_DIR=/path/to/samples
```

If the required smoke sample is missing, the integration test returns `77`, and CTest marks it as skipped.
