# rk446：C 合屏回调与准备容量边界

状态：进行中，尚未通过，不替代完整120秒媒体验收。生产源码50993608，当前RKMPP库；临时C11消费者严格编译/链接成功，实际依赖匹配隔离prefix。

## pressure 首次观察

实际命令：`/home/tang/MediaTranscode-composition-09093919/acceptance/rk446/consumer pressure`，stdout/stderr写入pressure.out/pressure.log。wrapper2186153，消费者2186157。正预算1字节，未启动媒体源。

start_status=0且句柄非空；堆配置擦除释放。ERROR为error2/stage3、detail=`Observed root storage exceeds deployment capacity`；错误回调get_snapshot成功，state5、encoded0。终态FAILED/STARTUP_FAILURE（completion4），worker_errors0；release后句柄为空。消费者漏识别该容量文本，pressure_errors0、pass0、exit1；不能将该执行标通过。

源码MediaStorageBudget.h:290–291在实际root几何超过部署capacity时返回该allocationFailed；不是源超时或输入零预算参数拒绝。修正临时消费者匹配该精确文本再复验，生产代码不变。另已修正观测竞态：finishFailure先发布FAILED快照，再发ERROR，消费者在既有看门狗内等待callback动作完成后才closing/release。

首轮清理清单：目标acceptance/rk446/pressure.out、pressure.log、pressure-pid.txt、pressure-result.txt。编译消费者仍用于后续场景，功能完成后删除。

## pressure2 复验

相同consumer pressure命令，输出改为pressure2.out/log。wrapper2198284，consumer2198285；严格重编译exit0。再次得到上述相同真实容量错误，ERROR回调snapshot_status0/state5/encoded0；action_done/once/action_ok均1，event_errors=pressure_errors=1，release_null1、pass1、exit0。client12 driver告警保留。本项仅证明root准备准入不足的早期失败和回调读取/释放，不证明深层部分准备回滚、RSS归零或媒体播放。首轮4文件已清理；本次pressure2.out、pressure2.log、pressure2-pid.txt、pressure2-result.txt按记录清理。

## Windows共享版本构建

50993608源码按VS2026技能300秒上限全核心clean-first/all-target重建，观察24个cl并行，configure/build exit0，四必需产物齐全；CLI SHA256 AAF06ED54A33D204AA08861E8B92854D6C9757A69C7AB4A3C575142AA159AF5C、7597568字节。尚未实流回归，未运行local测试、未改本机FFmpeg。

## 临时消费者实际源码

