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
constexpr auto MAX_QUEUE_LEN = 100;
constexpr auto MIN_QUEUE_LEN = 0;
PlayerPrivate::~PlayerPrivate()
{
    NEAPU_FUNC_TRACE;
}
bool PlayerPrivate::openMedia(const OpenMediaParams& params)
{
    // UI线程
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
    // UI线程
    NEAPU_FUNC_TRACE;
    m_audioDecoder.destroy();
    m_videoDecoder.destroy();
    m_audioPost.destroy();
    m_videoPost.destroy();
    m_demuxer.close();
}

bool PlayerPrivate::isPause() const
{
    return false;
}

bool PlayerPrivate::isOpen() const
{
    return m_demuxer.isOpen();
}

void PlayerPrivate::play()
{
    // UI线程
    NEAPU_FUNC_TRACE;
    if (m_readThread.joinable()) {
        // 逻辑上不允许走这个路径
        NEAPU_LOGE("Decode thread is already running");
        return;
    }

    m_running = true;
    m_readThread = std::thread(&PlayerPrivate::readThreadFunc, this);
    m_audioDecodeThread = std::thread(&PlayerPrivate::audioDecodeThreadFunc, this);
    m_videoDecodeThread = std::thread(&PlayerPrivate::videoDecodeThreadFunc, this);
}

void PlayerPrivate::stop()
{
    // UI线程
    NEAPU_FUNC_TRACE;
    m_running = false;
    m_audioFrameCondVar.notify_all();
    m_audioPacketCondVar.notify_all();
    m_videoPacketCondVar.notify_all();
    m_clock.clear();
    if (m_readThread.joinable()) {
        m_readThread.join();
    }
    if (m_audioDecodeThread.joinable()) {
        m_audioDecodeThread.join();
    }
    if (m_videoDecodeThread.joinable()) {
        m_videoDecodeThread.join();
    }
}

