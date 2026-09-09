#ifndef MEDIA_TRANSCODE_BETA_REALTIME_H
#define MEDIA_TRANSCODE_BETA_REALTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mt_beta_video_codec {
    MT_BETA_VIDEO_CODEC_UNKNOWN = 0,
    MT_BETA_VIDEO_CODEC_H264 = 1,
    MT_BETA_VIDEO_CODEC_HEVC = 2
} mt_beta_video_codec;

typedef enum mt_beta_rate_control_mode {
    MT_BETA_RATE_CONTROL_CBR = 1,
    MT_BETA_RATE_CONTROL_VBR = 2
} mt_beta_rate_control_mode;

typedef enum mt_beta_status {
    MT_BETA_STATUS_OK = 0,
    MT_BETA_STATUS_INVALID_ARGUMENT = 1,
    MT_BETA_STATUS_ALLOCATION_FAILED = 2,
    MT_BETA_STATUS_INVALID_STATE = 3,
    MT_BETA_STATUS_INTERNAL_ERROR = 4,
    MT_BETA_STATUS_BUSY = 5
} mt_beta_status;

typedef enum mt_beta_realtime_state {
    MT_BETA_REALTIME_STARTING = 1,
    MT_BETA_REALTIME_RUNNING = 2,
    MT_BETA_REALTIME_STOPPING = 3,
    MT_BETA_REALTIME_COMPLETED = 4,
    MT_BETA_REALTIME_FAILED = 5
} mt_beta_realtime_state;

typedef enum mt_beta_error_code {
    MT_BETA_ERROR_NONE = 0,
    MT_BETA_ERROR_INVALID_ARGUMENT = 1,
    MT_BETA_ERROR_ALLOCATION_FAILED = 2,
    MT_BETA_ERROR_UNSUPPORTED = 3,
    MT_BETA_ERROR_HARDWARE_UNAVAILABLE = 4,
    MT_BETA_ERROR_IO_FAILURE = 5,
    MT_BETA_ERROR_FFMPEG_FAILURE = 6,
    MT_BETA_ERROR_CANCELLED = 7,
    MT_BETA_ERROR_INTERNAL = 8,
    MT_BETA_ERROR_WOULD_BLOCK = 9,
    MT_BETA_ERROR_NOT_INITIALIZED = 10
} mt_beta_error_code;

typedef enum mt_beta_failure_stage {
    MT_BETA_FAILURE_NONE = 0,
    MT_BETA_FAILURE_INPUT_VALIDATION = 1,
    MT_BETA_FAILURE_SESSION_CREATION = 2,
    MT_BETA_FAILURE_PREFLIGHT = 3,
    MT_BETA_FAILURE_CAPABILITY = 4,
    MT_BETA_FAILURE_GRAPH_BUILD = 5,
    MT_BETA_FAILURE_RUNTIME_START = 6,
    MT_BETA_FAILURE_RUNTIME_EXECUTION = 7,
    MT_BETA_FAILURE_STOP = 8,
    MT_BETA_FAILURE_RELEASE = 9
} mt_beta_failure_stage;

typedef enum mt_beta_completion_reason {
    MT_BETA_COMPLETION_NONE = 0,
    MT_BETA_COMPLETION_SOURCE_COMPLETED = 1,
    MT_BETA_COMPLETION_REQUESTED_STOP = 2,
    MT_BETA_COMPLETION_SOURCE_LOSS = 3,
    MT_BETA_COMPLETION_STARTUP_FAILURE = 4,
    MT_BETA_COMPLETION_RUNTIME_FAILURE = 5
} mt_beta_completion_reason;

typedef enum mt_beta_realtime_event_type {
    MT_BETA_EVENT_STATE_CHANGED = 1,
    MT_BETA_EVENT_OUTPUT_READY = 2,
    MT_BETA_EVENT_ERROR = 3,
    MT_BETA_EVENT_COMPLETED = 4,
    MT_BETA_EVENT_OUTPUT_STATE_CHANGED = 5
} mt_beta_realtime_event_type;

