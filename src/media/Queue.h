//
// Created by liu86 on 2025/11/24.
//

#pragma once
#include "Packet.h"
#include "Frame.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace media {
class PacketQueue {
public:
    explicit PacketQueue(size_t maxDataSize);
    ~PacketQueue();
    PacketQueue(const PacketQueue&) = delete;
    PacketQueue& operator=(const PacketQueue&) = delete;
    PacketQueue(PacketQueue&& other) noexcept;
    PacketQueue& operator=(PacketQueue&& other) noexcept;

    void push(PacketPtr&& packet);
    PacketPtr pop();

    void notifyAll();
    void clear();
    void clearAndFlush(int serial);

private:
    std::queue<PacketPtr> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condVar;
    size_t m_dataSize{0};
    size_t m_maxDataSize{0};
    size_t m_clearToken{0};
};
class FrameQueue {
public:
    explicit FrameQueue(size_t maxQueueSize);
    ~FrameQueue();

    void push(FramePtr&& frame);
    FramePtr pop();

    void notifyAll();
    void clear();
    void clearAndFlush(int serial);

private:
    std::queue<FramePtr> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condVar;
    size_t m_queueSize{0};
    size_t m_maxQueueSize{0};
    size_t m_clearToken{0};
};
} // namespace media
