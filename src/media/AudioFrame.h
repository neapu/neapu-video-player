//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include <memory>
#include "pub.h"

namespace media {

class AudioFrame {
public:
    AudioFrame(const uint8_t* data, size_t len, int sampleRate, int channels, int nbSamples, int64_t pts);
    ~AudioFrame();
    AudioFrame(const AudioFrame&) = delete;
    AudioFrame& operator=(const AudioFrame&) = delete;
    AudioFrame(AudioFrame&&) = delete;
    AudioFrame& operator=(AudioFrame&&) = delete;

    uint8_t* data() const { return m_data; }
    size_t len() const { return m_len; }
    int sampleRate() const { return m_sampleRate; }
    int channels() const { return m_channels; }
    int nbSamples() const { return m_nbSamples; }

    double duration() const; // 单位：毫秒
    int64_t pts() const { return m_pts; }

private:
    uint8_t* m_data{nullptr};
    size_t m_len{0};
    int m_sampleRate{0};
    int m_channels{0};
    int m_nbSamples{0};

    int64_t m_pts{0};
};
using AudioFramePtr = std::unique_ptr<AudioFrame>;
} // namespace media
