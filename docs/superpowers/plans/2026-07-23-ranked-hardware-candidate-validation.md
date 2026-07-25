# Ranked Hardware Candidate Validation Plan

1. Add failing planner/probe tests for deterministic ranking, lazy ordered validation, one device creation per candidate, and hardware-only behavior unless explicitly disabled.
2. Separate static candidate enumeration from runtime device validation.
3. Make the scorer consume declared stage priority and use deterministic tie-breaking.
4. Move the ordered candidate validation loop into `MediaPipelinePlanner`; stop on the first valid hardware candidate and return `HardwareUnavailable` if all hardware candidates fail.
5. Run clean-first focused planner builds and tests with `/showIncludes` disabled.

## Completion evidence

- `media_transcode_planner_tests.exe`: passed, including deterministic ranking, ordered validation, and explicit hardware-disabled behavior.
- Direct FFmpeg-to-realtime-CLI RTP run: opened and negotiated the CUDA decoder/filter/NVENC encoder chain, selected it as the highest-ranked runnable chain, and completed without creating an unselected D3D11VA or QSV device.
- A failed decoder/filter/encoder negotiation advances to the next ranked hardware candidate. Exhausting hardware candidates returns `HardwareUnavailable`; software is selected only when the request explicitly disables hardware.
