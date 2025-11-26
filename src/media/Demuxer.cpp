//
// Created by liu86 on 2025/11/19.
//

#include "Demuxer.h"
#include <stdexcept>
#include <logger.h>
#include "Helper.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
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

    m_readThread = std::thread(&Demuxer::readThreadFunc, this);
}
Demuxer::Demuxer(Demuxer&& other) noexcept
{
    m_fmtCtx = other.m_fmtCtx;
    m_videoStream = other.m_videoStream;
    m_audioStream = other.m_audioStream;
    m_videoQueue = std::move(other.m_videoQueue);
    m_audioQueue = std::move(other.m_audioQueue);
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
        m_videoQueue = std::move(other.m_videoQueue);
        m_audioQueue = std::move(other.m_audioQueue);
        m_isEof.store(other.m_isEof.load());

        other.m_fmtCtx = nullptr;
        other.m_videoStream = nullptr;
        other.m_audioStream = nullptr;
    }
    return *this;
}
Demuxer::~Demuxer()
{
    m_running = false;
    m_audioQueue.clear();
    m_videoQueue.clear();
    if (m_readThread.joinable()) {
        m_readThread.join();
    }
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
PacketPtr Demuxer::getVideoPacket()
{
    if (!m_videoStream) {
        return nullptr;
    }
    return m_videoQueue.pop();
}
PacketPtr Demuxer::getAudioPacket()
{
    if (!m_audioStream) {
        return nullptr;
    }
    return m_audioQueue.pop();
}
void Demuxer::seek(double seconds, int serial)
{
    m_seekTarget = seconds;
    m_serial = serial;
    m_seekRequested = true;
    m_videoQueue.clear();
    m_audioQueue.clear();
    if (m_isEof) {
        m_running = false;
        if (m_readThread.joinable()) {
            m_readThread.join();
        }
        m_isEof = false;
        m_running = true;
        m_readThread = std::thread(&Demuxer::readThreadFunc, this);
    }
}

double Demuxer::durationSeconds() const
{
    if (!m_fmtCtx) {
        return 0.0;
    }

    if (m_fmtCtx->duration != AV_NOPTS_VALUE && m_fmtCtx->duration > 0) {
        return static_cast<double>(m_fmtCtx->duration) / AV_TIME_BASE;
    }

    if (m_videoStream && m_videoStream->duration != AV_NOPTS_VALUE && m_videoStream->duration > 0) {
        return static_cast<double>(m_videoStream->duration) * av_q2d(m_videoStream->time_base);
    }
    if (m_audioStream && m_audioStream->duration != AV_NOPTS_VALUE && m_audioStream->duration > 0) {
        return static_cast<double>(m_audioStream->duration) * av_q2d(m_audioStream->time_base);
    }

    double maxDur = 0.0;
    for (unsigned int i = 0; i < m_fmtCtx->nb_streams; ++i) {
        AVStream* st = m_fmtCtx->streams[i];
        if (st && st->duration != AV_NOPTS_VALUE && st->duration > 0) {
            const double d = static_cast<double>(st->duration) * av_q2d(st->time_base);
            if (d > maxDur) maxDur = d;
        }
    }
    return maxDur;
}
void Demuxer::readThreadFunc()
{
    while (m_running) {
        if (m_seekRequested) {
            double sec = m_seekTarget;
            if (sec < 0.0) sec = 0.0;
            const int64_t timestamp = static_cast<int64_t>(sec * AV_TIME_BASE);
            int ret = av_seek_frame(m_fmtCtx, -1, timestamp, AVSEEK_FLAG_BACKWARD);
            if (ret < 0) {
                NEAPU_LOGE("Failed to seek to {} seconds: {}", sec, getFFmpegErrorString(ret));
                m_isEof = true;
                break;
            }
            if (m_videoStream) {
                m_videoQueue.clear();
                m_videoQueue.push(std::make_unique<Packet>(Packet::PacketType::Flush, m_serial.load()));
            }
            if (m_audioStream) {
                m_audioQueue.clear();
                m_audioQueue.push(std::make_unique<Packet>(Packet::PacketType::Flush, m_serial.load()));
            }
            m_isEof = false;
            m_seekRequested = false;
        }

        auto packet = std::make_unique<Packet>(Packet::PacketType::Video, m_serial.load());
        int ret = av_read_frame(m_fmtCtx, packet->avPacket());
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                NEAPU_LOGI("Reached end of file");
                m_isEof.store(true);
                if (m_videoStream) {
                    m_videoQueue.push(std::make_unique<Packet>(Packet::PacketType::Eof, -1));
                }
                if (m_audioStream) {
                    m_audioQueue.push(std::make_unique<Packet>(Packet::PacketType::Eof, -1));
                }
                break;
            } else {
                NEAPU_LOGE("Error reading frame: {}", getFFmpegErrorString(ret));
                continue;
            }
        }
        if (m_videoStream && packet->avPacket()->stream_index == m_videoStream->index) {
            packet->setType(Packet::PacketType::Video);
            m_videoQueue.push(std::move(packet));
        } else if (m_audioStream && packet->avPacket()->stream_index == m_audioStream->index) {
            packet->setType(Packet::PacketType::Audio);
            m_audioQueue.push(std::move(packet));
        }
    }
}

} // namespace media