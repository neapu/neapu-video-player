//
// Created by liu86 on 2025/11/24.
//

#include "Packet.h"
#include <stdexcept>
extern "C" {
#include <libavcodec/packet.h>
}

namespace media {
Packet::Packet(PacketType type, int serial) : m_type(type), m_serial(serial)
{
    if (type != PacketType::Flush) {
        m_avPacket = av_packet_alloc();
        if (!m_avPacket) {
            throw std::runtime_error("Failed to allocate AVPacket");
        }
    }
}
Packet::~Packet()
{
    if (m_avPacket) {
        av_packet_free(&m_avPacket);
        m_avPacket = nullptr;
    }
}

Packet::Packet(Packet&& other) noexcept
    : m_avPacket(other.m_avPacket)
    , m_type(other.m_type)
{
    other.m_avPacket = nullptr;
}

Packet& Packet::operator=(Packet&& other) noexcept
{
    if (this != &other) {
        if (m_avPacket) {
            av_packet_free(&m_avPacket);
        }
        m_avPacket = other.m_avPacket;
        m_type = other.m_type;
        other.m_avPacket = nullptr;
    }
    return *this;
}

size_t Packet::size() const
{
    if (m_avPacket) {
        return static_cast<size_t>(m_avPacket->size);
    }
    return 0;
}

} // namespace media