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

    AudioFramePtr getAudioFrame();

    int audioSampleRate() const;
    int audioChannels() const;

    bool hasAudioStream() const;
    bool hasVideoStream() const;

    int64_t duration() const { return m_duration; }

    void seek(int64_t second);

private:
    void readThreadFunc();
    void audioDecodeThreadFunc();
    void videoDecodeThreadFunc();
    bool initVideoDecoder(const AVStream* stream);

private:
    OpenMediaParams m_openParams;
    Clock m_clock;

    Demuxer m_demuxer;
    AudioDecoder m_audioDecoder;
    AudioPost m_audioPost;

    VideoDecoder m_videoDecoder;
    VideoPost m_videoPost;

    std::thread m_readThread;
    std::thread m_audioDecodeThread;
    std::thread m_videoDecodeThread;
    std::atomic_bool m_running{false};
    int64_t m_duration{0};

    std::queue<AVPacketPtr> m_videoPacketQueue;
    std::mutex m_videoPacketMutex;
    std::condition_variable m_videoPacketCondVar;

    std::queue<AVPacketPtr> m_audioPacketQueue;
    std::mutex m_audioPacketMutex;
    std::condition_variable m_audioPacketCondVar;

    AudioFramePtr m_audioFrame{nullptr};
    std::mutex m_audioFrameMutex;
    std::condition_variable m_audioFrameCondVar;

    std::atomic_bool m_mediaReadOver{false};
    std::atomic_bool m_audioDecodeOver{false};

    std::atomic<int64_t> m_seekTargetSecond{-1};
    std::atomic_bool m_seekFlagAudio{false};
    std::atomic_bool m_seekFlagVideo{false};
    std::atomic<int64_t> m_afterSeekTimestampOffsetUs{0};
    bool m_firstAudioFrame{true};
    bool m_firstVideoFrame{true};
};

} // namespace media
