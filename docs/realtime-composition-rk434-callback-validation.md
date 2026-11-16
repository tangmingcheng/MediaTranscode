# rk434：启动阶段C回调生命周期补充检查

状态：两个独立模式已实际执行，启动回调边界符合源码预期，不是完整媒体PASS。使用当前真实C库，但不启动媒体源，只补充启动阶段回调边界，不等价RUNNING回调或完整媒体门禁，不替代rk430原120秒验收。

固定合屏没有dynamic OutputRunController，不能假设remove_output可触发回调；临时源码已删除remove/armed/SIGUSR1方案，未执行该旧方案。两个独立模式：0在STARTING事件回调内request_stop；1先用局部probe调用release，要求probe保持原句柄，再request_stop。模式1明确属于公开“release须回调外”前提之外的防误用拒绝检查，不是合法契约用法。

callback进入后登记inflight，在mtx/cnd等待main发布handle/start_done；cnd_wait释放锁，start不等待callback，因此没有互锁依赖。main在start返回后volatile覆盖/释放配置，release-store句柄并广播；callback不持锁调用API且one-shot。主线程只观察snapshot至终态，外部release前closing=1并等待inflight归零；context与同步对象保活至release返回。成功判据为实际once1/action_ok1、恰一CANCELLED7/STOP8错误、FAILED5/REQUESTED_STOP2、encoded/worker_errors0及release_null1，不以未执行动作充当通过。

