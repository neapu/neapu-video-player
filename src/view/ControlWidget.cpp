//
// Created by liu86 on 2025/11/7.
//

#include "ControlWidget.h"
#include <QVBoxLayout>
#include <logger.h>

namespace view {
ControlWidget::ControlWidget(std::unique_ptr<media::Player>& player, QWidget* parent)
    : QWidget(parent), m_player(player)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);
    createTimelineLayout(layout);
    createControlLayout(layout);
    setLayout(layout);
}
ControlWidget::~ControlWidget() {}

void ControlWidget::setDuration(int64_t durationUs)
{
    // 转换为秒
    m_durationSeconds = static_cast<int>(durationUs / 1000000);

    auto hours = m_durationSeconds / 3600;
    auto minutes = (m_durationSeconds % 3600) / 60;
    auto seconds = m_durationSeconds % 60;
    m_totalTimeLabel->setText(QString("%1:%2:%3")
                                  .arg(hours, 2, 10, QChar('0'))
                                  .arg(minutes, 2, 10, QChar('0'))
                                  .arg(seconds, 2, 10, QChar('0')));
    m_timelineSlider->setMaximum(m_durationSeconds);
    NEAPU_LOGI("Set duration: {} seconds", m_durationSeconds);
}

void ControlWidget::setCurrentPts(int64_t currentPtsUs)
{
    m_currentPtsSeconds = static_cast<int>(currentPtsUs / 1000000);

    auto hours = m_currentPtsSeconds / 3600;
    auto minutes = (m_currentPtsSeconds % 3600) / 60;
    auto seconds = m_currentPtsSeconds % 60;
    m_currentTimeLabel->setText(QString("%1:%2:%3")
                                    .arg(hours, 2, 10, QChar('0'))
                                    .arg(minutes, 2, 10, QChar('0'))
                                    .arg(seconds, 2, 10, QChar('0')));
    QMutexLocker locker(&m_mutex);
    if (!m_timelineSliderDragging) {
        m_timelineSlider->setValue(m_currentPtsSeconds);
    }
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

    m_fastRewindButton = new QPushButton("", this);
    m_fastRewindButton->setIcon(QIcon(":/svg/fast-rewind.svg"));
    m_fastRewindButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_fastRewindButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_fastRewindButton);

    m_stopButton = new QPushButton("", this);
    m_stopButton->setIcon(QIcon(":/svg/stop.svg"));
    m_stopButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_stopButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_stopButton);

    m_fastForwardButton = new QPushButton("", this);
    m_fastForwardButton->setIcon(QIcon(":/svg/fast-forward.svg"));
    m_fastForwardButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_fastForwardButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_fastForwardButton);

    m_repeatButton = new QPushButton("", this);
    m_repeatButton->setIcon(QIcon(":/svg/repeat.svg"));
    m_repeatButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_repeatButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_repeatButton);

    m_shuffleButton = new QPushButton("", this);
    m_shuffleButton->setIcon(QIcon(":/svg/shuffle.svg"));
    m_shuffleButton->setFixedSize({BUTTON_SIZE, BUTTON_SIZE});
    m_shuffleButton->setIconSize({ICON_SIZE, ICON_SIZE});
    buttonsLayout->addWidget(m_shuffleButton);

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
    if (m_player) {
        NEAPU_LOGI("Seeking to {} seconds", value);
        m_player->seek(value);
    }
}
void ControlWidget::onTimelineSliderPressed()
{
    QMutexLocker locker(&m_mutex);
    m_timelineSliderDragging = true;
    if (m_player) {
        NEAPU_LOGI("Seeking to {} seconds", m_timelineSlider->value());
        m_player->seek(m_timelineSlider->value());
    }
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