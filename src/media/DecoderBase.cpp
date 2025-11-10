//
// Created by liu86 on 2025/10/30.
//

#include "DecoderBase.h"
#include <logger.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}
#include "MediaUtils.h"

namespace {
std::string pixFmtToString(AVPixelFormat pixFmt)
{
    switch (pixFmt) {
    // 常见 YUV 平面/半平面格式
    case AV_PIX_FMT_YUV420P: return "AV_PIX_FMT_YUV420P";
    case AV_PIX_FMT_YUV422P: return "AV_PIX_FMT_YUV422P";
    case AV_PIX_FMT_YUV444P: return "AV_PIX_FMT_YUV444P";
    case AV_PIX_FMT_YUVA420P: return "AV_PIX_FMT_YUVA420P";
    case AV_PIX_FMT_NV12: return "AV_PIX_FMT_NV12";
    case AV_PIX_FMT_NV21: return "AV_PIX_FMT_NV21";
    case AV_PIX_FMT_NV16: return "AV_PIX_FMT_NV16";
    case AV_PIX_FMT_P010LE: return "AV_PIX_FMT_P010LE";
    case AV_PIX_FMT_P016LE: return "AV_PIX_FMT_P016LE";
    case AV_PIX_FMT_YUV420P10LE: return "AV_PIX_FMT_YUV420P10LE";
    case AV_PIX_FMT_YUV420P12LE: return "AV_PIX_FMT_YUV420P12LE";
    case AV_PIX_FMT_YUV422P10LE: return "AV_PIX_FMT_YUV422P10LE";
    case AV_PIX_FMT_YUV444P10LE: return "AV_PIX_FMT_YUV444P10LE";

    // 常见打包格式
    case AV_PIX_FMT_YUYV422: return "AV_PIX_FMT_YUYV422";
    case AV_PIX_FMT_UYVY422: return "AV_PIX_FMT_UYVY422";

    // 常见 RGB/BGR
    case AV_PIX_FMT_RGB24: return "AV_PIX_FMT_RGB24";
    case AV_PIX_FMT_BGR24: return "AV_PIX_FMT_BGR24";
    case AV_PIX_FMT_RGBA: return "AV_PIX_FMT_RGBA";
    case AV_PIX_FMT_BGRA: return "AV_PIX_FMT_BGRA";
    case AV_PIX_FMT_ARGB: return "AV_PIX_FMT_ARGB";
    case AV_PIX_FMT_ABGR: return "AV_PIX_FMT_ABGR";
    case AV_PIX_FMT_RGB0: return "AV_PIX_FMT_RGB0";
    case AV_PIX_FMT_BGR0: return "AV_PIX_FMT_BGR0";

    // 其他常见平面/透明格式
    case AV_PIX_FMT_GBRP: return "AV_PIX_FMT_GBRP";
    case AV_PIX_FMT_GBRAP: return "AV_PIX_FMT_GBRAP";
    case AV_PIX_FMT_GRAY8: return "AV_PIX_FMT_GRAY8";

    // 硬件帧格式（显式枚举一些常见类型）
    case AV_PIX_FMT_VAAPI: return "AV_PIX_FMT_VAAPI";
    case AV_PIX_FMT_QSV: return "AV_PIX_FMT_QSV";
    case AV_PIX_FMT_CUDA: return "AV_PIX_FMT_CUDA";
    case AV_PIX_FMT_D3D11: return "AV_PIX_FMT_D3D11";
    case AV_PIX_FMT_DRM_PRIME: return "AV_PIX_FMT_DRM_PRIME";
    case AV_PIX_FMT_MEDIACODEC: return "AV_PIX_FMT_MEDIACODEC";
    case AV_PIX_FMT_OPENCL: return "AV_PIX_FMT_OPENCL";
    case AV_PIX_FMT_VULKAN: return "AV_PIX_FMT_VULKAN";
    case AV_PIX_FMT_VDPAU: return "AV_PIX_FMT_VDPAU";
    case AV_PIX_FMT_MMAL: return "AV_PIX_FMT_MMAL";

    default: break;
    }

    // 通用回退：使用 FFmpeg 名称，并标注硬件格式
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pixFmt);
    const char* name = av_get_pix_fmt_name(pixFmt);
    if (name) {
        std::string s{name};
        for (auto& c : s) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
        std::string result = "AV_PIX_FMT_" + s;
        if (desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            result += " (HW)";
        }
        return result;
    }
    return "Unknown";
}

