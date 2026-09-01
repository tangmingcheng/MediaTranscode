#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "media_transcode_beta/realtime.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void sleep_one_second(void)
{
#ifdef _WIN32
    Sleep(1000);
#else
    const struct timespec duration = {1, 0};
    (void)nanosleep(&duration, NULL);
#endif
}

static int parse_u64(const char* text, uint64_t* value)
{
    char* end = NULL;
    unsigned long long parsed = 0;
    if (text == NULL || *text == '\0' || *text == '-') {
        return 0;
    }
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *value = (uint64_t)parsed;
    return 1;
}

static int parse_u32(const char* text, uint32_t* value)
{
    uint64_t parsed = 0;
    if (!parse_u64(text, &parsed) || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int parse_u16(const char* text, uint16_t* value)
{
    uint32_t parsed = 0;
    if (!parse_u32(text, &parsed) || parsed > UINT16_MAX) {
        return 0;
    }
    *value = (uint16_t)parsed;
    return 1;
}

static int parse_u8(const char* text, uint8_t* value)
{
    uint32_t parsed = 0;
    if (!parse_u32(text, &parsed) || parsed > UINT8_MAX) {
        return 0;
    }
    *value = (uint8_t)parsed;
    return 1;
}

static int parse_codec(const char* text, mt_beta_video_codec* codec)
{
    if (strcmp(text, "h264") == 0) {
        *codec = MT_BETA_VIDEO_CODEC_H264;
        return 1;
    }
    if (strcmp(text, "hevc") == 0) {
        *codec = MT_BETA_VIDEO_CODEC_HEVC;
        return 1;
    }
    return 0;
}

static void on_event(void* user_data, const mt_beta_realtime_event* event)
{
    (void)user_data;
    if (event == NULL) {
        return;
    }
    fprintf(
        stderr,
        "event=%d state=%d error=%d stage=%d completion=%d native=%d detail=%s\n",
        (int)event->type,
        (int)event->state,
        (int)event->error_code,
        (int)event->failure_stage,
        (int)event->completion_reason,
        (int)event->native_code,
        event->detail != NULL ? event->detail : "");
    if (event->output_description != NULL) {
        fprintf(stderr, "output_description:\n%s\n", event->output_description);
    }
}

static void print_usage(const char* program)
{
    fprintf(
        stderr,
        "CBR:\n  %s media_id bind_ip input_port input_codec input_pt input_clock "
        "destination_ip destination_port output_codec width height fps_num "
        "fps_den gop cbr target_bps egress_bps residence_ms\n"
        "VBR:\n  %s media_id bind_ip input_port input_codec input_pt input_clock "
        "destination_ip destination_port output_codec width height fps_num "
        "fps_den gop vbr min_bps target_bps max_bps egress_bps residence_ms\n",
        program,
        program);
}

int main(int argc, char** argv)
{
    mt_beta_realtime_config config = {0};
    mt_beta_realtime_callbacks callbacks = {0};
    mt_beta_realtime_session* session = NULL;
    mt_beta_realtime_snapshot snapshot = {0};
    mt_beta_status status;
    int stop_sent = 0;
    int terminal_state = MT_BETA_REALTIME_FAILED;
    const int is_cbr = argc == 19 && strcmp(argv[15], "cbr") == 0;
    const int is_vbr = argc == 21 && strcmp(argv[15], "vbr") == 0;

    if (!is_cbr && !is_vbr) {
        print_usage(argv[0]);
        return 2;
    }

    config.media_id = argv[1];
    config.input.bind_address = argv[2];
    config.output.destination_address = argv[7];
    if (!parse_u16(argv[3], &config.input.port) ||
        !parse_codec(argv[4], &config.input.codec) ||
        !parse_u8(argv[5], &config.input.payload_type) ||
        !parse_u32(argv[6], &config.input.clock_rate) ||
        !parse_u16(argv[8], &config.output.destination_port) ||
        !parse_codec(argv[9], &config.output.codec) ||
        !parse_u32(argv[10], &config.output.width) ||
        !parse_u32(argv[11], &config.output.height) ||
        !parse_u32(argv[12], &config.output.frame_rate_num) ||
        !parse_u32(argv[13], &config.output.frame_rate_den) ||
        !parse_u32(argv[14], &config.output.gop_frames)) {
        print_usage(argv[0]);
        return 2;
    }
    if (is_cbr) {
        config.output.rate_control_mode = MT_BETA_RATE_CONTROL_CBR;
        if (!parse_u64(argv[16], &config.output.rate_control.cbr.bitrate_bps) ||
            !parse_u64(argv[17], &config.deployment.provisioned_egress_capacity_bps) ||
            !parse_u32(argv[18], &config.deployment.maximum_wire_residence_ms)) {
            print_usage(argv[0]);
            return 2;
        }
    } else {
        config.output.rate_control_mode = MT_BETA_RATE_CONTROL_VBR;
        if (!parse_u64(argv[16], &config.output.rate_control.vbr.min_bitrate_bps) ||
            !parse_u64(argv[17], &config.output.rate_control.vbr.target_bitrate_bps) ||
            !parse_u64(argv[18], &config.output.rate_control.vbr.max_bitrate_bps) ||
            !parse_u64(argv[19], &config.deployment.provisioned_egress_capacity_bps) ||
            !parse_u32(argv[20], &config.deployment.maximum_wire_residence_ms)) {
            print_usage(argv[0]);
            return 2;
        }
    }

    callbacks.on_event = on_event;
    signal(SIGINT, handle_signal);
    status = mt_beta_realtime_start(&config, &callbacks, &session);
    if (status != MT_BETA_STATUS_OK) {
        fprintf(stderr, "mt_beta_realtime_start failed: %d\n", (int)status);
        return 1;
    }

    for (;;) {
        if (stop_requested && !stop_sent) {
            status = mt_beta_realtime_request_stop(session);
            if (status != MT_BETA_STATUS_OK) {
                fprintf(stderr, "mt_beta_realtime_request_stop failed: %d\n", (int)status);
                break;
            }
            stop_sent = 1;
        }
        status = mt_beta_realtime_get_snapshot(session, &snapshot);
        if (status != MT_BETA_STATUS_OK) {
            fprintf(stderr, "mt_beta_realtime_get_snapshot failed: %d\n", (int)status);
            break;
        }
        fprintf(
            stderr,
            "state=%d backend=%d filter=%d zero_copy=%u elapsed_ms=%" PRIu64
            " queued=%" PRIu64 " dropped=%" PRIu64 " cpu_single=%.2f rss=%" PRIu64 "\n",
            (int)snapshot.state,
            (int)snapshot.selected_backend,
            (int)snapshot.selected_filter,
            (unsigned)snapshot.zero_copy_planned,
            snapshot.running_time_ms,
            snapshot.queued_buffers,
            snapshot.dropped_buffers,
            snapshot.average_process_single_core_cpu_percent,
            snapshot.working_set_bytes);
        if (snapshot.state == MT_BETA_REALTIME_COMPLETED ||
            snapshot.state == MT_BETA_REALTIME_FAILED) {
            terminal_state = snapshot.state;
            break;
        }
        sleep_one_second();
    }

    mt_beta_realtime_release(&session);
    return terminal_state == MT_BETA_REALTIME_COMPLETED ? 0 : 1;
}
