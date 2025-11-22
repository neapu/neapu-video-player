//
// Created by liu86 on 2025/11/20.
//

#include "VideoDecoder.h"
#include <logger.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}
#ifdef _WIN32
#include <libavutil/hwcontext_d3d11va.h>
#endif

namespace media {
static AVHWDeviceType hwAccelTypeFromEnum(VideoDecoder::HWAccelMethod method)
{
    using enum VideoDecoder::HWAccelMethod;
    switch (method) {
    case D3D11VA: return AV_HWDEVICE_TYPE_D3D11VA;
    case DXVA: return AV_HWDEVICE_TYPE_DXVA2;
    case Vaapi: return AV_HWDEVICE_TYPE_VAAPI;
    case VideoToolBox: return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    default: return AV_HWDEVICE_TYPE_NONE;
    }
}
#ifdef _WIN32
VideoDecoder::VideoDecoder(AVStream* stream, const AVPacketCallback& packetCallback, HWAccelMethod hwaccelMethod, ID3D11Device* d3d11Device)
    : DecoderBase(stream, packetCallback)
    , m_hwaccelMethod(hwaccelMethod)
    , m_d3d11Device(d3d11Device)
#else
VideoDecoder::VideoDecoder(AVStream* stream, const AVPacketCallback& packetCallback, HWAccelMethod hwaccelMethod)
    : DecoderBase(stream, packetCallback)
    , m_hwaccelMethod(hwaccelMethod)
#endif
{
    NEAPU_FUNC_TRACE;
    if (!m_stream) {
        NEAPU_LOGE("Stream is null");
        throw std::runtime_error("Stream is null");
    }

    DecoderBase::initializeContext();

    VideoDecoder::initializeHWContext();

    int ret = avcodec_open2(m_codecCtx, m_codec, nullptr);
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to open video codec: {}", errStr);
        throw std::runtime_error("Failed to open video codec: " + errStr);
    }
}
VideoDecoder::~VideoDecoder()
{
    if (m_hwDeviceCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
}
void VideoDecoder::initializeHWContext()
{
    NEAPU_FUNC_TRACE;
    auto deviceType = hwAccelTypeFromEnum(m_hwaccelMethod);
    if (deviceType == AV_HWDEVICE_TYPE_NONE) {
        NEAPU_LOGI("No hardware acceleration selected");
        return;
    }

    for (int i = 0;; i++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(m_codec, i);
        if (!config) {
            NEAPU_LOGW("No suitable HW config found for codec {}", getAVCodecIDString(m_codec->id));
            return;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == deviceType) {
            m_hwPixelFormat = config->pix_fmt;
            break;
        }
    }
    if (m_hwPixelFormat == AV_PIX_FMT_NONE) {
        // 回退到软解
        NEAPU_LOGW("No suitable HW pixel format found for codec {}", getAVCodecIDString(m_codec->id));
        return;
    }

#ifdef _WIN32
    if (deviceType == AV_HWDEVICE_TYPE_D3D11VA && m_d3d11Device) {
        m_hwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (!m_hwDeviceCtx) {
            NEAPU_LOGE("Failed to allocate D3D11VA HW device context");
            return;
        }

        const auto hwDevCtx = reinterpret_cast<AVHWDeviceContext*>(m_hwDeviceCtx->data);
        const auto d3d11DevCtx = static_cast<AVD3D11VADeviceContext*>(hwDevCtx->hwctx);
        d3d11DevCtx->device = m_d3d11Device;
        d3d11DevCtx->device->AddRef();
        int ret = av_hwdevice_ctx_init(m_hwDeviceCtx);
        if (ret < 0) {
            std::string errStr = getFFmpegErrorString(ret);
            NEAPU_LOGE("Failed to initialize D3D11VA HW device context: {}", errStr);
            av_buffer_unref(&m_hwDeviceCtx);
            m_hwDeviceCtx = nullptr;
            return;
        }
        NEAPU_LOGI("Initialized D3D11VA HW device context for video decoding");
    } else
#endif
    {
        int ret = av_hwdevice_ctx_create(&m_hwDeviceCtx, deviceType, nullptr, nullptr, 0);
        if (ret < 0) {
            std::string errStr = getFFmpegErrorString(ret);
            NEAPU_LOGE("Failed to create HW device context: {}", errStr);
            m_hwDeviceCtx = nullptr;
            return;
        }
        NEAPU_LOGI("Created HW device context for video decoding");
    }

    m_codecCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
    m_codecCtx->opaque = this;
    m_codecCtx->get_format = [](AVCodecContext* ctx, const AVPixelFormat* pix_fmts) -> AVPixelFormat {
        const auto* decoder = static_cast<VideoDecoder*>(ctx->opaque);
        for (const AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            if (*p == decoder->m_hwPixelFormat) {
                NEAPU_LOGI("Using HW pixel format {} for video decoding", getAVPixelFormatString(*p));
                return *p;
            }
        }
        NEAPU_LOGW("HW pixel format {} not supported by decoder, falling back to software decoding", getAVPixelFormatString(decoder->m_hwPixelFormat));
        return AV_PIX_FMT_NONE;
    };
}
AVFrame* VideoDecoder::convertFixelFormat(AVFrame* avFrame)
{
    if (m_swsCtx &&
        (m_swsCtx->src_format != avFrame->format ||
         m_swsCtx->src_w != avFrame->width ||
         m_swsCtx->src_h != avFrame->height)) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (!m_swsCtx) {
        m_swsCtx = sws_getContext(
            avFrame->width,
            avFrame->height,
            static_cast<AVPixelFormat>(avFrame->format),
            avFrame->width,
            avFrame->height,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr);
        if (!m_swsCtx) {
            NEAPU_LOGE("Failed to create SwsContext for pixel format conversion");
            return nullptr;
        }
    }

    AVFrame* retFrame = av_frame_alloc();
    if (!retFrame) {
        NEAPU_LOGE("Failed to allocate frame for pixel format conversion");
        return nullptr;
    }
    retFrame->format = AV_PIX_FMT_YUV420P;
    retFrame->width = avFrame->width;
    retFrame->height = avFrame->height;
    int ret = av_frame_get_buffer(retFrame, 32);
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to allocate buffer for converted frame: {}", errStr);
        av_frame_free(&retFrame);
        return nullptr;
    }
    ret = sws_scale(
        m_swsCtx,
        avFrame->data,
        avFrame->linesize,
        0,
        avFrame->height,
        retFrame->data,
        retFrame->linesize);
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to scale frame for pixel format conversion: {}", errStr);
        av_frame_free(&retFrame);
        return nullptr;
    }

    retFrame->pts = avFrame->pts;
    retFrame->pkt_dts = avFrame->pkt_dts;
    retFrame->duration = avFrame->duration;
    retFrame->color_primaries = avFrame->color_primaries;
    retFrame->color_trc = avFrame->color_trc;
    retFrame->colorspace = avFrame->colorspace;
    retFrame->color_range = avFrame->color_range;
    av_frame_free(&avFrame);
    return retFrame;
}
AVFrame* VideoDecoder::hwFrameTransfer(AVFrame* avFrame)
{
    AVFrame* swFrame = av_frame_alloc();
    if (!swFrame) {
        NEAPU_LOGE("Failed to allocate software frame for downloading hardware frame");
        return nullptr;
    }
    int ret = av_hwframe_transfer_data(swFrame, avFrame, 0);
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to transfer hardware frame to software frame: {}", errStr);
        av_frame_free(&swFrame);
        return nullptr;
    }

    swFrame->pts = avFrame->pts;
    swFrame->pkt_dts = avFrame->pkt_dts;
    swFrame->duration = avFrame->duration;
    swFrame->color_primaries = avFrame->color_primaries;
    swFrame->color_trc = avFrame->color_trc;
    swFrame->colorspace = avFrame->colorspace;
    swFrame->color_range = avFrame->color_range;
    swFrame->time_base = m_stream->time_base;
    av_frame_free(&avFrame);
    return swFrame;
}
FramePtr VideoDecoder::postProcess(AVFrame* avFrame)
{
    if (avFrame->format == AV_PIX_FMT_D3D11) {
        avFrame->time_base = m_stream->time_base;
        return std::make_unique<Frame>(avFrame);
    }

    AVFrame* processedFrame = avFrame;
    if (processedFrame->hw_frames_ctx) {
        processedFrame = hwFrameTransfer(avFrame);
        if (!processedFrame) {
            return nullptr;
        }
    }

    if (processedFrame->format != AV_PIX_FMT_YUV420P) {
        AVFrame* convertedFrame = convertFixelFormat(processedFrame);
        if (!convertedFrame) {
            return nullptr;
        }
        processedFrame = convertedFrame;
    }

    processedFrame->time_base = m_stream->time_base;
    return std::make_unique<Frame>(processedFrame);
}
} // namespace media