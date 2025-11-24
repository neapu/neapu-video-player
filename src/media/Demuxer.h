//
// Created by liu86 on 2025/11/19.
//

#pragma once
#include <string>
#include "Helper.h"
#include <queue>
#include <mutex>
#include <atomic>

typedef struct AVFormatContext AVFormatContext;
typedef struct AVStream AVStream;

namespace media {

class Demuxer {
public:
    explicit Demuxer(const std::string& url);
    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;
    Demuxer(Demuxer&& other) noexcept;
    Demuxer& operator=(Demuxer&& other) noexcept;
    ~Demuxer();

    AVStream* videoStream() const { return m_videoStream; }
    AVStream* audioStream() const { return m_audioStream; }

    bool hasVideoStream() const { return m_videoStream != nullptr; }
    bool hasAudioStream() const { return m_audioStream != nullptr; }

    int videoStreamIndex() const;
    int audioStreamIndex() const;

    AVPacketPtr getVideoPacket();
    AVPacketPtr getAudioPacket();

    bool isEof() const { return m_isEof.load(); }

    void seek(double seconds);

    double durationSeconds() const;

private:
    void readFromFile();

private:
    AVFormatContext* m_fmtCtx{nullptr};
    AVStream* m_videoStream{nullptr};
    AVStream* m_audioStream{nullptr};

    std::queue<AVPacketPtr> m_videoPacketQueue;
    std::queue<AVPacketPtr> m_audioPacketQueue;
    std::mutex m_mutex;
    std::atomic_bool m_isEof{false};
};

} // namespace media
