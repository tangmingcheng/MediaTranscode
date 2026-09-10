#define _POSIX_C_SOURCE 200809L

#include <media_transcode_beta/realtime.h>

#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static atomic_int terminal_state = ATOMIC_VAR_INIT(0);
static volatile sig_atomic_t stop_requested = 0;

static void on_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void on_event(void* user_data, const mt_beta_realtime_event* event)
{
    (void)user_data;
    printf(
        "event type=%d state=%d error=%d stage=%d completion=%d native=%d "
        "output_id=%" PRIu64 " output_state=%d detail=%s output=%s\n",
        (int)event->type,
        (int)event->state,
        (int)event->error_code,
        (int)event->failure_stage,
        (int)event->completion_reason,
        (int)event->native_code,
        event->output_id,
        (int)event->output_state,
        event->detail != NULL ? event->detail : "",
        event->output_description_path != NULL ? event->output_description_path : "");
    fflush(stdout);
    if (event->state == MT_BETA_REALTIME_COMPLETED ||
        event->state == MT_BETA_REALTIME_FAILED) {
        atomic_store_explicit(&terminal_state, (int)event->state, memory_order_release);
    }
}

static int print_snapshot(mt_beta_realtime_session* session)
{
    mt_beta_realtime_snapshot snapshot = {0};
    const mt_beta_status status =
        mt_beta_realtime_get_snapshot(session, &snapshot);
    if (status != MT_BETA_STATUS_OK) {
        fprintf(stderr, "snapshot failed: %d\n", (int)status);
        return 0;
    }
    printf(
        "snapshot state=%d completion=%d backend=%d "
        "running_ms=%llu queued=%llu peak=%llu drops=%llu errors=%llu "
        "encoded=%llu/%llu rss=%llu peak_rss=%llu cpu_machine=%.3f cpu_core=%.3f\n",
        (int)snapshot.state,
        (int)snapshot.completion_reason,
        (int)snapshot.selected_backend,
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
    return 1;
}

static void list_outputs(mt_beta_realtime_session* session)
{
    mt_beta_output_snapshots* outputs = NULL;
    mt_beta_status status = mt_beta_realtime_get_output_snapshots(session, &outputs);
    if (status == MT_BETA_STATUS_OK) {
        for (size_t index = 0; index < mt_beta_output_snapshots_count(outputs); ++index) {
            mt_beta_output_snapshot output = {0};
            status = mt_beta_output_snapshots_get(outputs, index, &output);
            if (status != MT_BETA_STATUS_OK) break;
            printf("output_id=%" PRIu64 " output_state=%d error=%d detail=%s path=%s\n",
                output.output_id, (int)output.state, (int)output.error_code,
                output.detail != NULL ? output.detail : "",
                output.output_description_path != NULL ? output.output_description_path : "");
        }
    }
    /* Snapshot strings must not be retained after this release. */
    mt_beta_output_snapshots_release(&outputs);
    if (status != MT_BETA_STATUS_OK) fprintf(stderr, "list failed: %d\n", (int)status);
    fflush(stdout);
}

static int parse_id(const char* text, uint64_t* value)
{
    if (text == NULL || text[0] < '0' || text[0] > '9') return 0;
    char* end = NULL;
    errno = 0;
    const uintmax_t parsed = strtoumax(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed == 0 || parsed > UINT64_MAX) return 0;
    *value = (uint64_t)parsed;
    return 1;
}

static void run_command(mt_beta_realtime_session* session,
    const mt_beta_video_output* initial_output, char* line)
{
    char* cursor = NULL;
    char* command = strtok_r(line, " \t\r", &cursor);
    char* argument = strtok_r(NULL, " \t\r", &cursor);
    char* port_text = strtok_r(NULL, " \t\r", &cursor);
    char* profile_text = strtok_r(NULL, " \t\r", &cursor);
    char* extra = strtok_r(NULL, " \t\r", &cursor);
    uint64_t id = 0;
    if (command == NULL) return;
    if (strcmp(command, "list") == 0 && argument == NULL) {
        list_outputs(session);
        return;
    }
    mt_beta_status status;
    if (strcmp(command, "remove") == 0 && port_text == NULL && parse_id(argument, &id)) {
        status = mt_beta_realtime_remove_output(session, id);
    } else if (strcmp(command, "add") == 0 && argument != NULL && profile_text != NULL && extra == NULL &&
        parse_id(port_text, &id) && id < UINT16_MAX &&
        (strcmp(argument, "h264") == 0 || strcmp(argument, "hevc") == 0)) {
        /* Copy the caller's complete output intent; codec, profile and destination change. */
        mt_beta_video_output output = *initial_output;
        output.destination_port = (uint16_t)id;
        output.codec = strcmp(argument, "h264") == 0
            ? MT_BETA_VIDEO_CODEC_H264 : MT_BETA_VIDEO_CODEC_HEVC;
        output.profile = profile_text;
        id = 0;
        status = mt_beta_realtime_add_output(session, &output, &id);
    } else {
        fprintf(stderr, "commands: list | add <h264|hevc> <rtp-port> <profile> | remove <output-id>\n");
        return;
    }
    /* OK means admitted. Events/list confirm RUNNING or RETIRED asynchronously. */
    printf("command=%s status=%d output_id=%" PRIu64 "\n", command, (int)status, id);
    fflush(stdout);
}

int main(void)
{
    (void)signal(SIGINT, on_signal);
    (void)signal(SIGTERM, on_signal);
    mt_beta_realtime_config config = {0};
    config.input.kind = MT_BETA_INPUT_RTP_VIDEO;
    config.initial_output.protocol = MT_BETA_OUTPUT_MPEGTS_RTP;
    config.media_id = "rk-beta-dynamic-c-example";
    config.input.source.rtp.bind_address = "192.168.130.229";
    config.input.source.rtp.port = 61884;
    config.input.source.rtp.codec = MT_BETA_VIDEO_CODEC_H264;
    config.input.source.rtp.payload_type = 96;
    config.input.source.rtp.clock_rate = 90000;
    config.initial_output.destination_address = "192.168.96.122";
    config.initial_output.destination_port = 6200;
    config.initial_output.codec = MT_BETA_VIDEO_CODEC_HEVC;
    config.initial_output.profile = "main";
    config.initial_output.width = 1920;
    config.initial_output.height = 1080;
    config.initial_output.frame_rate_num = 25;
    config.initial_output.frame_rate_den = 1;
    config.initial_output.gop_frames = 50;
    config.initial_output.rate_control_mode = MT_BETA_RATE_CONTROL_CBR;
    config.initial_output.rate_control.cbr.bitrate_bps = UINT64_C(6000000);
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

    char line[256];
    size_t line_length = 0;
    int oversized = 0;
    int input_fd = STDIN_FILENO;
    int application_failed = 0;
    puts("commands: list | add <h264|hevc> <rtp-port> <profile> | remove <output-id>");
    puts("EOF leaves the session running; stop the source to finish the real RTP example.");
    while (!atomic_load_explicit(&terminal_state, memory_order_acquire)) {
        if (!print_snapshot(session)) {
            application_failed = 1;
            break;
        }
        if (atomic_load_explicit(&terminal_state, memory_order_acquire)) break;
        if (stop_requested) {
            const mt_beta_status status = mt_beta_realtime_request_stop(session);
            if (status != MT_BETA_STATUS_OK) {
                fprintf(stderr, "stop failed: %d\n", (int)status);
                application_failed = 1;
            }
            break;
        }
        struct pollfd input = {input_fd, POLLIN, 0};
        const int ready = poll(&input, 1, 1000);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0 || (input.revents & (POLLERR | POLLNVAL))) {
            fprintf(stderr, "stdin poll failed\n");
            application_failed = 1;
            break;
        }
        if (ready > 0 && (input.revents & (POLLIN | POLLHUP))) {
            char bytes[256];
            const ssize_t count = read(input_fd, bytes, sizeof(bytes));
            if (count < 0 && errno == EINTR) continue;
            if (count < 0) {
                perror("stdin read");
                application_failed = 1;
                break;
            }
            if (count == 0) {
                if (line_length != 0 || oversized) fprintf(stderr, "incomplete command discarded at EOF\n");
                input_fd = -1;
            }
            for (ssize_t index = 0; index < count; ++index) {
                if (bytes[index] == '\n') {
                    if (oversized) fprintf(stderr, "command exceeds input line limit\n");
                    else {
                        line[line_length] = '\0';
                        run_command(session, &config.initial_output, line);
                    }
                    line_length = 0;
                    oversized = 0;
                } else if (bytes[index] == '\0' || line_length == sizeof(line) - 1) {
                    oversized = 1;
                } else if (!oversized) {
                    line[line_length++] = bytes[index];
                }
            }
        }
    }

    if (!print_snapshot(session)) application_failed = 1;
    mt_beta_realtime_release(&session);
    const int final_state = atomic_load_explicit(&terminal_state, memory_order_acquire);
    return !application_failed && final_state == MT_BETA_REALTIME_COMPLETED ? 0 : 1;
}
