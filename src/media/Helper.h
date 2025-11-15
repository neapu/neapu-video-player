//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include <memory>

typedef struct AVPacket AVPacket;

namespace media {
struct AVPacketFreeDeleter {
    void operator()(AVPacket* pkt) const;
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketFreeDeleter>;
} // namespace media
