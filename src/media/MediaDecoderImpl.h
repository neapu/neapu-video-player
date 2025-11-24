//
// Created by liu86 on 2025/11/19.
//

#pragma once
#include "MediaDecoder.h"
#include "AudioDecoder.h"
#include "VideoDecoder.h"
#include "Demuxer.h"
#include <memory>

namespace media {

class MediaDecoderImpl : public MediaDecoder {
public:
    explicit MediaDecoderImpl(const CreateParam& param);
    ~MediaDecoderImpl() override;

    FramePtr getVideoFrame() override;
    FramePtr getAudioFrame() override;

    bool isEof() override;
    void seek(double timePointSeconds) override;

    int audioSampleRate() const override;
    int audioChannelCount() const override;

    bool hasAudio() const override;
    bool hasVideo() const override;

    double durationSeconds() const override;

private:
    void createAudioDecoder(const CreateParam& param);
    void createVideoDecoder(const CreateParam& param);

private:
    std::unique_ptr<VideoDecoder> m_videoDecoder;
    std::unique_ptr<AudioDecoder> m_audioDecoder;
    std::unique_ptr<Demuxer> m_demuxer;
};

} // namespace media
