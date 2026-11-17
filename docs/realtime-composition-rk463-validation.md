# rk463：音频启动裁剪代内关闭

状态：三文件双独立 SOURCE PASS；RKMPP 8 核增量 6 项完成、退出 0。尚未启用生产 BYE，不代表尾音或四屏完整验收通过。

trim 在前序 pending 输出完成后消费 EndGeneration，沿既有 retained-control 授权验证精确代次与对象，保留首个裁剪后样本未就绪时的错误。无额外内部样本队列；标记发布后等待原 purge，purge/lifecycle reset 清除关闭状态。非同步实例明确拒绝，防止无 purge owner 的永久等待。固定准备存储按实际 sizeof(MediaAudioStartupTrimLineageState) 计费。

source_drain_review_a、source_drain_review_b 均 SOURCE PASS；git diff --check 通过。符合原已审有序排空设计，不将代内关闭变为永久 EOF。

目标构建 PID 2563447，wait=0，6/6 完成。CLI 13070392 字节，SHA256=77cb53033672eb1715988f1defbec059876f90e825aae9dd6e271ab9900cdecc。原 CLI 与三文件源码保存在目标 out/rollback/rk463，附 incoming-files.txt。

本轮未运行 CLI/FFmpeg/VLC 实流或 local CLI，Windows FFmpeg 未修改。临时清单：本机 out/acceptance/rk463-source.tar，目标同名 tar 与 rk463-build.log。记录后按清单清理，保留回退与正式构建。

未完成：协议 BYE 旧代保留、接收 owner 截止、新代交接、binder 等前置节点完整接线，以及原规格双平台实流与发送失败退出回归。审查补充确认 SourcePresentation binder 无视频 lookahead；interval 的 lastPositiveVideoDelta 只是既有末包策略，不是实际末帧终点证据，不得混用结论。

清理完成：3项临时文件逐项删除，本机/目标无rk463临时项，构建PID已结束。
