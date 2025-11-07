//
// Created by liu86 on 2025/11/7.
//

#include "Clock.h"
#include <thread>

namespace media {
void Clock::start()
{
    std::lock_guard lk(m_mutex);
    if (m_started) return;
    m_startTimePoint = std::chrono::steady_clock::now();
    m_started = true;
}
void Clock::stop()
{
    std::lock_guard lk(m_mutex);
    m_started = false;
    m_startTimePoint = {};
}
bool Clock::isStarted()
{
    std::lock_guard lk(m_mutex);
    return m_started;
}
void Clock::wait(int64_t pts)
{
    std::chrono::steady_clock::time_point start;
    {
        std::lock_guard lk(m_mutex);
        if (!m_started) return;
        start = m_startTimePoint;
    }

    auto targetTimePoint = start + std::chrono::microseconds(pts);
    std::this_thread::sleep_until(targetTimePoint);
}
void Clock::correctStartTimePoint(int64_t pts)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_started) {
        return;
    }

    // 假设传入的 pts 单位为微秒（与 wait 中一致）
    const auto now = std::chrono::steady_clock::now();

    // 期望关系：now ≈ start + pts  => start ≈ now - pts
    const auto desiredStart = now - std::chrono::microseconds(pts);

    // 计算当前起始时间与期望起始时间的偏差
    const auto diff = desiredStart - m_startTimePoint; // 正值表示当前起点过晚

    // 为避免抖动，设置一个小阈值，偏差小于该值则不调整
    constexpr auto kJitterThreshold = std::chrono::microseconds(2000); // 2 ms

    if (diff > kJitterThreshold || diff < -kJitterThreshold) {
        // 采用部分修正，避免一次性跳变导致可感知抖动
        // 这里使用比例系数进行渐进式靠拢
        const double alpha = 0.2; // 20% 的误差校正比例

        auto adjust = std::chrono::duration_cast<std::chrono::microseconds>(
            diff * alpha
        );

        // 防止 adjust 为 0 导致无效修正（在非常小的 diff 时）
        if (adjust.count() == 0) {
            adjust = (diff.count() > 0) ? std::chrono::microseconds(1)
                                        : std::chrono::microseconds(-1);
        }

        m_startTimePoint += adjust;
    }
}
} // namespace media