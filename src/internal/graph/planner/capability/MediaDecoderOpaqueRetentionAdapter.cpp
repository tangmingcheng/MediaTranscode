#include "internal/graph/planner/capability/MediaDecoderOpaqueRetentionAdapter.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
namespace media::ffmpeg::graph {
namespace {
// Audited 20054712a242c54aa19d9ae9fca7a0381a2f1397 source containers.
// Configuration equality excludes LCEVC and unreviewed postprocessing builds.
// Deployment hashes remain the content identity check; this is not a DLL hash.
constexpr std::string_view auditedConfiguration = R"mtcfg( --disable-static --enable-shared --pkg-config=pkgconf --cc='ccache gcc' --cxx='ccache g++' --ld='ccache g++' --extra-cxxflags=-fpermissive --extra-cflags=-Wno-int-conversion --disable-autodetect --enable-amf --enable-bzlib --enable-cuda --enable-cuvid --enable-d3d12va --enable-d3d11va --enable-dxva2 --enable-iconv --enable-lzma --enable-nvenc --enable-zlib --enable-sdl2 --enable-ffnvcodec --enable-nvdec --enable-cuda-llvm --enable-gpl --enable-libmp3lame --enable-libopus --enable-libvorbis --enable-libvpx --enable-libx264 --enable-libx265 --enable-libdav1d --enable-libaom --disable-debug --enable-libfdk-aac --enable-fontconfig --enable-libass --enable-libbluray --enable-libfreetype --enable-libharfbuzz --enable-libvpl --enable-libmysofa --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libopenjpeg --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvo-amrwbenc --enable-libwebp --enable-libxml2 --enable-libzimg --enable-libshine --enable-avisynth --enable-libxvid --enable-libopenmpt --enable-version3 --enable-librav1e --enable-libsrt --enable-libgsm --enable-libvmaf --enable-libsvtav1 --enable-chromaprint --enable-decklink --enable-frei0r --enable-libaribb24 --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libflite --enable-libfribidi --enable-libgme --enable-libilbc --enable-libsvthevc --enable-libsvtvp9 --enable-libkvazaar --enable-libmodplug --enable-librist --enable-librtmp --enable-librubberband --enable-libtesseract --enable-libxavs --enable-libzmq --enable-libzvbi --enable-openal --enable-libcodec2 --enable-ladspa --enable-libglslang --enable-vulkan --enable-libdavs2 --enable-libxavs2 --enable-libuavs3d --enable-libplacebo --enable-libjxl --enable-libvvenc --enable-libvvdec --enable-liblc3 --enable-audiotoolbox --enable-libsvtjpegxs --enable-opencl --enable-opengl --enable-libopenh264 --enable-openssl --extra-cflags=-DLIBTWOLAME_STATIC --extra-cflags=-DCACA_STATIC --extra-cflags=-DMODPLUG_STATIC --extra-cflags=-DCHROMAPRINT_NODLL --extra-cflags=-DZMQ_STATIC --extra-libs=-lpsapi --extra-cflags=-DLIBXML_STATIC --extra-libs=-liconv --disable-w32threads --extra-cflags=-DKVZ_STATIC_LIB --enable-nonfree --extra-libs=-lAudioToolboxWrapper --disable-outdev=audiotoolbox --extra-cflags=-DAL_LIBTYPE_STATIC --shlibdir=/local64/bin-video)mtcfg";
bool matchesImplementation() noexcept
{
    return avcodec_version() == AV_VERSION_INT(62, 36, 101) &&
        std::string_view(av_version_info()) == "N-125148-g20054712a2-g9cbd889670+3" &&
        std::string_view(avcodec_configuration()) == auditedConfiguration &&
        std::string_view(avutil_configuration()) == auditedConfiguration;
}
}
std::optional<MediaDecoderOpaqueRetention> MediaDecoderOpaqueRetentionAdapter::readAfterOpen(
    const AVCodecContext& context, std::string_view hardwareBackend)
{
    if (!matchesImplementation() || !context.codec || !context.codec->name ||
        context.codec_type != AVMEDIA_TYPE_VIDEO || context.thread_count <= 0 ||
        context.get_buffer2 != avcodec_default_get_buffer2 ||
        (!hardwareBackend.empty() && hardwareBackend != "cuda")) return {};
    const std::string_view name(context.codec->name);
    if (name != "h264" && name != "hevc") return {};
    // h264dec.h: DPB[36], cur_pic, last_pic_for_ec, each f/f_grain.
    // hevcdec.h/ps.h: two layers, DPB[32], each f/frame_grain.
    const std::uint64_t pictures = name == "h264" ? 2 * (36 + 2) : 2 * 32 * 2;
    // internal.h + default null BSF: three packets, one BSF packet, one frame.
    constexpr std::uint64_t generic = 5;
    const auto workers = context.active_thread_type & FF_THREAD_FRAME
        ? static_cast<std::uint64_t>(context.thread_count) : 0;
    return MediaDecoderOpaqueRetention{context.thread_count, context.active_thread_type,
        pictures, generic, name == "hevc", workers};
}
}
