//
// Created by liu86 on 2025/11/25.
//

#pragma once
#include <QObject>
#include "VideoRenderer.h"
#include "AudioRenderer.h"

namespace view {

class PlayerController : public QObject {
    Q_OBJECT
public:
    enum class State {
        Stopped,
        Playing,
        Pause
    };
    explicit PlayerController(VideoRenderer* videoRenderer, QObject* parent = nullptr);
    ~PlayerController() override;

    void onOpen();
    void onClose();

    void seek(double seconds);

    State state() const { return m_state; }

signals:
    void durationChanged(double seconds);
    void positionChanged(double seconds);
    void stateChanged(State state);

private:
    void checkEof();

private slots:
    void onAudioEof();
    void onVideoEof();
    void onAudioPts(int64_t ptsUs);
    void onVideoPts(int64_t ptsUs);

private:
    VideoRenderer* m_videoRenderer{nullptr};
    AudioRenderer* m_audioRenderer{nullptr};

    std::atomic_bool m_audioEof{false};
    std::atomic_bool m_videoEof{false};

    int m_serial{0};
    State m_state{State::Stopped};
};

} // namespace view
