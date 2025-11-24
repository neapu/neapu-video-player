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
DecoderBase::DecoderBase(AVStream* stream, const AVPacketCallback& packetCallback)
    : m_stream(stream)
    , m_packetCallback(packetCallback)
{
}
DecoderBase::~DecoderBase()
{
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
}
auto DecoderBase::getFrame() -> std::expected<FramePtr, DecodeError>
{
    std::lock_guard lock(m_mutex);
    while (m_frameQueue.empty()) {
        auto ret = decode();
        if (ret != DecodeError::None) {
            return std::unexpected(ret);
        }
    }

    if (m_frameQueue.empty()) {
        return std::unexpected(DecodeError::Eof);
    }

    auto frame = std::move(m_frameQueue.front());
    m_frameQueue.pop();
    return std::expected<FramePtr, DecodeError>(std::move(frame));
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
DecoderBase::DecodeError DecoderBase::decode()
{
    auto packet = m_packetCallback();
    if (!packet) {
        return DecodeError::Eof;
    }

    int ret = avcodec_send_packet(m_codecCtx, packet.get());
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to send packet to decoder: {}", errStr);
        return DecodeError::Other;
    }

    for (;;) {
        AVFrame* avFrame = av_frame_alloc();
        ret = avcodec_receive_frame(m_codecCtx, avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&avFrame);
            break;
        }
        if (ret < 0) {
            std::string errStr = getFFmpegErrorString(ret);
            NEAPU_LOGE("Failed to receive frame from decoder: {}", errStr);
            av_frame_free(&avFrame);
            return DecodeError::Other;
        }

        auto frame = postProcess(avFrame);
        m_frameQueue.push(std::move(frame));
    }
    return DecodeError::None;
}
void DecoderBase::flush()
{
    std::lock_guard lock(m_mutex);
    while (!m_frameQueue.empty()) {
        m_frameQueue.pop();
    }
    if (m_codecCtx) {
        avcodec_flush_buffers(m_codecCtx);
    }
}
} // namespace media