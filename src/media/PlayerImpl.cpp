//
// Created by liu86 on 2025/11/25.
//

#include "PlayerImpl.h"
#include <logger.h>
extern "C"{
#include <libavformat/avformat.h>
}

namespace media {
static int64_t getCurrentTimeUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
FramePtr PlayerImpl::getVideoFrame()
{
    if (!m_playing.load()) {
        return nullptr;
    }
    if (!m_videoDecoder) {
        return nullptr;
    }
    for (;;) {
        if (!m_nextVideoFrame) {
            m_nextVideoFrame = m_videoDecoder->getFrame();
        }
        if (!m_nextVideoFrame) {
            return nullptr;
        }

        if (m_nextVideoFrame->type() == Frame::FrameType::EndOfStream) {
            NEAPU_LOGI("Video reached end of stream");
            m_videoEof = true;
            if (m_param.onPlayFinished && (m_audioEof.load() || !m_audioDecoder)) {
                m_param.onPlayFinished();
            }
            m_nextVideoFrame.reset();
            return nullptr;
        }

        // 丢弃过期帧
        if (m_nextVideoFrame->serial() < m_serial) {
            NEAPU_LOGD("Discarding expired video frame with serial {}, current serial is {}",
                m_nextVideoFrame->serial(), m_serial.load());
            m_nextVideoFrame.reset();
            continue;
        }

        if (m_nextVideoFrame->type() == Frame::FrameType::Flush) {
            {
                std::lock_guard<std::mutex> lock(m_seekMutex);
                m_videoSeeking = false;
            }
            if (!m_audioDecoder) m_startTimeUs = 0;
            m_nextVideoFrame.reset();
            continue;
        }
        
        if (m_startTimeUs > 0) {
            // 判断是否到播放时间
            auto expectedPlayTimeUs = getCurrentTimeUs() - m_startTimeUs.load();
            if (m_nextVideoFrame->ptsUs() > expectedPlayTimeUs) {
                // 还没到播放时间，返回空
                return nullptr;
            }
        } else if (!m_audioDecoder) {
            // 无音频时，初始化m_startTimeUs
            m_startTimeUs = getCurrentTimeUs() - m_nextVideoFrame->ptsUs();
        }
        // 到了播放时间，返回该帧
        auto frame = std::move(m_nextVideoFrame);
        if (!m_audioDecoder) {
            m_lastPlayPtsUs = frame->ptsUs();
            if (m_param.onPlayingPtsUs) {
                m_param.onPlayingPtsUs(m_lastPlayPtsUs.load());
            }
        }
        m_nextVideoFrame.reset();
        return frame;
    }
    return nullptr;
}
FramePtr PlayerImpl::getAudioFrame()
{
    if (!m_playing.load()) {
        return nullptr;
    }
    if (!m_audioDecoder) {
        return nullptr;
    }
    for (;;) {
        if (!m_nextAudioFrame) {
            m_nextAudioFrame = m_audioDecoder->getFrame();
        }
        if (!m_nextAudioFrame) {
            return nullptr;
        }
        if (m_nextAudioFrame->type() == Frame::FrameType::EndOfStream) {
            NEAPU_LOGI("Audio reached end of stream");
            m_audioEof = true;
            if (m_param.onPlayFinished && (m_videoEof.load() || !m_videoDecoder)) {
                m_param.onPlayFinished();
            }
            m_nextAudioFrame.reset();
            return nullptr;
        }
        // 丢弃过期帧
        if (m_nextAudioFrame->serial() < m_serial) {
            NEAPU_LOGD("Discarding expired audio frame with serial {}, current serial is {}",
                m_nextAudioFrame->serial(), m_serial.load());
            m_nextAudioFrame.reset();
            continue;
        }
        if (m_nextAudioFrame->type() == Frame::FrameType::Flush) {
            {
                std::lock_guard<std::mutex> lock(m_seekMutex);
                m_audioSeeking = false;
            }
            m_startTimeUs = 0;
            m_nextAudioFrame.reset();
            continue;
        }
        if (m_startTimeUs > 0) {
            // 判断是否到播放时间
            auto expectedPlayTimeUs = getCurrentTimeUs() - m_startTimeUs;
            auto waitDurationUs = m_nextAudioFrame->ptsUs() - expectedPlayTimeUs;
            if (waitDurationUs > m_nextAudioFrame->durationUs()) {
                // 还没到播放时间，返回空
                return nullptr;
            } else if (waitDurationUs < -m_nextAudioFrame->durationUs()) {
                // 已经过了播放时间，丢弃该帧
                NEAPU_LOGD("Discarding expired audio frame with PTS {}, expected play time is {}",
                    m_nextAudioFrame->ptsUs(), expectedPlayTimeUs);
                m_nextAudioFrame.reset();
                continue;
            }
        }
        auto frame = std::move(m_nextAudioFrame);;
        m_nextAudioFrame.reset();
        // 反响校准m_startTimeUs
        m_lastPlayPtsUs = frame->ptsUs();
        m_startTimeUs = getCurrentTimeUs() - m_lastPlayPtsUs.load();
        if (m_param.onPlayingPtsUs) {
            m_param.onPlayingPtsUs(m_lastPlayPtsUs.load());
        }
        return frame;
    }
    return nullptr;
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
    m_serial = 0;
    m_startTimeUs = 0;
    m_playing = false;
    m_lastPlayPtsUs = 0;
    m_nextVideoFrame.reset();
    m_nextAudioFrame.reset();
    m_videoEof = false;
    m_audioEof = false;
    NEAPU_LOGI("Media file closed");
}

void PlayerImpl::seek(double seconds)
{
    if (!m_demuxer) {
        NEAPU_LOGW("Cannot seek, demuxer is not opened");
        return;
    }
    if (!m_playing) {
        NEAPU_LOGW("Cannot seek, player is not playing");
        return;
    }
    if (seconds < 0.0 || seconds > durationSeconds()) {
        NEAPU_LOGW("Seek position {} seconds is out of range", seconds);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_seekMutex);
        if (m_videoSeeking || m_audioSeeking) {
            NEAPU_LOGW("A seek operation is already in progress, ignoring new seek request");
            return;
        }
        if (m_videoDecoder) {
            m_videoSeeking = true;
        }
        if (m_audioDecoder) {
            m_audioSeeking = true;
        }
    }
    m_serial++;
    NEAPU_LOGI("Seeking to {} seconds, serial {}", seconds, m_serial.load());
    m_demuxer->seek(seconds, m_serial.load());
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
            m_demuxer->seek(0, m_serial.load(), true);
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
void PlayerImpl::play() 
{
    m_startTimeUs = getCurrentTimeUs() - m_lastPlayPtsUs.load();
    m_playing = true;
}
void PlayerImpl::pause() 
{
    m_playing = false;
}
} // namespace media