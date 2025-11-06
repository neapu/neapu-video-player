//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include <string>

namespace media {

class MediaUtils {
public:
    static std::string getFFmpegError(int errNum);
};

} // namespace media
