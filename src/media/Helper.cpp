#include "Helper.h"
extern "C"{
#include <libavutil/error.h>
#include <libavcodec/packet.h>
}

namespace media {
std::string getFFmpegErrorString(int errNum)
{
    char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errNum, errBuf, sizeof(errBuf));
    return std::string(errBuf);
}
std::string getAVCodecIDString(int codecId)
{
    // TODO: implement codec name retrieval
    return "UNKNOWN:" + std::to_string(codecId);
}
std::string getAVPixelFormatString(int pixFmt)
{
    // TODO: implement pixel format name retrieval
    return "UNKNOWN:" + std::to_string(pixFmt);
}
} // namespace media