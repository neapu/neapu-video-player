//
// Created by liu86 on 2025/10/30.
//

#include "AudioFrame.h"
extern "C"{
#include <libavutil/avutil.h>
}

namespace media {
AudioFrame::AudioFrame(const uint8_t* data, size_t len, int sampleRate, int channels, int nbSamples, int64_t pts)
    : m_len(len)
    , m_sampleRate(sampleRate)
    , m_channels(channels)
    , m_nbSamples(nbSamples)
    , m_pts(pts)
{
    m_data = new uint8_t[len];
    std::memcpy(m_data, data, len);
}
AudioFrame::~AudioFrame()
{
    if (m_data) {
        delete[] m_data;
        m_data = nullptr;
    }
}
int64_t AudioFrame::duration() const
{
    if (m_sampleRate == 0 || m_channels == 0) {
        return 0;
    }
    // 每帧时长(微秒) = nb_samples / sample_rate * 1_000_000
    return av_rescale_q(m_nbSamples, AVRational{1, m_sampleRate}, AV_TIME_BASE_Q);
}
} // namespace media