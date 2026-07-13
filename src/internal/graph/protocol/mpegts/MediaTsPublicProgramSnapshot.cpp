#include "internal/graph/protocol/mpegts/MediaTsPublicProgramSnapshot.h"

namespace media::ffmpeg::graph {

::media::Result<std::vector<FFmpegInputProgramSnapshot>>
MediaTsPublicProgramSnapshotFactory::fromFormatContext(const AVFormatContext& context)
{
    std::vector<FFmpegInputProgramSnapshot> snapshots;
    snapshots.reserve(context.nb_programs);
    for (unsigned index = 0; index < context.nb_programs; ++index) {
        const AVProgram* program = context.programs[index];
        if (!program) {
            return ::media::Result<std::vector<FFmpegInputProgramSnapshot>>::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS public program is null"));
        }
        FFmpegInputProgramSnapshot snapshot;
        snapshot.programNumber = program->program_num;
        snapshot.pmtPid = program->pmt_pid;
        snapshot.pcrPid = program->pcr_pid;
        snapshot.streamBindings.reserve(program->nb_stream_indexes);
        for (unsigned stream = 0; stream < program->nb_stream_indexes; ++stream) {
            const int streamIndex = static_cast<int>(program->stream_index[stream]);
            if (streamIndex < 0 || static_cast<unsigned>(streamIndex) >= context.nb_streams ||
                !context.streams[streamIndex]) {
                return ::media::Result<std::vector<FFmpegInputProgramSnapshot>>::failure(
                    ::media::ErrorInfo::invalidArgument("MPEG-TS public program stream is missing"));
            }
            const int pid = context.streams[streamIndex]->id;
            if (pid <= 0 || pid > 0x1fff) {
                return ::media::Result<std::vector<FFmpegInputProgramSnapshot>>::failure(
                    ::media::ErrorInfo::invalidArgument("MPEG-TS elementary PID is out of range"));
            }
            for (const auto& binding : snapshot.streamBindings) {
                if (binding.streamIndex == streamIndex || binding.elementaryPid == pid) {
                    return ::media::Result<std::vector<FFmpegInputProgramSnapshot>>::failure(
                        ::media::ErrorInfo::invalidArgument("MPEG-TS public program binding is duplicated"));
                }
            }
            snapshot.streamBindings.push_back({streamIndex, pid});
        }
        snapshots.push_back(std::move(snapshot));
    }
    return ::media::Result<std::vector<FFmpegInputProgramSnapshot>>::success(
        std::move(snapshots));
}

} // namespace media::ffmpeg::graph
