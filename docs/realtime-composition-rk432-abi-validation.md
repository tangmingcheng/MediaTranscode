# rk432：旧C ABI目标机布局核验

状态：目标aarch64/GCC12.4同编译器布局与声明对照通过，不等于旧消费者已真实运行。旧头来自本机git show038f069c:include/media_transcode_beta/realtime.h，当前头来自工作树；未从网络猜版本，无生产修改。

自动从旧头展开13完整struct与13enum的sizeof/_Alignof；132字段路径的offsetof/sizeof（包含两个匿名union本体、每variant、全部递归叶字段及realtime_config嵌套）；73枚举常量数值；10旧函数以精确函数指针赋值，使用-Werror=incompatible-pointer-types编译。opaque session/output_snapshots不做非法sizeof。

旧/当前各编译layout与signatures，4条命令全部exit0；两个layout分别运行exit0，各231行输出逐字节相等。signature对象只编译不链接；只证明头声明可赋给旧精确函数类型，不冒充运行时ABI验证。以下完整清单即实际源码，结果只列一份相同输出。
旧头SHA256 5ba348ea3b8227b10416267529296325ce4d6d7801aced6a7f2241eb00e65431
当前头SHA256 793122af918c6522442799e7d90d3f9eed3b4442a2772365a9f9ac1f6c006bcd

