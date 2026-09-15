# 固定实时视频合屏实施记录

## 当前状态

2026-09-15：用户批准两阶段计划及新增合屏输入列表、网格、指定音源的 CLI/C API。基线 `d590cc3b`，工作分支 `feat/realtime-video-composition`。现有未跟踪依赖、`NUL` 和 `out/` 属于实施前状态，不纳入本轮提交。

Release 基线已通过 VS2026 全量 clean-first 构建（configure/build exit code 0）。尚未修改生产核心、尚未实现合屏、尚无合屏真实验收通过结论。

平台调查已定位 Windows CUDA 合成滤镜的启动错误传播缺陷，详见 [能力与失败语义门禁](realtime-video-composition-capability.md)。这不是运行复现；不能用全量构建成功或滤镜存在判定合屏门禁通过。正在明确原计划之外的 Windows FFmpeg 根因修复范围。

## 静态评估

独立审查的合屏就绪度为 42/100，并非项目整体质量分：DAG 复用/绑定 10/20，时钟/来源 8/20，断流/恢复 6/20，资源/背压 12/20，平台能力 2/10，观测/验收 4/10。

阻塞包括全图单解码器与单同步 binding、单源 lineage、缺少逐格 black/silence 产品及多路总体资源准入。既有 RawRtpInput 的 WaitForEvidence 和关键帧恢复应复用，不能将普通源失活误归为全图内部错误。

## 决策记录

- 在当前目录创建功能分支，复用已有构建目录；不创建额外 worktree。
- 保留旧 `plan.md` 的历史事项，通过新增当前计划入口引用本轮中文计划。
- 外部 FFmpeg CLI 的滤镜存在不能证明生产链接库、设备吞吐、分配和持帧边界可用；实际能力调查是前置门禁。
