//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include <functional>
#include "Demuxer.h"

typedef struct AVCodecContext AVCodecContext;
typedef struct AVCodec AVCodec;
typedef struct AVPacket AVPacket;
typedef struct AVFrame AVFrame;
typedef struct AVStream AVStream;

namespace media {
using FrameCallback = std::function<void(AVFrame* frame)>;
class DecoderBase {
public:
    DecoderBase() = default;
    virtual ~DecoderBase() = default;

    virtual bool initialize(const AVStream* stream);
    virtual void destroy();

    virtual bool decodePacket(AVPacketPtr packet, const FrameCallback& callback);

    virtual void flush();

protected:
    virtual bool initializeCodecContext(const AVStream* stream);

protected:
    const AVStream* m_stream{nullptr};
    AVCodecContext* m_codecCtx{nullptr};
    const AVCodec* m_codec{nullptr};
    AVFrame* m_frame{nullptr};
};
} // namespace media