layout.c
```c
#include "media_transcode_beta/realtime.h"
#include <stdio.h>
#define T(t) printf("T %s %zu %zu\n",#t,sizeof(t),_Alignof(t))
#define F(t,f) printf("F %s.%s %zu %zu\n",#t,#f,offsetof(t,f),sizeof(((t*)0)->f))
#define E(e) printf("E %s %d\n",#e,(int)e)
int main(void){
T(mt_beta_rtp_video_input);
T(mt_beta_url_video_input);
T(mt_beta_mpegts_udp_video_input);
T(mt_beta_realtime_input);
T(mt_beta_cbr);
T(mt_beta_vbr);
T(mt_beta_video_output);
T(mt_beta_realtime_deployment);
T(mt_beta_realtime_config);
T(mt_beta_realtime_callbacks);
T(mt_beta_realtime_event);
T(mt_beta_realtime_snapshot);
T(mt_beta_output_snapshot);
T(mt_beta_video_codec);
T(mt_beta_rate_control_mode);
T(mt_beta_status);
T(mt_beta_realtime_state);
T(mt_beta_error_code);
T(mt_beta_failure_stage);
T(mt_beta_completion_reason);
T(mt_beta_realtime_event_type);
T(mt_beta_selected_backend);
T(mt_beta_selected_filter);
T(mt_beta_realtime_input_kind);
T(mt_beta_video_output_protocol);
T(mt_beta_output_state);
F(mt_beta_rtp_video_input,bind_address);
F(mt_beta_rtp_video_input,port);
F(mt_beta_rtp_video_input,codec);
F(mt_beta_rtp_video_input,payload_type);
F(mt_beta_rtp_video_input,clock_rate);
F(mt_beta_rtp_video_input,fmtp);
F(mt_beta_url_video_input,url);
F(mt_beta_url_video_input,rtsp_transport);
F(mt_beta_mpegts_udp_video_input,url);
F(mt_beta_mpegts_udp_video_input,maximum_pcr_gap_ms);
F(mt_beta_realtime_input,kind);
F(mt_beta_realtime_input,source);
F(mt_beta_realtime_input,source.rtp);
F(mt_beta_realtime_input,source.rtp.bind_address);
F(mt_beta_realtime_input,source.rtp.port);
F(mt_beta_realtime_input,source.rtp.codec);
F(mt_beta_realtime_input,source.rtp.payload_type);
F(mt_beta_realtime_input,source.rtp.clock_rate);
F(mt_beta_realtime_input,source.rtp.fmtp);
F(mt_beta_realtime_input,source.url);
F(mt_beta_realtime_input,source.url.url);
F(mt_beta_realtime_input,source.url.rtsp_transport);
F(mt_beta_realtime_input,source.mpegts_udp);
F(mt_beta_realtime_input,source.mpegts_udp.url);
F(mt_beta_realtime_input,source.mpegts_udp.maximum_pcr_gap_ms);
F(mt_beta_cbr,bitrate_bps);
F(mt_beta_vbr,target_bitrate_bps);
F(mt_beta_vbr,min_bitrate_bps);
F(mt_beta_vbr,max_bitrate_bps);
F(mt_beta_video_output,protocol);
F(mt_beta_video_output,destination_address);
F(mt_beta_video_output,destination_port);
F(mt_beta_video_output,codec);
F(mt_beta_video_output,width);
F(mt_beta_video_output,height);
F(mt_beta_video_output,frame_rate_num);
F(mt_beta_video_output,frame_rate_den);
F(mt_beta_video_output,gop_frames);
F(mt_beta_video_output,rate_control_mode);
F(mt_beta_video_output,rate_control);
F(mt_beta_video_output,rate_control.cbr);
F(mt_beta_video_output,rate_control.cbr.bitrate_bps);
F(mt_beta_video_output,rate_control.vbr);
F(mt_beta_video_output,rate_control.vbr.target_bitrate_bps);
F(mt_beta_video_output,rate_control.vbr.min_bitrate_bps);
F(mt_beta_video_output,rate_control.vbr.max_bitrate_bps);
F(mt_beta_video_output,profile);
F(mt_beta_realtime_deployment,provisioned_egress_capacity_bps);
F(mt_beta_realtime_deployment,maximum_wire_residence_ms);
F(mt_beta_realtime_config,media_id);
F(mt_beta_realtime_config,input);
F(mt_beta_realtime_config,input.kind);
F(mt_beta_realtime_config,input.source);
F(mt_beta_realtime_config,input.source.rtp);
F(mt_beta_realtime_config,input.source.rtp.bind_address);
F(mt_beta_realtime_config,input.source.rtp.port);
F(mt_beta_realtime_config,input.source.rtp.codec);
F(mt_beta_realtime_config,input.source.rtp.payload_type);
F(mt_beta_realtime_config,input.source.rtp.clock_rate);
F(mt_beta_realtime_config,input.source.rtp.fmtp);
F(mt_beta_realtime_config,input.source.url);
F(mt_beta_realtime_config,input.source.url.url);
F(mt_beta_realtime_config,input.source.url.rtsp_transport);
F(mt_beta_realtime_config,input.source.mpegts_udp);
F(mt_beta_realtime_config,input.source.mpegts_udp.url);
F(mt_beta_realtime_config,input.source.mpegts_udp.maximum_pcr_gap_ms);
F(mt_beta_realtime_config,output);
F(mt_beta_realtime_config,output.protocol);
F(mt_beta_realtime_config,output.destination_address);
F(mt_beta_realtime_config,output.destination_port);
F(mt_beta_realtime_config,output.codec);
F(mt_beta_realtime_config,output.width);
F(mt_beta_realtime_config,output.height);
F(mt_beta_realtime_config,output.frame_rate_num);
F(mt_beta_realtime_config,output.frame_rate_den);
F(mt_beta_realtime_config,output.gop_frames);
F(mt_beta_realtime_config,output.rate_control_mode);
F(mt_beta_realtime_config,output.rate_control);
F(mt_beta_realtime_config,output.rate_control.cbr);
F(mt_beta_realtime_config,output.rate_control.cbr.bitrate_bps);
F(mt_beta_realtime_config,output.rate_control.vbr);
F(mt_beta_realtime_config,output.rate_control.vbr.target_bitrate_bps);
F(mt_beta_realtime_config,output.rate_control.vbr.min_bitrate_bps);
F(mt_beta_realtime_config,output.rate_control.vbr.max_bitrate_bps);
F(mt_beta_realtime_config,output.profile);
F(mt_beta_realtime_config,deployment);
F(mt_beta_realtime_config,deployment.provisioned_egress_capacity_bps);
F(mt_beta_realtime_config,deployment.maximum_wire_residence_ms);
F(mt_beta_realtime_callbacks,on_event);
F(mt_beta_realtime_callbacks,user_data);
F(mt_beta_realtime_event,type);
F(mt_beta_realtime_event,state);
F(mt_beta_realtime_event,error_code);
F(mt_beta_realtime_event,failure_stage);
F(mt_beta_realtime_event,completion_reason);
F(mt_beta_realtime_event,native_code);
F(mt_beta_realtime_event,detail);
F(mt_beta_realtime_event,output_description_path);
F(mt_beta_realtime_event,output_id);
F(mt_beta_realtime_event,output_state);
F(mt_beta_realtime_snapshot,state);
F(mt_beta_realtime_snapshot,completion_reason);
F(mt_beta_realtime_snapshot,selected_backend);
F(mt_beta_realtime_snapshot,input_codec);
F(mt_beta_realtime_snapshot,running_time_ms);
F(mt_beta_realtime_snapshot,queued_buffers);
F(mt_beta_realtime_snapshot,peak_queued_buffers);
F(mt_beta_realtime_snapshot,worker_progress);
F(mt_beta_realtime_snapshot,worker_process_calls);
F(mt_beta_realtime_snapshot,worker_waits);
F(mt_beta_realtime_snapshot,worker_wakeups);
F(mt_beta_realtime_snapshot,worker_errors);
F(mt_beta_realtime_snapshot,stalled_intervals);
F(mt_beta_realtime_snapshot,total_pushed);
F(mt_beta_realtime_snapshot,total_popped);
F(mt_beta_realtime_snapshot,dropped_buffers);
F(mt_beta_realtime_snapshot,encoded_packets_pushed);
F(mt_beta_realtime_snapshot,encoded_packets_popped);
F(mt_beta_realtime_snapshot,working_set_bytes);
F(mt_beta_realtime_snapshot,peak_working_set_bytes);
F(mt_beta_realtime_snapshot,logical_processor_count);
F(mt_beta_realtime_snapshot,average_process_machine_cpu_percent);
F(mt_beta_realtime_snapshot,peak_process_machine_cpu_percent);
F(mt_beta_realtime_snapshot,average_process_single_core_cpu_percent);
F(mt_beta_realtime_snapshot,peak_process_single_core_cpu_percent);
F(mt_beta_output_snapshot,output_id);
F(mt_beta_output_snapshot,state);
F(mt_beta_output_snapshot,error_code);
F(mt_beta_output_snapshot,failure_stage);
F(mt_beta_output_snapshot,native_code);
F(mt_beta_output_snapshot,detail);
F(mt_beta_output_snapshot,output_description_path);
E(MT_BETA_VIDEO_CODEC_UNKNOWN);
E(MT_BETA_VIDEO_CODEC_H264);
E(MT_BETA_VIDEO_CODEC_HEVC);
E(MT_BETA_RATE_CONTROL_CBR);
E(MT_BETA_RATE_CONTROL_VBR);
E(MT_BETA_STATUS_OK);
E(MT_BETA_STATUS_INVALID_ARGUMENT);
E(MT_BETA_STATUS_ALLOCATION_FAILED);
E(MT_BETA_STATUS_INVALID_STATE);
E(MT_BETA_STATUS_INTERNAL_ERROR);
E(MT_BETA_STATUS_BUSY);
E(MT_BETA_REALTIME_STARTING);
E(MT_BETA_REALTIME_RUNNING);
E(MT_BETA_REALTIME_STOPPING);
E(MT_BETA_REALTIME_COMPLETED);
E(MT_BETA_REALTIME_FAILED);
E(MT_BETA_ERROR_NONE);
E(MT_BETA_ERROR_INVALID_ARGUMENT);
E(MT_BETA_ERROR_ALLOCATION_FAILED);
E(MT_BETA_ERROR_UNSUPPORTED);
E(MT_BETA_ERROR_HARDWARE_UNAVAILABLE);
E(MT_BETA_ERROR_IO_FAILURE);
E(MT_BETA_ERROR_FFMPEG_FAILURE);
E(MT_BETA_ERROR_CANCELLED);
E(MT_BETA_ERROR_INTERNAL);
E(MT_BETA_ERROR_WOULD_BLOCK);
E(MT_BETA_ERROR_NOT_INITIALIZED);
E(MT_BETA_FAILURE_NONE);
E(MT_BETA_FAILURE_INPUT_VALIDATION);
E(MT_BETA_FAILURE_SESSION_CREATION);
E(MT_BETA_FAILURE_PREFLIGHT);
E(MT_BETA_FAILURE_CAPABILITY);
E(MT_BETA_FAILURE_GRAPH_BUILD);
E(MT_BETA_FAILURE_RUNTIME_START);
E(MT_BETA_FAILURE_RUNTIME_EXECUTION);
E(MT_BETA_FAILURE_STOP);
E(MT_BETA_FAILURE_RELEASE);
E(MT_BETA_COMPLETION_NONE);
E(MT_BETA_COMPLETION_SOURCE_COMPLETED);
E(MT_BETA_COMPLETION_REQUESTED_STOP);
E(MT_BETA_COMPLETION_SOURCE_LOSS);
E(MT_BETA_COMPLETION_STARTUP_FAILURE);
E(MT_BETA_COMPLETION_RUNTIME_FAILURE);
E(MT_BETA_EVENT_STATE_CHANGED);
E(MT_BETA_EVENT_OUTPUT_READY);
E(MT_BETA_EVENT_ERROR);
E(MT_BETA_EVENT_COMPLETED);
E(MT_BETA_EVENT_OUTPUT_STATE_CHANGED);
E(MT_BETA_BACKEND_UNKNOWN);
E(MT_BETA_BACKEND_NONE);
E(MT_BETA_BACKEND_D3D11VA);
E(MT_BETA_BACKEND_QSV);
E(MT_BETA_BACKEND_CUDA);
E(MT_BETA_BACKEND_VAAPI);
E(MT_BETA_BACKEND_RKMPP);
E(MT_BETA_BACKEND_VIDEOTOOLBOX);
E(MT_BETA_BACKEND_MEDIACODEC);
E(MT_BETA_FILTER_UNKNOWN);
E(MT_BETA_FILTER_NONE);
E(MT_BETA_FILTER_RGA);
E(MT_BETA_FILTER_HARDWARE);
E(MT_BETA_INPUT_RTP_VIDEO);
E(MT_BETA_INPUT_URL);
E(MT_BETA_INPUT_MPEGTS_UDP);
E(MT_BETA_OUTPUT_ELEMENTARY_RTP);
E(MT_BETA_OUTPUT_MPEGTS_UDP);
E(MT_BETA_OUTPUT_MPEGTS_RTP);
E(MT_BETA_OUTPUT_PREPARING);
E(MT_BETA_OUTPUT_WAITING_FOR_RANDOM_ACCESS);
E(MT_BETA_OUTPUT_RUNNING);
E(MT_BETA_OUTPUT_DRAINING);
E(MT_BETA_OUTPUT_RETIRED);
E(MT_BETA_OUTPUT_FAILED);
return 0;
}
```

