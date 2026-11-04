# 合屏 C API 当前验收缺口

2026-10-26 独立只读复核未发现可直接确证的新源码缺陷，但不构成完整门禁通过。近期 realtime CLI 实流不调用 mt_beta_realtime_start_composition，不能替代 C 包装层验收；旧版本记录不作为当前构建通过依据。

| 项目 | 当前源码证据 | 必须补充的当前版本证据 |
|---|---|---|
| 入口与配置 | MediaRealtimeBetaApi.cpp 的独立入口复用 startSession；OwnedConfig 同步映射到拥有字符串和 vector 的 request | 直接 C 调用的真实媒体链路；start 返回后释放调用方配置存储 |
| 旧 ABI | 头文件使用独立合屏类型，旧 config/snapshot 布局未改；当前静态库已核实新旧 C 链接符号 | 实际布局核验与旧消费者兼容运行 |
| 输入映射 | 2–4 源、fixed grid、音频索引、正 latency、协议声明进入共同 planner | 异构源、无效声明、选中音频缺失的正负向运行 |
| 音频与存储 | 显式正码率且精确 kbps；共享准备存储值映射后由 InputPlanner 校验 | 目标输出、压力失败及资源释放 |
| 发布顺序 | session handle 发布后 publishOwnership；事件线程先等待 ownership | 启动失败空句柄、立即 stop 与事件顺序 |
| 生命周期 | 外部 release 停止并 join；事件线程内 release 拒绝删除；callback 异常记录失败并停止 | 回调内 stop/release、外部 snapshot/停止及失败回调 |
| codec 快照 | 未准备及异构输入为现有 UNKNOWN，同构映射已有枚举 | 异构输入实际快照 |

保持 realtime 范围，不运行 local CLI，不新增永久测试体系或为测试改变生产核心。直接 C 消费者的验证方式须沿用仓库构建与真实媒体要求；未实际执行前所有运行项保持未完成。该清单不替代 Windows 合屏矩阵、内容 A/V、资源压力及 Windows 后 RKMPP 的门禁。

## 当前产物链接符号核验

CMakeLists.txt 将 beta 构建为 STATIC，故本项检查 archive 链接符号，不称为 DLL 导出验证。2026-10-26 11:28:28 的 media_transcode_beta.lib 为 9838186 字节；执行以下只读命令退出0，确认 start、start_composition、request_stop、get_snapshot、release、add_output、remove_output、get_output_snapshots 八个 C 符号均存在。

```powershell
& 'D:/VisualStudio2026/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/dumpbin.exe' /linkermember:1 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_beta.lib' | Select-String 'mt_beta_realtime_'; exit $LASTEXITCODE
```

同目录 realtime_beta_c.exe/obj 时间仍为2026-09-01，不是当前版本，未执行且未据此报告通过。此次未生成临时产物。后续重建后须以新产物继续验收，符号存在不证明布局、回调或真实媒体正确。
