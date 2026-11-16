# rk431：直接C合屏入口负向参数验证

状态：六项同步负向门禁通过，无媒体源启动，不替代真实媒体或全部C生命周期验收。使用5b5565d4当前静态库与隔离FFmpeg/MPP/GCC12.4，无生产改动。

源码预期：MediaRealtimeBetaRequestMapper.cpp290—296校验输入数与精确kbps码率，311校验正latency；map结尾同步调用MediaRealtimeCompositionInputPlanner，后者14—16校验正准备存储，44—46校验选中音频存在。MediaRealtimeBetaApi.cpp81先清空输出句柄，OwnedConfig创建失败在session启动/发布之前返回，因此预期同步INVALID_ARGUMENT且句柄NULL，无回调。

从rk430完整合法配置恢复，每例单独进程只改变一项：0=input_count1；1=input_count5（mapper先范围拒绝，不遍历越界数组）；2=source0 latency0；3=选中source0 audioNULL；4=audio_bitrate192001；5=preparation_storage0。实际六例均status1、session_nonnull0、callbacks_at_return0、100ms后callbacks_final0，各exit0。回调观察窗口不单独证明永不异步；同步失败在创建session之前的源码证据共同支持该边界。

PID依次518032/518033/518034/518035/518047/518055。每例运行命令为/home/tang/MediaTranscode-composition-09093919/acceptance/rk431/consumer N，N依次0至5；subprocess.communicate(timeout=15)取exit/stdout/stderr，没有启动FFmpeg或VLC。MPP加载时均报告client12 driver not ready，原文保留，不冒充该警告已修复。

首次临时C源码单行if触发-Werror=misleading-indentation，编译exit1；拆为独立行后C11 -Werror编译及当前静态库链接均exit0。完整最终源码、实际构建命令及全部结果如下。
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

static atomic_int event_errors;

