//
// Created by liu86 on 2025/11/15.
//

#include "Demuxer.h"
#include <logger.h>

namespace media {
Demuxer::Demuxer(QObject* parent)
    : QObject(parent)
{
    NEAPU_FUNC_TRACE;
}
} // namespace media