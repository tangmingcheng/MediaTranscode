# rk430：直接C合屏入口真实链路验证

状态：本轮直接C合屏正向真实链路、配置存储销毁、外部stop/release观测通过；不代表全部C API生命周期门禁通过。复用5b5565d4当前beta/realtime_application/core静态库和既有build.ninja链接依赖，没有更改CMake/生产核心，也不新增永久测试体系。

直接调用mt_beta_realtime_start_composition。双路原120秒规格：source0 H264+AAC61020/61022、latency200；source1 WindowsHEVC61050、250；audio_source0。输出MPEGTS/UDP62024、HEVC1280×720/30fps/8Mbps CBR/GOP60、AAC目标192kbps，2GiB准备存储、50Mbps出口、100ms wire residence。C API没有CLI的探测timeout参数，沿用既有C层固定profile设置（如timeout），这些是C层策略设置，不是源权威事实；本轮保持源与编码规格，不宣称CLI参数全等，不新增公共字段。

配置和全部指向字符串/输入数组位于单个堆块；start返回后通过volatile unsigned char*逐字节覆盖再free该块，callbacks位于main栈活至release后、user_data=NULL，回调不读取session。SIGTERM只设sig_atomic_t标志，主线程request_stop后等snapshot终态再release。每秒snapshot含backend、异构input_codec、state、completion、worker_errors、encoded、RSS/CPU；回调完整error/stage/native/detail。该轮不覆盖回调内release、旧ABI消费者或全部负向参数门禁。

GCC12.4以C11 -Wall -Wextra -Werror编译consumer.c，g++链接当前CMake产物及其LINK_LIBRARIES，C11编译exit0；首次链接exit1，错误为MPP userdata符号未解析，保留该失败。以下为目标机实际源码和命令；等待主线程START，不提前起源。

恢复原隔离LD_LIBRARY_PATH（prefix-ffmpeg/lib、prefix-mpp/lib、gcc-12.4-native/lib64）后重链exit0；补充native错误码输出后C11及链接再次均exit0。ldd确认六FFmpeg、MPP、GCC均解析到上述隔离路径；readelf确认MPP mpp_frame_get_user_data符号存在。首链接失败保留，不改系统库。最终实际源码及命令如下：