AudioFramePtr PlayerPrivate::getAudioFrame()
{
    // 音频播放线程
    std::unique_lock frameLock(m_audioFrameMutex);
    while (m_running && !m_audioFrame && !m_audioDecodeOver) {
        // 理论上不用等待
        NEAPU_LOGW("Audio frame is not ready, waiting...");
        m_audioFrameCondVar.wait(frameLock);
    }
    if (!m_running || m_audioDecodeOver) {
        return nullptr;
    }
    if (!m_audioFrame) {
        NEAPU_LOGE("Audio frame is null after wait");
        return nullptr;
    }
    // NEAPU_LOGD("Audio frame fetched. pts={}", m_audioFrame->pts());
    auto frame = std::move(m_audioFrame);
    m_audioFrame.reset();
    m_audioFrameCondVar.notify_one();

    if (m_firstAudioFrame) {
        m_clock.wait(frame->pts());
        m_firstAudioFrame = false;
    }
    m_clock.setAudioPts(frame->pts());

    if (m_openParams.ptsChangedCallback) {
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
    m_seekTargetSecond = second;
    {
        std::lock_guard lock1(m_audioPacketMutex);
        while (!m_audioPacketQueue.empty()) {
            m_audioPacketQueue.pop();
        }
        m_audioPacketCondVar.notify_all();
    }
    {
        std::lock_guard lock2(m_videoPacketMutex);
        while (!m_videoPacketQueue.empty()) {
            m_videoPacketQueue.pop();
        }
        m_videoPacketCondVar.notify_all();
    }


}

void PlayerPrivate::readThreadFunc()
{
    // 媒体读取线程
    NEAPU_FUNC_TRACE;
    m_mediaReadOver = false;
    while (m_running) {
        auto seekTarget = m_seekTargetSecond.load();
        if (seekTarget >= 0) {
            NEAPU_LOGI("Seeking to {} seconds", seekTarget);
            m_afterSeekTimestampOffsetUs = m_demuxer.seek(seekTarget);
            m_seekTargetSecond = -1;
            {
                std::lock_guard lock1(m_audioPacketMutex);
                while (!m_audioPacketQueue.empty()) {
                    m_audioPacketQueue.pop();
                }
            }
            {
                std::lock_guard lock2(m_videoPacketMutex);
                while (!m_videoPacketQueue.empty()) {
                    m_videoPacketQueue.pop();
                }
            }
            m_seekFlagAudio = true;
            m_seekFlagVideo = true;
            m_clock.clear();
        }

        auto packet = m_demuxer.read();
        if (!packet) {
            NEAPU_LOGI("Media read over");
            break;
        }

        auto tb = packet->stream_index == m_demuxer.videoStreamIndex()
                            ? m_demuxer.videoStream()->time_base
                            : m_demuxer.audioStream()->time_base;
        // NEAPU_LOGD("Packet read: stream_index={}, pts={}", packet->stream_index, av_rescale_q(packet->pts, tb, AV_TIME_BASE_Q));


        if (packet->stream_index == m_demuxer.videoStreamIndex()) {
            std::unique_lock lock(m_videoPacketMutex);
            while (m_running
                && m_videoPacketQueue.size() >= MAX_QUEUE_LEN) {
                m_videoPacketCondVar.wait(lock);
            }
            if (!m_running) {
                break;
            }
            m_videoPacketQueue.push(std::move(packet));
            m_videoPacketCondVar.notify_all();
        } else if (packet->stream_index == m_demuxer.audioStreamIndex()) {
            std::unique_lock lock(m_audioPacketMutex);
            while (m_running
                && m_audioPacketQueue.size() >= MAX_QUEUE_LEN) {
                m_audioPacketCondVar.wait(lock);
            }
            if (!m_running) {
                break;
            }
            // NEAPU_LOGD("Pushing audio packet to queue. queue_size={}", m_audioPacketQueue.size());
            m_audioPacketQueue.push(std::move(packet));
            m_audioPacketCondVar.notify_all();
        } else {
            // 字幕/封面等其他流，以后实现
        }
    }
    m_mediaReadOver = true;
    m_audioPacketCondVar.notify_all();
    m_videoPacketCondVar.notify_all();
}

void PlayerPrivate::audioDecodeThreadFunc()
{
    // 音频解码线程
    NEAPU_FUNC_TRACE;
    m_audioDecodeOver = false;
    while (m_running) {
        AVPacketPtr packet;
        {
            std::unique_lock lock(m_audioPacketMutex);
            while (m_running && m_audioPacketQueue.empty() && !m_mediaReadOver) {
                // NEAPU_LOGD("Waiting for audio packet...");
                m_audioPacketCondVar.wait(lock);
            }
            if (!m_running || m_audioPacketQueue.empty()) {
                break;
            }
            packet = std::move(m_audioPacketQueue.front());
            m_audioPacketQueue.pop();
            m_audioPacketCondVar.notify_one();
        }

        if (m_seekFlagAudio) {
            if (packet->pts > 0) { // seek后没有重置pts，证明流自带pts不需要偏移
                m_afterSeekTimestampOffsetUs = 0;
            } else {
                NEAPU_LOGW("Audio packet pts is invalid after seek, applying offset: {} us", m_afterSeekTimestampOffsetUs.load());
            }
            m_audioDecoder.flush();
            m_seekFlagAudio = false;
        }

        // NEAPU_LOGD("pts={}, dts={}", packet->pts, packet->dts);

        // NEAPU_LOGD("Start decoding audio packet. pts={}", packet->pts);
        auto ret = m_audioDecoder.decodePacket(std::move(packet), [this](AVFrame* frame) {
            AudioFramePtr audioFrame = m_audioPost.resampleAudioFrame(frame, m_afterSeekTimestampOffsetUs);
            if (!audioFrame) {
                NEAPU_LOGE("Failed to resample audio frame");
                return;
            }
            // NEAPU_LOGD("Audio pts={}", audioFrame->pts());
            std::unique_lock lock(m_audioFrameMutex);
            while (m_running && m_audioFrame) {
                // NEAPU_LOGD("Waiting for audio frame to be consumed...");
                m_audioFrameCondVar.wait(lock);
            }
            if (!m_running) {
                m_audioFrameCondVar.notify_one();
                return;
            }
            m_audioFrame = std::move(audioFrame);
            m_audioFrameCondVar.notify_one();
        });
        if (!ret) {
            NEAPU_LOGE("Failed to decode audio packet");
        }
    }
    m_audioDecodeOver = true;
    m_audioFrameCondVar.notify_all();
}

void PlayerPrivate::videoDecodeThreadFunc() 
{
    // 视频解码线程
    NEAPU_FUNC_TRACE;
    while (m_running) {
        AVPacketPtr packet;
        {
            std::unique_lock lock(m_videoPacketMutex);
            while (m_running && m_videoPacketQueue.empty() && !m_mediaReadOver) {
                m_videoPacketCondVar.wait(lock);
            }
            if (!m_running || m_videoPacketQueue.empty()) {
                break;
            }
            packet = std::move(m_videoPacketQueue.front());
            m_videoPacketQueue.pop();
            m_videoPacketCondVar.notify_one();
        }

        if (m_seekFlagVideo) {
            m_videoDecoder.flush();
            m_seekFlagVideo = false;
        }

        // NEAPU_LOGD("Start decoding video packet. pts={}, dts={}", packet->pts, packet->dts);
        auto ret = m_videoDecoder.decodePacket(std::move(packet), [this](AVFrame* frame) {
            // 处理解码后的视频帧
            if (!m_openParams.videoFrameCallback) {
                return;
            }
            
            VideoFramePtr videoFrame = m_videoPost.copyFrame(frame);
            if (!videoFrame) {
                NEAPU_LOGE("Failed to copy video frame");
                return;
            }
            // NEAPU_LOGD("Video pts={}", videoFrame->pts());

            if (m_firstVideoFrame) {
                m_firstVideoFrame = false;
            }

            // 视频和音频不同，每一帧都要对齐时间轴
            (void)m_clock.wait(videoFrame->pts());

            if (!m_demuxer.audioStream()) {
                // 无音频流时，通过视频帧更新时间点回调
                if (m_openParams.ptsChangedCallback) {
                    m_openParams.ptsChangedCallback(videoFrame->pts());
                }
            }

            m_openParams.videoFrameCallback(std::move(videoFrame));
        });
        if (!ret) {
            NEAPU_LOGE("Failed to decode video packet");
        }
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
            if (!m_videoDecoder.decodePacket(std::move(packet), testCallback)) {
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