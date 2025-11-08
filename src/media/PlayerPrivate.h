//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include "pub.h"
#include "VideoFrame.h"
#include "AudioFrame.h"
#include "Demuxer.h"
#include "AudioDecoder.h"
#include "AudioPost.h"
#include "VideoDecoder.h"
#include "VideoPost.h"
#include <thread>
#include <atomic>
#include "Clock.h"
#include <shared_mutex>

namespace media {

class PlayerPrivate {
public:
    PlayerPrivate() = default;
    ~PlayerPrivate();

    bool openMedia(const OpenMediaParams& params);
    void closeMedia();

    bool isPause() const;
    bool isOpen() const;

    void play();
    void stop();

    VideoFramePtr getVideoFrame();
    AudioFramePtr getAudioFrame();

    int audioSampleRate() const;
    int audioChannels() const;

    bool hasAudioStream() const;
    bool hasVideoStream() const;

    int64_t duration() const { return m_duration; }

    void seek(int64_t second);

private:
    void decodeThreadFunc();
    bool initVideoDecoder(const AVStream* stream);

private:
    OpenMediaParams m_openParams;
    Clock m_clock;

    Demuxer m_demuxer;
    AudioDecoder m_audioDecoder;
    AudioPost m_audioPost{m_clock};

    VideoDecoder m_videoDecoder;
    VideoPost m_videoPost{m_clock};

    std::thread m_decodeThread;
    std::shared_mutex m_mutex;
    bool m_stopFlag{false};
    std::atomic<bool> m_pause{false};
    int64_t m_duration{0};
};

} // namespace media
