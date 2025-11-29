//
// Created by liu86 on 2025/11/24.
//

#include "Queue.h"

namespace media {

PacketQueue::PacketQueue(size_t maxDataSize)
    : m_maxDataSize(maxDataSize)
{
}

PacketQueue::~PacketQueue()
{
}

PacketQueue::PacketQueue(PacketQueue&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_queue = std::move(other.m_queue);
    m_dataSize = other.m_dataSize;
    m_maxDataSize = other.m_maxDataSize;
    m_clearToken = other.m_clearToken;
}
PacketQueue& PacketQueue::operator=(PacketQueue&& other) noexcept
{
    if (this != &other) {
        std::lock_guard<std::mutex> lock1(m_mutex);
        std::lock_guard<std::mutex> lock2(other.m_mutex);
        m_queue = std::move(other.m_queue);
        m_dataSize = other.m_dataSize;
        m_maxDataSize = other.m_maxDataSize;
        m_clearToken = other.m_clearToken;
    }
    return *this;
}
void PacketQueue::push(PacketPtr&& packet)
{
    size_t sz = packet->size();
    std::unique_lock<std::mutex> lock(m_mutex);
    size_t token = m_clearToken;
    m_condVar.wait(lock, [&]() {
        return m_clearToken != token || (m_dataSize + sz <= m_maxDataSize);
    });
    if (m_clearToken != token) {
        return;
    }
    m_queue.push(std::move(packet));
    m_dataSize += sz;
    m_condVar.notify_all();
}

PacketPtr PacketQueue::pop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    size_t token = m_clearToken;
    m_condVar.wait(lock, [&]() {
        return m_clearToken != token || !m_queue.empty();
    });
    if (m_clearToken != token) {
        return nullptr;
    }
    if (m_queue.empty()) {
        return nullptr;
    }
    auto pkt = std::move(m_queue.front());
    m_queue.pop();
    m_dataSize -= pkt->size();
    m_condVar.notify_all();
    return pkt;
}

void PacketQueue::notifyAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_condVar.notify_all();
}

void PacketQueue::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_clearToken;
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_dataSize = 0;
    m_condVar.notify_all();
}

void PacketQueue::clearAndFlush(int serial)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_clearToken;
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_dataSize = 0;
    auto packet = std::make_unique<Packet>(Packet::PacketType::Flush, serial);
    m_dataSize += packet->size();
    m_queue.push(std::move(packet));
    m_condVar.notify_all();
}

FrameQueue::FrameQueue(size_t maxQueueSize)
    : m_maxQueueSize(maxQueueSize)
{
}

FrameQueue::~FrameQueue()
{
}

void FrameQueue::push(FramePtr&& frame)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    size_t token = m_clearToken;
    m_condVar.wait(lock, [&]() {
        return m_clearToken != token || (m_queueSize + 1 <= m_maxQueueSize);
    });
    if (m_clearToken != token) {
        return;
    }
    m_queue.push(std::move(frame));
    ++m_queueSize;
    m_condVar.notify_all();
}

FramePtr FrameQueue::pop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_queue.empty()) {
        return nullptr;
    }
    FramePtr fr = std::move(m_queue.front());
    m_queue.pop();
    --m_queueSize;
    m_condVar.notify_all();
    return fr;
}

void FrameQueue::notifyAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_condVar.notify_all();
}

void FrameQueue::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_clearToken;
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_queueSize = 0;
    m_condVar.notify_all();
}
void FrameQueue::clearAndFlush(int serial)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_clearToken;
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_queueSize = 0;
    m_queue.push(std::make_unique<Frame>(Frame::FrameType::Flush, serial));
    ++m_queueSize;
    m_condVar.notify_all();
}

} // namespace media