signatures.c
```c
#include "media_transcode_beta/realtime.h"
mt_beta_status (*check_mt_beta_realtime_add_output)(
    mt_beta_realtime_session* session,
    const mt_beta_video_output* output,
    uint64_t* output_id) = mt_beta_realtime_add_output;
mt_beta_status (*check_mt_beta_realtime_remove_output)(
    mt_beta_realtime_session* session,
    uint64_t output_id) = mt_beta_realtime_remove_output;
mt_beta_status (*check_mt_beta_realtime_get_output_snapshots)(
    mt_beta_realtime_session* session,
    mt_beta_output_snapshots** snapshots) = mt_beta_realtime_get_output_snapshots;
size_t (*check_mt_beta_output_snapshots_count)(const mt_beta_output_snapshots* snapshots) = mt_beta_output_snapshots_count;
mt_beta_status (*check_mt_beta_output_snapshots_get)(
    const mt_beta_output_snapshots* snapshots,
    size_t index,
    mt_beta_output_snapshot* snapshot) = mt_beta_output_snapshots_get;
void (*check_mt_beta_output_snapshots_release)(mt_beta_output_snapshots** snapshots) = mt_beta_output_snapshots_release;
mt_beta_status (*check_mt_beta_realtime_start)(
    const mt_beta_realtime_config* config,
    const mt_beta_realtime_callbacks* callbacks,
    mt_beta_realtime_session** session) = mt_beta_realtime_start;
mt_beta_status (*check_mt_beta_realtime_request_stop)(mt_beta_realtime_session* session) = mt_beta_realtime_request_stop;
mt_beta_status (*check_mt_beta_realtime_get_snapshot)(
    mt_beta_realtime_session* session,
    mt_beta_realtime_snapshot* snapshot) = mt_beta_realtime_get_snapshot;
void (*check_mt_beta_realtime_release)(mt_beta_realtime_session** session) = mt_beta_realtime_release;
```

