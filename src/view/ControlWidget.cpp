//
// Created by liu86 on 2025/11/7.
//

#include "ControlWidget.h"
#include <QVBoxLayout>
#include <logger.h>

namespace view {
ControlWidget::ControlWidget(PlayerController* playerController, QWidget* parent)
    : QWidget(parent), m_playerController(playerController)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);
    createTimelineLayout(layout);
    createControlLayout(layout);
    setLayout(layout);
    m_timelineSlider->setEnabled(false);

    connect(m_playerController, &PlayerController::durationChanged, this, &ControlWidget::onDurationChanged, Qt::QueuedConnection);
    connect(m_playerController, &PlayerController::positionChanged, this, &ControlWidget::onPlayingTimeChanged, Qt::QueuedConnection);
    connect(m_playerController, &PlayerController::stateChanged, this, &ControlWidget::onPlayStateChanged, Qt::QueuedConnection);
}
ControlWidget::~ControlWidget() {}

void ControlWidget::onDurationChanged(double durationSeconds)
{
    m_duration = durationSeconds;

    int hours = static_cast<int>(m_duration) / 3600;
    int minutes = (static_cast<int>(m_duration) % 3600) / 60;
    int seconds = static_cast<int>(m_duration) % 60;
    m_totalTimeLabel->setText(QString("%1:%2:%3")
                                  .arg(hours, 2, 10, QChar('0'))
                                  .arg(minutes, 2, 10, QChar('0'))
                                  .arg(seconds, 2, 10, QChar('0')));
    m_timelineSlider->setMaximum(static_cast<int>(m_duration*1000));
}

void ControlWidget::onPlayingTimeChanged(double seconds)
{
    m_currentTime = seconds;
    int hours = static_cast<int>(m_currentTime) / 3600;
    int minutes = (static_cast<int>(m_currentTime) % 3600) / 60;
    int secs = static_cast<int>(m_currentTime) % 60;
    m_currentTimeLabel->setText(QString("%1:%2:%3")
                                    .arg(hours, 2, 10, QChar('0'))
                                    .arg(minutes, 2, 10, QChar('0'))
                                    .arg(secs, 2, 10, QChar('0')));
    QMutexLocker locker(&m_mutex);
    if (!m_timelineSliderDragging) {
        m_timelineSlider->setValue(static_cast<int>(m_currentTime*1000));
    }
}
void ControlWidget::onPlayStateChanged(PlayerController::State state)
{
    if (state == PlayerController::State::Playing) {
        m_playPauseButton->setIcon(QIcon(":/svg/pause.svg"));
        QMutexLocker locker(&m_mutex);
        m_timelineSlider->setEnabled(true);
    } else if (state == PlayerController::State::Pause) {
        m_playPauseButton->setIcon(QIcon(":/svg/play.svg"));
        m_timelineSlider->setEnabled(false);
    } else if (state == PlayerController::State::Stopped) {
        NEAPU_LOGD("ControlWidget::onPlayStateChanged: Stopped");
        m_playPauseButton->setIcon(QIcon(":/svg/play.svg"));
        QMutexLocker locker(&m_mutex);
        m_timelineSlider->setValue(0);
        m_currentTimeLabel->setText("00:00:00");
        m_totalTimeLabel->setText("00:00:00");
        m_currentTime = 0;
        m_timelineSlider->setEnabled(false);
    }
}
void ControlWidget::onPlayPauseButtonClicked()
{
    m_playerController->onPauseOrResume();
}
void ControlWidget::onFastForwardButtonClicked()
{
    m_playerController->fastForward();
}
void ControlWidget::onFastRewindButtonClicked()
{
    m_playerController->fastRewind();
}
void ControlWidget::onStopButtonClicked()
{
    m_playerController->onClose();
}

void ControlWidget::createTimelineLayout(QBoxLayout* parentLayout)
{
    auto* timelineLayout = new QHBoxLayout();
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    timelineLayout->setSpacing(3);

    m_currentTimeLabel = new QLabel("00:00:00", this);
    m_timelineSlider = new QSlider(Qt::Horizontal, this);
    m_totalTimeLabel = new QLabel("00:00:00", this);
    timelineLayout->addWidget(m_currentTimeLabel);
    timelineLayout->addWidget(m_timelineSlider, 1);
    timelineLayout->addWidget(m_totalTimeLabel);
    parentLayout->addLayout(timelineLayout);

    connect(m_timelineSlider, &QSlider::sliderMoved, this, &ControlWidget::onTimelineSliderMoved);
    // connect(m_timelineSlider, &QSlider::valueChanged, this, &ControlWidget::onTimelineSliderValueChanged);
    connect(m_timelineSlider, &QSlider::sliderPressed, this, &ControlWidget::onTimelineSliderPressed);
    connect(m_timelineSlider, &QSlider::sliderReleased, this, &ControlWidget::onTimelineSliderReleased);
}

