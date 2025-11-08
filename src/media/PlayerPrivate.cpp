//
// Created by liu86 on 2025/10/30.
//

#include "PlayerPrivate.h"
#include <logger.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/packet.h>
}
namespace media {
PlayerPrivate::~PlayerPrivate()
{
    NEAPU_FUNC_TRACE;
}
bool PlayerPrivate::openMedia(const OpenMediaParams& params)
{
    NEAPU_FUNC_TRACE;
    if (m_demuxer.isOpen()) {
        NEAPU_LOGE("Media is already open");
        return false;
    }

    if (!m_demuxer.open(params.url)) {
        NEAPU_LOGE("Failed to open media: {}", params.url);
        return false;
    }

    m_openParams = params;

    const auto* audioStream = m_demuxer.audioStream();
    if (audioStream) {
        if (!m_audioDecoder.initialize(audioStream)) {
            NEAPU_LOGE("Failed to initialize audio decoder");
            return false;
        }
        m_audioPost.initialize(audioStream->time_base);
    }

    const auto* videoStream = m_demuxer.videoStream();
    if (videoStream) {
        if (!initVideoDecoder(videoStream)) {
            NEAPU_LOGE("Failed to initialize video decoder");
            return false;
        }
    }

    m_duration = m_demuxer.mediaDuration();

    return true;
}

void PlayerPrivate::closeMedia()
{
    NEAPU_FUNC_TRACE;
    m_audioDecoder.destroy();
    m_videoDecoder.destroy();
    m_audioPost.destroy();
    m_videoPost.destroy();
    m_demuxer.close();
}

bool PlayerPrivate::isPause() const
{
    return m_pause;
}

bool PlayerPrivate::isOpen() const
{
    return m_demuxer.isOpen();
}

void PlayerPrivate::play()
{
    NEAPU_FUNC_TRACE;
    if (m_decodeThread.joinable()) {
        // 逻辑上不允许走这个路径
        NEAPU_LOGE("Decode thread is already running");
        return;
    }

    std::unique_lock lock(m_mutex);
    m_stopFlag = false;
    m_pause = false;
    m_decodeThread = std::thread(&PlayerPrivate::decodeThreadFunc, this);
}

void PlayerPrivate::stop()
{
    NEAPU_FUNC_TRACE;
    std::unique_lock lock(m_mutex);
    m_stopFlag = true;
    m_clock.clear();
    m_audioPost.clear();
    m_videoPost.clear();
    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }
}

VideoFramePtr PlayerPrivate::getVideoFrame()
{
    std::shared_lock lock(m_mutex);
    if (m_stopFlag) {
        return nullptr;
    }
    auto frame = m_videoPost.popVideoFrame();
    if (!m_demuxer.audioStream() && frame && m_duration > 0 && m_openParams.ptsChangedCallback) {
        // 无音频流时，使用视频帧时间点作为播放进度
        m_openParams.ptsChangedCallback(frame->pts());
    }
    return frame;
}

AudioFramePtr PlayerPrivate::getAudioFrame()
{
    std::shared_lock lock(m_mutex);
    if (m_stopFlag) {
        return nullptr;
    }
    auto frame = m_audioPost.popAudioFrame();
    if (frame && m_duration > 0 && m_openParams.ptsChangedCallback) {
        m_openParams.ptsChangedCallback(frame->pts());
    }
    return frame;
}

int PlayerPrivate::audioSampleRate() const
{
    return m_audioDecoder.sampleRate();
}

int PlayerPrivate::audioChannels() const
{
    return m_audioDecoder.channels();
}

bool PlayerPrivate::hasAudioStream() const
{
    return m_demuxer.audioStream() != nullptr;
}

bool PlayerPrivate::hasVideoStream() const
{
    return m_demuxer.videoStream() != nullptr;
}

void PlayerPrivate::seek(int64_t second)
{
    NEAPU_FUNC_TRACE;
    if (!m_demuxer.isOpen()) {
        NEAPU_LOGE("Media is not open");
        return;
    }

    if (second < 0 || (m_duration > 0 && second * 1000000 > m_duration)) {
        NEAPU_LOGE("Seek position out of range: {}", second);
        return;
    }

    std::unique_lock lock(m_mutex);
    m_demuxer.seek(static_cast<double>(second));
    m_audioPost.clear();
    m_videoPost.clear();
    m_clock.clear();
}

