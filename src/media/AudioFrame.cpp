//
// Created by liu86 on 2025/11/15.
//

#include "AudioFrame.h"

namespace media {
AudioFrame::AudioFrame(void* avFrame)
    : MediaFrame(avFrame, FrameType::Audio)
{}
const uint8_t* AudioFrame::data() const
{
    return MediaFrame::data(0);
}
int AudioFrame::len() const
{
    return MediaFrame::lineSize(0);
}

} // namespace media