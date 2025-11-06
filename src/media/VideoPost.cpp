//
// Created by liu86 on 2025/11/2.
//

#include "VideoPost.h"
#include <logger.h>
#include "MediaUtils.h"
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

namespace media {
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
void VideoPost::setFps(double fps)
{
    m_fps = fps;
}
void VideoPost::setCopyBackRender(bool copyBackRender)
{
    m_copyBackRender = copyBackRender;
}
void VideoPost::pushVideoFrame(const AVFrame* frame)
{
    auto videoFrame = copyFrame(frame);
    if (!videoFrame) {
        NEAPU_LOGW("Failed to copy video frame");
        return;
    }

    std::lock_guard lock(m_mutex);
    int64_t durationUs = videoFrame->duration();
    if (durationUs <= 0) {
        if (m_fps > 0) {
            durationUs = static_cast<int64_t>(1000000.0 / m_fps + 0.5); // 约等于 1e6/fps 微秒
        } else {
            durationUs = 40000; // 默认25fps -> 40ms
        }
    }
    m_waterLevel += durationUs;
    if (videoFrame->pts() < 0) {
        videoFrame->setPts(m_basePts);
        m_basePts += durationUs;
    }
    m_videoFrameQueue.push(std::move(videoFrame));
}

VideoFramePtr VideoPost::popVideoFrame()
{
    if (m_stopFlag || !m_startTimePointSet) {
        return nullptr;
    }
    auto popFrame = [this]() -> VideoFramePtr {
        auto videoFrame = std::move(m_videoFrameQueue.front());
        m_videoFrameQueue.pop();
        m_waterLevel -= videoFrame->duration();
        NEAPU_LOGD("Pop video. pts: {}", videoFrame->pts());
        return videoFrame;
    };

    int64_t targetPts = 0;
    std::unique_lock lock(m_mutex);
    if (m_videoFrameQueue.empty()) {
        return nullptr;
    }
    // NEAPU_LOGD("Queue size: {}", m_videoFrameQueue.size());
    const auto& frame = m_videoFrameQueue.front();
    targetPts = frame->pts();

    // 计算m_startTimePoint+pts为目标时间点
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    const auto targetTimePoint = m_startTimePoint + std::chrono::microseconds(targetPts);
    if (targetTimePoint < now) {
        // 目标时间点已过，直接弹出
        return popFrame();
    }

    const auto waitDuration = targetTimePoint - now;
    // 等待直到目标时间点或收到中断/停止信号
    while (!m_stopFlag) {
        if (m_waitCondVar.wait_for(lock, waitDuration) == std::cv_status::timeout) {
            break;
        }
    }
    if (m_stopFlag) {
        return nullptr;
    } else {
        return popFrame();
    }
}

void VideoPost::clear()
{
    std::lock_guard lock(m_mutex);
    while (!m_videoFrameQueue.empty()) {
        m_videoFrameQueue.pop();
    }
    m_waterLevel = -1.0;
    // 重置吐帧时间
    m_hasLastOutputTime = false;
    m_stopFlag = true;
    m_startTimePointSet = false;
}

void VideoPost::setStopFlag(bool stop)
{
    m_stopFlag = stop;
    m_waitCondVar.notify_all();
}

void VideoPost::setStartTimePoint(const std::chrono::steady_clock::time_point& timePoint)
{
    m_startTimePoint = timePoint;
    m_startTimePointSet = true;
    NEAPU_LOGD_STREAM << "Set start time point: " 
        << std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
}
bool VideoPost::isQueueEmpty()
{
    std::lock_guard lock(m_mutex);
    return m_videoFrameQueue.empty();
}
VideoFramePtr VideoPost::copyFrame(const AVFrame* frame)
{
    if (!frame) {
        return nullptr;
    }

    if (frame->format == AV_PIX_FMT_YUV420P) {
        return VideoFrame::fromAVFrame(frame, m_timeBase);
    }

    if (frame->format == AV_PIX_FMT_D3D11) {
        return processHWFrame(frame);
    }

    if (convertFrame(frame)) {
        return VideoFrame::fromAVFrame(m_targetFrame, m_timeBase);
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
        return VideoFrame::fromAVFrame(frame, m_timeBase);
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
        return VideoFrame::fromAVFrame(m_swFrame, m_timeBase);
    }

    if (!convertFrame(m_swFrame)) {
        NEAPU_LOGE("Failed to convert SW frame to YUV420P");
        return nullptr;
    }

    return VideoFrame::fromAVFrame(m_targetFrame, m_timeBase);
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