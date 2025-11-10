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
    void clear();

    void wait(int64_t pts);
    void setAudioPts(int64_t audioPts);

private:
    std::atomic<int64_t> m_startTimePointUs{0};
};

} // namespace media