typedef enum mt_beta_selected_backend {
    MT_BETA_BACKEND_UNKNOWN = 0,
    MT_BETA_BACKEND_NONE = 1,
    MT_BETA_BACKEND_D3D11VA = 2,
    MT_BETA_BACKEND_QSV = 3,
    MT_BETA_BACKEND_CUDA = 4,
    MT_BETA_BACKEND_VAAPI = 5,
    MT_BETA_BACKEND_RKMPP = 6,
    MT_BETA_BACKEND_VIDEOTOOLBOX = 7,
    MT_BETA_BACKEND_MEDIACODEC = 8
} mt_beta_selected_backend;

typedef enum mt_beta_selected_filter {
    MT_BETA_FILTER_UNKNOWN = 0,
    MT_BETA_FILTER_NONE = 1,
    MT_BETA_FILTER_RGA = 2,
    MT_BETA_FILTER_HARDWARE = 3
} mt_beta_selected_filter;

typedef enum mt_beta_realtime_input_kind {
    MT_BETA_INPUT_RTP_VIDEO = 1,
    MT_BETA_INPUT_URL = 2,
    MT_BETA_INPUT_MPEGTS_UDP = 3
} mt_beta_realtime_input_kind;

typedef enum mt_beta_video_output_protocol {
    MT_BETA_OUTPUT_ELEMENTARY_RTP = 1,
    MT_BETA_OUTPUT_MPEGTS_UDP = 2,
    MT_BETA_OUTPUT_MPEGTS_RTP = 3
} mt_beta_video_output_protocol;

typedef enum mt_beta_output_state {
    MT_BETA_OUTPUT_PREPARING = 1,
    MT_BETA_OUTPUT_WAITING_FOR_RANDOM_ACCESS = 2,
    MT_BETA_OUTPUT_RUNNING = 3,
    MT_BETA_OUTPUT_DRAINING = 4,
    MT_BETA_OUTPUT_RETIRED = 5,
    MT_BETA_OUTPUT_FAILED = 6
} mt_beta_output_state;

typedef struct mt_beta_rtp_video_input {
    const char* bind_address;
    uint16_t port;
    mt_beta_video_codec codec;
    uint8_t payload_type;
    uint32_t clock_rate;
    const char* fmtp;
} mt_beta_rtp_video_input;

typedef struct mt_beta_url_video_input {
    const char* url;
    const char* rtsp_transport;
} mt_beta_url_video_input;

typedef struct mt_beta_mpegts_udp_video_input {
    const char* url;
    /* Maximum PCR gap guaranteed by the source protocol session. */
    uint32_t maximum_pcr_gap_ms;
} mt_beta_mpegts_udp_video_input;

typedef struct mt_beta_realtime_input {
    mt_beta_realtime_input_kind kind;
    union {
        mt_beta_rtp_video_input rtp;
        mt_beta_url_video_input url;
        mt_beta_mpegts_udp_video_input mpegts_udp;
    } source;
} mt_beta_realtime_input;

typedef struct mt_beta_cbr {
    uint64_t bitrate_bps;
} mt_beta_cbr;

typedef struct mt_beta_vbr {
    uint64_t target_bitrate_bps;
    uint64_t min_bitrate_bps;
    uint64_t max_bitrate_bps;
} mt_beta_vbr;

typedef struct mt_beta_video_output {
    mt_beta_video_output_protocol protocol;
    const char* destination_address;
    uint16_t destination_port;
    mt_beta_video_codec codec;
    uint32_t width;
    uint32_t height;
    uint32_t frame_rate_num;
    uint32_t frame_rate_den;
    uint32_t gop_frames;
    mt_beta_rate_control_mode rate_control_mode;
    union {
        mt_beta_cbr cbr;
        mt_beta_vbr vbr;
    } rate_control;
} mt_beta_video_output;

typedef struct mt_beta_realtime_deployment {
    uint64_t provisioned_egress_capacity_bps;
    uint32_t maximum_wire_residence_ms;
} mt_beta_realtime_deployment;

typedef struct mt_beta_realtime_config {
    const char* media_id;
    mt_beta_realtime_input input;
    mt_beta_video_output initial_output;
    mt_beta_realtime_deployment deployment;
} mt_beta_realtime_config;