old.txt
```text
T mt_beta_rtp_video_input 32 8
T mt_beta_url_video_input 16 8
T mt_beta_mpegts_udp_video_input 16 8
T mt_beta_realtime_input 40 8
T mt_beta_cbr 8 8
T mt_beta_vbr 24 8
T mt_beta_video_output 80 8
T mt_beta_realtime_deployment 16 8
T mt_beta_realtime_config 144 8
T mt_beta_realtime_callbacks 16 8
T mt_beta_realtime_event 56 8
T mt_beta_realtime_snapshot 184 8
T mt_beta_output_snapshot 40 8
T mt_beta_video_codec 4 4
T mt_beta_rate_control_mode 4 4
T mt_beta_status 4 4
T mt_beta_realtime_state 4 4
T mt_beta_error_code 4 4
T mt_beta_failure_stage 4 4
T mt_beta_completion_reason 4 4
T mt_beta_realtime_event_type 4 4
T mt_beta_selected_backend 4 4
T mt_beta_selected_filter 4 4
T mt_beta_realtime_input_kind 4 4
T mt_beta_video_output_protocol 4 4
T mt_beta_output_state 4 4
F mt_beta_rtp_video_input.bind_address 0 8
F mt_beta_rtp_video_input.port 8 2
F mt_beta_rtp_video_input.codec 12 4
F mt_beta_rtp_video_input.payload_type 16 1
F mt_beta_rtp_video_input.clock_rate 20 4
F mt_beta_rtp_video_input.fmtp 24 8
F mt_beta_url_video_input.url 0 8
F mt_beta_url_video_input.rtsp_transport 8 8
F mt_beta_mpegts_udp_video_input.url 0 8
F mt_beta_mpegts_udp_video_input.maximum_pcr_gap_ms 8 4
F mt_beta_realtime_input.kind 0 4
F mt_beta_realtime_input.source 8 32
F mt_beta_realtime_input.source.rtp 8 32
F mt_beta_realtime_input.source.rtp.bind_address 8 8
F mt_beta_realtime_input.source.rtp.port 16 2
F mt_beta_realtime_input.source.rtp.codec 20 4
F mt_beta_realtime_input.source.rtp.payload_type 24 1
F mt_beta_realtime_input.source.rtp.clock_rate 28 4
F mt_beta_realtime_input.source.rtp.fmtp 32 8
F mt_beta_realtime_input.source.url 8 16
F mt_beta_realtime_input.source.url.url 8 8
F mt_beta_realtime_input.source.url.rtsp_transport 16 8
F mt_beta_realtime_input.source.mpegts_udp 8 16
F mt_beta_realtime_input.source.mpegts_udp.url 8 8
F mt_beta_realtime_input.source.mpegts_udp.maximum_pcr_gap_ms 16 4
F mt_beta_cbr.bitrate_bps 0 8
F mt_beta_vbr.target_bitrate_bps 0 8
F mt_beta_vbr.min_bitrate_bps 8 8
F mt_beta_vbr.max_bitrate_bps 16 8
F mt_beta_video_output.protocol 0 4
F mt_beta_video_output.destination_address 8 8
F mt_beta_video_output.destination_port 16 2
F mt_beta_video_output.codec 20 4
F mt_beta_video_output.width 24 4
F mt_beta_video_output.height 28 4
F mt_beta_video_output.frame_rate_num 32 4
F mt_beta_video_output.frame_rate_den 36 4
F mt_beta_video_output.gop_frames 40 4
F mt_beta_video_output.rate_control_mode 44 4
F mt_beta_video_output.rate_control 48 24
F mt_beta_video_output.rate_control.cbr 48 8
F mt_beta_video_output.rate_control.cbr.bitrate_bps 48 8
F mt_beta_video_output.rate_control.vbr 48 24
F mt_beta_video_output.rate_control.vbr.target_bitrate_bps 48 8
F mt_beta_video_output.rate_control.vbr.min_bitrate_bps 56 8
F mt_beta_video_output.rate_control.vbr.max_bitrate_bps 64 8
F mt_beta_video_output.profile 72 8
F mt_beta_realtime_deployment.provisioned_egress_capacity_bps 0 8
F mt_beta_realtime_deployment.maximum_wire_residence_ms 8 4
F mt_beta_realtime_config.media_id 0 8
F mt_beta_realtime_config.input 8 40
F mt_beta_realtime_config.input.kind 8 4
F mt_beta_realtime_config.input.source 16 32
F mt_beta_realtime_config.input.source.rtp 16 32
F mt_beta_realtime_config.input.source.rtp.bind_address 16 8
F mt_beta_realtime_config.input.source.rtp.port 24 2
F mt_beta_realtime_config.input.source.rtp.codec 28 4
F mt_beta_realtime_config.input.source.rtp.payload_type 32 1
F mt_beta_realtime_config.input.source.rtp.clock_rate 36 4
F mt_beta_realtime_config.input.source.rtp.fmtp 40 8
F mt_beta_realtime_config.input.source.url 16 16
F mt_beta_realtime_config.input.source.url.url 16 8
F mt_beta_realtime_config.input.source.url.rtsp_transport 24 8
F mt_beta_realtime_config.input.source.mpegts_udp 16 16
F mt_beta_realtime_config.input.source.mpegts_udp.url 16 8
F mt_beta_realtime_config.input.source.mpegts_udp.maximum_pcr_gap_ms 24 4
F mt_beta_realtime_config.output 48 80
F mt_beta_realtime_config.output.protocol 48 4
F mt_beta_realtime_config.output.destination_address 56 8
F mt_beta_realtime_config.output.destination_port 64 2
F mt_beta_realtime_config.output.codec 68 4
F mt_beta_realtime_config.output.width 72 4
F mt_beta_realtime_config.output.height 76 4
F mt_beta_realtime_config.output.frame_rate_num 80 4
F mt_beta_realtime_config.output.frame_rate_den 84 4
F mt_beta_realtime_config.output.gop_frames 88 4
F mt_beta_realtime_config.output.rate_control_mode 92 4
F mt_beta_realtime_config.output.rate_control 96 24
F mt_beta_realtime_config.output.rate_control.cbr 96 8
F mt_beta_realtime_config.output.rate_control.cbr.bitrate_bps 96 8
F mt_beta_realtime_config.output.rate_control.vbr 96 24
F mt_beta_realtime_config.output.rate_control.vbr.target_bitrate_bps 96 8
F mt_beta_realtime_config.output.rate_control.vbr.min_bitrate_bps 104 8
F mt_beta_realtime_config.output.rate_control.vbr.max_bitrate_bps 112 8
F mt_beta_realtime_config.output.profile 120 8
F mt_beta_realtime_config.deployment 128 16
F mt_beta_realtime_config.deployment.provisioned_egress_capacity_bps 128 8
F mt_beta_realtime_config.deployment.maximum_wire_residence_ms 136 4
F mt_beta_realtime_callbacks.on_event 0 8
F mt_beta_realtime_callbacks.user_data 8 8
F mt_beta_realtime_event.type 0 4
F mt_beta_realtime_event.state 4 4
F mt_beta_realtime_event.error_code 8 4
F mt_beta_realtime_event.failure_stage 12 4
F mt_beta_realtime_event.completion_reason 16 4
F mt_beta_realtime_event.native_code 20 4
F mt_beta_realtime_event.detail 24 8
F mt_beta_realtime_event.output_description_path 32 8
F mt_beta_realtime_event.output_id 40 8
F mt_beta_realtime_event.output_state 48 4
F mt_beta_realtime_snapshot.state 0 4
F mt_beta_realtime_snapshot.completion_reason 4 4
F mt_beta_realtime_snapshot.selected_backend 8 4
F mt_beta_realtime_snapshot.input_codec 12 4
F mt_beta_realtime_snapshot.running_time_ms 16 8
F mt_beta_realtime_snapshot.queued_buffers 24 8
F mt_beta_realtime_snapshot.peak_queued_buffers 32 8
F mt_beta_realtime_snapshot.worker_progress 40 8
F mt_beta_realtime_snapshot.worker_process_calls 48 8
F mt_beta_realtime_snapshot.worker_waits 56 8
F mt_beta_realtime_snapshot.worker_wakeups 64 8
F mt_beta_realtime_snapshot.worker_errors 72 8
F mt_beta_realtime_snapshot.stalled_intervals 80 8
F mt_beta_realtime_snapshot.total_pushed 88 8
F mt_beta_realtime_snapshot.total_popped 96 8
F mt_beta_realtime_snapshot.dropped_buffers 104 8
F mt_beta_realtime_snapshot.encoded_packets_pushed 112 8
F mt_beta_realtime_snapshot.encoded_packets_popped 120 8
F mt_beta_realtime_snapshot.working_set_bytes 128 8
F mt_beta_realtime_snapshot.peak_working_set_bytes 136 8
F mt_beta_realtime_snapshot.logical_processor_count 144 4
F mt_beta_realtime_snapshot.average_process_machine_cpu_percent 152 8
F mt_beta_realtime_snapshot.peak_process_machine_cpu_percent 160 8
F mt_beta_realtime_snapshot.average_process_single_core_cpu_percent 168 8
F mt_beta_realtime_snapshot.peak_process_single_core_cpu_percent 176 8
F mt_beta_output_snapshot.output_id 0 8
F mt_beta_output_snapshot.state 8 4
F mt_beta_output_snapshot.error_code 12 4
F mt_beta_output_snapshot.failure_stage 16 4
F mt_beta_output_snapshot.native_code 20 4
F mt_beta_output_snapshot.detail 24 8
F mt_beta_output_snapshot.output_description_path 32 8
E MT_BETA_VIDEO_CODEC_UNKNOWN 0
E MT_BETA_VIDEO_CODEC_H264 1
E MT_BETA_VIDEO_CODEC_HEVC 2
E MT_BETA_RATE_CONTROL_CBR 1
E MT_BETA_RATE_CONTROL_VBR 2
E MT_BETA_STATUS_OK 0
E MT_BETA_STATUS_INVALID_ARGUMENT 1
E MT_BETA_STATUS_ALLOCATION_FAILED 2
E MT_BETA_STATUS_INVALID_STATE 3
E MT_BETA_STATUS_INTERNAL_ERROR 4
E MT_BETA_STATUS_BUSY 5
E MT_BETA_REALTIME_STARTING 1
E MT_BETA_REALTIME_RUNNING 2
E MT_BETA_REALTIME_STOPPING 3
E MT_BETA_REALTIME_COMPLETED 4
E MT_BETA_REALTIME_FAILED 5
E MT_BETA_ERROR_NONE 0
E MT_BETA_ERROR_INVALID_ARGUMENT 1
E MT_BETA_ERROR_ALLOCATION_FAILED 2
E MT_BETA_ERROR_UNSUPPORTED 3
E MT_BETA_ERROR_HARDWARE_UNAVAILABLE 4
E MT_BETA_ERROR_IO_FAILURE 5
E MT_BETA_ERROR_FFMPEG_FAILURE 6
E MT_BETA_ERROR_CANCELLED 7
E MT_BETA_ERROR_INTERNAL 8
E MT_BETA_ERROR_WOULD_BLOCK 9
E MT_BETA_ERROR_NOT_INITIALIZED 10
E MT_BETA_FAILURE_NONE 0
E MT_BETA_FAILURE_INPUT_VALIDATION 1
E MT_BETA_FAILURE_SESSION_CREATION 2
E MT_BETA_FAILURE_PREFLIGHT 3
E MT_BETA_FAILURE_CAPABILITY 4
E MT_BETA_FAILURE_GRAPH_BUILD 5
E MT_BETA_FAILURE_RUNTIME_START 6
E MT_BETA_FAILURE_RUNTIME_EXECUTION 7
E MT_BETA_FAILURE_STOP 8
E MT_BETA_FAILURE_RELEASE 9
E MT_BETA_COMPLETION_NONE 0
E MT_BETA_COMPLETION_SOURCE_COMPLETED 1
E MT_BETA_COMPLETION_REQUESTED_STOP 2
E MT_BETA_COMPLETION_SOURCE_LOSS 3
E MT_BETA_COMPLETION_STARTUP_FAILURE 4
E MT_BETA_COMPLETION_RUNTIME_FAILURE 5
E MT_BETA_EVENT_STATE_CHANGED 1
E MT_BETA_EVENT_OUTPUT_READY 2
E MT_BETA_EVENT_ERROR 3
E MT_BETA_EVENT_COMPLETED 4
E MT_BETA_EVENT_OUTPUT_STATE_CHANGED 5
E MT_BETA_BACKEND_UNKNOWN 0
E MT_BETA_BACKEND_NONE 1
E MT_BETA_BACKEND_D3D11VA 2
E MT_BETA_BACKEND_QSV 3
E MT_BETA_BACKEND_CUDA 4
E MT_BETA_BACKEND_VAAPI 5
E MT_BETA_BACKEND_RKMPP 6
E MT_BETA_BACKEND_VIDEOTOOLBOX 7
E MT_BETA_BACKEND_MEDIACODEC 8
E MT_BETA_FILTER_UNKNOWN 0
E MT_BETA_FILTER_NONE 1
E MT_BETA_FILTER_RGA 2
E MT_BETA_FILTER_HARDWARE 3
E MT_BETA_INPUT_RTP_VIDEO 1
E MT_BETA_INPUT_URL 2
E MT_BETA_INPUT_MPEGTS_UDP 3
E MT_BETA_OUTPUT_ELEMENTARY_RTP 1
E MT_BETA_OUTPUT_MPEGTS_UDP 2
E MT_BETA_OUTPUT_MPEGTS_RTP 3
E MT_BETA_OUTPUT_PREPARING 1
E MT_BETA_OUTPUT_WAITING_FOR_RANDOM_ACCESS 2
E MT_BETA_OUTPUT_RUNNING 3
E MT_BETA_OUTPUT_DRAINING 4
E MT_BETA_OUTPUT_RETIRED 5
E MT_BETA_OUTPUT_FAILED 6
```

