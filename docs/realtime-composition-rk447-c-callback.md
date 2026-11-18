# rk447：RUNNING 回调停止复验

状态：FAIL。8a6f8af1停止顺序修复双独立SOURCE PASS，目标8核增量5项exit0；CLI SHA256 a39220632f28f8d58d1eefcc9e6e514b15b4488f7287bd1ac693ff5c08f92f4b。临时消费者严格C11编译/链接exit0，源码逐字复用rk446归档代码（含精确容量错误匹配），本轮不改判据。

## 实际命令与进程

`/home/tang/MediaTranscode-composition-09093919/acceptance/rk447/consumer running`，输出running.out/log。两路FFmpeg及VLC完整命令复用rk446原规格命令，仅本机日志前缀改composition-rk447；远端source.out/log。wrapper2269455，consumer2269456，远端H264/AAC源2269454；Windows HEVC源42540，VLC4496。监控命令`ps -p 2269456 -o pid=,etimes=,pcpu=,rss=`每秒采样，持续至消费者退出。

## 结果

回调get_snapshot/request_stop成功、encoded0。原credit取消错误消失，workerErrors=0；约1309ms仍FAILED、completion5（SHUTDOWN_FAILURE），首错project MPEG-TS mux session cannot finish before complete binding；event_errors1、release_null1、pass0、exit1。尚无实际播放或内容A/V证据。

```text
mpp[2269456]: mpp_platform: client 12 driver is not ready!
start_status=0 handle_nonnull=1 config_destroyed=1
snapshot state=1 completion=0 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
event type=1 state=1 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
snapshot state=1 completion=0 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
mpp[2269456]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x7f840ad1d0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7f840ad1d0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 16581.970744
mpp[2269456]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[2269456]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x7f84062640] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7f84062640] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 10669.119644
mpp[2269456]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[2269456]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[2269456]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[2269456]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[2269456]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[2269456]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[2269456]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[2269456]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[2269456]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[2269456]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
event type=5 state=0 output=1 output_state=2 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=2 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
callback_action mode=0 snapshot_status=0 snapshot_state=2 encoded=0 stop_called=1 stop_status=0
[aac @ 0x7efc000b80] Qavg: nan
mpp[2269456]: mpp_dec: task no send to hal que must clr current frame hal status
mpp[2269456]: mpp_dec: reset at info change
event type=5 state=0 output=1 output_state=6 error=10 stage=9 completion=0 native=0 detail=project MPEG-TS mux session cannot finish before complete binding
event type=5 state=0 output=1 output_state=5 error=10 stage=9 completion=0 native=0 detail=project MPEG-TS mux session cannot finish before complete binding
event type=3 state=0 output=0 output_state=0 error=10 stage=7 completion=0 native=0 detail=project MPEG-TS mux session cannot finish before complete binding
event type=1 state=5 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
event type=4 state=0 output=0 output_state=0 error=0 stage=0 completion=5 native=0 detail=
snapshot state=5 completion=5 backend=6 codec=0 elapsed=1309 encoded=0 errors=0 rss=34156544 cpu=0.000000
final mode=0 action_done=1 once=1 action_ok=1 event_errors=1 pressure_errors=0 release_null=1 pass=0

```

CPU/RSS采样：
```text
2269456       0  0.0 16028
2269456       1 26.0 28812

```

测试判定失败后，核对身份停止远端源2269454（SIGTERM/exit255）与Windows源42540（进程清理），VLC RC quit；不是源自然结束或完整120秒通过。

独立调查：FileMuxNode.stop无条件ensureSession并finish，在尚未完成绑定时错误要求正常媒体收尾；正常EOF已有Streaming门禁。后续应由session owner区分从未激活取消、已激活严格finish和原失败保留，不得以bindingsReady=false吞错。两adapter共享停止契约，Windows同受影响。

## 清理清单

远端acceptance/rk447：consumer、consumer.c、consumer.o、monitor.log、running-evidence.json、running-pid.txt、running-result.txt、running.log、running.out、source-pid.txt、source.log、source.out。本机out/acceptance：composition-rk447-source1.out、composition-rk447-source1.log、composition-rk447-vlc.log；本机out/dependency-work/rk447-callback：consumer.c、running-evidence.json。另记录后清除双端rk447-source.tar及目标rk447-build.log/exit，保留源码、构建产物、out/rollback/rk447与原120秒源。

清理已完成：12远端测试项、本机5测试项，以及3远端/1本机构建传输项逐项删除；远端目录为空，全部本轮PID退出。源清理退出未计通过。
