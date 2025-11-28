//
// Created by liu86 on 2025/10/28.
//

#include "AudioRenderer.h"
#include <miniaudio.h>
#include <logger.h>
#include <cmath>
#include "../media/Player.h"

namespace view {
static int64_t getCurrentTimeUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
AudioRenderer::AudioRenderer(QObject* parent)
    : QObject(parent)
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
    m_startTimeUs = getCurrentTimeUs();
    m_running = true;

    return true;
}

void AudioRenderer::stop()
{
    NEAPU_FUNC_TRACE;
    m_running = false;
    if (m_device) {
        ma_device_stop(m_device);
        ma_device_uninit(m_device);
        delete m_device;
        m_device = nullptr;
    }
}
void AudioRenderer::seek(int serial)
{
    m_seeking = true;
    m_serial = serial;
}
void AudioRenderer::maDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount)
{
    const auto channels = static_cast<size_t>(pDevice->playback.channels);
    const auto requireSize = static_cast<size_t>(frameCount) * channels * sizeof(int16_t);
    size_t copyOffset = 0;
    while (copyOffset < requireSize) {
        updateCurrentData();
        auto expectedPlayTimeUs = getCurrentTimeUs() - m_startTimeUs;
        // 本次请求的播放时长
        auto requestDurationUs = static_cast<int64_t>(1e6 * (requireSize - copyOffset) / (channels * sizeof(int16_t) * pDevice->sampleRate));
        if (!m_currentData ||
            m_currentData->ptsUs() > expectedPlayTimeUs + requestDurationUs) {
            const size_t remainSize = requireSize - copyOffset;
            std::memset(static_cast<uint8_t*>(pOutput) + copyOffset, 0, remainSize);
            setPlaying(false);
            break;
        }

        setPlaying(true);
        size_t frameBytes = static_cast<size_t>(m_currentData->nbSamples()) * channels * sizeof(int16_t);
        if (m_offset > frameBytes) m_offset = frameBytes; // 防御式修正
        size_t availableSize = frameBytes - m_offset;
        const size_t toCopy = std::min(availableSize, requireSize - copyOffset);
        std::memcpy(static_cast<uint8_t*>(pOutput) + copyOffset, m_currentData->data(0) + m_offset, toCopy);
        m_offset += toCopy;
        copyOffset += toCopy;
        if (m_offset >= frameBytes) {
            m_currentData.reset();
            m_offset = 0;
        }
    }
}
void AudioRenderer::updateCurrentData()
{
    // if (!m_currentData) {
    //     auto newFrame = media::Player::instance().getAudioFrame();
    //     if (newFrame && newFrame->type() == media::Frame::FrameType::EndOfStream) {
    //         emit eof();
    //         return;
    //     }
    //     while (newFrame && m_serial > newFrame->serial()) {
    //         // 丢弃旧数据，重新开始计时
    //         newFrame = media::Player::instance().getAudioFrame();
    //     }
    //     if (newFrame->type() == media::Frame::FrameType::Flush) {
    //         m_startTimeUs = 0;
    //         newFrame = media::Player::instance().getAudioFrame();
    //         m_seeking = false;
    //     }
    //     if (!newFrame) return;
    //     if (m_startTimeUs > 0) {
    //         auto expectedPlayTimeUs = getCurrentTimeUs() - m_startTimeUs;
    //         while (newFrame && newFrame->ptsUs() > expectedPlayTimeUs + newFrame->durationUs()) {
    //             newFrame = media::Player::instance().getAudioFrame();
    //         }
    //     }
    //     if (!newFrame) return;
    //     // 反向校准
    //     m_startTimeUs = getCurrentTimeUs() - newFrame->ptsUs();
    //     emit playingPts(newFrame->ptsUs());
    //     m_currentData = std::move(newFrame);
    //     m_offset = 0;
    // }
    if (!m_currentData) {
        while (m_running) {
            auto nextFrame = media::Player::instance().getAudioFrame();
            if (!nextFrame) {
                // 不应出现这种情况
                NEAPU_LOGE("Failed to get audio frame from player");
                continue;
            }
            if (nextFrame->type() == media::Frame::FrameType::EndOfStream) {
                NEAPU_LOGI("Audio renderer reached end of stream");
                emit eof();
                return;
            }
            if (m_serial > nextFrame->serial()) {
                NEAPU_LOGD("Discarding old audio frame with serial {}, current serial {}",
                    nextFrame->serial(), m_serial.load());
                continue;
            }
            if (nextFrame->type() == media::Frame::FrameType::Flush) {
                m_startTimeUs = 0;
                m_seeking = false;
                NEAPU_LOGI("Audio renderer received flush frame, resetting start time");
                continue;
            }
            if (m_startTimeUs > 0) {
                auto expectedPlayTimeUs = getCurrentTimeUs() - m_startTimeUs;
                // 如果当前帧的PTS小于预播放时间，并且超过了当前帧的持续时间，表示该帧已经过期，继续取下一帧
                if (nextFrame->ptsUs() + nextFrame->durationUs() < expectedPlayTimeUs) {
                    NEAPU_LOGD("Discarding expired audio frame PTS {} (expected {})",
                        nextFrame->ptsUs(), expectedPlayTimeUs);
                    continue;
                }
                // 如果当前帧的PTS已经超过预期播放时间，表示还没到播放时间，需要等待
                auto sleepNeeded = nextFrame->ptsUs() - expectedPlayTimeUs;
                if (sleepNeeded > 1000000) {
                    // 超过1秒，不正常情况
                    NEAPU_LOGW("Audio frame PTS {} is far in the future (expected {}). Resetting start time.",
                        nextFrame->ptsUs(), expectedPlayTimeUs);
                    sleepNeeded = 1000000;
                }
                if (sleepNeeded > 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(sleepNeeded));
                }
            }
            m_currentData = std::move(nextFrame);
            m_offset = 0;
            // 反向校准开始时间
            m_startTimeUs = getCurrentTimeUs() - m_currentData->ptsUs();
            emit playingPts(m_currentData->ptsUs());
            break;
        }
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