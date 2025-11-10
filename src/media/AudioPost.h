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
    AudioPost() {}
    ~AudioPost();

    void initialize(AVRational timeBase);
    void destroy();

    AudioFramePtr resampleAudioFrame(const AVFrame* frame, int64_t ptsOffset);

private:
    bool initSwrContext(const AVFrame* frame);


private:
    SwrContext* m_swrCtx{nullptr};
    uint8_t** m_dstData{nullptr};
    int m_dstLineSize{0};
    int m_dstNbSamples{0};

    AVRational m_timeBase{1, 1000000}; // 默认微秒时间基
};

} // namespace media
