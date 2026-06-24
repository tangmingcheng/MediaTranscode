# MediaTranscode test samples

Integration and regression tests look for media samples in this directory by default.

The first integration smoke test expects:

```text
sample_h264_aac_320x240.mp4
```

Guidelines for committed samples:

- Keep each file small, ideally 1-3 seconds.
- Prefer low resolutions such as 320x240 or 640x360.
- Keep samples stable; do not replace existing files in-place unless all references are intentionally updated.
- Do not make default tests depend on hardware codecs.

You can override the directory at configure time:

```bash
cmake -S . -B build -DMEDIA_TRANSCODE_TEST_SAMPLES_DIR=/path/to/samples
```

If the required sample is missing, the integration test returns `77`, and CTest marks it as skipped.
