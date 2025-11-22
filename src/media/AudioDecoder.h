//
// Created by liu86 on 2025/11/20.
//

#pragma once
#include "DecoderBase.h"

typedef struct SwrContext SwrContext;
typedef struct AVChannelLayout AVChannelLayout;

namespace media {

class AudioDecoder : public DecoderBase{
public:
    AudioDecoder(AVStream* stream, const AVPacketCallback& packetCallback);
    ~AudioDecoder() override;

    int sampleRate() const;
    int channelCount() const;

protected:
    FramePtr postProcess(AVFrame* avFrame) override;

protected:
    SwrContext* m_swrCtx{nullptr};
    int m_lastSampleRate{0};
    int m_lastSampleFmt{-1};
    AVChannelLayout* m_lastChLayout{nullptr};
};

} // namespace media
