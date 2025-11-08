//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>
#include "AudioFrame.h"
#include "pub.h"
extern "C"{
#include <libavutil/avutil.h>
}
#include "Clock.h"

typedef struct AVFrame AVFrame;
typedef struct SwrContext SwrContext;

namespace media {
// 音频后处理模块，负责音频重采样和缓冲管理
class AudioPost {
public:
    explicit AudioPost(Clock& clock) : m_clock(clock) {}
    ~AudioPost();

    void initialize(AVRational timeBase);
    void destroy();
    void clear();

    // 推入音频帧
    // videoWaterLevelUs: 视频缓冲水位，单位为微秒；解码在同一线程，统一在这里控制水位
    bool pushAudioFrame(const AVFrame* frame, int64_t videoWaterLevelUs);
    AudioFramePtr popAudioFrame();

    int64_t waterLevel() const { return m_waterLevelUs; }
    bool isQueueEmpty();

    int64_t currentPts() const { return m_currentPts; }

private:
    bool initSwrContext(const AVFrame* frame);
    AudioFramePtr resampleAudioFrame(const AVFrame* frame);

private:
    Clock& m_clock;
    std::queue<AudioFramePtr> m_audioFrameQueue;
    std::mutex m_mutex;
    std::condition_variable m_condVar;
    std::atomic_bool m_initialized{false};
    int64_t m_waterLevelUs{0};
    int64_t m_basePts{0};

    SwrContext* m_swrCtx{nullptr};
    uint8_t** m_dstData{nullptr};
    int m_dstLineSize{0};
    int m_dstNbSamples{0};

    AVRational m_timeBase{1, 1000000}; // 默认微秒时间基
    // 首帧等待标记：仅在第一帧按起始时间点等待，后续不等待直接出帧
    bool m_firstFrame{false};
    int64_t m_currentPts{0};
};

} // namespace media
