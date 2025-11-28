//
// Created by liu86 on 2025/11/25.
//

#include "PlayerImpl.h"
#include <logger.h>
extern "C"{
#include <libavformat/avformat.h>
}

namespace media {
FramePtr PlayerImpl::getVideoFrame()
{
    if (!m_videoDecoder) {
        return nullptr;
    }
    return m_videoDecoder->getFrame();
}
FramePtr PlayerImpl::getAudioFrame()
{
    if (!m_audioDecoder) {
        return nullptr;
    }
    return m_audioDecoder->getFrame();
}
bool PlayerImpl::open(const OpenParam& param)
{
    close();
    try {
        m_param = param;
        m_demuxer = std::make_unique<Demuxer>(param.url);
        if (m_demuxer->videoStream() &&
            !(m_demuxer->videoStream()->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            createVideoDecoder();
        }
        if (m_demuxer->audioStream()) {
            createAudioDecoder();
        }
        NEAPU_LOGI("Media file opened successfully: {}", param.url);
        return true;
    } catch (const std::exception& e) {
        NEAPU_LOGE("Failed to open media file: {}", e.what());
        m_audioDecoder.reset();
        m_videoDecoder.reset();
        m_demuxer.reset();
        return false;
    }
}
void PlayerImpl::close()
{
    if (m_videoDecoder) {
        m_videoDecoder->stop();
        m_videoDecoder.reset();
    }
    if (m_audioDecoder) {
        m_audioDecoder->stop();
        m_audioDecoder.reset();
    }
    m_demuxer.reset();
    NEAPU_LOGI("Media file closed");
}

void PlayerImpl::seek(double seconds, int serial)
{
    m_param.baseSerial = serial;
    NEAPU_LOGI("Seeking to {} seconds, serial {}", seconds, serial);
    m_demuxer->seek(seconds, serial);
}
bool PlayerImpl::isOpened() const
{
    return m_demuxer != nullptr;
}
bool PlayerImpl::hasVideo() const
{
    return m_videoDecoder != nullptr;
}
bool PlayerImpl::hasAudio() const
{
    return m_audioDecoder != nullptr;
}
double PlayerImpl::fps() const
{
    if (!m_demuxer->videoStream()) {
        return 0;
    }

    const AVRational fr = m_demuxer->videoStream()->r_frame_rate;
    if (fr.den == 0) {
        return 0;
    }
    return static_cast<double>(fr.num) / fr.den;
}
int PlayerImpl::sampleRate() const
{
    if (!m_audioDecoder) {
        return 0;
    }
    return m_audioDecoder->sampleRate();
}
int PlayerImpl::channelCount() const
{
    if (!m_audioDecoder) {
        return 0;
    }
    return m_audioDecoder->channelCount();
}

double PlayerImpl::durationSeconds() const
{
    if (!m_demuxer) {
        return 0.0;
    }
    return m_demuxer->durationSeconds();
}

void PlayerImpl::createVideoDecoder()
{
    std::vector<VideoDecoder::HWAccelMethod> hwaccelMethods;
    using enum VideoDecoder::HWAccelMethod;
    if (!m_param.swDecodeOnly) {
#ifdef _WIN32
        hwaccelMethods.push_back(D3D11VA);
        hwaccelMethods.push_back(DXVA);
#elif defined(__linux__)
        hwaccelMethods.push_back(Vaapi);
#elif defined(__APPLE__)
        hwaccelMethods.push_back(VideoToolBox);
#endif
    }
    hwaccelMethods.push_back(None);

    for (auto method : hwaccelMethods) {
        try {
            auto videoDecoder = std::make_unique<VideoDecoder>(
                m_demuxer->videoStream(),
                [this]() { return m_demuxer->getVideoPacket(); },
                method
#ifdef _WIN32
                , m_param.d3d11Device
#endif
            );

            auto ret = videoDecoder->testDecode();
            m_demuxer->seek(0, m_param.baseSerial, true);
            if (!ret) {
                NEAPU_LOGW("Video decoder test decode failed with method {}", static_cast<int>(method));
                continue;
            }
            m_videoDecoder = std::move(videoDecoder);
            m_videoDecoder->start();
            NEAPU_LOGI("Video decoder created successfully with method {}", static_cast<int>(method));
            return;
        } catch (const std::exception& e) {
            NEAPU_LOGW("Failed to create video decoder with method {}: {}", static_cast<int>(method), e.what());
            continue;
        }
    }
    if (!m_videoDecoder) {
        NEAPU_LOGE("Failed to create video decoder with all methods");
        throw std::runtime_error("Failed to create video decoder");
    }
}
void PlayerImpl::createAudioDecoder()
{
    m_audioDecoder = std::make_unique<AudioDecoder>(
        m_demuxer->audioStream(),
        [this]() { return m_demuxer->getAudioPacket(); });
    m_audioDecoder->start();
    NEAPU_LOGI("Audio decoder created successfully");
}
} // namespace media