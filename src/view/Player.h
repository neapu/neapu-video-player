//
// Created by liu86 on 2025/11/18.
//

#pragma once
#include <QObject>
#include <memory>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include "../media/MediaDecoder.h"
#include "../media/Frame.h"
#include "AudioRenderer.h"
#include "VideoRenderer.h"

#ifdef _WIN32
#include <d3d11.h>
#endif

namespace view {

class Player : public QObject {
    Q_OBJECT
public:
    explicit Player(QObject* parent = nullptr);
    ~Player() override;
    void setAudioRenderer(AudioRenderer* renderer);
    void setVideoRenderer(VideoRenderer* renderer);

    void open();
    void close();

    void play();
    void pause();
    void stop();

    media::FramePtr getAudioFrame();

    bool isOpen() const { return m_mediaDecoder != nullptr; }

#ifdef _WIN32
    void setD3D11Device(ID3D11Device* device);
#endif

private:
    void AudioThreadFunc();
    void VideoThreadFunc();

    void waitForPts(int64_t ptsUS);
    bool isPlaying();

private:
    std::unique_ptr<media::MediaDecoder> m_mediaDecoder{nullptr};
    bool m_playing{false};
    std::shared_mutex m_playMutex;
    std::condition_variable_any m_playCond;

    media::FramePtr m_currentAudioFrame{nullptr};
    std::mutex m_audioFrameMutex;
    std::condition_variable m_audioFrameCondVar;
    bool m_firstAudioFrame{false};
    std::atomic<int64_t> m_startTimePointUS{0};
    std::thread m_audioThread;
    std::thread m_videoThread;

    std::atomic_bool m_stopThread{true};

    std::atomic_bool m_videoStopped{false};
    std::atomic_bool m_audioStopped{false};

    AudioRenderer* m_audioRenderer{nullptr};
    VideoRenderer* m_videoRenderer{nullptr};

#ifdef _WIN32
    ID3D11Device* m_d3d11Device{nullptr};
#endif
};

} // namespace view
