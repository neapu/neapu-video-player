//
// Created by liu86 on 2025/10/30.
//

#include "AudioPost.h"
#include <logger.h>
#include "MediaUtils.h"
extern "C"{
#include <libswresample/swresample.h>
}

namespace media {
constexpr int HIGH_WATER_MARK = 900; // 单位：毫秒
constexpr int LOW_WATER_MARK = 300;
constexpr int HIGHEST_WATER_MARK = 1500;
constexpr AVSampleFormat TARGET_SAMPLE_FORMAT = AV_SAMPLE_FMT_S16;

AudioPost::~AudioPost()
{
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }
}

bool AudioPost::pushAudioFrame(const AVFrame* frame, double videoWaterLevel)
{
    auto audioFrame = resampleAudioFrame(frame);
    if (!audioFrame) {
        NEAPU_LOGW("Failed to resample audio frame");
        return false;
    }

    std::unique_lock lock(m_mutex);
    m_waterLevelMs += audioFrame->duration();
    auto isWait = [=, this]() {
        if (m_stopFlag) {
            return false;
        }

        if (m_waterLevelMs > HIGHEST_WATER_MARK) {
            return true;
        }

        if (m_waterLevelMs > HIGH_WATER_MARK && (videoWaterLevel > LOW_WATER_MARK || videoWaterLevel < 0)) {
            return true;
        }
        return false;
    };
    while (isWait()) {
        m_condVar.wait(lock);
    }

    if (m_stopFlag) {
        return false;
    }
    // NEAPU_LOGD("Pushed audio frame: duration={}, Water level: {}", audioFrame->duration(), m_waterLevelMs);
    m_audioFrameQueue.push(std::move(audioFrame));

    return true;
}

AudioFramePtr AudioPost::popAudioFrame()
{
    // std::lock_guard lock(m_mutex);
    // if (m_audioFrameQueue.empty()) {
    //     return nullptr;
    // }
    //
    // auto frame = std::move(m_audioFrameQueue.front());
    // m_audioFrameQueue.pop();
    // m_waterLevelMs -= frame->duration();
    // m_condVar.notify_all();
    // return frame;

    auto popFrame = [this]() -> AudioFramePtr {
        auto audioFrame = std::move(m_audioFrameQueue.front());
        m_audioFrameQueue.pop();
        m_waterLevelMs -= audioFrame->duration();
        m_condVar.notify_all();
        return audioFrame;
    };

    int64_t targetPts = 0;
    std::unique_lock lock(m_mutex);
    if (m_audioFrameQueue.empty()) {
        return nullptr;
    }
    const auto& frame = m_audioFrameQueue.front();
    targetPts = frame->pts();

    // 计算m_startTimePoint+pts为目标时间点
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    const auto targetTimePoint = m_startTimePoint + std::chrono::milliseconds(targetPts);
    if (targetTimePoint < now) {
        // 目标时间点已过，直接弹出
        return popFrame();
    }

    const auto waitDuration = targetTimePoint - now;
    // 等待直到目标时间点或收到中断/停止信号
    while (!m_stopFlag) {
        if (m_waitCondVar.wait_for(lock, waitDuration) == std::cv_status::timeout) {
            break;
        }
    }
    if (m_stopFlag) {
        return nullptr;
    } else {
        return popFrame();
    }
}

void AudioPost::clear()
{
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }

    std::lock_guard lock(m_mutex);
    while (!m_audioFrameQueue.empty()) {
        m_audioFrameQueue.pop();
    }

    m_waterLevelMs = 0;
    m_basePts = 0;
    m_timeBase = {0, 1000};
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

    return true;
}

AudioFramePtr AudioPost::resampleAudioFrame(const AVFrame* frame)
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

    int64_t pts = 0;
    if (frame->pts == AV_NOPTS_VALUE) {
        pts = m_basePts;
        m_basePts += frame->nb_samples * m_timeBase.den / (frame->sample_rate * frame->ch_layout.nb_channels * m_timeBase.num);
    } else if (frame->time_base.num != 0) {
        pts = av_rescale_q(frame->pts, frame->time_base, AVRational{1, 1000});
    } else {
        pts = av_rescale_q(frame->pts, {m_timeBase.num, m_timeBase.den}, AVRational{1, 1000});
    }

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