C11 -O2 -Wall -Wextra -Werror及当前库链接均exit0。完整最终源码与命令如下，最终源码经主端审阅修正预期后重新编译、链接均exit0。
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
#include <threads.h>
struct context {
 _Atomic(mt_beta_realtime_session*) published;
 atomic_int once,action_ok,event_errors,expected_cancellations;
 mtx_t lock; cnd_t idle; int closing,start_done; unsigned inflight; int mode;
};
static void event(void* user,const mt_beta_realtime_event* e) {
 struct context* c=user;
 if(!e)return;
 fprintf(stderr,"event type=%d state=%d output=%"PRIu64" output_state=%d error=%d stage=%d completion=%d native=%d detail=%s\n",e->type,e->state,e->output_id,e->output_state,e->error_code,e->failure_stage,e->completion_reason,e->native_code,e->detail?e->detail:"");
 if(e->type==MT_BETA_EVENT_ERROR){atomic_fetch_add(&c->event_errors,1);if(e->error_code==MT_BETA_ERROR_CANCELLED&&e->failure_stage==MT_BETA_FAILURE_STOP)atomic_fetch_add(&c->expected_cancellations,1);}
 mtx_lock(&c->lock);
 if(c->closing){mtx_unlock(&c->lock);return;}
 ++c->inflight;
 while(!c->start_done&&!c->closing)cnd_wait(&c->idle,&c->lock);
 int can_call=!c->closing;mtx_unlock(&c->lock);
 if(can_call&&e->type==MT_BETA_EVENT_STATE_CHANGED&&e->state==MT_BETA_REALTIME_STARTING) {
  int expected=0;
  if(atomic_compare_exchange_strong(&c->once,&expected,1)) {
   mt_beta_realtime_session* session=atomic_load_explicit(&c->published,memory_order_acquire);
   int kept=session!=NULL;
   if(c->mode==1&&session){mt_beta_realtime_session* probe=session;mt_beta_realtime_release(&probe);kept=probe==session;fprintf(stderr,"callback_release_unchanged=%d\n",kept);}
   mt_beta_status st=session&&kept?mt_beta_realtime_request_stop(session):MT_BETA_STATUS_INVALID_STATE;
   fprintf(stderr,"callback_action mode=%d stop_status=%d handle_nonnull=%d\n",c->mode,st,session!=NULL);
   atomic_store(&c->action_ok,kept&&st==MT_BETA_STATUS_OK);
  }
 }
 mtx_lock(&c->lock);--c->inflight;if(!c->inflight)cnd_broadcast(&c->idle);mtx_unlock(&c->lock);
}
struct config_storage {
 mt_beta_realtime_composition_config c;
 mt_beta_composition_input in[2];
 mt_beta_rtp_audio_input audio;
 char media[64],loop[32],remote[32],destination[32],codec[8],fmtp[160];
};
int main(int argc,char** argv){
 if(argc!=2)return 2;
 struct context ctx={0};
 ctx.mode=atoi(argv[1]);if(ctx.mode<0||ctx.mode>1)return 2;
 if(mtx_init(&ctx.lock,mtx_plain)!=thrd_success)return 2;
 if(cnd_init(&ctx.idle)!=thrd_success){mtx_destroy(&ctx.lock);return 2;}
 struct config_storage* b=calloc(1,sizeof(*b));
 mt_beta_realtime_session* session=NULL;
 mt_beta_realtime_callbacks cb={event,&ctx};
 mt_beta_realtime_snapshot s={0};
 int ok=0;
 if(!b) return 2;
 strcpy(b->media,"composition-rk434-callback");strcpy(b->loop,"127.0.0.1");strcpy(b->remote,"192.168.130.229");strcpy(b->destination,"192.168.96.122");strcpy(b->codec,"aac");
 strcpy(b->fmtp,"profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210");
 b->in[0].video=(mt_beta_rtp_video_input){b->loop,61020,MT_BETA_VIDEO_CODEC_H264,96,90000,NULL};
 b->in[1].video=(mt_beta_rtp_video_input){b->remote,61050,MT_BETA_VIDEO_CODEC_HEVC,96,90000,NULL};
 b->audio=(mt_beta_rtp_audio_input){b->loop,61022,b->codec,97,44100,2,b->fmtp};
 b->in[0].audio=&b->audio;b->in[0].input_latency_ms=200;b->in[1].input_latency_ms=250;
 b->c.media_id=b->media;b->c.inputs=b->in;b->c.input_count=2;b->c.grid=MT_BETA_COMPOSITION_GRID_FIXED;b->c.audio_source=0;b->c.audio_bitrate_bps=192000;b->c.preparation_storage_limit_bytes=2147483648ULL;
 b->c.output.protocol=MT_BETA_OUTPUT_MPEGTS_UDP;b->c.output.destination_address=b->destination;b->c.output.destination_port=62024;b->c.output.codec=MT_BETA_VIDEO_CODEC_HEVC;b->c.output.width=1280;b->c.output.height=720;b->c.output.frame_rate_num=30;b->c.output.frame_rate_den=1;b->c.output.gop_frames=60;b->c.output.rate_control_mode=MT_BETA_RATE_CONTROL_CBR;b->c.output.rate_control.cbr.bitrate_bps=8000000;
 b->c.deployment.provisioned_egress_capacity_bps=50000000;b->c.deployment.maximum_wire_residence_ms=100;
 mt_beta_status st=mt_beta_realtime_start_composition(&b->c,&cb,&session);
 volatile unsigned char* wipe=(volatile unsigned char*)b;
 for(size_t i=0;i<sizeof(*b);++i)wipe[i]=0xa5;
 free(b);b=NULL;
 fprintf(stderr,"start_status=%d handle_nonnull=%d config_destroyed=1\n",st,session!=NULL);
 mtx_lock(&ctx.lock);
 atomic_store_explicit(&ctx.published,session,memory_order_release);ctx.start_done=1;cnd_broadcast(&ctx.idle);mtx_unlock(&ctx.lock);
 if(st!=MT_BETA_STATUS_OK||session==NULL)goto done;
 for(;;){
  st=mt_beta_realtime_get_snapshot(session,&s);if(st!=MT_BETA_STATUS_OK)break;
  fprintf(stderr,"snapshot state=%d completion=%d backend=%d codec=%d elapsed=%"PRIu64" encoded=%"PRIu64" errors=%"PRIu64" rss=%"PRIu64" cpu=%.6f\n",s.state,s.completion_reason,s.selected_backend,s.input_codec,s.running_time_ms,s.encoded_packets_pushed,s.worker_errors,s.working_set_bytes,s.average_process_single_core_cpu_percent);
  if(s.state==MT_BETA_REALTIME_COMPLETED||s.state==MT_BETA_REALTIME_FAILED){ok=s.state==MT_BETA_REALTIME_FAILED&&s.completion_reason==MT_BETA_COMPLETION_REQUESTED_STOP&&s.worker_errors==0&&s.encoded_packets_pushed==0;break;}
  struct timespec delay={1,0};nanosleep(&delay,NULL);
 }
 done:
 mtx_lock(&ctx.lock);ctx.closing=1;while(ctx.inflight)cnd_wait(&ctx.idle,&ctx.lock);mtx_unlock(&ctx.lock);
 mt_beta_realtime_release(&session);
 int pass=ok&&session==NULL&&atomic_load(&ctx.once)==1&&atomic_load(&ctx.action_ok)&&atomic_load(&ctx.event_errors)==1&&atomic_load(&ctx.expected_cancellations)==1;
 fprintf(stderr,"final mode=%d once=%d action_ok=%d event_errors=%d expected_cancellations=%d release_null=%d pass=%d\n",ctx.mode,atomic_load(&ctx.once),atomic_load(&ctx.action_ok),atomic_load(&ctx.event_errors),atomic_load(&ctx.expected_cancellations),session==NULL,pass);
 cnd_destroy(&ctx.idle);mtx_destroy(&ctx.lock);return pass?0:1;
}
```

```bash
/opt/mt-tools/gcc-12.4-native/bin/gcc -std=c11 -O2 -Wall -Wextra -Werror -I/home/tang/MediaTranscode-composition-09093919/include -c /home/tang/MediaTranscode-composition-09093919/acceptance/rk434/consumer.c -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk434/consumer.o
/opt/mt-tools/gcc-12.4-native/bin/g++ /home/tang/MediaTranscode-composition-09093919/acceptance/rk434/consumer.o -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk434/consumer /home/tang/MediaTranscode-composition-09093919/out/build/rk-release/libmedia_transcode_beta.a libmedia_transcode_realtime_application.a libmedia_transcode_core.a /usr/local/lib/aarch64-linux-gnu/librga.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavfilter.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavformat.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libswscale.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libswresample.so -lpthread
```

consumer SHA256 ff15df681639585b984c9c9ce886eee29e0a154ea242bac23282b10748342647

```text
MODE=0 PID=586695 EXIT=0
mpp[586695]: mpp_platform: client 12 driver is not ready!
start_status=0 handle_nonnull=1 config_destroyed=1
snapshot state=1 completion=0 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
event type=1 state=1 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
callback_action mode=0 stop_status=0 handle_nonnull=1
event type=3 state=0 output=0 output_state=0 error=7 stage=8 completion=0 native=0 detail=realtime video run stop was requested before preflight
event type=1 state=5 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
event type=4 state=0 output=0 output_state=0 error=0 stage=0 completion=2 native=0 detail=
snapshot state=5 completion=2 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
final mode=0 once=1 action_ok=1 event_errors=1 expected_cancellations=1 release_null=1 pass=1

