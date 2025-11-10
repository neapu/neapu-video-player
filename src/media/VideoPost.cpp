//
// Created by liu86 on 2025/11/2.
//

#include "VideoPost.h"
#include <logger.h>
#include <cmath>
#include "MediaUtils.h"
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

namespace media {
constexpr int64_t MAX_WATER_MARK_US = 3000000;
VideoPost::~VideoPost()
{
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (m_targetFrame) {
        av_frame_free(&m_targetFrame);
        m_targetFrame = nullptr;
    }

    if (m_swFrame) {
        av_frame_free(&m_swFrame);
        m_swFrame = nullptr;
    }
}

void VideoPost::initialize(double fps, bool copyBackRender, AVRational timeBase)
{
    m_fps = fps;
    m_copyBackRender = copyBackRender;
    m_timeBase = timeBase;
    m_firstFrame = true;
}

void VideoPost::destroy()
{
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (m_targetFrame) {
        av_frame_free(&m_targetFrame);
        m_targetFrame = nullptr;
    }

    if (m_swFrame) {
        av_frame_free(&m_swFrame);
        m_swFrame = nullptr;
    }

    m_fps = 0;
    m_basePts = 0;
}

VideoFramePtr VideoPost::copyFrame(const AVFrame* frame)
{
    if (!frame) {
        return nullptr;
    }

    if (frame->format == AV_PIX_FMT_YUV420P) {
        return VideoFrame::fromAVFrame(frame, m_timeBase.num, m_timeBase.den);
    }

    if (frame->format == AV_PIX_FMT_D3D11) {
        return processHWFrame(frame);
    }

    if (convertFrame(frame)) {
        return VideoFrame::fromAVFrame(m_targetFrame, m_timeBase.num, m_timeBase.den);
    }

    NEAPU_LOGE("Failed to convert frame to YUV420P");
    return nullptr;
}
VideoFramePtr VideoPost::processHWFrame(const AVFrame* frame)
{
#ifdef _WIN32
    D3D11_TEXTURE2D_DESC desc;
    auto* texture = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
    texture->GetDesc(&desc);
    if (desc.Format != DXGI_FORMAT_NV12) {
        m_copyBackRender = true;
    }
#endif
    if (!m_copyBackRender) {
        return VideoFrame::fromAVFrame(frame, m_timeBase.num, m_timeBase.den);
    }

    if (!m_swFrame) {
        m_swFrame = av_frame_alloc();
        if (!m_swFrame) {
            NEAPU_LOGE("Failed to allocate software frame for copy-back rendering");
            return nullptr;
        }
    }

    av_frame_unref(m_swFrame);
    if (int ret = av_hwframe_transfer_data(m_swFrame, frame, 0); ret < 0) {
        NEAPU_LOGE("Failed to transfer HW frame to SW frame: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        return nullptr;
    }

    // copy metadata
    m_swFrame->pts = frame->pts;
    m_swFrame->pkt_dts = frame->pkt_dts;
    m_swFrame->duration = frame->duration;
    m_swFrame->best_effort_timestamp = frame->best_effort_timestamp;
    m_swFrame->colorspace = frame->colorspace;
    m_swFrame->color_range = frame->color_range;
    m_swFrame->color_primaries = frame->color_primaries;
    m_swFrame->color_trc = frame->color_trc;

    if (m_swFrame->format == AV_PIX_FMT_YUV420P) {
        return VideoFrame::fromAVFrame(m_swFrame, m_timeBase.num, m_timeBase.den);
    }

    if (!convertFrame(m_swFrame)) {
        NEAPU_LOGE("Failed to convert SW frame to YUV420P");
        return nullptr;
    }

    return VideoFrame::fromAVFrame(m_targetFrame, m_timeBase.num, m_timeBase.den);
}

bool VideoPost::convertFrame(const AVFrame* frame)
{
    if (m_swsCtx && (frame->width != m_lastWidth || frame->height != m_lastHeight || frame->format != m_lastFormat)) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (!m_swsCtx) {
        // clang-format off
        m_swsCtx = sws_getContext(
            frame->width,
            frame->height,
            static_cast<AVPixelFormat>(frame->format),
            frame->width,
            frame->height,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr);
        // clang-format on
        if (!m_swsCtx) {
            NEAPU_LOGE("Failed to create SwsContext for frame conversion");
            return false;
        }
        m_lastWidth = frame->width;
        m_lastHeight = frame->height;
        m_lastFormat = frame->format;
    }

    if (!m_targetFrame) {
        m_targetFrame = av_frame_alloc();
        if (!m_targetFrame) {
            NEAPU_LOGE("Failed to allocate target frame for conversion");
            return false;
        }
        m_targetFrame->format = AV_PIX_FMT_YUV420P;
        m_targetFrame->width = frame->width;
        m_targetFrame->height = frame->height;
        int ret = av_frame_get_buffer(m_targetFrame, 32);
        if (ret < 0) {
            NEAPU_LOGE("Failed to allocate buffer for target frame: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
            return false;
        }
    }

    // clang-format off
    int ret = sws_scale(
            m_swsCtx,
            frame->data,
            frame->linesize,
            0,
            frame->height,
            m_targetFrame->data,
            m_targetFrame->linesize);
    // clang-format on
    if (ret <= 0) {
        NEAPU_LOGE("Failed to scale frame: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        return false;
    }

    // copy metadata
    m_targetFrame->pts = frame->pts;
    m_targetFrame->pkt_dts = frame->pkt_dts;
    m_targetFrame->duration = frame->duration;
    m_targetFrame->best_effort_timestamp = frame->best_effort_timestamp;
    m_targetFrame->colorspace = frame->colorspace;
    m_targetFrame->color_range = frame->color_range;
    m_targetFrame->color_primaries = frame->color_primaries;
    m_targetFrame->color_trc = frame->color_trc;

    return true;
}
} // namespace media