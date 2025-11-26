//
// Created by liu86 on 2025/11/25.
//

#pragma once
#include "Player.h"
#include "Demuxer.h"
#include "VideoDecoder.h"
#include "AudioDecoder.h"

namespace media {

class PlayerImpl : public Player {
public:
    PlayerImpl() = default;
    ~PlayerImpl() override = default;

    FramePtr getVideoFrame() override;
    FramePtr getAudioFrame() override;

    bool open(const OpenParam& param) override;
    void close() override;

    void seek(double seconds, int serial) override;

    bool isOpened() const override;

    bool hasVideo() const override;
    bool hasAudio() const override;

    double fps() const override;
    int sampleRate() const override;
    int channelCount() const override;

    double durationSeconds() const override;

private:
    void createVideoDecoder();
    void createAudioDecoder();

private:
    OpenParam m_param;
    std::unique_ptr<Demuxer> m_demuxer;
    std::unique_ptr<AudioDecoder> m_audioDecoder;
    std::unique_ptr<VideoDecoder> m_videoDecoder;
};

} // namespace media
