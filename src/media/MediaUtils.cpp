//
// Created by liu86 on 2025/10/30.
//

#include "MediaUtils.h"
extern "C"{
#include <libavutil/error.h>
}

namespace media {
std::string MediaUtils::getFFmpegError(int errNum)
{
    char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errNum, errBuf, sizeof(errBuf));
    return std::string(errBuf);
}
} // namespace media