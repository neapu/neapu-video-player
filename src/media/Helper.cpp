//
// Created by liu86 on 2025/11/15.
//

#include "Helper.h"
extern "C"{
#include <libavcodec/avcodec.h>
}

namespace media {
void AVPacketFreeDeleter::operator()(AVPacket* pkt) const
{
    if (pkt) {
        av_packet_free(&pkt);
    }
}
} // namespace media