MODE=1 PID=586863 EXIT=0
mpp[586863]: mpp_platform: client 12 driver is not ready!
start_status=0 handle_nonnull=1 config_destroyed=1
snapshot state=1 completion=0 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
event type=1 state=1 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
callback_release_unchanged=1
callback_action mode=1 stop_status=0 handle_nonnull=1
event type=3 state=0 output=0 output_state=0 error=7 stage=8 completion=0 native=0 detail=realtime video run stop was requested before preflight
event type=1 state=5 output=0 output_state=0 error=0 stage=0 completion=0 native=0 detail=
event type=4 state=0 output=0 output_state=0 error=0 stage=0 completion=2 native=0 detail=
snapshot state=5 completion=2 backend=0 codec=0 elapsed=0 encoded=0 errors=0 rss=0 cpu=0.000000
final mode=1 once=1 action_ok=1 event_errors=1 expected_cancellations=1 release_null=1 pass=1
```
## 实际执行与边界

实际命令分别为/home/tang/MediaTranscode-composition-09093919/acceptance/rk434/consumer 0与同路径consumer 1，均由subprocess启动并communicate(timeout=15)取完整输出。PID586695/586863各exit0，未启动FFmpeg/VLC或媒体源。C API start_status0、非空句柄、配置销毁后回调one-shot均执行；模式B的callback_release_unchanged1证明局部probe未被删除，随后request_stop0。

两模式均恰一ERROR_CANCELLED7/FAILURE_STOP8事件，detail为realtime video run stop was requested before preflight；终态STATE_FAILED5/COMPLETION_REQUESTED_STOP2，encoded/worker_errors0，once/action_ok1、event_errors/expected_cancellations1、release_null1。COMPLETED事件type4不表示state=COMPLETED，错误没有隐藏；消费者exit0表示满足本次明确启动边界预期，不是媒体运行成功。

源码依据：Session先发布STARTING事件，controller在preflight前读stopRequested返回Cancelled/CallerStop，Beta现有handleOutcome投影为FAILED/STOP/REQUESTED_STOP。回调内release为公开前提之外防误用检查，不是建议调用方式。没有覆盖运行期回调停止，也不替代rk430完整源实流。

清理清单：consumer.c、consumer.o、consumer、build-commands.txt、results.txt。五项已精确删除，目录为空，两PID无残留。