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
    void onPauseOrResume();

    void seek(double seconds);

    void fastForward();
    void fastRewind();

    State state() const { return m_state; }

private slots:
    void onStreamEof();
    void onAudioPlayingStateChanged(bool playing);

signals:
    void fileNameChanged(const QString& fileName);
    void durationChanged(double seconds);
    void positionChanged(double seconds);
    void stateChanged(State state);

private:
    VideoRenderer* m_videoRenderer{nullptr};
    AudioRenderer* m_audioRenderer{nullptr};

    State m_state{State::Stopped};
    std::atomic_bool m_streamEof{false};
};

} // namespace view
