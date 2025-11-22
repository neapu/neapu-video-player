//
// Created by liu86 on 2025/11/20.
//

#include "AudioDecoder.h"
#include <logger.h>
extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}

namespace media {
AudioDecoder::AudioDecoder(AVStream* stream, const AVPacketCallback& packetCallback)
    : DecoderBase(stream, packetCallback)
{
    NEAPU_FUNC_TRACE;
    if (!m_stream) {
        NEAPU_LOGE("AudioDecoder initialized with null stream");
        throw std::runtime_error("AudioDecoder initialized with null stream");
    }

    DecoderBase::initializeContext();

    int ret = avcodec_open2(m_codecCtx, m_codec, nullptr);
    if (ret < 0) {
        std::string errStr = getFFmpegErrorString(ret);
        NEAPU_LOGE("Failed to open audio codec: {}", errStr);
        throw std::runtime_error("Failed to open audio codec: " + errStr);
    }
}
AudioDecoder::~AudioDecoder() {
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }
    if (m_lastChLayout) {
        av_channel_layout_uninit(m_lastChLayout);
        delete m_lastChLayout;
        m_lastChLayout = nullptr;
    }
}
int AudioDecoder::sampleRate() const
{
    if (!m_stream) {
        return 0;
    }
    return m_stream->codecpar->sample_rate;
}
int AudioDecoder::channelCount() const
{
    if (!m_stream) {
        return 0;
    }
    return m_stream->codecpar->ch_layout.nb_channels;
}
FramePtr AudioDecoder::postProcess(AVFrame* avFrame)
{
    if (!avFrame) {
        return nullptr;
    }

    if (avFrame->format == AV_SAMPLE_FMT_S16) {
        avFrame->time_base = m_stream->time_base;
        return std::make_unique<Frame>(avFrame);
    }

    // 格式转换为 AV_SAMPLE_FMT_S16，采样率和通道数保持不变
    AVFrame* convertedFrame = av_frame_alloc();
    if (!convertedFrame) {
        NEAPU_LOGE("Failed to allocate converted audio frame");
        av_frame_free(&avFrame);
        return nullptr;
    }

    bool needReinit = false;
    if (!m_swrCtx) needReinit = true;
    if (m_lastSampleRate != avFrame->sample_rate) needReinit = true;
    if (m_lastSampleFmt != avFrame->format) needReinit = true;
    if (!m_lastChLayout) {
        needReinit = true;
    } else if (av_channel_layout_compare(m_lastChLayout, &avFrame->ch_layout) != 0) {
        needReinit = true;
    }

    if (needReinit) {
        if (m_swrCtx) {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }

        AVChannelLayout inLayout{};
        AVChannelLayout outLayout{};
        int ret = av_channel_layout_copy(&inLayout, &avFrame->ch_layout);
        if (ret < 0) {
            NEAPU_LOGE("Failed to copy input channel layout: {}", getFFmpegErrorString(ret));
            av_frame_free(&convertedFrame);
            av_frame_free(&avFrame);
            return nullptr;
        }
        ret = av_channel_layout_copy(&outLayout, &avFrame->ch_layout);
        if (ret < 0) {
            NEAPU_LOGE("Failed to copy output channel layout: {}", getFFmpegErrorString(ret));
            av_channel_layout_uninit(&inLayout);
            av_frame_free(&convertedFrame);
            av_frame_free(&avFrame);
            return nullptr;
        }

        ret = swr_alloc_set_opts2(&m_swrCtx,
            &outLayout, AV_SAMPLE_FMT_S16, avFrame->sample_rate,
            &inLayout, static_cast<AVSampleFormat>(avFrame->format), avFrame->sample_rate,
            0, nullptr);
        if (ret < 0 || !m_swrCtx) {
            NEAPU_LOGE("Failed to alloc swr context: {}", getFFmpegErrorString(ret));
            av_channel_layout_uninit(&inLayout);
            av_channel_layout_uninit(&outLayout);
            av_frame_free(&convertedFrame);
            av_frame_free(&avFrame);
            return nullptr;
        }
        ret = swr_init(m_swrCtx);
        av_channel_layout_uninit(&inLayout);
        av_channel_layout_uninit(&outLayout);
        if (ret < 0) {
            NEAPU_LOGE("Failed to init swr context: {}", getFFmpegErrorString(ret));
            swr_free(&m_swrCtx);
            av_frame_free(&convertedFrame);
            av_frame_free(&avFrame);
            return nullptr;
        }

        m_lastSampleRate = avFrame->sample_rate;
        m_lastSampleFmt = avFrame->format;
        if (m_lastChLayout) {
            av_channel_layout_uninit(m_lastChLayout);
            delete m_lastChLayout;
            m_lastChLayout = nullptr;
        }
        m_lastChLayout = new AVChannelLayout{};
        if (av_channel_layout_copy(m_lastChLayout, &avFrame->ch_layout) < 0) {
            NEAPU_LOGE("Failed to cache channel layout");
            delete m_lastChLayout;
            m_lastChLayout = nullptr;
            av_frame_free(&convertedFrame);
            av_frame_free(&avFrame);
            return nullptr;
        }
    }

    convertedFrame->format = AV_SAMPLE_FMT_S16;
    convertedFrame->sample_rate = avFrame->sample_rate;
    if (av_channel_layout_copy(&convertedFrame->ch_layout, &avFrame->ch_layout) < 0) {
        NEAPU_LOGE("Failed to set output channel layout");
        av_frame_free(&convertedFrame);
        av_frame_free(&avFrame);
        return nullptr;
    }

    int ret = swr_convert_frame(m_swrCtx, convertedFrame, avFrame);
    if (ret < 0) {
        NEAPU_LOGE("Failed to convert audio frame: {}", getFFmpegErrorString(ret));
        av_frame_free(&convertedFrame);
        av_frame_free(&avFrame);
        return nullptr;
    }

    convertedFrame->pts = avFrame->pts;
    convertedFrame->pkt_dts = avFrame->pkt_dts;
    convertedFrame->duration = avFrame->duration;
    convertedFrame->time_base = m_stream->time_base;
    av_frame_free(&avFrame);
    return std::make_unique<Frame>(convertedFrame);
}
} // namespace media