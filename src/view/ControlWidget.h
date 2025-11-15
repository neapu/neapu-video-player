//
// Created by liu86 on 2025/11/7.
//

#pragma once
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QBoxLayout>
#include <QRecursiveMutex>

namespace view {

class ControlWidget : public QWidget {
    Q_OBJECT
public:
    explicit ControlWidget(QWidget* parent = nullptr);
    ~ControlWidget() override;

    void setDuration(int64_t durationUs);
    void setCurrentPts(int64_t currentPtsUs);

private:
    void createTimelineLayout(QBoxLayout* parentLayout);
    void createControlLayout(QBoxLayout* parentLayout);

    void onTimelineSliderMoved(int value);
    void onTimelineSliderPressed();
    void onTimelineSliderReleased();

    void onVolumeSliderMoved(int value);

private:
    QLabel* m_currentTimeLabel{nullptr};
    QLabel* m_totalTimeLabel{nullptr};
    QSlider* m_timelineSlider{nullptr};

    QPushButton* m_playPauseButton{nullptr};
    QPushButton* m_fastRewindButton{nullptr};
    QPushButton* m_stopButton{nullptr};
    QPushButton* m_fastForwardButton{nullptr};

    QPushButton* m_repeatButton{nullptr};
    QPushButton* m_shuffleButton{nullptr};

    QPushButton* m_muteButton{nullptr};
    QSlider* m_volumeSlider{nullptr};
    QLabel* m_volumeLabel{nullptr};

    int m_durationSeconds{0};
    int m_currentPtsSeconds{0};

    QRecursiveMutex m_mutex;
    bool m_timelineSliderDragging{false};
};

} // namespace view
