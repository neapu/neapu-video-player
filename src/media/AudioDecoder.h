//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include "DecoderBase.h"

namespace media {

class AudioDecoder :public DecoderBase {
public:
    AudioDecoder() = default;
    ~AudioDecoder() override = default;

    int sampleRate() const;
    int channels() const;
};

} // namespace media