std::string codecIdToString(AVCodecID codecId)
{
    switch (codecId) {
    // 常用视频编码
    case AV_CODEC_ID_H264: return "AV_CODEC_ID_H264";
    case AV_CODEC_ID_HEVC: // H265
        return "AV_CODEC_ID_HEVC";
    case AV_CODEC_ID_VVC: // H266
        return "AV_CODEC_ID_VVC";
    case AV_CODEC_ID_MPEG4: return "AV_CODEC_ID_MPEG4";
    case AV_CODEC_ID_MPEG2VIDEO: return "AV_CODEC_ID_MPEG2VIDEO";
    case AV_CODEC_ID_MPEG1VIDEO: return "AV_CODEC_ID_MPEG1VIDEO";
    case AV_CODEC_ID_VP8: return "AV_CODEC_ID_VP8";
    case AV_CODEC_ID_VP9: return "AV_CODEC_ID_VP9";
    case AV_CODEC_ID_AV1: return "AV_CODEC_ID_AV1";
    case AV_CODEC_ID_MJPEG: return "AV_CODEC_ID_MJPEG";
    case AV_CODEC_ID_H263: return "AV_CODEC_ID_H263";
    case AV_CODEC_ID_THEORA: return "AV_CODEC_ID_THEORA";
    case AV_CODEC_ID_PRORES: return "AV_CODEC_ID_PRORES";
    case AV_CODEC_ID_DVVIDEO: return "AV_CODEC_ID_DVVIDEO";
    case AV_CODEC_ID_WMV3: return "AV_CODEC_ID_WMV3";
    case AV_CODEC_ID_VC1: return "AV_CODEC_ID_VC1";
    case AV_CODEC_ID_FLV1: return "AV_CODEC_ID_FLV1";

    // 常用音频编码
    case AV_CODEC_ID_AAC: return "AV_CODEC_ID_AAC";
    case AV_CODEC_ID_MP3: return "AV_CODEC_ID_MP3";
    case AV_CODEC_ID_MP2: return "AV_CODEC_ID_MP2";
    case AV_CODEC_ID_AC3: return "AV_CODEC_ID_AC3";
    case AV_CODEC_ID_EAC3: return "AV_CODEC_ID_EAC3";
    case AV_CODEC_ID_DTS: return "AV_CODEC_ID_DTS";
    case AV_CODEC_ID_TRUEHD: return "AV_CODEC_ID_TRUEHD";
    case AV_CODEC_ID_FLAC: return "AV_CODEC_ID_FLAC";
    case AV_CODEC_ID_ALAC: return "AV_CODEC_ID_ALAC";
    case AV_CODEC_ID_OPUS: return "AV_CODEC_ID_OPUS";
    case AV_CODEC_ID_VORBIS: return "AV_CODEC_ID_VORBIS";

    // 常用 PCM/采样格式
    case AV_CODEC_ID_PCM_S16LE: return "AV_CODEC_ID_PCM_S16LE";
    case AV_CODEC_ID_PCM_S16BE: return "AV_CODEC_ID_PCM_S16BE";
    case AV_CODEC_ID_PCM_F32LE: return "AV_CODEC_ID_PCM_F32LE";
    case AV_CODEC_ID_PCM_U8: return "AV_CODEC_ID_PCM_U8";
    case AV_CODEC_ID_PCM_S24LE: return "AV_CODEC_ID_PCM_S24LE";
    case AV_CODEC_ID_PCM_S32LE: return "AV_CODEC_ID_PCM_S32LE";
    case AV_CODEC_ID_PCM_ALAW: return "AV_CODEC_ID_PCM_ALAW";
    case AV_CODEC_ID_PCM_MULAW: return "AV_CODEC_ID_PCM_MULAW";

    // 其他常见音频编码
    case AV_CODEC_ID_WMAV1: return "AV_CODEC_ID_WMAV1";
    case AV_CODEC_ID_WMAV2: return "AV_CODEC_ID_WMAV2";
    case AV_CODEC_ID_AMR_NB: return "AV_CODEC_ID_AMR_NB";
    case AV_CODEC_ID_AMR_WB: return "AV_CODEC_ID_AMR_WB";
    case AV_CODEC_ID_ADPCM_MS: return "AV_CODEC_ID_ADPCM_MS";
    case AV_CODEC_ID_ADPCM_IMA_WAV: return "AV_CODEC_ID_ADPCM_IMA_WAV";

    default: break;
    }

    // 通用回退：使用 FFmpeg 名称并转为 AV_CODEC_ID_XXXX
    const char* name = avcodec_get_name(codecId);
    if (name) {
        std::string s{name};
        for (auto& c : s) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
        return std::string("AV_CODEC_ID_") + s;
    }
    return "Unknown";
}

} // namespace

