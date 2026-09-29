# 输出身份、音频贡献与编码 FIFO 边界

## 本轮实施

源 AU 与输出 AU 使用不同类型身份，调度序号使用域中性类型；视频重定时保留身份，不将输出序号称为源序号。音频 canonicalizer 从既有同步组产品取得输出 ID，逐片段保留真实源身份、代次、映射可信度、源时间证据及精确输出样本区间。贡献记录不递归引用输出 lineage，不以首段来源代表整个输出。构造时校验贡献完整、连续覆盖输出区间。

音频区间累积及帧/包封装检查完整时间轴身份，而非仅比较 generation 数值，避免不同源或输出在相同代次下混入一个连续区间。严格包时间轴校验保留。

同步编码 FIFO 容量由 realtime planner 形成：输入块上界覆盖真实补偿窗口最大距离及已探测重采样块；编码节点读取下一输入前排空完整帧，因此最大暂存样本为 B + F - 1。PCM 样本 payload 字节由 prepared 声道数、样本格式及样本上界计算，不包含 vector/string/AVFrame 或 codec 内部物理分配；每片段至少一个样本，片段数独立受样本上界限制。在扩容和写入前检查契约。非同步本地路径尚缺少同等重采样 probe 产品，本轮不声称其具备该保证。

## 工业依据与边界

- [GStreamer 同步设计](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)区分公共 running-time 与输入时间映射；[GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)以独立输入队列、单聚合线程、明确 GAP 区间和输出事件顺序实现连续聚合。本轮身份分类与贡献保存只是必要基础，尚未等价实现其连续聚合生命周期。
- [FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)是有背压的状态机，不能将编码器 priming 或源恢复当作任意可重置状态。
- [AVCodecContext 官方定义](https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html)与部署头文件均说明 audio encoding 的 `delay` 未使用；`initial_padding` 表示编码 priming，也不能直接作为最大提交缓存。既有 capability provider 读取 `context->delay` 并作为 `encoderLookaheadSamples` 的问题已发现，本轮新增 FIFO 预算不使用该事实；提交 mapper 的硬界需另行补全权威事实。

## 尚未完成

输出 generation/origin 仍来自原启动 epoch；runtime compiler 与 protocol authority 仍共用源同步组，registrar 仍对源/输出执行整体 purge。AAC codec/FIFO/mapper 的跨重入连续保留尚未接通，不声称恢复修复。

贡献目前逐 mapper 片段保留至 canonical AU；scheduled builder 目前剥离媒体包，尚未提供协议全链路贡献追踪。缺少 planner 元数据字节和全链路片段驻留硬预算；视频多源贡献、GeneratedSilence/黑帧、持续输出聚合、逐源恢复权限和多源 CLI/API 尚未完成。硬件 Upload/Map/Unmap 节点尚未实现，旧 packet-layout probe 的清零帧不构成正确黑帧与完成语义证明。保留原 Windows→RKMPP、2–4 路与断流/恢复验收标准，完整合屏仍 FAIL，评分不提高。

## 验证

前三次 Release 全量构建因直接 include 依赖缺失失败（第一次输出截断，第二次取得解码视图错误；第三次发现两个 canonical 帧基类依赖）。修复后第四、五次达到固定120秒截止；Debug尝试被会话中断，无成功结果。用户要求继续Release后，全量构建成功；r19暴露同步组角色契约遗漏，修复后Release再次全量成功。均使用规定入口和clean-first/all-target，没有放宽门禁或使用旧二进制。

两位独立审查者在include和同步组契约修复后均明确Standards/局部Spec PASS。原审查漏掉GroupContracts接入，真实CLI已补证；完整合屏仍FAIL。r19、r20结果如下。

r19 CLI exit1，09:37:56.657 compile.failed：Synchronized graph contains an unsupported group consumer；CUDA capability成功，但未进入runtime，未取得CPU/内存趋势或A/V漂移。源PID32972、VLC PID3116因测试失败按身份清理，不计自然完成。
### r19 实际执行命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-identity-r19 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60780 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60782 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61780 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r19.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r19-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r19-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r19-vlc.log --extraintf=rc --rc-host=127.0.0.1:62780 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-identity-r19- --snapshot-format=png rtp://@127.0.0.1:61780
```

r19 源/VLC已按精确PID清理，日志和SDP按本轮清单删除；无本轮截图/抓包。修复：将 EncodedAudioCanonicalizer 的既有同步组参数纳入 CommonCoreShapeValidator 的显式角色契约，要求其值与 runtime binding 完全一致；未知消费者仍拒绝。

## r20：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

修复 group contract 后 Release 再次全量重建成功，configure/build exit 0，两个 CLI 链接完成。r20 **完整恢复 FAIL**，不能创建通过验收的成功提交。

CLI PID32084 自然退出1；首源PID12196自然退出0，3600帧/120秒。首源结束事件立即启动重入源PID9880，命令仍指定同一120秒源。VLC PID8956，已实际查看09:42:13的1280×720截图；这只证明首段画面，不证明恢复。

09:43:29.897七组purge全部ack；09:43:35.172锁generation2，35.177生成MPEG-TS计划，35.178 AudioEncode报 Audio encoder packet timeline is discontinuous。新代sender仅3datagrams/600payload bytes，MPEG-TS access_units=0，未恢复媒体。保留严格校验，未通过重写PTS或丢包绕过。

最终worker/queue/payload字节及对象均0，70828次申请/释放相等；errors/workerErrors=1/1、stalledIntervals=1。CPU410样本/22逻辑核，整机平均1.241717%、峰3.778338%，单核等效27.317785%/83.123426%。工作集109293568→201060352字节，telemetry高水204394496；不证明长期无增长。payload高水15648535字节/398对象，pressure failures=0。额外18轮进程采样完成，计量口径不混用。

漂移generation1共117条，raw/filtered绝对最大6612ns；generation2仅2条，最大13735ns，不证明恢复后持续同步。sender累计82938datagrams/100733532payload bytes，deadline/pressure/partial/ambiguous均0，pacing取消1，backlog取消0，delivery_evidence=not_proven。

当前测试已因明确AAC失败结束；在记录必要诊断后按精确PID清理重入源和VLC，重入源被中止不记自然完成。未使用Windows测试脚本、抓包或额外FFmpeg监控。截图、日志、SDP随后按本轮清单清理，固定120秒源保留。

### r20 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-identity-r20 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60780 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60782 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61780 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r20.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r20-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r20-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r20-vlc.log --extraintf=rc --rc-host=127.0.0.1:62780 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-identity-r20- --snapshot-format=png rtp://@127.0.0.1:61780
```

重入实际命令：

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-identity-r20-source-rejoin.log 2>&1
exit $LASTEXITCODE
```

两位独立审查者原局部源码PASS未捕获GroupContracts漏接；r19真实编译失败后修复并由两者再次明确PASS。该源码结论不覆盖本节仍失败的恢复验收。

清理核验：重入源执行会话在强制清理后返回-1，不计自然结束；首源和CLI退出码分别为0/1。PID32084、12196、9880、8956均已不存在；本轮composition-identity前缀产物检查为空，七份构建诊断日志也已在记录结果后删除。未启动远程测试，本轮无远程产物。
