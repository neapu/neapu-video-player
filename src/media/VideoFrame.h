#pragma once
#include "MediaFrame.h"
#include <memory>

namespace media {
class VideoFrame : public MediaFrame {
public:
    explicit VideoFrame(void* avFrame);
    ~VideoFrame() override = default;
};
using VideoFramePtr = std::unique_ptr<VideoFrame>;
} // namespace media