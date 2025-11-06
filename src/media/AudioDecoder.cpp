//
// Created by liu86 on 2025/10/30.
//

#include "AudioDecoder.h"
extern "C"{
#include <libavformat/avformat.h>
}

namespace media {
bool AudioDecoder::initialize(const AVStream* stream)
{
    if (!DecoderBase::initialize(stream)) {
        return false;
    }

    return true;
}

int AudioDecoder::sampleRate() const
{
    if (m_stream) {
        return m_stream->codecpar->sample_rate;
    }
    return 0;
}

int AudioDecoder::channels() const
{
    if (m_stream) {
        return m_stream->codecpar->ch_layout.nb_channels;
    }
    return 0;
}
} // namespace media