namespace media {
bool DecoderBase::initialize(const AVStream* stream)
{
    NEAPU_FUNC_TRACE;
    if (m_codecCtx) {
        NEAPU_LOGE("Codec context is already initialized");
        return false;
    }

    if (!initializeCodecContext(stream)) {
        return false;
    }

    if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
        NEAPU_LOGE("Failed to open codec");
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    NEAPU_LOGI("Decoder initialized successfully: codec_name={}, codec_id={}, pix_fmt={}", m_codec->name,
               codecIdToString(m_codecCtx->codec_id), pixFmtToString(m_codecCtx->pix_fmt));
    return true;
}

void DecoderBase::destroy()
{
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    m_stream = nullptr;
    m_codec = nullptr;
}

bool DecoderBase::decodePacket(AVPacketPtr packet, const FrameCallback& callback)
{
    if (!packet) {
        NEAPU_LOGE("Input packet is null");
        return false;
    }
    int ret = avcodec_send_packet(m_codecCtx, packet.get());
    if (ret < 0) {
        NEAPU_LOGE("Error sending packet to decoder: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        return false;
    }

    for (;;) {
        if (!m_frame) {
            m_frame = av_frame_alloc();
            if (!m_frame) {
                NEAPU_LOGE("Failed to allocate AVFrame");
                return false;
            }
        }
        if (!m_frame) {
            NEAPU_LOGE("Failed to allocate AVFrame");
            return false;
        }
        ret = avcodec_receive_frame(m_codecCtx, m_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_unref(m_frame);
            break;
        }

        if (ret < 0) {
            NEAPU_LOGE("Error receiving frame from decoder: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
            av_frame_unref(m_frame);
            return false;
        }

        if (callback) {
            callback(m_frame);
        }
        av_frame_unref(m_frame);
    }
    return true;
}

void DecoderBase::flush()
{
    NEAPU_FUNC_TRACE;
    if (!m_codecCtx) {
        NEAPU_LOGE("Codec context is not initialized");
        return;
    }

    avcodec_flush_buffers(m_codecCtx);
}

bool DecoderBase::initializeCodecContext(const AVStream* stream)
{
    NEAPU_FUNC_TRACE;
    if (!stream) {
        NEAPU_LOGE("Input stream is null");
        return false;
    }

    m_stream = stream;
    m_codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!m_codec) {
        NEAPU_LOGE("Failed to find decoder for codec id {}", codecIdToString(stream->codecpar->codec_id));
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        NEAPU_LOGE("Failed to allocate codec context");
        return false;
    }

    if (avcodec_parameters_to_context(m_codecCtx, stream->codecpar) < 0) {
        NEAPU_LOGE("Failed to copy codec parameters to codec context");
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    m_codecCtx->pkt_timebase = stream->time_base;
    m_codecCtx->time_base = stream->time_base;

    return true;
}
} // namespace media