//
// Created by liu86 on 2025/11/24.
//

#pragma once
#include <memory>

typedef struct AVPacket AVPacket;

namespace media {
class Packet {
public:
    enum class PacketType {
        Normal,
        Flush,
        Eof
    };
    explicit Packet(PacketType type, int serial);
    ~Packet();

    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;

    Packet(Packet&& other) noexcept;
    Packet& operator=(Packet&& other) noexcept;

    PacketType type() const { return m_type; }
    void setType(PacketType type) { m_type = type; }
    AVPacket* avPacket() const { return m_avPacket; }

    size_t size() const;

    int serial() const { return m_serial; }

private:
    AVPacket* m_avPacket{nullptr};
    PacketType m_type{PacketType::Normal};
    int m_serial{0};
};
using PacketPtr = std::unique_ptr<Packet>;
} // namespace media
