//
// Created by liu86 on 2025/10/29.
//

#include "Demuxer.h"
extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include <logger.h>
#include "MediaUtils.h"

namespace media {
bool Demuxer::open(const std::string& url)
{
    NEAPU_FUNC_TRACE;
    if (m_formatCtx) {
        return false;
    }

    m_videoStream = nullptr;
    m_audioStream = nullptr;

    int ret = avformat_open_input(&m_formatCtx, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        NEAPU_LOGE("Failed to open input {}: {}; code: {}", url, MediaUtils::getFFmpegError(ret), ret);
        m_formatCtx = nullptr;
        return false;
    }

    ret = avformat_find_stream_info(m_formatCtx, nullptr);
    if (ret < 0) {
        NEAPU_LOGE("Failed to find stream info: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        avformat_close_input(&m_formatCtx);
        return false;
    }

    const int vIdx = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (vIdx >= 0) {
        m_videoStream = m_formatCtx->streams[vIdx];
    } else {
        NEAPU_LOGW("No video stream found");
    }

    const int aIdx = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (aIdx >= 0) {
        m_audioStream = m_formatCtx->streams[aIdx];
    } else {
        NEAPU_LOGW("No audio stream found");
    }

    if (!m_videoStream && !m_audioStream) {
        NEAPU_LOGE("No audio/video streams present in input");
        avformat_close_input(&m_formatCtx);
        return false;
    }

    NEAPU_LOGI("Demuxer opened: url={}, video_idx={}, audio_idx={}", url,
               m_videoStream ? m_videoStream->index : -1,
               m_audioStream ? m_audioStream->index : -1);
    // 预分配一个可复用的 AVPacket
    m_packet = av_packet_alloc();
    if (!m_packet) {
        NEAPU_LOGE("Failed to allocate AVPacket");
        avformat_close_input(&m_formatCtx);
        return false;
    }
    return true;
}

void Demuxer::close()
{
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }
    m_videoStream = nullptr;
    m_audioStream = nullptr;
    if (m_packet) {
        av_packet_free(&m_packet);
    }
}

bool Demuxer::isOpen() const
{
    return m_formatCtx != nullptr;
}

AVPacket* Demuxer::read()
{
    if (!m_formatCtx || !m_packet) {
        NEAPU_LOGE("Demuxer is not open or packet is not allocated");
        return nullptr;
    }

    av_packet_unref(m_packet);
    const int ret = av_read_frame(m_formatCtx, m_packet);
    if (ret < 0) {
        if (ret != AVERROR_EOF) {
            NEAPU_LOGE("Failed to read frame: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        }
        return nullptr;
    }
    return m_packet;
}

int Demuxer::videoStreamIndex() const
{
    return m_videoStream ? m_videoStream->index : -1;
}

int Demuxer::audioStreamIndex() const
{
    return m_audioStream ? m_audioStream->index : -1;
}

AVStream* Demuxer::videoStream() const
{
    return m_videoStream;
}

AVStream* Demuxer::audioStream() const
{
    return m_audioStream;
}

void Demuxer::seek(double timestamp)
{
    NEAPU_FUNC_TRACE;
    if (!m_formatCtx) {
        NEAPU_LOGE("Demuxer is not open");
        return;
    }

    int streamIndex = -1;
    int64_t targetPts = 0;
    if (m_videoStream) {
        streamIndex = m_videoStream->index;
        const double tb = av_q2d(m_videoStream->time_base);
        targetPts = static_cast<int64_t>(timestamp / tb);
    } else {
        // 退化处理：无视频流时按全局时间基（秒）跳转
        streamIndex = -1;
        targetPts = static_cast<int64_t>(timestamp * 1000000.0); // AV_TIME_BASE = 1e6
    }

    const int ret = av_seek_frame(m_formatCtx, streamIndex, targetPts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        NEAPU_LOGE("Seek failed: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        return;
    }
    avformat_flush(m_formatCtx);
}

void Demuxer::seekToStart()
{
    NEAPU_FUNC_TRACE;
    if (!m_formatCtx) {
        NEAPU_LOGE("Demuxer is not open");
        return;
    }

    int streamIndex = m_videoStream ? m_videoStream->index : -1;
    const int ret = av_seek_frame(m_formatCtx, streamIndex, 0, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        NEAPU_LOGE("Seek to start failed: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        return;
    }
    avformat_flush(m_formatCtx);
}
} // namespace media