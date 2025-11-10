//
// Created by liu86 on 2025/11/7.
//

#include "Clock.h"
#include <thread>
#include <logger.h>

namespace media {
void Clock::clear()
{
    m_startTimePointUs = 0;
}
void Clock::wait(int64_t pts)
{
    if (pts <= 0) {
        return;
    }

    while (true) {
        const int64_t baseUs = m_startTimePointUs.load(std::memory_order_relaxed);
        if (baseUs == 0) {
            return;
        }

        const int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
        const int64_t currentPts = nowUs - baseUs;
        const int64_t diffUs = pts - currentPts;

        if (diffUs <= 0) {
            return;
        }

        if (diffUs > 50000) {
            std::this_thread::sleep_for(std::chrono::microseconds(5000));
        } else if (diffUs > 10000) {
            std::this_thread::sleep_for(std::chrono::microseconds(2000));
        } else if (diffUs > 3000) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
        } else if (diffUs > 1000) {
            std::this_thread::sleep_for(std::chrono::microseconds(300));
        } else if (diffUs > 200) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } else if (diffUs > 50) {
            std::this_thread::sleep_for(std::chrono::microseconds(30));
        } else {
            std::this_thread::yield();
        }
    }
}
void Clock::setAudioPts(int64_t audioPts)
{
    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
    m_startTimePointUs = nowUs - audioPts;
}

} // namespace media