实际命令
```bash
/opt/mt-tools/gcc-12.4-native/bin/gcc -std=c11 -O2 -Wall -Wextra -Werror -Werror=incompatible-pointer-types -I/home/tang/MediaTranscode-composition-09093919/acceptance/rk432/old /home/tang/MediaTranscode-composition-09093919/acceptance/rk432/layout.c -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk432/layout-old
/opt/mt-tools/gcc-12.4-native/bin/gcc -std=c11 -O2 -Wall -Wextra -Werror -Werror=incompatible-pointer-types -I/home/tang/MediaTranscode-composition-09093919/acceptance/rk432/old /home/tang/MediaTranscode-composition-09093919/acceptance/rk432/signatures.c -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk432/signatures-old.o -c
/opt/mt-tools/gcc-12.4-native/bin/gcc -std=c11 -O2 -Wall -Wextra -Werror -Werror=incompatible-pointer-types -I/home/tang/MediaTranscode-composition-09093919/acceptance/rk432/current /home/tang/MediaTranscode-composition-09093919/acceptance/rk432/layout.c -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk432/layout-current
/opt/mt-tools/gcc-12.4-native/bin/gcc -std=c11 -O2 -Wall -Wextra -Werror -Werror=incompatible-pointer-types -I/home/tang/MediaTranscode-composition-09093919/acceptance/rk432/current /home/tang/MediaTranscode-composition-09093919/acceptance/rk432/signatures.c -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk432/signatures-current.o -c
acceptance/rk432/layout-old
acceptance/rk432/layout-current
```

清理：临时旧/当前头、layout/signatures源码、两个layout可执行文件、两个signature对象、两份结果、覆盖清单、命令清单；传输payload已删。归档后精确删除，保留正式库。12个临时文件已精确删除，四个空头文件目录已移除，rk432目录为空。未运行媒体进程。