```c
#define _POSIX_C_SOURCE 200809L
#include "media_transcode_beta/realtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>
#include <time.h>
#include <stdatomic.h>
static volatile sig_atomic_t stop_requested;
static atomic_int event_errors;
static void signal_stop(int n) { (void)n; stop_requested=1; }
static void event(void* u,const mt_beta_realtime_event* e) {
 (void)u;
 if(!e) return;
 fprintf(stderr,"event type=%d state=%d error=%d stage=%d completion=%d native=%d detail=%s\n",e->type,e->state,e->error_code,e->failure_stage,e->completion_reason,e->native_code,e->detail?e->detail:"");
 if(e->type==MT_BETA_EVENT_ERROR) atomic_fetch_add(&event_errors,1);
}
struct config_storage {
 mt_beta_realtime_composition_config c;
 mt_beta_composition_input in[2];
 mt_beta_rtp_audio_input audio;
 char media[64],loop[32],remote[32],destination[32],codec[8],fmtp[160];
};
int main(void) {
 struct config_storage* b=calloc(1,sizeof(*b));
 mt_beta_realtime_session* session=NULL;
 mt_beta_realtime_callbacks cb={event,NULL};
 mt_beta_realtime_snapshot s={0};
 int ok=0,stop_sent=0;
 if(!b) return 2;
 strcpy(b->media,"composition-rk430-c-api");strcpy(b->loop,"127.0.0.1");strcpy(b->remote,"192.168.130.229");strcpy(b->destination,"192.168.96.122");strcpy(b->codec,"aac");
 strcpy(b->fmtp,"profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210");
 b->in[0].video=(mt_beta_rtp_video_input){b->loop,61020,MT_BETA_VIDEO_CODEC_H264,96,90000,NULL};
 b->in[1].video=(mt_beta_rtp_video_input){b->remote,61050,MT_BETA_VIDEO_CODEC_HEVC,96,90000,NULL};
 b->audio=(mt_beta_rtp_audio_input){b->loop,61022,b->codec,97,44100,2,b->fmtp};
 b->in[0].audio=&b->audio;b->in[0].input_latency_ms=200;b->in[1].input_latency_ms=250;
 b->c.media_id=b->media;b->c.inputs=b->in;b->c.input_count=2;b->c.grid=MT_BETA_COMPOSITION_GRID_FIXED;b->c.audio_source=0;b->c.audio_bitrate_bps=192000;b->c.preparation_storage_limit_bytes=2147483648ULL;
 b->c.output.protocol=MT_BETA_OUTPUT_MPEGTS_UDP;b->c.output.destination_address=b->destination;b->c.output.destination_port=62024;b->c.output.codec=MT_BETA_VIDEO_CODEC_HEVC;b->c.output.width=1280;b->c.output.height=720;b->c.output.frame_rate_num=30;b->c.output.frame_rate_den=1;b->c.output.gop_frames=60;b->c.output.rate_control_mode=MT_BETA_RATE_CONTROL_CBR;b->c.output.rate_control.cbr.bitrate_bps=8000000;
 b->c.deployment.provisioned_egress_capacity_bps=50000000;b->c.deployment.maximum_wire_residence_ms=100;
 signal(SIGINT,signal_stop);signal(SIGTERM,signal_stop);
 mt_beta_status st=mt_beta_realtime_start_composition(&b->c,&cb,&session);
 fprintf(stderr,"start_status=%d handle_nonnull=%d config_bytes=%zu\n",st,session!=NULL,sizeof(*b));
 volatile unsigned char* wipe=(volatile unsigned char*)b;
 for(size_t i=0;i<sizeof(*b);++i) wipe[i]=0xa5;
 free(b);b=NULL;
 fprintf(stderr,"caller_config_storage_destroyed=1\n");
 if(st!=MT_BETA_STATUS_OK || session==NULL) { if(session) mt_beta_realtime_release(&session);return 1; }
 for(;;) {
  if(stop_requested&&!stop_sent) { st=mt_beta_realtime_request_stop(session);fprintf(stderr,"stop_status=%d\n",st);if(st!=MT_BETA_STATUS_OK)break;stop_sent=1; }
  st=mt_beta_realtime_get_snapshot(session,&s);if(st!=MT_BETA_STATUS_OK){fprintf(stderr,"snapshot_status=%d\n",st);break;}
  fprintf(stderr,"snapshot state=%d completion=%d backend=%d input_codec=%d elapsed=%"PRIu64" encoded=%"PRIu64" dropped=%"PRIu64" worker_errors=%"PRIu64" rss=%"PRIu64" cpu=%.6f\n",s.state,s.completion_reason,s.selected_backend,s.input_codec,s.running_time_ms,s.encoded_packets_pushed,s.dropped_buffers,s.worker_errors,s.working_set_bytes,s.average_process_single_core_cpu_percent);
  if(s.state==MT_BETA_REALTIME_COMPLETED||s.state==MT_BETA_REALTIME_FAILED){ok=s.state==MT_BETA_REALTIME_COMPLETED&&s.completion_reason==MT_BETA_COMPLETION_REQUESTED_STOP&&s.input_codec==MT_BETA_VIDEO_CODEC_UNKNOWN&&s.worker_errors==0&&s.encoded_packets_pushed>0;break;}
  struct timespec delay={1,0};nanosleep(&delay,NULL);
 }
 mt_beta_realtime_release(&session);fprintf(stderr,"release_null=%d event_errors=%d\n",session==NULL,atomic_load(&event_errors));return ok&&session==NULL&&atomic_load(&event_errors)==0?0:1;
}
```

```bash
/opt/mt-tools/gcc-12.4-native/bin/gcc -std=c11 -O2 -Wall -Wextra -Werror -I/home/tang/MediaTranscode-composition-09093919/include -c /home/tang/MediaTranscode-composition-09093919/acceptance/rk430/consumer.c -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk430/consumer.o
/opt/mt-tools/gcc-12.4-native/bin/g++ /home/tang/MediaTranscode-composition-09093919/acceptance/rk430/consumer.o -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk430/consumer /home/tang/MediaTranscode-composition-09093919/out/build/rk-release/libmedia_transcode_beta.a libmedia_transcode_realtime_application.a libmedia_transcode_core.a /usr/local/lib/aarch64-linux-gnu/librga.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavfilter.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavformat.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libswscale.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libswresample.so -lpthread
```

consumer SHA256 f8cc1e0f481e734ad1b028a606faf96f02bb5f53ac4c9a43df4074bf7cb1231f

实际运行脚本（等待START）：
```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk430
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/acceptance/rk430/consumer >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s CLI=%s\n' "$s0" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt
```

