//
// Created by liu86 on 2025/11/2.
//

#pragma once
#include "VideoFrame.h"
extern "C"{
#include <libavutil/avutil.h>
}
#include <queue>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include "Clock.h"

typedef struct AVFrame AVFrame;
typedef struct SwsContext SwsContext;

namespace media {
// 视频后处理模块，负责处理视频帧的后续操作，硬件帧转换，帧率控制等
class VideoPost {
public:
    explicit VideoPost() {}
    ~VideoPost();

    void initialize(double fps, bool copyBackRender, AVRational timeBase);
    void destroy();

    VideoFramePtr copyFrame(const AVFrame* frame);

private:
    VideoFramePtr processHWFrame(const AVFrame* frame);
    bool convertFrame(const AVFrame* frame);

private:
    double m_fps{0};    // 用于在没有pts的情况下计算帧间隔
    bool m_copyBackRender{true};    // 硬件解码时，是否拷贝回内存渲染帧
    AVFrame* m_swFrame{nullptr};
    AVFrame* m_targetFrame{nullptr};
    SwsContext* m_swsCtx{nullptr};

    int m_lastWidth{0};
    int m_lastHeight{0};
    int m_lastFormat{0};

    AVRational m_timeBase{1, 1000000};
    int64_t m_basePts{0};
    bool m_firstFrame{false};
};

} // namespace media
