//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>
#include "AudioFrame.h"
#include "pub.h"

typedef struct AVFrame AVFrame;
typedef struct SwrContext SwrContext;

namespace media {
// 音频后处理模块，负责音频重采样和缓冲管理
class AudioPost {
public:
    ~AudioPost();

    // 推入音频帧
    // videoWaterLevel: 视频缓冲水位，单位为秒；解码在同一线程，统一在这里控制水位
    bool pushAudioFrame(const AVFrame* frame, double videoWaterLevel);
    AudioFramePtr popAudioFrame();
    void clear();

    double waterLevel() const { return m_waterLevelMs; }

    void setStopFlag(bool stop) { m_stopFlag.store(stop); }
    void setTimebase(Rational timeBase) { m_timeBase = timeBase; }

    void setStartTimePoint(const std::chrono::steady_clock::time_point& timePoint) { m_startTimePoint = timePoint; }

private:
    bool initSwrContext(const AVFrame* frame);
    AudioFramePtr resampleAudioFrame(const AVFrame* frame);

private:
    std::queue<AudioFramePtr> m_audioFrameQueue;
    std::mutex m_mutex;
    std::condition_variable m_condVar;
    double m_waterLevelMs{0};
    int64_t m_basePts{0};

    SwrContext* m_swrCtx{nullptr};
    uint8_t** m_dstData{nullptr};
    int m_dstLineSize{0};
    int m_dstNbSamples{0};

    std::atomic_bool m_stopFlag{false};

    Rational m_timeBase{1, 1000}; // 默认毫秒时间基
    std::chrono::steady_clock::time_point m_startTimePoint;
    std::condition_variable m_waitCondVar;
};

} // namespace media
