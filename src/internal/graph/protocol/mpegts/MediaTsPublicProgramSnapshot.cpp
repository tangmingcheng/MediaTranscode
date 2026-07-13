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
        snapshot.streamIndexes.reserve(program->nb_stream_indexes);
        for (unsigned stream = 0; stream < program->nb_stream_indexes; ++stream) {
            snapshot.streamIndexes.push_back(static_cast<int>(program->stream_index[stream]));
        }
        snapshots.push_back(std::move(snapshot));
    }
    return ::media::Result<std::vector<FFmpegInputProgramSnapshot>>::success(
        std::move(snapshots));
}

} // namespace media::ffmpeg::graph
