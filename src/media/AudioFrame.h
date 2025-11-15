//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include "MediaFrame.h"

namespace media {
class AudioFrame : public MediaFrame {
public:
    explicit AudioFrame(void* avFrame);
    ~AudioFrame() override = default;

    virtual const uint8_t* data() const;
    virtual int len() const;
};
using AudioFramePtr = std::unique_ptr<AudioFrame>;
} // namespace media
