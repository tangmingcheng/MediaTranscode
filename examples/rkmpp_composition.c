#define _POSIX_C_SOURCE 200809L
#include <media_transcode_beta/realtime.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

static volatile sig_atomic_t stop_requested;

static void on_signal(int number)
{
    (void)number;
    stop_requested = 1;
}

static void on_event(void* user_data, const mt_beta_realtime_event* event)
{
    (void)user_data;
    /* Borrowed event strings must not be retained after this callback. */
    fprintf(stderr, "event=%d state=%d completion=%d error=%d stage=%d native=%d "
        "output=%" PRIu64 " output_state=%d detail=%s description=%s\n",
        event->type, event->state, event->completion_reason, event->error_code,
        event->failure_stage, event->native_code, event->output_id,
        event->output_state, event->detail ? event->detail : "",
        event->output_description_path ? event->output_description_path : "");
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <RTP-bind-address> <output-address>\n", argv[0]);
        return 2;
    }
    /* Explicit sample deployment, not library defaults. Edit these fields to
       match the actual RTP sessions and output/deployment requirements. */
    mt_beta_rtp_audio_input audio = {
        .bind_address = argv[1], .port = 61022, .codec_name = "aac",
        .payload_type = 97, .clock_rate = 44100, .channels = 2,
        .fmtp = "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210"
    };
    mt_beta_composition_input inputs[4] = {
        {.video = {argv[1], 61030, MT_BETA_VIDEO_CODEC_H264, 96, 90000, NULL},
         .audio = NULL, .input_latency_ms = 100},
        {.video = {argv[1], 61040, MT_BETA_VIDEO_CODEC_H264, 96, 90000, NULL},
         .audio = NULL, .input_latency_ms = 150},
        {.video = {argv[1], 61020, MT_BETA_VIDEO_CODEC_H264, 96, 90000, NULL},
         .audio = &audio, .input_latency_ms = 200},
        {.video = {argv[1], 61050, MT_BETA_VIDEO_CODEC_HEVC, 96, 90000, NULL},
         .audio = NULL, .input_latency_ms = 250}
    };
    mt_beta_realtime_composition_config config = {0};
    config.media_id = "four-source-composition";
    config.inputs = inputs;
    config.input_count = 4;
    config.grid = MT_BETA_COMPOSITION_GRID_FIXED;
    config.audio_source = 2;
    config.audio_bitrate_bps = UINT64_C(192000);
    config.preparation_storage_limit_bytes = UINT64_C(2147483648);
    config.output.protocol = MT_BETA_OUTPUT_MPEGTS_RTP;
    config.output.destination_address = argv[2];
    config.output.destination_port = 62024;
    config.output.codec = MT_BETA_VIDEO_CODEC_H264;
    config.output.profile = NULL;
    config.output.width = 1280;
    config.output.height = 720;
    config.output.frame_rate_num = 30;
    config.output.frame_rate_den = 1;
    config.output.gop_frames = 60;
    config.output.rate_control_mode = MT_BETA_RATE_CONTROL_VBR;
    config.output.rate_control.vbr.target_bitrate_bps = UINT64_C(8000000);
    config.output.rate_control.vbr.min_bitrate_bps = UINT64_C(8000000);
    config.output.rate_control.vbr.max_bitrate_bps = UINT64_C(12000000);
    config.deployment.provisioned_egress_capacity_bps = UINT64_C(50000000);
    config.deployment.maximum_wire_residence_ms = 100;

    struct sigaction action = {0};
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) {
        perror("sigaction");
        return 1;
    }
    mt_beta_realtime_callbacks callbacks = {.on_event = on_event, .user_data = NULL};
    mt_beta_realtime_session* session = NULL;
    mt_beta_status status = mt_beta_realtime_start_composition(&config, &callbacks, &session);
    if (status != MT_BETA_STATUS_OK || !session) {
        fprintf(stderr, "start_status=%d\n", status);
        if (session) mt_beta_realtime_release(&session);
        return 1;
    }
    int stop_sent = 0;
    int result = 1;
    for (;;) {
        if (stop_requested && !stop_sent) {
            status = mt_beta_realtime_request_stop(session);
            if (status != MT_BETA_STATUS_OK) {
                fprintf(stderr, "stop_status=%d\n", status);
                break;
            }
            stop_sent = 1;
        }
        mt_beta_realtime_snapshot snapshot = {0};
        status = mt_beta_realtime_get_snapshot(session, &snapshot);
        if (status != MT_BETA_STATUS_OK) {
            fprintf(stderr, "snapshot_status=%d\n", status);
            break;
        }
        printf("state=%d completion=%d backend=%d input_codec=%d elapsed_ms=%" PRIu64
            " encoded=%" PRIu64 " worker_errors=%" PRIu64 " rss_bytes=%" PRIu64
            " cpu_machine_percent=%.3f\n", snapshot.state, snapshot.completion_reason,
            snapshot.selected_backend, snapshot.input_codec, snapshot.running_time_ms,
            snapshot.encoded_packets_pushed, snapshot.worker_errors,
            snapshot.working_set_bytes, snapshot.average_process_machine_cpu_percent);
        fflush(stdout);
        if (snapshot.state == MT_BETA_REALTIME_FAILED) break;
        if (snapshot.state == MT_BETA_REALTIME_COMPLETED) {
            result = snapshot.completion_reason == MT_BETA_COMPLETION_REQUESTED_STOP ? 0 : 1;
            break;
        }
        struct timespec remaining = {1, 0};
        while (nanosleep(&remaining, &remaining) && errno == EINTR && !stop_requested) {}
    }
    /* No other API calls run concurrently with release; never release in callback. */
    mt_beta_realtime_release(&session);
    return session == NULL ? result : 1;
}
