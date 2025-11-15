#include "VideoFrame.h"

namespace media {
VideoFrame::VideoFrame(void* avFrame)
    : MediaFrame(avFrame, FrameType::Video)
{}
} // namespace media