消费者不纳入生产或测试源码目录；完成后删除临时.c/o及可执行文件。

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
 atomic_int once,action_ok,action_done,event_errors,pressure_errors;
 mtx_t lock; cnd_t idle; int closing,start_done; unsigned inflight; int mode;
};
static void event(void* user,const mt_beta_realtime_event* e) {
 struct context* c=user;
 if(!e)return;
 fprintf(stderr,"event type=%d state=%d output=%"PRIu64" output_state=%d error=%d stage=%d completion=%d native=%d detail=%s\n",e->type,e->state,e->output_id,e->output_state,e->error_code,e->failure_stage,e->completion_reason,e->native_code,e->detail?e->detail:"");
 if(e->type==MT_BETA_EVENT_ERROR){atomic_fetch_add(&c->event_errors,1);if(e->detail && (strstr(e->detail,"storage request exceeds the admitted capacity") || strstr(e->detail,"storage budget reservation exceeds the admitted capacity") || strstr(e->detail,"Storage guarantees exceed") || strcmp(e->detail,"Observed root storage exceeds deployment capacity")==0))atomic_fetch_add(&c->pressure_errors,1);}
 mtx_lock(&c->lock);
 if(c->closing){mtx_unlock(&c->lock);return;}
 ++c->inflight;
 while(!c->start_done&&!c->closing)cnd_wait(&c->idle,&c->lock);
 int can_call=!c->closing;mtx_unlock(&c->lock);
 if(can_call && ((c->mode==0 && e->type==MT_BETA_EVENT_STATE_CHANGED && e->state==MT_BETA_REALTIME_RUNNING) || (c->mode==1 && e->type==MT_BETA_EVENT_ERROR))) {
  int expected=0;
  if(atomic_compare_exchange_strong(&c->once,&expected,1)) {
   mt_beta_realtime_session* session=atomic_load_explicit(&c->published,memory_order_acquire);
   mt_beta_realtime_snapshot snapshot={0};
   mt_beta_status read=session?mt_beta_realtime_get_snapshot(session,&snapshot):MT_BETA_STATUS_INVALID_STATE;
   mt_beta_status stop=MT_BETA_STATUS_OK;
   if(c->mode==0)stop=session?mt_beta_realtime_request_stop(session):MT_BETA_STATUS_INVALID_STATE;
   fprintf(stderr,"callback_action mode=%d snapshot_status=%d snapshot_state=%d encoded=%"PRIu64" stop_called=%d stop_status=%d\n",c->mode,read,snapshot.state,snapshot.encoded_packets_pushed,c->mode==0,stop);
   atomic_store(&c->action_ok,read==MT_BETA_STATUS_OK && stop==MT_BETA_STATUS_OK && (c->mode==1 || snapshot.state==MT_BETA_REALTIME_RUNNING));
   atomic_store_explicit(&c->action_done,1,memory_order_release);
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
 if(strcmp(argv[1],"running")==0)ctx.mode=0;else if(strcmp(argv[1],"pressure")==0)ctx.mode=1;else return 2;
 if(mtx_init(&ctx.lock,mtx_plain)!=thrd_success)return 2;
 if(cnd_init(&ctx.idle)!=thrd_success){mtx_destroy(&ctx.lock);return 2;}
 struct config_storage* b=calloc(1,sizeof(*b));
 mt_beta_realtime_session* session=NULL;
 mt_beta_realtime_callbacks cb={event,&ctx};
 mt_beta_realtime_snapshot s={0};
 int ok=0,timed_out=0;
 struct timespec began;clock_gettime(CLOCK_MONOTONIC,&began);
 if(!b){cnd_destroy(&ctx.idle);mtx_destroy(&ctx.lock);return 2;}
 strcpy(b->media,"composition-rk446-c-boundary");strcpy(b->loop,"127.0.0.1");strcpy(b->remote,"192.168.130.229");strcpy(b->destination,"192.168.96.122");strcpy(b->codec,"aac");
 strcpy(b->fmtp,"profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210");
 b->in[0].video=(mt_beta_rtp_video_input){b->loop,61020,MT_BETA_VIDEO_CODEC_H264,96,90000,NULL};
 b->in[1].video=(mt_beta_rtp_video_input){b->remote,61050,MT_BETA_VIDEO_CODEC_HEVC,96,90000,NULL};
 b->audio=(mt_beta_rtp_audio_input){b->loop,61022,b->codec,97,44100,2,b->fmtp};
 b->in[0].audio=&b->audio;b->in[0].input_latency_ms=200;b->in[1].input_latency_ms=250;
 b->c.media_id=b->media;b->c.inputs=b->in;b->c.input_count=2;b->c.grid=MT_BETA_COMPOSITION_GRID_FIXED;b->c.audio_source=0;b->c.audio_bitrate_bps=192000;b->c.preparation_storage_limit_bytes=ctx.mode==1?1ULL:2147483648ULL;
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
  struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
  if(now.tv_sec-began.tv_sec>=180){timed_out=1;fprintf(stderr,"consumer_watchdog_expired=1 boundary_not_passed=1\n");break;}
  st=mt_beta_realtime_get_snapshot(session,&s);if(st!=MT_BETA_STATUS_OK)break;
  fprintf(stderr,"snapshot state=%d completion=%d backend=%d codec=%d elapsed=%"PRIu64" encoded=%"PRIu64" errors=%"PRIu64" rss=%"PRIu64" cpu=%.6f\n",s.state,s.completion_reason,s.selected_backend,s.input_codec,s.running_time_ms,s.encoded_packets_pushed,s.worker_errors,s.working_set_bytes,s.average_process_single_core_cpu_percent);
  if(s.state==MT_BETA_REALTIME_COMPLETED||s.state==MT_BETA_REALTIME_FAILED){ok=ctx.mode==0?(s.state==MT_BETA_REALTIME_COMPLETED&&s.completion_reason==MT_BETA_COMPLETION_REQUESTED_STOP&&s.worker_errors==0):(s.state==MT_BETA_REALTIME_FAILED&&s.completion_reason==MT_BETA_COMPLETION_STARTUP_FAILURE&&s.encoded_packets_pushed==0&&s.worker_errors==0);
   /* Snapshot publication precedes ERROR callback dispatch in finishFailure. */
   if(!ok || atomic_load_explicit(&ctx.action_done,memory_order_acquire))break;
  }
  struct timespec delay={1,0};nanosleep(&delay,NULL);
 }
 done:
 mtx_lock(&ctx.lock);ctx.closing=1;while(ctx.inflight)cnd_wait(&ctx.idle,&ctx.lock);mtx_unlock(&ctx.lock);
 mt_beta_realtime_release(&session);
 int pass=ok&&!timed_out&&session==NULL&&atomic_load(&ctx.action_done)==1&&atomic_load(&ctx.once)==1&&atomic_load(&ctx.action_ok)&&(ctx.mode==0?atomic_load(&ctx.event_errors)==0:(atomic_load(&ctx.event_errors)==1&&atomic_load(&ctx.pressure_errors)==1));
 fprintf(stderr,"final mode=%d action_done=%d once=%d action_ok=%d event_errors=%d pressure_errors=%d release_null=%d pass=%d\n",ctx.mode,atomic_load(&ctx.action_done),atomic_load(&ctx.once),atomic_load(&ctx.action_ok),atomic_load(&ctx.event_errors),atomic_load(&ctx.pressure_errors),session==NULL,pass);
 cnd_destroy(&ctx.idle);mtx_destroy(&ctx.lock);return pass?0:1;
}

```
