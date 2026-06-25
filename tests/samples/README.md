# MediaTranscode test samples

Integration and regression tests look for media samples in this directory by default.

Required smoke sample:

```text
sample_h264_aac_320x240.mp4
```

Optional strict cancellation sample:

```text
sample_h264_aac_320x240_10s.mp4
```

The strict cancellation subtest runs only when the optional 10-second sample exists. If it is missing, the subtest prints a skip message and the rest of the integration test still runs.

Guidelines for committed samples:

- Keep normal smoke samples small, ideally 1-3 seconds.
- Use a longer sample, around 10 seconds, only for cancellation behavior that must not finish before `stopLocalVideoTranscode()` is called.
- Prefer low resolutions such as 320x240 or 640x360.
- Keep samples stable; do not replace existing files in-place unless all references are intentionally updated.
- Do not make default tests depend on hardware codecs.

You can override the directory at configure time:

```bash
cmake -S . -B build -DMEDIA_TRANSCODE_TEST_SAMPLES_DIR=/path/to/samples
```

If the required smoke sample is missing, the integration test returns `77`, and CTest marks it as skipped.
