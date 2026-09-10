# RKMPP 驻留与随机访问适配证据

状态：实现完成，尚未构建及真实流复验。旧版本 4bf43635 在 RK 原规格 H.264 RTP 720p30 输入、HEVC MPEG-TS over RTP 1080p25 CBR 6 Mbps GOP 50 输出的准备阶段，因缺少压缩输入驻留 adapter 拒绝，输出零包；该失败不能标为通过。

适用版本更新为 FFmpeg-rockchip `ab1e61adaa21ff129caa8e01a1198156044567ed` 与 MPP `c08762ebfadeb4e986d2fed993bc7a54862d3ebe`。三个 RK adapter 统一校验运行时 `ab1e61a` 及相关库配置一致性；部署仍需核实实际库哈希与加载映射。父版本 d90e3a1 存在私有 last_pkt 关闭泄漏，不再授权这些契约。[依赖来源与最小修复](../3rds/ffmpeg-rockchip/README.md) 保留可追溯差异。Windows native/CUVID 驻留路径及 NVENC 策略保持原逻辑，共享 planner 和 DAG 仍须 Windows 回归。

## 压缩输入所有权

[FFmpeg rkmppdec.c](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavcodec/rkmppdec.c) 使用一个 last_pkt，提交 EAGAIN 时可能保留它并返回一帧，成功提交后立即 unref。[MPP mpp.c](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/mpp.c) 对未携带 MppBuffer 的输入先执行 copy_init，再排入任务；[mpp_packet.c](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/base/mpp_packet.c) 将该输入复制至 MPP 自有存储。

新修订在 decoder close 中无条件 unref last_pkt，补齐停止时实际所有权释放。adapter 记录公共 handoff 一个、私有 last_pkt 一个；公共 BSF 别名不重复计费。只适用于无 FFmpeg frame workers 的 h264_rkmpp/hevc_rkmpp。既有 planner 再组合原子 AU 准备和节点 pending 容量；MPP 内部副本不冒充 engine AVBuffer 驻留，也不声称其驱动字节受 engine 账本硬控。

## 自然 IDR 周期

[rkmppenc.c](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavcodec/rkmppenc.c) 在 open 中设置 rc:gop、固定输入输出 cadence 并禁用 RC 丢帧。adapter 实读 intra_refresh/refresh_num，要求正 GOP、禁用刷新，且后端 av_reduce 的 65535 表示范围能精确表示帧率；不足则不产生有界加入证明。

[MPP refs](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/base/mpp_enc_refs.c) 以 rc igop 在禁用刷新时重置序号，序号零标记 IDR；目标 mpp_enc_ref.c 第 283–308 行已核实默认单一短期参考、temporal_id=0、max_tlayers=1；目标 rkmppenc.c 检索 SET_REF_CFG/temporal/ref_cfg 均无匹配。H.264 slice 将 is_idr 映射至 NAL 5；[HEVC syntax](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/codec/enc/h265/h265e_syntax.c) 将 temporal 0 的 intra 映射至 IDR_W_RADL。运行时仍逐 AU 解析 NAL，绝不以 KEY 标志替代 IDR 证据。

该周期只说明连续媒体提交条件下的帧间隔，不保证 CPU/驱动墙钟完成时间。加入预算由已有事务剩余期限与协议 activation lead 共同检查；不改 GOP，不扩大外部超时，不新增外参。RK shared surface copy 和完整实时多输出验收另行闭环。
