#define _POSIX_C_SOURCE 200809L

#include <media_transcode_beta/realtime.h>

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static atomic_int terminal_state = ATOMIC_VAR_INIT(0);

static void on_event(void* user_data, const mt_beta_realtime_event* event)
{
    (void)user_data;
    printf(
        "event type=%d state=%d error=%d stage=%d completion=%d native=%d detail=%s output=%s\n",
        (int)event->type,
        (int)event->state,
        (int)event->error_code,
        (int)event->failure_stage,
        (int)event->completion_reason,
        (int)event->native_code,
        event->detail != NULL ? event->detail : "",
        event->output_description != NULL ? event->output_description : "");
    fflush(stdout);
    if (event->state == MT_BETA_REALTIME_COMPLETED ||
        event->state == MT_BETA_REALTIME_FAILED) {
        atomic_store_explicit(&terminal_state, (int)event->state, memory_order_release);
    }
}

static void wait_one_second(void)
{
    const struct timespec delay = {1, 0};
    (void)nanosleep(&delay, NULL);
}

static void print_snapshot(mt_beta_realtime_session* session)
{
    mt_beta_realtime_snapshot snapshot = {0};
    const mt_beta_status status =
        mt_beta_realtime_get_snapshot(session, &snapshot);
    if (status != MT_BETA_STATUS_OK) {
        fprintf(stderr, "snapshot failed: %d\n", (int)status);
        atomic_store_explicit(&terminal_state, MT_BETA_REALTIME_FAILED, memory_order_release);
        return;
    }
    printf(
        "snapshot state=%d completion=%d backend=%d filter=%d zero_copy=%u "
        "running_ms=%llu queued=%llu peak=%llu drops=%llu errors=%llu "
        "encoded=%llu/%llu rss=%llu peak_rss=%llu cpu_machine=%.3f cpu_core=%.3f\n",
        (int)snapshot.state,
        (int)snapshot.completion_reason,
        (int)snapshot.selected_backend,
        (int)snapshot.selected_filter,
        (unsigned)snapshot.zero_copy_planned,
        (unsigned long long)snapshot.running_time_ms,
        (unsigned long long)snapshot.queued_buffers,
        (unsigned long long)snapshot.peak_queued_buffers,
        (unsigned long long)snapshot.dropped_buffers,
        (unsigned long long)snapshot.worker_errors,
        (unsigned long long)snapshot.encoded_packets_pushed,
        (unsigned long long)snapshot.encoded_packets_popped,
        (unsigned long long)snapshot.working_set_bytes,
        (unsigned long long)snapshot.peak_working_set_bytes,
        snapshot.average_process_machine_cpu_percent,
        snapshot.average_process_single_core_cpu_percent);
    fflush(stdout);
    if (snapshot.state == MT_BETA_REALTIME_COMPLETED ||
        snapshot.state == MT_BETA_REALTIME_FAILED) {
        atomic_store_explicit(&terminal_state, (int)snapshot.state, memory_order_release);
    }
}

int main(void)
{
    mt_beta_realtime_config config = {0};
    config.media_id = "rk-beta-datagram-c-example";
    config.input.bind_address = "192.168.130.229";
    config.input.port = 61884;
    config.input.codec = MT_BETA_VIDEO_CODEC_H264;
    config.input.payload_type = 96;
    config.input.clock_rate = 90000;
    config.output.destination_address = "192.168.96.122";
    config.output.destination_port = 6200;
    config.output.codec = MT_BETA_VIDEO_CODEC_HEVC;
    config.output.width = 1920;
    config.output.height = 1080;
    config.output.frame_rate_num = 25;
    config.output.frame_rate_den = 1;
    config.output.gop_frames = 50;
    config.output.rate_control_mode = MT_BETA_RATE_CONTROL_CBR;
    config.output.rate_control.cbr.bitrate_bps = UINT64_C(6000000);
    config.deployment.provisioned_egress_capacity_bps = UINT64_C(50000000);
    config.deployment.maximum_wire_residence_ms = 100;

    const mt_beta_realtime_callbacks callbacks = {on_event, NULL};
    mt_beta_realtime_session* session = NULL;
    const mt_beta_status started =
        mt_beta_realtime_start(&config, &callbacks, &session);
    if (started != MT_BETA_STATUS_OK) {
        fprintf(stderr, "start failed: %d\n", (int)started);
        return 1;
    }

    while (!atomic_load_explicit(&terminal_state, memory_order_acquire)) {
        print_snapshot(session);
        if (!atomic_load_explicit(&terminal_state, memory_order_acquire)) {
            wait_one_second();
        }
    }

    print_snapshot(session);
    const int final_state = atomic_load_explicit(&terminal_state, memory_order_acquire);
    mt_beta_realtime_release(&session);
    return final_state == MT_BETA_REALTIME_COMPLETED ? 0 : 1;
}
