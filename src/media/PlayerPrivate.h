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

namespace media {

class PlayerPrivate {
public:
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

private:
    void decodeThreadFunc();
    bool initVideoDecoder(const AVStream* stream);

private:
    OpenMediaParams m_openParams;

    Demuxer m_demuxer;
    AudioDecoder m_audioDecoder;
    AudioPost m_audioPost;

    VideoDecoder m_videoDecoder;
    VideoPost m_videoPost;

    std::thread m_decodeThread;
    std::atomic<bool> m_stopFlag{false};
    std::atomic<bool> m_pause{false};
    bool m_decodeFirstFrame{true};
};

} // namespace media
