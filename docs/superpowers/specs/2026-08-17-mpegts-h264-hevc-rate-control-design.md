# MPEG-TS H.264/HEVC and RKMPP Rate-Control Design

## Scope

Project MPEG-TS output shall accept planner-selected H.264 or HEVC video for
VideoOnly and AudioVideo graphs. H.264 and HEVC share the existing DAG mux,
PES, transport-packet, scheduled-batch, pacing, and RTP sender modules. No
codec-specific or RKMPP-specific media chain is introduced.

RKMPP shall support only the rate-control modes exposed by the target encoder:
CBR and VBR. RKMPP CRF requests fail before DAG startup because the installed
`h264_rkmpp` and `hevc_rkmpp` encoders do not expose an authoritative CRF
capability. Other backends retain their independently probed capabilities.

## Planner Products

The planner produces one typed MPEG-TS video elementary-stream contract:

- codec: H.264 or HEVC;
- PMT stream type: `0x1B` for H.264 or `0x24` for HEVC;
- encoded packet layout: Annex-B or length-prefixed with an explicit length
  field width;
- parameter-set policy: inject the codec's complete parameter sets before a
  random-access access unit;
- H.264 parameter sets: SPS and PPS;
- HEVC parameter sets: VPS, SPS, and PPS.

The product is serialized exactly into the graph plan. Builders and runtime
nodes only map or validate it; they do not select codecs, stream types,
layouts, parameter sets, or rate-control modes.

The video encoder product remains the authority for output width, height,
frame rate, GOP, bitrate, minimum bitrate, maximum bitrate, and rate-control
mode. RKMPP capability materialization maps CBR and VBR to the encoder's
advertised `rc_mode` values. Missing or conflicting facts fail during planning
or codec-context construction without fallback.

## Runtime

The FFmpeg codec-parameter materializer validates that the configured codec
matches the MPEG-TS plan. It parses AVCDecoderConfigurationRecord or H.264
Annex-B into SPS/PPS, and HEVCDecoderConfigurationRecord or HEVC Annex-B into
VPS/SPS/PPS. Malformed, incomplete, duplicate, or mismatched configuration is
terminal.

One codec-aware access-unit framer converts length-prefixed packets to Annex-B,
validates Annex-B packets, and injects the planned parameter sets before random
access units. PES serialization, PCR/PTS/DTS handling, TS continuity, pacing,
batch ownership, transport, and diagnostics remain shared.

Runtime diagnostics report the selected codec, stream type, packet layout,
rate-control mode, bitrate bounds, frame rate, GOP, dimensions, encoder name,
and zero-copy frame contract. Diagnostics observe planner products and opened
contexts; they do not create policy.

## Failure Semantics

- Unsupported codec/layout/rate-control combinations fail before DAG startup.
- RKMPP CRF fails explicitly and is never translated to VBR or QP defaults.
- Encoder options that cannot be applied or verified fail codec-context
  construction.
- Runtime packets or codec parameters that violate the selected elementary-
  stream contract terminate the graph.
- No software codec, transfer, mux, or packetization fallback is permitted in
  strict RKMPP mode.

## Acceptance

Windows and RK use the same planner, framer, mux, and sender modules. The final
gate covers H.264-to-H.264, H.264-to-HEVC, HEVC-to-H.264, and HEVC-to-HEVC with
resolution conversion. CBR and VBR are exercised for both output codecs.

Each output is inspected with ffprobe and a complete decode read. Evidence must
show the requested codec, dimensions, frame rate, GOP behavior, measured
bitrate appropriate to the selected rate-control contract, DRM PRIME zero-copy
on RK, zero drops, bounded memory, CPU, and a truthful exit reason. No source
parameter or acceptance threshold may be reduced to obtain a pass.

The protocol facts follow ITU-T H.222.0 stream type assignments and FFmpeg's
documented requirement that HEVC carried in MPEG-TS use Annex-B form.
