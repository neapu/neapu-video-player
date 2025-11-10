//
// Created by liu86 on 2025/10/30.
//

#include "AudioPost.h"
#include <logger.h>
#include "MediaUtils.h"
extern "C"{
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}

namespace media {
constexpr int64_t HIGH_WATER_MARK_US = 900000; // 单位：微秒
constexpr int64_t LOW_WATER_MARK_US = 300000;
constexpr int64_t VERY_HIGH_WATER_MARK_US = 1500000;
constexpr int64_t MAX_WATER_MARK_US = 3000000;
constexpr AVSampleFormat TARGET_SAMPLE_FORMAT = AV_SAMPLE_FMT_S16;

AudioPost::~AudioPost()
{
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }

    if (m_dstData) {
        av_freep(&m_dstData[0]);
        av_freep(&m_dstData);
        m_dstData = nullptr;
    }
}

void AudioPost::initialize(AVRational timeBase)
{
    NEAPU_FUNC_TRACE;
    m_timeBase = timeBase;
}
void AudioPost::destroy()
{
    NEAPU_FUNC_TRACE;

    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }

    if (m_dstData) {
        av_freep(&m_dstData[0]);
        av_freep(&m_dstData);
        m_dstData = nullptr;
    }
}

bool AudioPost::initSwrContext(const AVFrame* frame)
{
    NEAPU_FUNC_TRACE;
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }

    AVChannelLayout inLayout;
    av_channel_layout_copy(&inLayout, &frame->ch_layout);
    if (inLayout.nb_channels == 0) {
        av_channel_layout_default(&inLayout, frame->ch_layout.nb_channels);
    }
    if (av_channel_layout_compare(&inLayout, &frame->ch_layout) != 0) {
        NEAPU_LOGE("Channel layout mismatch");
        av_channel_layout_uninit(&inLayout);
        return false;
    }
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, frame->ch_layout.nb_channels);

    m_swrCtx = swr_alloc();
    int ret = swr_alloc_set_opts2(
        &m_swrCtx,
        &outLayout,
        TARGET_SAMPLE_FORMAT,
        frame->sample_rate,
        &inLayout,
        static_cast<AVSampleFormat>(frame->format),
        frame->sample_rate,
        0,
        nullptr);
    av_channel_layout_uninit(&inLayout);
    av_channel_layout_uninit(&outLayout);
    if (ret < 0) {
        NEAPU_LOGE("Failed to set swr options: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        swr_free(&m_swrCtx);
        return false;
    }

    ret = swr_init(m_swrCtx);
    if (ret < 0) {
        NEAPU_LOGE("Failed to initialize swr context: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        swr_free(&m_swrCtx);
        return false;
    }

    // 预分配目标缓冲区
    m_dstNbSamples = 1024;
    ret = av_samples_alloc_array_and_samples(
        &m_dstData,
        &m_dstLineSize,
        frame->ch_layout.nb_channels,
        m_dstNbSamples,
        TARGET_SAMPLE_FORMAT,
        0);
    if (ret < 0) {
        NEAPU_LOGE("Failed to allocate destination samples: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
        swr_free(&m_swrCtx);
        return false;
    }

    NEAPU_LOGI("Initialized SwrContext for audio resampling");
    return true;
}

AudioFramePtr AudioPost::resampleAudioFrame(const AVFrame* frame, int64_t ptsOffset)
{
    if (!frame) {
        NEAPU_LOGE("Input frame is null");
        return nullptr;
    }

    if (frame->format != TARGET_SAMPLE_FORMAT && !m_swrCtx) {
        if (!initSwrContext(frame)) {
            NEAPU_LOGE("Failed to initialize SwrContext");
            return nullptr;
        }
    }

    int64_t pts = -1;
    // if (frame->pts == AV_NOPTS_VALUE) {
    //     pts = m_basePts;
    //     // 使用 nb_samples / sample_rate -> 微秒
    //     m_basePts += av_rescale_q(frame->nb_samples, AVRational{1, frame->sample_rate}, AV_TIME_BASE_Q);
    // } else if (frame->time_base.num != 0) {
    //     pts = av_rescale_q(frame->pts, frame->time_base, AV_TIME_BASE_Q);
    // } else {
    //     pts = av_rescale_q(frame->pts, m_timeBase, AV_TIME_BASE_Q);
    // }
    if (frame->time_base.num != 0 && frame->time_base.den != 0) {
        pts = av_rescale_q(frame->best_effort_timestamp, frame->time_base, AV_TIME_BASE_Q);
    } else {
        pts = av_rescale_q(frame->best_effort_timestamp, m_timeBase, AV_TIME_BASE_Q);
    }
    pts += ptsOffset;

    if (m_swrCtx) {
        // 需要重采样
        // 计算目标采样数
        int64_t delay = swr_get_delay(m_swrCtx, frame->sample_rate);
        int64_t dstNbSamples = av_rescale_rnd(
            delay + frame->nb_samples,
            frame->sample_rate,
            frame->sample_rate,
            AV_ROUND_UP);
        if (dstNbSamples > m_dstNbSamples) {
            // 重新分配目标缓冲区
            av_freep(&m_dstData[0]);
            int ret = av_samples_alloc(
                &m_dstData[0],
                &m_dstLineSize,
                frame->ch_layout.nb_channels,
                static_cast<int>(dstNbSamples),
                TARGET_SAMPLE_FORMAT,
                1);
            if (ret < 0) {
                NEAPU_LOGE("Failed to allocate destination samples: {}; code: {}", MediaUtils::getFFmpegError(ret), ret);
                return nullptr;
            }
            m_dstNbSamples = static_cast<int>(dstNbSamples);
        }

        int ourSize = swr_convert(
            m_swrCtx,
            m_dstData,
            static_cast<int>(dstNbSamples),
            const_cast<const uint8_t**>(frame->data),
            frame->nb_samples);
        if (ourSize < 0) {
            NEAPU_LOGE("Failed to convert audio frame: {}; code: {}", MediaUtils::getFFmpegError(ourSize), ourSize);
            return nullptr;
        }

        auto audioFrame = std::make_unique<AudioFrame>(
            m_dstData[0],
            ourSize * av_get_bytes_per_sample(TARGET_SAMPLE_FORMAT) * frame->ch_layout.nb_channels,
            frame->sample_rate,
            frame->ch_layout.nb_channels,
            ourSize, pts);
        return audioFrame;
    }

    // 无需重采样，直接拷贝
    size_t dataSize = frame->nb_samples * av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format)) * frame->ch_layout.nb_channels;
    auto audioFrame = std::make_unique<AudioFrame>(
        frame->data[0],
        dataSize,
        frame->sample_rate,
        frame->ch_layout.nb_channels,
        frame->nb_samples, pts);
    return audioFrame;
}
} // namespace media