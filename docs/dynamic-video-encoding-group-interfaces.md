# 动态视频编码组实现与验证

状态：Windows 第 16 轮真实链路验收通过，对应提交 `4bf43635090bbe8b9d58c7bcf07b91ecd57d06a2`。RKMPP 动态多输出尚未验证，整体双审、质量评分和 PR 尚未完成。完整命令与结果见 [Windows 第 16 轮报告](dynamic-video-windows-16.md)。

## 已实现

| 层次 | 当前职责 |
| --- | --- |
| planner | `normalizeEncodingRequest` 在 capability probe 前形成不可变请求合同，覆盖源与输出 codec、尺寸、帧率、完整 RC、quality、preset、tune、profile、level、GOP、B 帧、global header、low latency 和 filter/copy 要求。协议地址、端口和 session 不参与编码等价比较。 |
| preparer | 先核对源 epoch、hwframes 身份和运行编码组的完整规范化合同；匹配后复用该组已验证的 pipeline/readback，仅规划协议输出，不执行 probe/open。未匹配时才准备新编码器，并持有真实 encoder 到发布。 |
| builder | 初始图和新增图使用同一共享输入、编码组、协议输出三段模型。编码组通过 EncodedVideoOutputFanout 分发，协议使用明确的 CodecParameters 元数据。 |
| application | Registry 管理组、执行段、不可变 witness 和输出引用；发布前复核源身份、图版本和组状态。删除输出先排空协议段，最后一个消费者退役后才排空并释放编码组。零输出保留共享输入和出口整形时钟。 |
| runtime | 每个消费者使用独立有界队列；溢出隔离该分支。跨段绑定来自真实执行 context，资源保留至物理退休。所有输出和 RTCP 共用 controller 内同一接口 scope 整形器。 |

## 加入与资源合同

新消费者等待自然 IDR，不改变健康编码组 GOP。共用 NAL 扫描器验证完整 AU：H.264 NAL 5、HEVC NAL 19/20 可开放加入；CRA 或单独的 AV_PKT_FLAG_KEY 不足以开门。READY 需要该消费者首个完整 IDR 的最后媒体 datagram 实际提交。VideoOnly 不伪造 A/V canonical lineage。

初始 shared 账户在启动前暂存三段固定存储总量，再逐段提取独立固定存储 lease；最终媒体额度不包含这些字节。新增编码组分别预留 payload、自身固定存储和源 frame producer 的 retention 增量；新增协议只预留固定存储及既有编码 producer 的 retention 增量，不创建虚假 producer。共享 arbiter/clock 仅计入 shared，channel 按消费段计费。

## Windows 已验证范围

连续 120 秒 H.264 RTP 1280×720、30 fps、约 8 Mbps 输入，输出 HEVC/H.264 MPEG-TS over RTP 1920×1080、25 fps、CBR 6 Mbps、GOP 50。第 16 轮证实同规格请求 action=reused 且未新增 encoder；同时覆盖异编码新组、删除共享组单个消费者、删除至零和重新添加。

四路 VLC 硬解画面均已查看；RTP 零丢包，TS 连续性无错。聚合出口服务曲线最大超额 1356 B，等于最大 datagram；最终 payload bytes/objects 为零。RTP 源结束后按既有无进展超时策略退出，不能描述为会话自然 EOS。

算法与生命周期对照 GStreamer [tee 独立队列](https://gstreamer.freedesktop.org/documentation/coreelements/tee.html)、[动态管线移除](https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html)，编码 session 生命周期参照 [NVENC 文档](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/nvenc-video-encoder-api-prog-guide/index.html)。本轮通过不代表 RKMPP 固定池适配、全部链路矩阵或最终独立审查通过。

## 第 17 轮失败后的修复状态

GOP 750 单变量真实诊断暴露动态复用错误沿用静态 10 秒启动期限：新消费者尚未等到自然 IDR 即被 scheduler 拒绝，初始输出继续运行。已实现内部随机访问周期产品，NVENC 在 probe 和真实 encoder open 后实读 GOP、帧率以及两个 intra-refresh 私有选项。动态 planner 按运行组完整 IDR 间隔或新组首帧启动，加已有组合 activation lead 检查剩余 first-output 事务预算；同一产品同时驱动 scheduler 和 controller，不能只修其中一层。周期是连续媒体条件下的事实，不构成 CPU/driver 墙钟执行保证。

执行段新增单次 reclamation owner 产品，固定存储按实际运行段与 owner 对象布局计费，原生线程栈及库分配仍为观测范围。固定存储由 planner 统一计入各段，不并入媒体 producer 额度。41bb17a1整批源码双审通过，Windows22完成原规格动态矩阵，Windows23/24分别完成GOP500真实IDR等待与GOP750原事务期限拒绝。RKMPP第2轮暴露共享解码时间基及依赖关闭所有权缺口，正在修复和重新验证；不能以Windows历史通过替代新修改验收。版本适用边界见[适配记录](dynamic-video-rkmpp-adapter-evidence.md)。
