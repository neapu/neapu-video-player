//
// Created by liu86 on 2025/11/20.
//

#include "DecoderBase.h"
#include <stdexcept>
#include <logger.h>
#include "Helper.h"
#include "VideoDecoder.h"
#include "AudioDecoder.h"
extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace media {
DecoderBase::DecoderBase(AVStream* stream, const AVPacketCallback& packetCallback, CodecType type)
    : m_type(type)
    , m_stream(stream)
    , m_packetCallback(packetCallback)
    , m_frameQueue(type == CodecType::Video ? 5 : 15)
{
    NEAPU_FUNC_TRACE;
}
DecoderBase::~DecoderBase()
{
    NEAPU_FUNC_TRACE;
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
}
void DecoderBase::start()
{
    NEAPU_FUNC_TRACE;
    stop();
    m_running = true;
    m_decodeThread = std::thread(&DecoderBase::decodeThreadFunc, this);
}
void DecoderBase::stop()
{
    NEAPU_FUNC_TRACE;
    m_running = false;
    m_frameQueue.clear();
    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }
}
bool DecoderBase::testDecode()
{
    for (;;) {
        auto packet = m_packetCallback();
        if (!packet) {
            NEAPU_LOGE("Decoder received null packet during test decode");
            return false;
        }
        if (packet->type() == Packet::PacketType::Eof) {
            NEAPU_LOGW("Reached EOF during test decode");
            return false;
        }
        if (packet->type() == Packet::PacketType::Flush) {
            avcodec_flush_buffers(m_codecCtx);
            continue;
        }

        int ret = avcodec_send_packet(m_codecCtx, packet->avPacket());
        if (ret < 0) {
            NEAPU_LOGE("Failed to send packet to decoder during test decode: {}", getFFmpegErrorString(ret));
            return false;
        }

        for (;;) {
            AVFrame* frame = av_frame_alloc();
            ret = avcodec_receive_frame(m_codecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&frame);
                break;
            }
            if (ret < 0) {
                NEAPU_LOGE("Failed to receive frame from decoder during test decode: {}", getFFmpegErrorString(ret));
                av_frame_free(&frame);
                return false;
            }
            av_frame_free(&frame);
            return true; // 成功解码出一帧
        }
    }
}
FramePtr DecoderBase::getFrame()
{
    auto frame = m_frameQueue.pop();
    NEAPU_LOGD("{} Decoder get frame PTS {}", m_type == CodecType::Video ? "Video" : "Audio", frame ? frame->ptsUs() : -1);
    return frame;
}
void DecoderBase::initializeContext()
{
    m_codec = avcodec_find_decoder(m_stream->codecpar->codec_id);
    if (!m_codec) {
        NEAPU_LOGE("Failed to find decoder for codec id: {}", getAVCodecIDString(m_stream->codecpar->codec_id));
        throw std::runtime_error("Failed to find decoder");
    }

    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        NEAPU_LOGE("Failed to allocate codec context");
        throw std::runtime_error("Failed to allocate codec context");
    }

    int ret = avcodec_parameters_to_context(m_codecCtx, m_stream->codecpar);
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to copy codec parameters to context: {}", errStr);
        throw std::runtime_error("Failed to copy codec parameters: " + errStr);
    }
}
void DecoderBase::decodeThreadFunc()
{
    NEAPU_FUNC_TRACE;
    while (m_running) {
        auto packet = m_packetCallback();
        if (!packet) {
            NEAPU_LOGE("{} Decoder received null packet", m_type == CodecType::Video ? "Video" : "Audio");
            continue;
        }
        if (packet->type() == Packet::PacketType::Eof) {
            NEAPU_LOGI("{} Decoder received EOF packet", m_type == CodecType::Video ? "Video" : "Audio");
            m_frameQueue.push(std::make_unique<Frame>(Frame::FrameType::EndOfStream, -1));
            break;
        }
        if (packet->type() == Packet::PacketType::Flush) {
            avcodec_flush_buffers(m_codecCtx);
            m_serial = packet->serial();
            m_frameQueue.clearAndFlush(m_serial);
            NEAPU_LOGI("{} Decoder received Flush packet, serial {}", m_type == CodecType::Video ? "Video" : "Audio", m_serial);
            continue;
        }

        if (packet->serial() != m_serial) {
            NEAPU_LOGW("{} Decoder packet serial {} does not match base serial {}, skipping packet",
                m_type == CodecType::Video ? "Video" : "Audio",
                packet->serial(),
                m_serial);
            continue;
        }

        // NEAPU_LOGD("{} Decoder received packet of size {}", m_type == CodecType::Video ? "Video" : "Audio", packet->size());
        int ret = avcodec_send_packet(m_codecCtx, packet->avPacket());
        if (ret < 0) {
            NEAPU_LOGE("Failed to send packet to decoder: {}", getFFmpegErrorString(ret));
            continue;
        }

        for (;;) {
            auto frame = std::make_unique<Frame>(Frame::FrameType::Normal, m_serial);
            ret = avcodec_receive_frame(m_codecCtx, frame->avFrame());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                NEAPU_LOGE("Failed to receive frame from decoder: {}", getFFmpegErrorString(ret));
                break;
            }
            frame->avFrame()->time_base = m_stream->time_base;
            auto processedFrame = postProcess(std::move(frame));
            if (processedFrame) {
                // NEAPU_LOGD("{} Decoder produced frame PTS {}", m_type == CodecType::Video ? "Video" : "Audio", processedFrame->avFrame()->pts);
                m_frameQueue.push(std::move(processedFrame));
            } else {
                NEAPU_LOGE("{} Post processing of frame failed", m_type == CodecType::Video ? "Video" : "Audio");
            }
        }
    }
}

} // namespace media