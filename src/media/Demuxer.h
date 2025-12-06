//
// Created by liu86 on 2025/11/19.
//

#pragma once
#include <string>
#include "Helper.h"
#include <atomic>
#include <thread>
#include "Queue.h"

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

    PacketPtr getVideoPacket();
    PacketPtr getAudioPacket();

    bool isEof() const { return m_isEof.load(); }

    void seek(double seconds, int serial, bool noFlush = false);
    void clear();

    double durationSeconds() const;

private:
    void readThreadFunc();

private:
    AVFormatContext* m_fmtCtx{nullptr};
    AVStream* m_videoStream{nullptr};
    AVStream* m_audioStream{nullptr};
    PacketQueue m_videoQueue{50 * 1024 * 1024}; // 50 MB
    PacketQueue m_audioQueue{10 * 1024 * 1024}; // 10 MB
    std::thread m_readThread;

    std::atomic_bool m_isEof{false};

    std::atomic<double> m_seekTarget{0.0};
    std::atomic_bool m_seekRequested{false};

    std::atomic_bool m_running{true};

    std::atomic_int m_serial{0};
    std::atomic_bool m_noFlush{false};
};

} // namespace media
