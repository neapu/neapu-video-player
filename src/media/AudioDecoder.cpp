//
// Created by liu86 on 2025/11/15.
//

#include "AudioDecoder.h"
#include <logger.h>

namespace media {
AudioDecoder::AudioDecoder(QObject* parent)
    : Decoder(parent)
{
    NEAPU_FUNC_TRACE;
}
} // namespace media