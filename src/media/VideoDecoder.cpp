//
// Created by liu86 on 2025/10/30.
//

#include "VideoDecoder.h"
#include <logger.h>
#include "MediaUtils.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}
#ifdef _WIN32
#include <libavutil/hwcontext_d3d11va.h>
#endif

namespace media {
static std::string hwAccelName(AVHWDeviceType type)
{
    switch (type) {
    case AV_HWDEVICE_TYPE_VDPAU: return "AV_HWDEVICE_TYPE_VDPAU";
    case AV_HWDEVICE_TYPE_CUDA: return "AV_HWDEVICE_TYPE_CUDA";
    case AV_HWDEVICE_TYPE_VAAPI: return "AV_HWDEVICE_TYPE_VAAPI";
    case AV_HWDEVICE_TYPE_DXVA2: return "AV_HWDEVICE_TYPE_DXVA2";
    case AV_HWDEVICE_TYPE_QSV: return "AV_HWDEVICE_TYPE_QSV";
    case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: return "AV_HWDEVICE_TYPE_VIDEOTOOLBOX";
    case AV_HWDEVICE_TYPE_D3D11VA: return "AV_HWDEVICE_TYPE_D3D11VA";
    case AV_HWDEVICE_TYPE_DRM: return "AV_HWDEVICE_TYPE_DRM";
    case AV_HWDEVICE_TYPE_OPENCL: return "AV_HWDEVICE_TYPE_OPENCL";
    case AV_HWDEVICE_TYPE_MEDIACODEC: return "AV_HWDEVICE_TYPE_MEDIACODEC";
    case AV_HWDEVICE_TYPE_VULKAN: return "AV_HWDEVICE_TYPE_VULKAN";
    case AV_HWDEVICE_TYPE_D3D12VA: return "AV_HWDEVICE_TYPE_D3D12VA";
    case AV_HWDEVICE_TYPE_AMF: return "AV_HWDEVICE_TYPE_AMF";
    default: return "AV_HWDEVICE_TYPE_NONE";
    }
}
static AVHWDeviceType hwAccelTypeFromEnum(HWAccelType type)
{
    switch (type) {
    case HWAccelType::D3D12VA: return AV_HWDEVICE_TYPE_D3D12VA;
    case HWAccelType::D3D11VA: return AV_HWDEVICE_TYPE_D3D11VA;
    case HWAccelType::DXVA2: return AV_HWDEVICE_TYPE_DXVA2;
    case HWAccelType::CUDA: return AV_HWDEVICE_TYPE_CUDA;
    case HWAccelType::VAAPI: return AV_HWDEVICE_TYPE_VAAPI;
    case HWAccelType::VIDEOTOOLBOX: return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    default: return AV_HWDEVICE_TYPE_NONE;
    }
}
bool VideoDecoder::initialize(const AVStream* stream, const HWAccelConfig& config)
{
    NEAPU_FUNC_TRACE;
    if (config.type == HWAccelType::None) {
        return DecoderBase::initialize(stream);
    }

    if (m_codecCtx) {
        NEAPU_LOGE("Codec context is already initialized");
        return false;
    }

    if (!initializeCodecContext(stream)) {
        return false;
    }

    m_hwConfig = config;
    if (!initializeHWAccel(config.type)) {
        NEAPU_LOGE("Failed to initialize HW acceleration");
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
        NEAPU_LOGE("Failed to open codec with HW acceleration");
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    return true;
}
void VideoDecoder::destroy()
{
    DecoderBase::destroy();
    if (m_hwDeviceCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }
    m_hwPixelFormat = AV_PIX_FMT_NONE;
}
bool VideoDecoder::initializeHWAccel(HWAccelType type)
{
    NEAPU_FUNC_TRACE;
    auto deviceType = hwAccelTypeFromEnum(type);
    if (deviceType == AV_HWDEVICE_TYPE_NONE) {
        NEAPU_LOGE("Unsupported HW acceleration type");
        return false;
    }

    for (int j = 0;; ++j) {
        const AVCodecHWConfig* hwConfig = avcodec_get_hw_config(m_codec, j);
        if (!hwConfig) {
            NEAPU_LOGW("No more HW configs for codec: {}", m_codec->name);
            break;
        }
        if (hwConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            hwConfig->device_type == deviceType) {
            NEAPU_LOGI("Found matching HW config: device_type={}, pix_fmt={}", hwAccelName(hwConfig->device_type),
                       static_cast<int>(hwConfig->pix_fmt));
            m_hwPixelFormat = hwConfig->pix_fmt;
            break;
        }
    }

#ifdef _WIN32
    if (deviceType == AV_HWDEVICE_TYPE_D3D11VA && m_hwConfig.device) {
        m_hwDeviceCtx = av_hwdevice_ctx_alloc(deviceType);
        if (!m_hwDeviceCtx) {
            NEAPU_LOGE("Failed to allocate HW device context for D3D11VA");
            return false;
        }

        const auto hwDevCtx = reinterpret_cast<AVHWDeviceContext*>(m_hwDeviceCtx->data);
        const auto d3d11DevCtx = static_cast<AVD3D11VADeviceContext*>(hwDevCtx->hwctx);
        d3d11DevCtx->device = m_hwConfig.device;
        d3d11DevCtx->device_context = m_hwConfig.context;
        if (av_hwdevice_ctx_init(m_hwDeviceCtx) < 0) {
            NEAPU_LOGE("Failed to initialize D3D11VA HW device context");
            av_buffer_unref(&m_hwDeviceCtx);
            m_hwDeviceCtx = nullptr;
            return false;
        }
        NEAPU_LOGI("Successfully created D3D11VA HW device context with external device");
    } else
#endif
    {
        if (av_hwdevice_ctx_create(&m_hwDeviceCtx, deviceType, nullptr, nullptr, 0) < 0) {
            NEAPU_LOGE("Failed to create HW device context: {}", hwAccelName(deviceType));
            return false;
        }
        NEAPU_LOGI("Successfully created HW device context: {}", hwAccelName(deviceType));
    }

    m_codecCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
    m_codecCtx->opaque = this;
    m_codecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
        VideoDecoder* decoder = static_cast<VideoDecoder*>(ctx->opaque);
        for (const enum AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == decoder->m_hwPixelFormat) {
                NEAPU_LOGI("Using HW pixel format: {}", static_cast<int>(*p));
                return *p;
            }
        }
        NEAPU_LOGE("Failed to get suitable HW pixel format");
        return AV_PIX_FMT_NONE;
    };

    return true;
}

}