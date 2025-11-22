#pragma once
#include <string>
#include <memory>

typedef struct AVPacket AVPacket;

namespace media {
std::string getFFmpegErrorString(int errNum);

struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const;
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

std::string getAVCodecIDString(int codecId);

std::string getAVPixelFormatString(int pixFmt);
}