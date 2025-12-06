//
// Created by liu86 on 2025/11/20.
//

#pragma once
#include "Frame.h"
#include "Helper.h"
#include "Packet.h"
#include "Queue.h"

#include <functional>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <expected>
#include <thread>

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
    using AVPacketCallback = std::function<PacketPtr()>;

    DecoderBase(AVStream* stream, const AVPacketCallback& packetCallback, CodecType type);
    virtual ~DecoderBase();

    void start();
    void stop();

    bool testDecode();

    FramePtr getFrame();

protected:
    virtual void initializeContext();
    virtual FramePtr postProcess(FramePtr&& frame) = 0;
    virtual void decodeThreadFunc();

protected:
    CodecType m_type;
    AVStream* m_stream{nullptr};
    AVCodecContext* m_codecCtx{nullptr};
    const AVCodec* m_codec{nullptr};

    FrameQueue m_frameQueue;
    AVPacketCallback m_packetCallback;
    int m_serial{0};

    std::thread m_decodeThread;
    std::atomic_bool m_running{false};
};

} // namespace media
