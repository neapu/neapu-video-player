//
// Created by liu86 on 2025/11/15.
//

#include "VideoDecoder.h"
#include <logger.h>

namespace media {
VideoDecoder::VideoDecoder(QObject* parent)
    : Decoder(parent)
{
    NEAPU_FUNC_TRACE;
}
} // namespace media