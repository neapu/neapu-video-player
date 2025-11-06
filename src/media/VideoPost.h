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

typedef struct AVFrame AVFrame;
typedef struct SwsContext SwsContext;

namespace media {
// 视频后处理模块，负责处理视频帧的后续操作，硬件帧转换，帧率控制等
class VideoPost {
public:
    ~VideoPost();

    void setFps(double fps);
    void setCopyBackRender(bool copyBackRender);

    void pushVideoFrame(const AVFrame* frame);
    VideoFramePtr popVideoFrame();
    void clear();

    // 立即退出等待（例如外部停止渲染时调用）
    void setStopFlag(bool stop);

    int64_t waterLevel() const { return m_waterLevel; }

    void setTimeBase(const AVRational& timeBase) { m_timeBase = timeBase; }

    void setStartTimePoint(const std::chrono::steady_clock::time_point& timePoint);

    bool isQueueEmpty();

private:
    VideoFramePtr copyFrame(const AVFrame* frame);
    VideoFramePtr processHWFrame(const AVFrame* frame);
    bool convertFrame(const AVFrame* frame);

private:
    double m_fps{0};
    bool m_copyBackRender{true};

    int64_t m_waterLevel{-1};

    std::queue<VideoFramePtr> m_videoFrameQueue;
    std::mutex m_mutex;
    AVFrame* m_swFrame{nullptr};
    AVFrame* m_targetFrame{nullptr};
    SwsContext* m_swsCtx{nullptr};

    int m_lastWidth{0};
    int m_lastHeight{0};
    int m_lastFormat{0};

    // 吐帧等待控制
    std::condition_variable m_waitCondVar;
    std::atomic_bool m_stopFlag{false};
    std::atomic_bool m_interruptWait{false};
    std::chrono::steady_clock::time_point m_lastOutputTime{};
    bool m_hasLastOutputTime{false};

    AVRational m_timeBase{1, 1000000};
    std::chrono::steady_clock::time_point m_startTimePoint;
    int64_t m_basePts{0};
    std::atomic_bool m_startTimePointSet{false};
};

} // namespace media
