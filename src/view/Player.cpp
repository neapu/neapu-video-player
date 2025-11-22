//
// Created by liu86 on 2025/11/18.
//

#include "Player.h"
#include <QFileDialog>
#include <logger.h>
#include <cassert>

using media::MediaDecoder;

namespace view {
int64_t getCurrentTimestampUS()
{
    using namespace std::chrono;
    const auto now = high_resolution_clock::now();
    const auto nowUs = duration_cast<microseconds>(now.time_since_epoch()).count();
    return nowUs;
}
Player::Player(QObject* parent)
    : QObject(parent)
{
    NEAPU_FUNC_TRACE;
}
Player::~Player()
{
    NEAPU_FUNC_TRACE;
    close();
}
bool Player::isPlaying()
{
    std::shared_lock<std::shared_mutex> lk(m_playMutex);
    return m_playing;
}
void Player::setAudioRenderer(AudioRenderer* renderer)
{
    m_audioRenderer = renderer;
}
void Player::setVideoRenderer(VideoRenderer* renderer)
{
    m_videoRenderer = renderer;
}
void Player::open()
{
    NEAPU_FUNC_TRACE;
    QString filePath = QFileDialog::getOpenFileName(nullptr, tr("Open Video File"), "", tr("Video Files (*.mp4 *.mkv *.avi);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    if (!m_videoRenderer || !m_audioRenderer) {
        NEAPU_LOGE("VideoRenderer or AudioRenderer is null");
        return;
    }

    close();

    MediaDecoder::CreateParam param;
    param.url = filePath.toStdString();
    param.swDecode = true;

    auto result = MediaDecoder::createMediaDecoder(param);
    if (!result) {
        auto err = result.error();
        NEAPU_LOGE("Failed to create media decoder: {}", err);
        return;
    }

    m_mediaDecoder = std::move(result.value());
    m_stopThread = false;
    m_playing = false;
    m_firstAudioFrame = true;
    if (m_mediaDecoder->hasAudio()) {
        m_audioRenderer->start(m_mediaDecoder->audioSampleRate(), m_mediaDecoder->audioChannelCount());
        m_audioThread = std::thread(&Player::AudioThreadFunc, this);
    }

    if (m_mediaDecoder->hasVideo()) {
        m_videoThread = std::thread(&Player::VideoThreadFunc, this);
    }
}
void Player::close()
{
    NEAPU_FUNC_TRACE;
    stop();
    if (m_audioRenderer) {
        m_audioRenderer->stop();
    }
    m_stopThread = true;
    if (m_audioThread.joinable()) {
        m_playCond.notify_all();
        m_audioFrameCondVar.notify_all();
        m_audioThread.join();
    }
    if (m_videoThread.joinable()) {
        m_playCond.notify_all();
        m_videoThread.join();
    }
    m_mediaDecoder.reset();
}
void Player::play() 
{
    NEAPU_FUNC_TRACE;
    if (!m_mediaDecoder) {
        NEAPU_LOGE("MediaDecoder is null in play");
        return;
    }

    if (!isPlaying()) {
        // 考虑两种情况
        // 1.从暂停中恢复，直接赋值m_playing即可
        // 2.已经播放完毕，需要seek到开头，再播放
        if (m_audioStopped && m_videoStopped) {
            m_mediaDecoder->seek(0);
            m_startTimePointUS = 0;
            m_audioStopped = false;
            m_videoStopped = false;
        }
        {
            std::lock_guard<std::shared_mutex> plk(m_playMutex);
            m_playing = true;
        }
        m_playCond.notify_all();


    }
}
void Player::pause()
{
    if (!m_mediaDecoder) {
        NEAPU_LOGE("MediaDecoder is null in play");
        return;
    }

    if (isPlaying()) {
        {
            std::unique_lock lock(m_playMutex);
            m_playing = false;
        }
        m_playCond.notify_all();
    }
}
void Player::stop()
{
    NEAPU_FUNC_TRACE;
    if (!m_mediaDecoder) {
        NEAPU_LOGW("MediaDecoder is null in stop");
        return;
    }
    {
        std::unique_lock lock(m_playMutex);
        m_playing = false;
    }
    m_audioStopped = true;
    m_videoStopped = true;
    m_playCond.notify_all();

}
media::FramePtr Player::getAudioFrame()
{
    std::unique_lock lock(m_audioFrameMutex);
    if (!m_currentAudioFrame) {
        return nullptr;
    }
    auto frame = std::move(m_currentAudioFrame);
    m_currentAudioFrame = nullptr;
    lock.unlock();
    m_audioFrameCondVar.notify_one();
    // 第一帧需要等待pts并设置起始时间点，后续则不需要
    if (m_firstAudioFrame) {
        m_firstAudioFrame = false;
        waitForPts(frame->ptsUs());
    }
    // 根据当前播放的pts校准m_startTimePointUS，这个值会用于计算视频帧的等待时间
    int64_t currentTimestampUS = getCurrentTimestampUS();
    m_startTimePointUS = currentTimestampUS - frame->ptsUs();
    return frame;
}
void Player::AudioThreadFunc()
{
    NEAPU_FUNC_TRACE;
    while (!m_stopThread) {
        if (!isPlaying()) {
            std::unique_lock<std::shared_mutex> plk(m_playMutex);
            m_playCond.wait(plk, [this]() { return m_stopThread.load() || m_playing; });
            continue;
        }

        assert(m_mediaDecoder != nullptr && "MediaDecoder is null in AudioThreadFunc");

        {
            std::unique_lock lock(m_audioFrameMutex);
            if (m_currentAudioFrame != nullptr) {
                m_audioFrameCondVar.wait(lock, [this]() {
                    return m_currentAudioFrame == nullptr || m_stopThread.load() || !m_playing;
                });
            }
        }

        auto nextFrame = m_mediaDecoder->getAudioFrame();
        if (!nextFrame) {
            if (m_mediaDecoder->isEof()) {
                m_audioStopped = true;
                if (m_videoStopped) {
                    {
                        std::lock_guard<std::shared_mutex> plk(m_playMutex);
                        m_playing = false;
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }
        {
            std::lock_guard<std::mutex> lk(m_audioFrameMutex);
            m_currentAudioFrame = std::move(nextFrame);
        }
        // 如果m_startTimePointUS为0，设置起始时间点
        int64_t expectedStart = 0;
        const int64_t desiredStart = getCurrentTimestampUS();
        while (expectedStart == 0 && !m_startTimePointUS.compare_exchange_weak(expectedStart, desiredStart)) {
        }
    }
}
void Player::VideoThreadFunc()
{
    NEAPU_FUNC_TRACE;
    while (!m_stopThread) {
        if (!isPlaying()) {
            std::unique_lock<std::shared_mutex> plk(m_playMutex);
            m_playCond.wait(plk, [this]() { return m_stopThread.load() || m_playing; });
            continue;
        }

        assert(m_mediaDecoder != nullptr && "MediaDecoder is null in VideoThreadFunc");
        auto nextFrame = m_mediaDecoder->getVideoFrame();
        if (!nextFrame) {
            if (m_mediaDecoder->isEof()) {
                m_videoStopped = true;
                if (m_audioStopped) {
                    {
                        std::lock_guard<std::shared_mutex> plk(m_playMutex);
                        m_playing = false;
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }

        // 如果m_startTimePointUS为0，设置起始时间点
        int64_t expectedStart = 0;
        const int64_t desiredStart = getCurrentTimestampUS();
        while (expectedStart == 0 && !m_startTimePointUS.compare_exchange_weak(expectedStart, desiredStart)) {
        }

        // 等待视频帧的pts时间到达
        waitForPts(nextFrame->ptsUs());
        if (m_stopThread || !isPlaying()) {
            continue;
        }

        // 回调视频帧
        if (m_videoRenderer) {
            m_videoRenderer->onFrameReady(std::move(nextFrame));
        }
    }
}
void Player::waitForPts(int64_t ptsUS)
{
    if (m_stopThread || !isPlaying()) {
        return;
    }
    if (m_startTimePointUS.load() == 0) {
        return;
    }
    // 自旋等待，以便m_startTimePointUS更新时可以及时响应
    while (true) {
        int64_t currentTimestampUS = getCurrentTimestampUS();
        int64_t elapsedUS = currentTimestampUS - m_startTimePointUS.load();
        if (elapsedUS >= ptsUS) {
            break;
        }
        if (m_stopThread || !isPlaying()) {
            break;
        }
        // 适当休眠，避免忙等待
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}
#ifdef _WIN32
void Player::setD3D11Device(ID3D11Device* device)
{
    m_d3d11Device = device;
}
#endif
} // namespace view