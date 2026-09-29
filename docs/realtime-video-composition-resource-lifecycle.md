# 停止阶段节点自持资源释放

基线 ac74f8e8；完整合屏仍 FAIL42/100。仅验证 realtime，未测试 local 或 RKMPP，未修改外部 FFmpeg。

## 根因与设计

r56 在工作线程和队列归零后仍保留4个逻辑对象（71879 reservations/71875 releases，0payloadBytes）。CodecResolverNode 没有 stop/abort 清理，保存的输入与时间戳快照直到节点析构才释放；生产完成报告在节点析构前采集。FileMuxNode::stop 释放会话，abort 却只关闭会话内部传输；MPEG-TS adapter 自持的两个 codec 配置引用仍随会话存活。源码证明持有路径，不预认全部4个对象的数量归属。

采用 GStreamer 官方 [states](https://gstreamer.freedesktop.org/documentation/additional/design/states.html) 与 [MT-refcounting](https://gstreamer.freedesktop.org/documentation/additional/design/MT-refcounting.html) 的停止执行后释放资源、以引用所有权决定存续原则。项目 stop/abort 并非 GStreamer 完整状态机的等价实现。

resolver 在基类 stop/abort 后调用同一个 resetRuntimeState；清理快照时与 getter 共用 mutex，codec/context 先于设备释放，基础失败原样返回。运行期间的动态输出查询保持有效，外部已持有 shared_ptr 不被强制销毁。关闭阶段按固定5个 buffer 槽记录引用与 credit；日志异常不阻止清理，引用数不等于对象总数。mux abort 复用已有 releaseSession，与 stop 对称；generation purge 保留 codec 配置用于恢复的策略不变。

沿用执行器 join 后逆拓扑回收，无新线程、队列、容量或对外输入。无 ledger 重置、credit 解绑、BYE→EOF 变更。Windows/Linux 共用节点；正常 stop 若其他节点提前失败仍可能中断遍历，本项未改该既有 scheduler 行为。

## r57：原规格 realtime 复验

实际执行：RTP/RTCP H.264/AAC → MPEG-TS over RTP HEVC CBR 8Mbps 1280×720 30fps / AAC CBR192kbps 44.1kHz双声道，指定连续120秒源。最终指标与退出原因必须独立判断，不因对象归零将无进展失败记为验收通过。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-lifecycle-r57 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-lifecycle-r57.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-lifecycle-r57-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-lifecycle-r57-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-lifecycle-r57-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-lifecycle-r57- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果与清理清单

2026-09-29 Release全量构建session96937成功，clean665/all666、exit0；产物12:46:31/5185536bytes。CLI PID33952/session31374自然exit1，源PID32980/session3995自然exit0、3600帧120秒；VLC PID31836启动exit0，已查看1280×720游戏画面，仍记录converter错误。截图为composition-lifecycle-r57-2026-09-29-12h48m15s312.png，仅证明单时刻画面。

首release12:47:34.341，gen1 video16/audio27；20项退休媒体释放，编码包入出均39955、总入出171540。abort12:49:38.824→38.883。最终queued/workers/payloadBytes为0，但4对象仍在，71849 reservations/71845 releases、高水12212555bytes/89对象；workerErrors/errors/pressure0，stalledIntervals1，CLI仍NotInitialized/no-progress。AAC关闭仍剩2帧，本次FAIL，不作成功验收提交。

resolver关闭日志：input bufferReferences2/hasCredit0/creditReferences0，encoder_parameters 1/0/0，其他槽为空。因此本次未证明4对象来自resolver；释放改动执行不等于计费残留修复，继续检查账本刷新和实际credit持有者。

400 CPU样本/22逻辑核，整机口径均值1.344485%、峰5.052632%，单核29.578675%/111.157895%。runtime WS103964672→190492672、峰194965504bytes。外部monitor session70156自然exit0，21次5秒样本12:47:55.733→12:49:36.087，CPU7.875→36.671875s，WS192368640→190492672、峰194965504，Private397434880→396173312、峰398917632。只覆盖本轮短窗口。

118条gen1漂移raw/filtered最大156ns；无恢复证明。sender86220datagrams/104692640payloadBytes，deadline/pressure/partial/ambiguous均0，pacing取消1，delivery not proven。源完整发送不等于输出完整交付。

已先记录命令、失败和指标；清理清单为本轮3日志、SDP及上述PNG，共5文件。CLI与源自然结束；已核对身份清理VLC31836，5文件与3个PID均无残留。保留指定源124427809bytes及Release产物；无远程运行、抓包、录制或Windows测试脚本。

后续源码核对：MediaGraphRuntimeReporter::capture直接调用ledger->snapshot，4对象不是陈旧报告。0payloadBytes不能据此分类为元数据；MediaGraphPayloadProducerRegistryCompiler与ExecutionContext对外部设备帧可采用ObservedOnlyExternalBytesAndEngineManagedObject，只计对象。当前尚未证明实际4对象的producer/最终持有者，不能称其泄漏或宣称残留已修复。

独立根因复审确认候选链：VideoEncodeNode→retainMediaFfmpegPayload将credit绑定AVFrame.buf的AVBuffer生命周期，FFmpeg或外部codec owner可能合法共持；registry只持lineage元数据，不能无证据归因。合屏output role的preparedVideoEncoder虽存在，但本轮普通AV链路未证明经过该role。下一最小诊断是codec owner释放前引用数与同ledger释放前后配对，不增加每帧registry、不解绑credit、不额外flush。

2026-09-29后续更正：r58逐节点诊断证明VideoEncode abort实际4→0，但报告在作用域RAII reset触发该abort之前已采集。直接调用ledger->snapshot不能排除采集时序错误，此前“4对象不是陈旧报告”结论过强。根因是stop失败未完成收尾即返回，详见[后续证据与修复](realtime-video-composition-credit-retention.md)。
