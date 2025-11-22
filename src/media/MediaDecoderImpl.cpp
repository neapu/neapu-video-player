//
// Created by liu86 on 2025/11/19.
//

#include "MediaDecoderImpl.h"
#include <logger.h>

namespace media {

MediaDecoderImpl::MediaDecoderImpl(const CreateParam& param)
{
    m_demuxer = std::make_unique<Demuxer>(param.url);
    if (m_demuxer->audioStream()) {
        createAudioDecoder(param);
    }

    if (m_demuxer->videoStream()) {
        createVideoDecoder(param);
    }

}

MediaDecoderImpl::~MediaDecoderImpl()
{
}

FramePtr MediaDecoderImpl::getVideoFrame()
{
    if (!m_videoDecoder) {
        return nullptr;
    }
    auto ret = m_videoDecoder->getFrame();
    if (!ret) {
        return nullptr;
    }
    // NEAPU_LOGD("Got video frame, pts: {}", ret.value()->ptsUs());
    return std::move(ret.value());
}

FramePtr MediaDecoderImpl::getAudioFrame()
{
    if (!m_audioDecoder) {
        return nullptr;
    }
    auto ret = m_audioDecoder->getFrame();
    if (!ret) {
        return nullptr;
    }
    // NEAPU_LOGD("Got audio frame, pts: {}", ret.value()->ptsUs());
    return std::move(ret.value());
}

bool MediaDecoderImpl::isEof()
{
    if (!m_demuxer) {
        return true;
    }
    return m_demuxer->isEof();
}

void MediaDecoderImpl::seek(double timePointSeconds)
{
    if (!m_demuxer) {
        return;
    }
    int sec = static_cast<int>(timePointSeconds);
    if (sec < 0) sec = 0;
    m_demuxer->seek(sec);
}

int MediaDecoderImpl::audioSampleRate() const
{
    if (!m_audioDecoder) {
        return 0;
    }
    return m_audioDecoder->sampleRate();
}
int MediaDecoderImpl::audioChannelCount() const
{
    if (!m_audioDecoder) {
        return 0;
    }
    return m_audioDecoder->channelCount();
}

bool MediaDecoderImpl::hasAudio() const
{
    return m_demuxer->audioStream() != nullptr;
}
bool MediaDecoderImpl::hasVideo() const
{
    return m_demuxer->videoStream() != nullptr;
}

void MediaDecoderImpl::createAudioDecoder(const CreateParam& param)
{
    m_audioDecoder = std::make_unique<AudioDecoder>(m_demuxer->audioStream(),
        [this]() {
            return m_demuxer->getAudioPacket();
        });
}
void MediaDecoderImpl::createVideoDecoder(const CreateParam& param)
{
    using enum VideoDecoder::HWAccelMethod;
    std::vector<VideoDecoder::HWAccelMethod> preferredMethods;
    if (!param.swDecode) {
#ifdef _WIN32
        preferredMethods.push_back(D3D11VA);
        preferredMethods.push_back(DXVA);
#elif __linux__
        preferredMethods.push_back(Vaapi);
#elif __APPLE__
        preferredMethods.push_back(VideoToolBox);
#endif
    }
    preferredMethods.push_back(None); // 最后退化到软件解码

    for (auto method : preferredMethods) {
        try {
#ifdef _WIN32
            m_videoDecoder = std::make_unique<VideoDecoder>(m_demuxer->videoStream(),
                [this]() {
                    return m_demuxer->getVideoPacket();
                },
                method,
                static_cast<ID3D11Device*>(param.d3d11Device));
#else
            m_videoDecoder = std::make_unique<VideoDecoder>(m_demuxer->videoStream(),
                [this]() {
                    return m_demuxer->getVideoPacket();
                },
                method);
#endif
            // 测试解码一帧，确保解码器可用
            auto frame = m_videoDecoder->getFrame();
            m_demuxer->seek(0); // 重置回文件开头
            if (!frame) {
                // 正常现象，有些硬件解码器就是能初始化成功但是无法解码
                NEAPU_LOGW("Failed to decode test frame with method {}", static_cast<int>(method));
                m_videoDecoder.reset();
                continue;
            }
            NEAPU_LOGI("Successfully created VideoDecoder with method {}", static_cast<int>(method));
            // 解码器创建成功，退出循环
            break;
        } catch (const std::exception& e) {
            NEAPU_LOGW("Failed to create VideoDecoder with method {}: {}", static_cast<int>(method), e.what());
            m_videoDecoder.reset();
            continue;
        }
    }

    if (!m_videoDecoder) {
        throw std::runtime_error("Failed to create VideoDecoder with any method");
    }
}
} // namespace media