void PlayerPrivate::decodeThreadFunc()
{
    NEAPU_FUNC_TRACE;
    while (!m_stopFlag) {
        if (m_pause) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto* packet = m_demuxer.read();
        if (!packet) {
            NEAPU_LOGI("End of stream reached");
            break;
        }

        auto onAudioFrame = [this] (AVFrame* frame) {
            m_audioPost.pushAudioFrame(frame, m_videoPost.waterLevel());
        };

        auto onVideoFrame = [this] (AVFrame* frame) {
            m_videoPost.pushVideoFrame(frame);
        };

        if (packet->stream_index == m_demuxer.audioStreamIndex()) {
            auto ret = m_audioDecoder.decodePacket(packet, onAudioFrame);
            if (!ret) {
                NEAPU_LOGE("Failed to decode audio packet");
            }
        } else if (packet->stream_index == m_demuxer.videoStreamIndex()) {
            auto ret = m_videoDecoder.decodePacket(packet, onVideoFrame);
            if (!ret) {
                NEAPU_LOGE("Failed to decode video packet");
            }
        }

    }

    if (m_openParams.decodeOverCallback) {
        m_openParams.decodeOverCallback();
    }
}

bool PlayerPrivate::initVideoDecoder(const AVStream* stream)
{
    std::vector<HWAccelType> hwAccelTypes;
    if (m_openParams.enableHWAccel) {
#ifdef _WIN32
        // hwAccelTypes.push_back(HWAccelType::D3D12VA);
        hwAccelTypes.push_back(HWAccelType::D3D11VA);
        hwAccelTypes.push_back(HWAccelType::DXVA2);
        hwAccelTypes.push_back(HWAccelType::CUDA);
#endif
    }
    hwAccelTypes.push_back(HWAccelType::None);

    for (auto type : hwAccelTypes) {
        HWAccelConfig config;
        config.type = type;
#ifdef _WIN32
        if (type == HWAccelType::D3D11VA && m_openParams.d3d11Device && m_openParams.d3d11DeviceContext) {
            config.device = m_openParams.d3d11Device;
            config.context = m_openParams.d3d11DeviceContext;
        }
#endif
        if (!m_videoDecoder.initialize(stream, config)) {
            continue;
        }

        // 测试解码
        bool decodeSuccess = false;
        while (auto packet = m_demuxer.read()) {
            if (packet->stream_index != m_demuxer.videoStreamIndex()) {
                continue;
            }
            auto testCallback = [&decodeSuccess] (AVFrame* frame) {
                decodeSuccess = frame != nullptr;
            };
            if (!m_videoDecoder.decodePacket(packet, testCallback)) {
                NEAPU_LOGW("Failed to decode test video packet");
                break;
            }
            if (decodeSuccess) {
                NEAPU_LOGI("Video decoder test decode succeeded");
                break;
            }
        }

        m_demuxer.seekToStart();

        if (!decodeSuccess) {
            NEAPU_LOGW("Video decoder test decode failed, trying next HW acceleration type");
            m_videoDecoder.destroy();
            continue;
        }

        NEAPU_LOGI("Video decoder initialized with HW acceleration type: {}", static_cast<int>(type));
        if (type != HWAccelType::D3D11VA) {
            m_openParams.copyBackRender = true;
        }
#ifdef _WIN32
        if (!m_openParams.d3d11Device) {
            m_openParams.copyBackRender = true;
        }
#endif

        m_videoPost.initialize(av_q2d(stream->avg_frame_rate), m_openParams.copyBackRender, stream->time_base);

        NEAPU_LOGI("Video decoder initialized successfully. copyBackRender={}; fps={}",
                   m_openParams.copyBackRender ? "true" : "false", av_q2d(stream->avg_frame_rate));

        return true;
    }

    return false;
}
} // namespace media