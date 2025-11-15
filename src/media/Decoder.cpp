//
// Created by liu86 on 2025/11/15.
//

#include "Decoder.h"
#include <logger.h>

namespace media {
Decoder::Decoder(QObject* parent)
    : QObject(parent)
{
    NEAPU_FUNC_TRACE;
}
} // namespace media