//
// Created by liu86 on 2025/10/30.
//

#include "AudioFrame.h"

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
double AudioFrame::duration() const
{
    if (m_sampleRate == 0 || m_channels == 0) {
        return 0.;
    }
    return static_cast<double>(m_nbSamples) * 1000. / (m_sampleRate * m_channels);
}
} // namespace media