独立审阅修正：采用volatile逐字节擦除，避免-O2消除memset/free前写入；OK返回时仍要求session非NULL。C11 -Werror和链接重新执行均exit0，最终hash以上述更新值为准。C层现有open30000/read2000/analyze5000000/probe5000000四项设置与rk429 CLI相同，但不是源探测事实。

## 实际运行与结果

在acceptance/rk430执行nohup /bin/bash run.sh >runner.log 2>&1 &，runner486530、source0=486532、consumer486533、monitor486535。Windows source30672、初始VLC39132、父4968；初始VLC RC63020 bind Access denied后主线程仅替换接收器为VLC37260/RC42120，源与C会话未重启。完整Windows命令和接收器更换由主线程独立记录。

start_status=0、handle_nonnull=1、config_bytes624；start返回后caller_config_storage_destroyed=1，volatile逐字节覆盖并释放拥有全部配置/字符串的堆块。之后持续真实输出。snapshot RUNNING2、backend6 RKMPP、input_codec0 UNKNOWN，符合异构映射。elapsed67秒encoded21802、worker_errors/dropped0、RSS43,073,536B；后段RSS43,302,912B保持。

远端H264+AAC源完整3600帧/120秒自然exit0，WindowsHEVC源3600帧/119.90秒完整结束。VLC原生三张快照由主线程确认双实画→右黑左实→全黑（最后12:55:31），没有使用系统截图。全失源consumer继续RUNNING、encoded从39237增长至43597及后续62729。

STOP后核/proc/486533/exe为本轮consumer，SIGTERM仅置位，主线程request_stop。约1027.912ms观察PID消失，含消费者1秒snapshot轮询。stop_status0；事件STOPPING3→COMPLETED4、COMPLETED事件completion2 REQUESTED_STOP；终snapshot state4/completion2/backend6/codec0、encoded62789、worker_errors/dropped0、RSS43,302,912B，平均单核CPU95.398501%。release_null1、event_errors0。consumer result exit0，source0 exit0、RUN_FINISHED1。

本轮C消费者stdout为空，公共snapshot不暴露payload ledger，不能声称直接观察ledger归零；此前CLI停止门禁的账本证据不能冒充本次C接口字段。本轮不覆盖回调内stop/release、负向配置、旧ABI运行、source重入或无限期稳定性。

完整C回调/终态原文：

```text
start_status=0 handle_nonnull=1 config_bytes=624
caller_config_storage_destroyed=1
event type=1 state=1 error=0 stage=0 completion=0 native=0 detail=
event type=5 state=0 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=2 error=0 stage=0 completion=0 native=0 detail=
event type=5 state=0 error=0 stage=0 completion=0 native=0 detail=
event type=2 state=0 error=0 stage=0 completion=0 native=0 detail=
stop_status=0
event type=5 state=0 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=3 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=4 error=0 stage=0 completion=0 native=0 detail=
event type=4 state=0 error=0 stage=0 completion=2 native=0 detail=
release_null=1 event_errors=0
snapshot state=2 completion=0 backend=6 input_codec=0 elapsed=187962 encoded=62386 dropped=0 worker_errors=0 rss=43302912 cpu=95.749743
snapshot state=2 completion=0 backend=6 input_codec=0 elapsed=188974 encoded=62729 dropped=0 worker_errors=0 rss=43302912 cpu=95.438156
stop_status=0
snapshot state=2 completion=0 backend=6 input_codec=0 elapsed=189075 encoded=62758 dropped=0 worker_errors=0 rss=43302912 cpu=95.398501
[aac @ 0x7f14000b80] Qavg: 24736.352
event type=5 state=0 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=3 error=0 stage=0 completion=0 native=0 detail=
event type=1 state=4 error=0 stage=0 completion=0 native=0 detail=
event type=4 state=0 error=0 stage=0 completion=2 native=0 detail=
snapshot state=4 completion=2 backend=6 input_codec=0 elapsed=189245 encoded=62789 dropped=0 worker_errors=0 rss=43302912 cpu=95.398501
release_null=1 event_errors=0
mpp[486533]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
```

临时产物清单：consumer.c、consumer.o、consumer、build-commands.txt；含run.sh在内本轮13项基础运行记录。归档后逐项删除，不保留临时测试代码/构建设施，保留120秒源和正式产物。远端总计17项已精确删除，目录为空、四PID无残留。主线程本机7项产物已清空，VLC39132/37260和源30672均不存在，详见独立windows-observation记录。
