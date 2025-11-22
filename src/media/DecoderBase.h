//
// Created by liu86 on 2025/11/20.
//

#pragma once
#include "Frame.h"
#include "Helper.h"
#include <functional>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <expected>

typedef struct AVStream AVStream;
typedef struct AVFrame AVFrame;
typedef struct AVCodecContext AVCodecContext;
typedef struct AVCodec AVCodec;

namespace media {
class DecoderBase {
public:
    enum class CodecType {
        Audio,
        Video,
    };
    using AVPacketCallback = std::function<AVPacketPtr()>;

    DecoderBase(AVStream* stream, const AVPacketCallback& packetCallback);
    virtual ~DecoderBase();

    enum class DecodeError {
        None,
        Other,
        Eof,
    };
    auto getFrame() -> std::expected<FramePtr, DecodeError>;

protected:
    virtual void initializeContext();
    virtual FramePtr postProcess(AVFrame* avFrame) = 0;
    virtual DecodeError decode();

protected:
    AVStream* m_stream{nullptr};
    AVCodecContext* m_codecCtx{nullptr};
    const AVCodec* m_codec{nullptr};

    std::queue<FramePtr> m_frameQueue;
    std::mutex m_mutex;
    AVPacketCallback m_packetCallback;
};

} // namespace media
