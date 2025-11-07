//
// Created by liu86 on 2025/11/7.
//

#pragma once
#include <cstdint>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace media {

class Clock {
public:
    void start();
    void stop();

    bool isStarted();
    void wait(int64_t pts);
    // 通过音频pts反向修正起始时间点，达到音视频同步的目的
    void correctStartTimePoint(int64_t pts);

private:
    std::chrono::steady_clock::time_point m_startTimePoint;
    bool m_started{false};
    std::mutex m_mutex; // 保护起始时间点的并发访问
};

} // namespace media
