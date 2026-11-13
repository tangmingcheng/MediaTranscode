#include "internal/graph/planner/capability/MediaDecoderOpaqueRetentionAdapter.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/capability/MediaRkmppDependencyIdentity.h"
#include <sstream>
#include <cstdint>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}
namespace media::ffmpeg::graph {
namespace {
// Audited 20054712a242c54aa19d9ae9fca7a0381a2f1397 source containers.
// Configuration equality excludes LCEVC and unreviewed postprocessing builds.
// Deployment hashes remain the content identity check; this is not a DLL hash.
constexpr std::string_view auditedConfiguration = R"mtcfg( --disable-static --enable-shared --pkg-config=pkgconf --cc='ccache gcc' --cxx='ccache g++' --ld='ccache g++' --extra-cxxflags=-fpermissive --extra-cflags=-Wno-int-conversion --disable-autodetect --enable-amf --enable-bzlib --enable-cuda --enable-cuvid --enable-d3d12va --enable-d3d11va --enable-dxva2 --enable-iconv --enable-lzma --enable-nvenc --enable-zlib --enable-sdl2 --enable-ffnvcodec --enable-nvdec --enable-cuda-llvm --enable-gpl --enable-libmp3lame --enable-libopus --enable-libvorbis --enable-libvpx --enable-libx264 --enable-libx265 --enable-libdav1d --enable-libaom --disable-debug --enable-libfdk-aac --enable-fontconfig --enable-libass --enable-libbluray --enable-libfreetype --enable-libharfbuzz --enable-libvpl --enable-libmysofa --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libopenjpeg --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvo-amrwbenc --enable-libwebp --enable-libxml2 --enable-libzimg --enable-libshine --enable-avisynth --enable-libxvid --enable-libopenmpt --enable-version3 --enable-librav1e --enable-libsrt --enable-libgsm --enable-libvmaf --enable-libsvtav1 --enable-chromaprint --enable-decklink --enable-frei0r --enable-libaribb24 --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libflite --enable-libfribidi --enable-libgme --enable-libilbc --enable-libsvthevc --enable-libsvtvp9 --enable-libkvazaar --enable-libmodplug --enable-librist --enable-librtmp --enable-librubberband --enable-libtesseract --enable-libxavs --enable-libzmq --enable-libzvbi --enable-openal --enable-libcodec2 --enable-ladspa --enable-libglslang --enable-vulkan --enable-libdavs2 --enable-libxavs2 --enable-libuavs3d --enable-libplacebo --enable-libjxl --enable-libvvenc --enable-libvvdec --enable-liblc3 --enable-audiotoolbox --enable-libsvtjpegxs --enable-opencl --enable-opengl --enable-libopenh264 --enable-openssl --extra-cflags=-DLIBTWOLAME_STATIC --extra-cflags=-DCACA_STATIC --extra-cflags=-DMODPLUG_STATIC --extra-cflags=-DCHROMAPRINT_NODLL --extra-cflags=-DZMQ_STATIC --extra-libs=-lpsapi --extra-cflags=-DLIBXML_STATIC --extra-libs=-liconv --disable-w32threads --extra-cflags=-DKVZ_STATIC_LIB --enable-nonfree --extra-libs=-lAudioToolboxWrapper --disable-outdev=audiotoolbox --extra-cflags=-DAL_LIBTYPE_STATIC --shlibdir=/local64/bin-video)mtcfg";
// FFmpeg's native codec descriptor belongs to its loaded module. Resolve the
// exported callback there: an MSVC import thunk has a different code address.
auto defaultBufferCallback(const AVCodecContext& context) noexcept
    -> decltype(&avcodec_default_get_buffer2)
{
#if defined(_WIN32)
    HMODULE module = nullptr;
    if (!context.codec || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(context.codec), &module)) return nullptr;
    // Borrowed module; the opened context and its linked codec keep it live.
    // No LoadLibrary or FreeLibrary belongs to this capability query.
    return reinterpret_cast<decltype(&avcodec_default_get_buffer2)>(
        GetProcAddress(module, "avcodec_default_get_buffer2"));
#else
    (void)context;
    return &avcodec_default_get_buffer2;
#endif
}
std::optional<MediaDecoderOpaqueRetention> readRkmppAfterOpen(const AVCodecContext& context)
{
    if (!MediaRkmppDependencyIdentity::matchesRuntime() || !avcodec_is_open(&context) ||
        !context.codec || !context.codec->name || !context.priv_data ||
        context.codec_type != AVMEDIA_TYPE_VIDEO || context.thread_count <= 0 ||
        context.active_thread_type != 0 || !(context.flags & AV_CODEC_FLAG_COPY_OPAQUE)) return {};
    const std::string_view name(context.codec->name);
    if ((name != "h264_rkmpp" && name != "hevc_rkmpp") ||
        context.codec != avcodec_find_decoder_by_name(context.codec->name)) return {};
    // Search the opened decoder itself, never a similarly named child option.
    const auto* versionOption = av_opt_find(context.priv_data, "opaque_contract_version", nullptr, 0, 0);
    const auto* retainedOption = av_opt_find(context.priv_data, "opaque_retained_packets", nullptr, 0, 0);
    if (!versionOption || !retainedOption || versionOption->type != AV_OPT_TYPE_INT ||
        retainedOption->type != AV_OPT_TYPE_INT64 ||
        !(versionOption->flags & AV_OPT_FLAG_READONLY) || !(retainedOption->flags & AV_OPT_FLAG_READONLY)) return {};
    std::int64_t version = 0;
    std::int64_t retained = 0;
    if (av_opt_get_int(context.priv_data, "opaque_contract_version", 0, &version) < 0 ||
        av_opt_get_int(context.priv_data, "opaque_retained_packets", 0, &retained) < 0 ||
        version != 1 || retained <= 0) return {};
    return MediaBackendDecoderOpaqueRetention{context.thread_count, context.active_thread_type,
        static_cast<std::uint64_t>(version), static_cast<std::uint64_t>(retained)};
}
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
    if (hardwareBackend == "rkmpp") return readRkmppAfterOpen(context);
    if (!matchesImplementation() || !context.codec || !context.codec->name ||
        context.codec_type != AVMEDIA_TYPE_VIDEO || context.thread_count <= 0 ||
        (!hardwareBackend.empty() && hardwareBackend != "cuda")) return {};
    const std::string_view name(context.codec->name);
    if ((name != "h264" && name != "hevc") ||
        context.codec != avcodec_find_decoder_by_name(context.codec->name)) return {};
    const auto exported = defaultBufferCallback(context);
    if (mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) {
        std::ostringstream message;
        message << "decoder_opaque_callback codec=" << name
            << " callback=" << reinterpret_cast<std::uintptr_t>(context.get_buffer2)
            << " exported=" << reinterpret_cast<std::uintptr_t>(exported)
            << " imported=" << reinterpret_cast<std::uintptr_t>(&avcodec_default_get_buffer2);
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::PlannerCapability, message.str());
    }
    if (!exported || context.get_buffer2 != exported) return {};
    // h264dec.h: DPB[36], cur_pic, last_pic_for_ec, each f/f_grain.
    // hevcdec.h/ps.h: two layers, DPB[32], each f/frame_grain.
    const std::uint64_t pictures = name == "h264" ? 2 * (36 + 2) : 2 * 32 * 2;
    // internal.h + default null BSF: three packets, one BSF packet, one frame.
    constexpr std::uint64_t generic = 5;
    const auto workers = context.active_thread_type & FF_THREAD_FRAME
        ? static_cast<std::uint64_t>(context.thread_count) : 0;
    return MediaNativeDecoderOpaqueRetention{context.thread_count, context.active_thread_type,
        pictures, generic, name == "hevc", workers};
}
}
