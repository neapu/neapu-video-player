//
// Created by liu86 on 2025/10/28.
//

#include "AudioRenderer.h"
#include <miniaudio.h>
#include <logger.h>

namespace view {
AudioRenderer::AudioRenderer(const AudioFrameCallback& cb, QObject* parent)
    : QObject(parent)
    , m_audioFrameCallback(cb)
{

}
bool AudioRenderer::start(int sampleRate, int channels)
{
    NEAPU_FUNC_TRACE;
    NEAPU_LOGI("Starting audio renderer: sampleRate={}, channels={}", sampleRate, channels);
    if (m_device) {
        NEAPU_LOGW("Audio device is already started");
        return false;
    }
    if (sampleRate <= 0 || channels <= 0) {
        NEAPU_LOGE("Invalid sample rate or channels");
        return false;
    }
    ma_device_config config;
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = static_cast<ma_uint32>(channels);
    config.sampleRate = static_cast<ma_uint32>(sampleRate);
    config.dataCallback = [](ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
        auto* renderer = static_cast<AudioRenderer*>(pDevice->pUserData);
        renderer->maDataCallback(pDevice, pOutput, pInput, frameCount);
    };
    config.pUserData = this;

    m_device = new ma_device;
    if (ma_device_init(nullptr, &config, m_device) != MA_SUCCESS) {
        NEAPU_LOGE("Failed to initialize audio device");
        delete m_device;
        m_device = nullptr;
        return false;
    }

    if (ma_device_start(m_device) != MA_SUCCESS) {
        NEAPU_LOGE("Failed to start audio device");
        ma_device_uninit(m_device);
        delete m_device;
        m_device = nullptr;
        return false;
    }

    return true;
}

void AudioRenderer::stop()
{
    if (m_device) {
        ma_device_stop(m_device);
        ma_device_uninit(m_device);
        delete m_device;
        m_device = nullptr;
    }
}
void AudioRenderer::maDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount)
{
    const auto requireSize = static_cast<size_t>(frameCount) * 2 * sizeof(int16_t); // 2 channels, 16 bits
    size_t copyOffset = 0;
    while (copyOffset < requireSize) {
        updateCurrentData();
        if (!m_currentData) {
            // 数据拉取完毕，填充静音
            const size_t remainSize = requireSize - copyOffset;
            std::memset(static_cast<uint8_t*>(pOutput) + copyOffset, 0, remainSize);
            setPlaying(false);
            break;
        }
        setPlaying(true);
        size_t availableSize = m_currentData->len() - m_offset;
        const size_t toCopy = std::min(availableSize, requireSize - copyOffset);
        std::memcpy(static_cast<uint8_t*>(pOutput) + copyOffset, m_currentData->data() + m_offset, toCopy);
        m_offset += toCopy;
        copyOffset += toCopy;
    }
}
void AudioRenderer::updateCurrentData()
{
    if (!m_audioFrameCallback) return;
    if (m_currentData && m_currentData->len() == m_offset) {
        m_currentData.reset();
    }
    if (!m_currentData) {
        m_currentData = m_audioFrameCallback();
        m_offset = 0;
    }
}

void AudioRenderer::setPlaying(bool playing)
{
    if (m_playing.load() != playing) {
        m_playing = playing;
        emit playingStateChanged(playing);
    }
}

} // namespace view