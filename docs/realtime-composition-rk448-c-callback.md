# rk448：RUNNING 回调早停通过

状态：本轮精准回调早停边界PASS，整体验收未完成。ce71265d六文件修复双独立SOURCE PASS；目标8核增量9项exit0，CLI SHA256 ab5329a1ffa9d810e4f525ae467bf7c14c5cb205d1b1131ef462d32efc4eacfe。临时消费者以严格C11编译链接成功，逐字复用rk446归档源码和原成功判据。

## 实际命令与进程

`/home/tang/MediaTranscode-composition-09093919/acceptance/rk448/consumer running`，stdout/stderr为running.out/log。两路原规格FFmpeg和VLC完整命令见rk446实际命令节，本轮仅日志前缀改composition-rk448、远端目录改acceptance/rk448；源参数不变。wrapper2366138，consumer2366139，远端H264/AAC源2366137；Windows HEVC源44276、VLC40980。`ps -p 2366139 -o pid=,etimes=,pcpu=,rss=`每秒采样。

## 结果与范围

RUNNING回调内snapshot成功、request_stop成功，动作一次；最终COMPLETED/REQUESTED_STOP、workerErrors0、event_errors0，外部release句柄为空、pass1、exit0。运行2193ms、encoded0，因此只关闭运行状态回调提前停止边界，不证明持续播放、已激活输出drain、EOF或内容A/V。内部WS35766272字节；CPU统计样本不足，快照0%不作性能结论。公开snapshot无ledger，不从release_null推断全内存归零。

```text
mpp[2366139]: mpp_platform: client 12 driver is not ready!
start_status=0 handle_nonnull=1 config_destroyed=1
snapshot state=1 completion=0 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
event type=1 state=1 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
mpp[2366139]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x7f980ad050] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7f980ad050] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 8411.670689
mpp[2366139]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
snapshot state=1 completion=0 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
snapshot state=1 completion=0 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
[in @ 0x7f980aec40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7f980aec40] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 3268.497022
mpp[2366139]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[2366139]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[2366139]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[2366139]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[2366139]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[2366139]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[2366139]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[2366139]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[2366139]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[2366139]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
event type=5 state=0 output=1 output_state=2 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=2 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
callback_action mode=0 snapshot_status=0 snapshot_state=2 encoded=0 stop_called=1 stop_status=0
[aac @ 0x7f24000b80] Qavg: nan
mpp[2366139]: mpp_dec: task no send to hal que must clr current frame hal status
mpp[2366139]: mpp_dec: reset at info change
event type=5 state=0 output=1 output_state=5 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=3 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=4 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
event type=4 state=0 output=0 output_state=0 error=0 stage=0 completion=2 native=0 detail=
snapshot state=4 completion=2 backend=6 codec=0 elapsed=2193 encoded=0 errors=0 rss=35766272 cpu=0.000000
final mode=0 action_done=1 once=1 action_ok=1 event_errors=0 pressure_errors=0 release_null=1 pass=1

```

外部PID/秒/单核CPU%/RSS KiB：
```text
2366139       0  0.0 13816
2366139       1 34.0 27896
2366139       2 30.0 31664

```

测试结束后精确清理源：远端SIGTERM2366137/exit255，Windows44276强制结束（工具exit1），VLC RC quit。不是源自然EOF或完整120秒播放通过。无快照或抓包产物。

## 清理清单

远端acceptance/rk448：consumer、consumer.c、consumer.o、monitor.log、running-evidence.json、running-pid.txt、running-result.txt、running.log、running.out、source-pid.txt、source.log、source.out。本机out/acceptance：composition-rk448-source1.out、composition-rk448-source1.log、composition-rk448-vlc.log；本机out/dependency-work/rk448-callback：consumer.c、running-evidence.json。双端out/dependency-work/rk448-source.tar及目标rk448-build.log/exit在记录结果后清理；保留原120秒源、正式构建与out/rollback/rk448。

清理复核完成：12远端测试项、本机5测试项，3远端/1本机构建传输项均逐项删除；远端目录为空，全部本轮进程退出。
