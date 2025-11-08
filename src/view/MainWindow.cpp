//
// Created by liu86 on 2025/10/28.
//

#include "MainWindow.h"
#include <QMenuBar>
#include "../media/Player.h"
#include "AudioRenderer.h"
#include "VideoRenderer.h"
#include "logger.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include "ControlWidget.h"

namespace view {
MainWindow::MainWindow()
{
    resize(800, 600);
    setMinimumSize(800, 600);

    createMenus();

    m_player = std::make_unique<media::Player>();
    m_audioRenderer = new AudioRenderer([this]() {
        return m_player->getAudioFrame();
    }, this);
    connect(m_audioRenderer, &AudioRenderer::playingStateChanged, this, &MainWindow::onAudioPlayingStateChanged, Qt::QueuedConnection);

    createLayout();
}
MainWindow::~MainWindow()
{
    NEAPU_FUNC_TRACE;
    disconnect(m_audioRenderer, nullptr, this, nullptr);
    if (m_player->isOpen()) {
        m_audioRenderer->stop();
        m_videoRenderer->stop(false);
        m_player->stop();
        m_player->closeMedia();
    }
}
void MainWindow::createMenus()
{
    auto* fineManu = menuBar()->addMenu(tr("&File"));
    auto* openAction = fineManu->addAction(tr("&Open"));
    auto* exitAction = fineManu->addAction(tr("E&xit"));

    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    connect(exitAction, &QAction::triggered, [this]() { close(); });
}
void MainWindow::createLayout()
{
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* layout = new QVBoxLayout(centralWidget);
    m_videoRenderer = new VideoRenderer([this]() {
        return m_player->getVideoFrame();
    }, centralWidget);
    layout->addWidget(m_videoRenderer, 1);
    m_controlWidget = new ControlWidget(m_player, centralWidget);
    m_controlWidget->setFixedHeight(80);
    layout->addWidget(m_controlWidget, 0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}
void MainWindow::onOpenFile()
{
    QString filter = tr(
            "Media Files (*.mp4 *.avi *.mkv *.mov *.webm *.flv *.mp3 *.flac *.aac *.wav *.ogg *.m4a *.opus);;"
            "Video Files (*.mp4 *.avi *.mkv *.mov *.webm *.flv);;"
            "Audio Files (*.mp3 *.flac *.aac *.wav *.ogg *.m4a *.opus);;"
            "All Files (*.*)");
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Media File"), "", filter);
    if (filePath.isEmpty()) {
        return;
    }

    if (m_player->isOpen()) {
        m_player->stop();
        m_videoRenderer->stop(true);
        m_audioRenderer->stop();
        m_player->closeMedia();
    }

    media::OpenMediaParams params;
    params.url = filePath.toStdString();
    params.decodeOverCallback = [this]() {
        m_isDecodeOver = true;
    };
    params.ptsChangedCallback = [this](int64_t currentPts) {
        QMetaObject::invokeMethod(m_controlWidget, [this, currentPts]() {
            m_controlWidget->setCurrentPts(currentPts);
        }, Qt::QueuedConnection);
    };
    params.enableHWAccel = true;
    params.copyBackRender = true;
#ifdef _WIN32
    if (m_videoRenderer->d3d11Device() && m_videoRenderer->d3d11DeviceContext()) {
        params.d3d11Device = m_videoRenderer->d3d11Device();
        params.d3d11DeviceContext = m_videoRenderer->d3d11DeviceContext();
        params.copyBackRender = false;
    }
#endif
    if (!m_player->openMedia(params)) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to open media file."));
        NEAPU_LOGE("Failed to open media file: {}", params.url);
        return;
    }

    m_controlWidget->setDuration(m_player->duration());

    m_isDecodeOver = false;
    if (m_player->hasAudioStream()) {
        m_audioRenderer->start(m_player->audioSampleRate(), m_player->audioChannels());
    }

    if (m_player->hasVideoStream()) {
        m_videoRenderer->start();
    }

    m_player->play();
}
void MainWindow::onAudioPlayingStateChanged(bool playing)
{
    NEAPU_FUNC_TRACE;
    if (playing == false) {
        if (m_isDecodeOver) {
            m_videoRenderer->stop(true);
            m_audioRenderer->stop();
            m_controlWidget->setCurrentPts(0);
            NEAPU_LOGI("Playback finished, renderers stopped");
        }
    }
}
} // namespace view