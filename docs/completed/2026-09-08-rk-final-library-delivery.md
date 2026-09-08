# RKMPP最终库与四路验收交付

代码ba190e3c：媒体核心73ad5fbd加Beta错误分类四行修复。CBR/VBR双向输入损伤恢复run53/55/56/57分别独立提交通过；H.264使用指定真实源，HEVC按用户授权使用已有本地高规格源。两位独立审查者完成master全增量功能/架构风险审查及修复复审，均PASS，评分分别88。最终PR32以master为目标。

通过`/home/tang/package_media_transcode_beta.sh`在目标机生成：

- 目录：`/home/tang/packages/media-transcode-beta-rkmpp-ba190e3c-20260908`
- 压缩包：同路径加`.tar.gz`，19103145 B，SHA-256 `f05f16278fe563c66191b7ad3edd47616a711560b29921eb1a995e1755787644`
- 静态库：`lib/libmedia_transcode_beta.a`，SHA-256 `f06370932695f29a82e2285c2cdf59856be831ab7fa5c6d6c268b49c1c5d3212`
- C示例：`examples/beta_minimal.c`，已编译程序`bin/beta_minimal`；C11严格编译链接exit0，动态依赖无缺失。示例没有单独重跑媒体矩阵，不冒称C入口实流验证。
- 包内15文件manifest全部校验通过；CLI SHA-256仍为`7464cf735973a6f68d71a935e0f4a343bbff92ddd0615a8880451260e738f470`，与四路验收一致。未下载库至本机，旧库保留。

风险：驱动入队后聚集仍存在；其他音视频/平台/多小时负载未全面覆盖；裸RTP停源按无输入超时exit1。run57输入捕获自身丢包已披露，输出与接收全量匹配。代码分类变化仅Beta结果投影，目标机红绿验证和8核Beta构建成功，临时诊断已删除。