typedef struct mt_beta_realtime_event mt_beta_realtime_event;
typedef struct mt_beta_realtime_session mt_beta_realtime_session;

typedef void (*mt_beta_realtime_event_callback)(
    void* user_data,
    const mt_beta_realtime_event* event);

typedef struct mt_beta_realtime_callbacks {
    mt_beta_realtime_event_callback on_event;
    void* user_data;
} mt_beta_realtime_callbacks;

struct mt_beta_realtime_event {
    mt_beta_realtime_event_type type;
    mt_beta_realtime_state state;
    mt_beta_error_code error_code;
    mt_beta_failure_stage failure_stage;
    mt_beta_completion_reason completion_reason;
    int32_t native_code;
    const char* detail;
    /* Strings are borrowed only for the duration of the callback. */
    const char* output_description_path;
    uint64_t output_id;
    mt_beta_output_state output_state;
};

typedef struct mt_beta_realtime_snapshot {
    mt_beta_realtime_state state;
    mt_beta_completion_reason completion_reason;
    mt_beta_selected_backend selected_backend;
    mt_beta_video_codec input_codec;
    uint64_t running_time_ms;
    uint64_t queued_buffers;
    uint64_t peak_queued_buffers;
    uint64_t worker_progress;
    uint64_t worker_process_calls;
    uint64_t worker_waits;
    uint64_t worker_wakeups;
    uint64_t worker_errors;
    uint64_t stalled_intervals;
    uint64_t total_pushed;
    uint64_t total_popped;
    uint64_t dropped_buffers;
    uint64_t encoded_packets_pushed;
    uint64_t encoded_packets_popped;
    uint64_t working_set_bytes;
    uint64_t peak_working_set_bytes;
    uint32_t logical_processor_count;
    double average_process_machine_cpu_percent;
    double peak_process_machine_cpu_percent;
    double average_process_single_core_cpu_percent;
    double peak_process_single_core_cpu_percent;
} mt_beta_realtime_snapshot;

typedef struct mt_beta_output_snapshot {
    uint64_t output_id;
    mt_beta_output_state state;
    mt_beta_error_code error_code;
    mt_beta_failure_stage failure_stage;
    int32_t native_code;
    const char* detail;
    const char* output_description_path;
} mt_beta_output_snapshot;

/* A copied, immutable collection. Its strings live until collection release.
 * A description path names a file owned by the session; copy the file before
 * retiring the output or releasing the session if it must be retained. */
typedef struct mt_beta_output_snapshots mt_beta_output_snapshots;

/* Input strings are copied before start/add returns.
 * Add/remove acknowledge command admission; observe final state via events.
 * Output IDs are session-local, nonzero, and never reused.
 * Calls may originate in callbacks. Release must originate outside callbacks.
 * Session release must be externally synchronized with all other API calls. */
mt_beta_status mt_beta_realtime_add_output(
    mt_beta_realtime_session* session,
    const mt_beta_video_output* output,
    uint64_t* output_id);
mt_beta_status mt_beta_realtime_remove_output(
    mt_beta_realtime_session* session,
    uint64_t output_id);
mt_beta_status mt_beta_realtime_get_output_snapshots(
    mt_beta_realtime_session* session,
    mt_beta_output_snapshots** snapshots);
size_t mt_beta_output_snapshots_count(const mt_beta_output_snapshots* snapshots);
mt_beta_status mt_beta_output_snapshots_get(
    const mt_beta_output_snapshots* snapshots,
    size_t index,
    mt_beta_output_snapshot* snapshot);
void mt_beta_output_snapshots_release(mt_beta_output_snapshots** snapshots);

mt_beta_status mt_beta_realtime_start(
    const mt_beta_realtime_config* config,
    const mt_beta_realtime_callbacks* callbacks,
    mt_beta_realtime_session** session);
mt_beta_status mt_beta_realtime_request_stop(mt_beta_realtime_session* session);
mt_beta_status mt_beta_realtime_get_snapshot(
    mt_beta_realtime_session* session,
    mt_beta_realtime_snapshot* snapshot);
void mt_beta_realtime_release(mt_beta_realtime_session** session);

#ifdef __cplusplus
}
#endif

#endif
