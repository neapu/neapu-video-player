//
// Created by liu86 on 2025/11/19.
//

#include "Demuxer.h"
#include <stdexcept>
#include <logger.h>
#include "Helper.h"
extern "C" {
#include <libavformat/avformat.h>
}

namespace media {
Demuxer::Demuxer(const std::string& url)
{
    NEAPU_FUNC_TRACE;
    int ret = avformat_open_input(&m_fmtCtx, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to open input file {}: {}", url, errStr);
        throw std::runtime_error("Failed to open input file: " + errStr);
    }

    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to find stream info for file {}: {}", url, errStr);
        throw std::runtime_error("Failed to find stream info: " + errStr);
    }

    int videoStreamIndex = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex >= 0) {
        m_videoStream = m_fmtCtx->streams[videoStreamIndex];
        NEAPU_LOGI("Found video stream index: {}", videoStreamIndex);
    }

    int audioStreamIndex = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIndex >= 0) {
        m_audioStream = m_fmtCtx->streams[audioStreamIndex];
        NEAPU_LOGI("Found audio stream index: {}", audioStreamIndex);
    }

    if (videoStreamIndex < 0 && audioStreamIndex < 0) {
        NEAPU_LOGE("No video or audio streams found in file {}", url);
        throw std::runtime_error("No video or audio streams found");
    }

    m_isEof.store(false);
}
Demuxer::Demuxer(Demuxer&& other) noexcept
{
    m_fmtCtx = other.m_fmtCtx;
    m_videoStream = other.m_videoStream;
    m_audioStream = other.m_audioStream;
    m_videoPacketQueue = std::move(other.m_videoPacketQueue);
    m_audioPacketQueue = std::move(other.m_audioPacketQueue);
    m_isEof.store(other.m_isEof.load());

    other.m_fmtCtx = nullptr;
    other.m_videoStream = nullptr;
    other.m_audioStream = nullptr;
}
Demuxer& Demuxer::operator=(Demuxer&& other) noexcept
{
    if (this != &other) {
        if (m_fmtCtx) {
            avformat_close_input(&m_fmtCtx);
        }

        m_fmtCtx = other.m_fmtCtx;
        m_videoStream = other.m_videoStream;
        m_audioStream = other.m_audioStream;
        m_videoPacketQueue = std::move(other.m_videoPacketQueue);
        m_audioPacketQueue = std::move(other.m_audioPacketQueue);
        m_isEof.store(other.m_isEof.load());

        other.m_fmtCtx = nullptr;
        other.m_videoStream = nullptr;
        other.m_audioStream = nullptr;
    }
    return *this;
}
Demuxer::~Demuxer()
{
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }
}
int Demuxer::videoStreamIndex() const
{
    if (!m_videoStream) {
        return -1;
    }
    return m_videoStream->index;
}
int Demuxer::audioStreamIndex() const
{
    if (!m_audioStream) {
        return -1;
    }
    return m_audioStream->index;
}
AVPacketPtr Demuxer::getVideoPacket()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while (m_videoPacketQueue.empty() && !m_isEof.load()) {
        readFromFile();
    }
    if (m_videoPacketQueue.empty()) {
        return nullptr;
    }
    auto pkt = std::move(m_videoPacketQueue.front());
    m_videoPacketQueue.pop();
    return pkt;
}
AVPacketPtr Demuxer::getAudioPacket()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while (m_audioPacketQueue.empty() && !m_isEof.load()) {
        readFromFile();
    }
    if (m_audioPacketQueue.empty()) {
        return nullptr;
    }
    auto pkt = std::move(m_audioPacketQueue.front());
    m_audioPacketQueue.pop();
    return pkt;
}
void Demuxer::seek(int sec)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const int64_t timestamp = static_cast<int64_t>(sec < 0 ? 0 : sec) * AV_TIME_BASE;
    int ret = av_seek_frame(m_fmtCtx, -1, timestamp, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        NEAPU_LOGE("Failed to seek to {} seconds: {}", sec, getFFmpegErrorString(ret));
        return;
    }
    while (!m_videoPacketQueue.empty()) {
        m_videoPacketQueue.pop();
    }
    while (!m_audioPacketQueue.empty()) {
        m_audioPacketQueue.pop();
    }
    m_isEof.store(false);
}
void Demuxer::readFromFile()
{
    AVPacketPtr packet{av_packet_alloc()};
    const int ret = av_read_frame(m_fmtCtx, packet.get());
    if (ret < 0) {
        if (ret != AVERROR_EOF) {
            NEAPU_LOGE("Error reading frame: {}", getFFmpegErrorString(ret));
        }
        m_isEof.store(true);
        return;
    }
    if (m_videoStream && packet->stream_index == m_videoStream->index) {
        m_videoPacketQueue.push(std::move(packet));
    } else if (m_audioStream && packet->stream_index == m_audioStream->index) {
        m_audioPacketQueue.push(std::move(packet));
    }
}

} // namespace media