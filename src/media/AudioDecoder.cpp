//
// Created by liu86 on 2025/10/30.
//

#include "AudioDecoder.h"
extern "C"{
#include <libavformat/avformat.h>
}

namespace media {
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