void ControlWidget::createControlLayout(QBoxLayout* parentLayout)
{
    constexpr auto BUTTON_SIZE = 36;
    constexpr auto ICON_SIZE = 24;
    auto* buttonsLayout = new QHBoxLayout();
    buttonsLayout->setContentsMargins(0, 10, 0, 0);
    buttonsLayout->setSpacing(3);

    m_playPauseButton = new QPushButton("", this);
    m_playPauseButton->setIcon(QIcon(":/svg/play.svg"));
    m_playPauseButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_playPauseButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_playPauseButton);
    connect(m_playPauseButton, &QPushButton::clicked, this, &ControlWidget::onPlayPauseButtonClicked);

    m_fastRewindButton = new QPushButton("", this);
    m_fastRewindButton->setIcon(QIcon(":/svg/fast-rewind.svg"));
    m_fastRewindButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_fastRewindButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_fastRewindButton);
    connect(m_fastRewindButton, &QPushButton::clicked, this, &ControlWidget::onFastRewindButtonClicked);

    m_stopButton = new QPushButton("", this);
    m_stopButton->setIcon(QIcon(":/svg/stop.svg"));
    m_stopButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_stopButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_stopButton);
    connect(m_stopButton, &QPushButton::clicked, this, &ControlWidget::onStopButtonClicked);

    m_fastForwardButton = new QPushButton("", this);
    m_fastForwardButton->setIcon(QIcon(":/svg/fast-forward.svg"));
    m_fastForwardButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_fastForwardButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_fastForwardButton);
    connect(m_fastForwardButton, &QPushButton::clicked, this, &ControlWidget::onFastForwardButtonClicked);

    // m_repeatButton = new QPushButton("", this);
    // m_repeatButton->setIcon(QIcon(":/svg/repeat.svg"));
    // m_repeatButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    // m_repeatButton->setIconSize({ICON_SIZE, ICON_SIZE});
    // buttonsLayout->addWidget(m_repeatButton);
    //
    // m_shuffleButton = new QPushButton("", this);
    // m_shuffleButton->setIcon(QIcon(":/svg/shuffle.svg"));
    // m_shuffleButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    // m_shuffleButton->setIconSize({ICON_SIZE, ICON_SIZE});
    // buttonsLayout->addWidget(m_shuffleButton);

    buttonsLayout->addStretch();

    m_muteButton = new QPushButton("", this);
    m_muteButton->setIcon(QIcon(":/svg/volume-up.svg"));
    m_muteButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_muteButton->setIconSize({ICON_SIZE, ICON_SIZE});
    m_muteButton->setFlat(true);
    m_muteButton->setStyleSheet(
        "QPushButton { background-color: transparent; border: none; }"
        "QPushButton:hover { background-color: transparent; }"
        "QPushButton:pressed { background-color: transparent; }"
    );
    buttonsLayout->addWidget(m_muteButton);

    auto* offsetLayout = new QHBoxLayout();
    offsetLayout->setContentsMargins(0, 3, 0, 0);
    offsetLayout->setSpacing(3);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setFixedWidth(100);
    m_volumeSlider->setMaximum(100);
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setValue(100);
    offsetLayout->addWidget(m_volumeSlider);

    m_volumeLabel = new QLabel("100%", this);
    m_volumeLabel->setFixedWidth(40);
    offsetLayout->addWidget(m_volumeLabel);

    buttonsLayout->addLayout(offsetLayout);

    parentLayout->addLayout(buttonsLayout);

    connect(m_volumeSlider, &QSlider::sliderMoved, this, &ControlWidget::onVolumeSliderMoved);
    connect(m_volumeSlider, &QSlider::valueChanged, [](int value) {
        NEAPU_LOGI("Volume changed: {}", value);
    });
}

void ControlWidget::onTimelineSliderMoved(int value)
{
    QMutexLocker locker(&m_mutex);
    double sec = static_cast<double>(value) / 1000.0;
    NEAPU_LOGI("Timeline Slider Moved: {} seconds", sec);
    m_playerController->seek(sec);
}
void ControlWidget::onTimelineSliderPressed()
{
    QMutexLocker locker(&m_mutex);
    m_timelineSliderDragging = true;
    double sec = static_cast<double>(m_timelineSlider->value()) / 1000.0;
    NEAPU_LOGI("Timeline Slider Pressed: {} seconds", sec);
    m_playerController->seek(sec);
}
void ControlWidget::onTimelineSliderReleased()
{
    QMutexLocker locker(&m_mutex);
    m_timelineSliderDragging = false;
}
void ControlWidget::onVolumeSliderMoved(int value)
{
    NEAPU_LOGI("Volume Slider Moved: {}", value);
}
} // namespace view