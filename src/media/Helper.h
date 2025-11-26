#pragma once
#include <string>
#include <memory>

typedef struct AVPacket AVPacket;

namespace media {
std::string getFFmpegErrorString(int errNum);

std::string getAVCodecIDString(int codecId);

std::string getAVPixelFormatString(int pixFmt);
}