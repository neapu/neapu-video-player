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
void AVPacketFreeDeleter::operator()(AVPacket* pkt) const
{
    if (pkt) {
        av_packet_free(&pkt);
    }
}
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
}

bool Demuxer::isOpen() const
{
    return m_formatCtx != nullptr;
}

AVPacketPtr Demuxer::read()
{
    if (!m_formatCtx) {
        NEAPU_LOGE("Demuxer is not open or packet is not allocated");
        return nullptr;
    }

    auto* packet = av_packet_alloc();
    if (!packet) {
        NEAPU_LOGE("Failed to allocate AVPacket");;
        return nullptr;
    }
    const int ret = av_read_frame(m_formatCtx, packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            NEAPU_LOGI("End of file reached");
        } else {
            NEAPU_LOGE("Failed to read frame: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        }
        return nullptr;
    }
    return AVPacketPtr{packet};
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

int64_t Demuxer::mediaDuration() const
{
    if (!m_formatCtx) {
        return 0;
    }

    if (m_formatCtx->duration) {
        return av_rescale_q(m_formatCtx->duration, AV_TIME_BASE_Q, AV_TIME_BASE_Q);
    }

    if (m_audioStream->duration) {
        return av_rescale_q(m_audioStream->duration, m_audioStream->time_base, AV_TIME_BASE_Q);
    }

    if (m_videoStream->duration) {
        return av_rescale_q(m_videoStream->duration, m_videoStream->time_base, AV_TIME_BASE_Q);
    }

    return 0;
}

int64_t Demuxer::seek(int64_t second)
{
    NEAPU_FUNC_TRACE;
    if (!m_formatCtx) {
        NEAPU_LOGE("Demuxer is not open");
        return -1;
    }
    // 优先选择可用的视频流作为锚定流，但要排除封面(仅一帧/附加图片)等非时间轴的视频流
    AVStream* anchor = nullptr;
    auto is_cover_video = [this]() -> bool {
        if (!m_videoStream) return false;
        // 典型封面流：带有附加图片标志
        if (m_videoStream->disposition & AV_DISPOSITION_ATTACHED_PIC) return true;
        // 兼容性判断：明显只有一帧且没有有效帧率的情况也视为封面
        if (m_videoStream->nb_frames == 1) return true;
        return false;
    };

    if (m_videoStream && !is_cover_video()) {
        anchor = m_videoStream;
    } else if (m_audioStream) {
        anchor = m_audioStream;
    }

    const int64_t targetUs = second * AV_TIME_BASE;
    int flags = AVSEEK_FLAG_BACKWARD;
    // 纯音频或使用音频流定位时允许非关键帧，提高定位精度
    if (anchor && anchor->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        flags |= AVSEEK_FLAG_ANY;
    }

    int ret = -1;
    if (anchor) {
        const int64_t streamTs = av_rescale_q(targetUs, AV_TIME_BASE_Q, anchor->time_base);
        ret = av_seek_frame(m_formatCtx, anchor->index, streamTs, flags);
        if (ret < 0) {
            NEAPU_LOGW("Seek by anchor stream failed: {}; fallback to global", MediaUtils::getFFmpegError(ret));
        } else {
            avformat_flush(m_formatCtx);
            NEAPU_LOGI("Seek success (stream): ts={}s, stream_ts={}, flags={}", second, streamTs, flags);
            return targetUs;
        }
    }

    // 全局时间轴回退
    ret = av_seek_frame(m_formatCtx, -1, targetUs, flags);
    if (ret < 0) {
        NEAPU_LOGE("Seek failed: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        return -1;
    }
    avformat_flush(m_formatCtx);
    NEAPU_LOGI("Seek success (global): ts={}s, targetUs={}, flags={}", second, targetUs, flags);
    return targetUs;
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