static void event(void* u,const mt_beta_realtime_event* e) {
 (void)u;
 if(!e) return;
 fprintf(stderr,"event type=%d state=%d error=%d stage=%d completion=%d native=%d detail=%s\n",e->type,e->state,e->error_code,e->failure_stage,e->completion_reason,e->native_code,e->detail?e->detail:"");
 atomic_fetch_add(&event_errors,1);
}
struct config_storage {
 mt_beta_realtime_composition_config c;
 mt_beta_composition_input in[2];
 mt_beta_rtp_audio_input audio;
 char media[64],loop[32],remote[32],destination[32],codec[8],fmtp[160];
};
int main(int argc,char** argv) {
 struct config_storage* b=calloc(1,sizeof(*b));
 mt_beta_realtime_session* session=NULL;
 mt_beta_realtime_callbacks cb={event,NULL};

 if(argc!=2) return 2;
 int id=atoi(argv[1]);
 if(id<0||id>5)return 2;
 if(!b) return 2;
 strcpy(b->media,"composition-rk431-c-api");strcpy(b->loop,"127.0.0.1");strcpy(b->remote,"192.168.130.229");strcpy(b->destination,"192.168.96.122");strcpy(b->codec,"aac");
 strcpy(b->fmtp,"profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210");
 b->in[0].video=(mt_beta_rtp_video_input){b->loop,61020,MT_BETA_VIDEO_CODEC_H264,96,90000,NULL};
 b->in[1].video=(mt_beta_rtp_video_input){b->remote,61050,MT_BETA_VIDEO_CODEC_HEVC,96,90000,NULL};
 b->audio=(mt_beta_rtp_audio_input){b->loop,61022,b->codec,97,44100,2,b->fmtp};
 b->in[0].audio=&b->audio;b->in[0].input_latency_ms=200;b->in[1].input_latency_ms=250;
 b->c.media_id=b->media;b->c.inputs=b->in;b->c.input_count=2;b->c.grid=MT_BETA_COMPOSITION_GRID_FIXED;b->c.audio_source=0;b->c.audio_bitrate_bps=192000;b->c.preparation_storage_limit_bytes=2147483648ULL;
 b->c.output.protocol=MT_BETA_OUTPUT_MPEGTS_UDP;b->c.output.destination_address=b->destination;b->c.output.destination_port=62024;b->c.output.codec=MT_BETA_VIDEO_CODEC_HEVC;b->c.output.width=1280;b->c.output.height=720;b->c.output.frame_rate_num=30;b->c.output.frame_rate_den=1;b->c.output.gop_frames=60;b->c.output.rate_control_mode=MT_BETA_RATE_CONTROL_CBR;b->c.output.rate_control.cbr.bitrate_bps=8000000;
 b->c.deployment.provisioned_egress_capacity_bps=50000000;b->c.deployment.maximum_wire_residence_ms=100;
 switch(id) {
 case 0:b->c.input_count=1;break;
 case 1:b->c.input_count=5;break;
 case 2:b->in[0].input_latency_ms=0;break;
 case 3:b->in[0].audio=NULL;break;
 case 4:b->c.audio_bitrate_bps=192001;break;
 case 5:b->c.preparation_storage_limit_bytes=0;break;
 }
 mt_beta_status st=mt_beta_realtime_start_composition(&b->c,&cb,&session);
 int returned_nonnull=session!=NULL;
 fprintf(stderr,"case=%d status=%d session_nonnull=%d callbacks_at_return=%d\n",id,st,returned_nonnull,atomic_load(&event_errors));
 if(session) { fprintf(stderr,"unexpected_handle_stop=%d\n",mt_beta_realtime_request_stop(session)); mt_beta_realtime_release(&session); }
 struct timespec delay={0,100000000};nanosleep(&delay,NULL);
 int callbacks=atomic_load(&event_errors);
 fprintf(stderr,"case=%d callbacks_final=%d release_null=%d\n",id,callbacks,session==NULL);
 free(b);
 return st==MT_BETA_STATUS_INVALID_ARGUMENT&&!returned_nonnull&&callbacks==0?0:1;
}```

```bash
/opt/mt-tools/gcc-12.4-native/bin/gcc -std=c11 -O2 -Wall -Wextra -Werror -I/home/tang/MediaTranscode-composition-09093919/include -c /home/tang/MediaTranscode-composition-09093919/acceptance/rk431/consumer.c -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk431/consumer.o
/opt/mt-tools/gcc-12.4-native/bin/g++ /home/tang/MediaTranscode-composition-09093919/acceptance/rk431/consumer.o -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk431/consumer /home/tang/MediaTranscode-composition-09093919/out/build/rk-release/libmedia_transcode_beta.a libmedia_transcode_realtime_application.a libmedia_transcode_core.a /usr/local/lib/aarch64-linux-gnu/librga.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavfilter.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavformat.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libswscale.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libswresample.so -lpthread
```

```text
CASE=0 PID=518032 EXIT=0
mpp[518032]: mpp_platform: client 12 driver is not ready!
case=0 status=1 session_nonnull=0 callbacks_at_return=0
case=0 callbacks_final=0 release_null=1

CASE=1 PID=518033 EXIT=0
mpp[518033]: mpp_platform: client 12 driver is not ready!
case=1 status=1 session_nonnull=0 callbacks_at_return=0
case=1 callbacks_final=0 release_null=1

CASE=2 PID=518034 EXIT=0
mpp[518034]: mpp_platform: client 12 driver is not ready!
case=2 status=1 session_nonnull=0 callbacks_at_return=0
case=2 callbacks_final=0 release_null=1

CASE=3 PID=518035 EXIT=0
mpp[518035]: mpp_platform: client 12 driver is not ready!
case=3 status=1 session_nonnull=0 callbacks_at_return=0
case=3 callbacks_final=0 release_null=1

CASE=4 PID=518047 EXIT=0
mpp[518047]: mpp_platform: client 12 driver is not ready!
case=4 status=1 session_nonnull=0 callbacks_at_return=0
case=4 callbacks_final=0 release_null=1

CASE=5 PID=518055 EXIT=0
mpp[518055]: mpp_platform: client 12 driver is not ready!
case=5 status=1 session_nonnull=0 callbacks_at_return=0
case=5 callbacks_final=0 release_null=1
```

清理清单：consumer.c、consumer.o、consumer、build-commands.txt、results.txt，保留正式库与用户源。五项临时文件已精确删除，目录为空、六进程无残留。
