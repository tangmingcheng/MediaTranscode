# A/V 同步主观验证

## 2026-07-29

- MPEG-TS 输入/输出：主观 A/V 同步通过。
- RTP 输入/输出：主观 A/V 同步通过。
- 长时间观察未发现 CPU 持续增长、内存持续增长或可感知 A/V 漂移。
- MPEG-TS 在约 4 分钟内偶发 2 次花屏。

## 后续优化

MPEG-TS 偶发花屏不阻塞本轮 A/V 同步验收，但必须单独诊断。复现时同步保存输入与输出传输流，并关联检查 TS continuity counter、PES 边界、H.264 SPS/PPS/IDR、运行时丢弃计数和 generation 变化。修复不得放宽 A/V 时钟约束、引入 fallback，或建立第二套 DAG 装配链路。
