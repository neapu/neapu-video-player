//
// Created by liu86 on 2025/11/25.
//

#include "PlayerController.h"
#include <logger.h>
#include <QFileDialog>
#include <QMessageBox>
#include "../media/Player.h"
using media::Player;
namespace view {
PlayerController::PlayerController(VideoRenderer* videoRenderer, QObject* parent) : QObject(parent), m_videoRenderer(videoRenderer)
{
    m_audioRenderer = new AudioRenderer(this);
    connect(m_audioRenderer, &AudioRenderer::eof, this, &PlayerController::onAudioEof, Qt::QueuedConnection);
    connect(m_videoRenderer, &VideoRenderer::eof, this, &PlayerController::onVideoEof, Qt::QueuedConnection);
    connect(m_audioRenderer, &AudioRenderer::playingPts, m_videoRenderer, &VideoRenderer::onAudioPtsUpdated, Qt::QueuedConnection);
    connect(m_audioRenderer, &AudioRenderer::playingPts, this, &PlayerController::onAudioPts, Qt::QueuedConnection);
    connect(m_videoRenderer, &VideoRenderer::playingPts, this, &PlayerController::onVideoPts, Qt::QueuedConnection);
}
PlayerController::~PlayerController()
{
    m_audioRenderer->stop();
    Player::instance().close();
}
void PlayerController::onOpen()
{
    QString filePath = QFileDialog::getOpenFileName(nullptr, tr("Open Media File"), "", tr("Media Files (*.mp4 *.mkv *.avi *.mov *.flv);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    onClose();

    Player::OpenParam param;
    param.url = filePath.toStdString();
    // param.swDecodeOnly = true;
    param.baseSerial = m_serial;
#ifdef _WIN32
    param.d3d11Device = m_videoRenderer->getD3D11Device();
#endif

    if (!Player::instance().open(param)) {
        NEAPU_LOGE("Failed to open media file: {}", param.url);
        QMessageBox::critical(nullptr, tr("Error"), tr("Failed to open media file."));
        return;
    }
    if (Player::instance().hasVideo()) {
        m_videoEof = false;
        m_videoRenderer->start(Player::instance().fps());
    } else {
        m_videoEof = true;
    }
    if (Player::instance().hasAudio()) {
        m_audioEof = false;
        m_audioRenderer->start(Player::instance().sampleRate(), Player::instance().channelCount());
    } else {
        m_audioEof = true;
    }
    m_state = State::Playing;
    emit stateChanged(m_state);
    emit durationChanged(Player::instance().durationSeconds());
}
void PlayerController::onClose()
{
    m_audioRenderer->stop();
    m_videoRenderer->stop();
    Player::instance().close();
    m_serial = 0;
    m_state = State::Stopped;
    emit stateChanged(m_state);
}
void PlayerController::seek(double seconds)
{
    if (m_videoRenderer->seeking() || m_audioRenderer->seeking()) {
        return;
    }
    m_serial++;
    m_videoRenderer->seek(m_serial);
    m_audioRenderer->seek(m_serial);
    Player::instance().seek(seconds, m_serial);
}
void PlayerController::checkEof()
{
    if (m_audioEof && m_videoEof) {
        NEAPU_LOGI("Playback reached end of file");
        Player::instance().close();
        m_serial = 0;
        m_state = State::Stopped;
        emit stateChanged(m_state);
    }
}
void PlayerController::onAudioEof()
{
    m_audioEof = true;
    m_audioRenderer->stop();
    checkEof();
}
void PlayerController::onVideoEof()
{
    m_videoEof = true;
    m_videoRenderer->stop();
    checkEof();
}

void PlayerController::onAudioPts(int64_t ptsUs)
{
    emit positionChanged(static_cast<double>(ptsUs) / 1e6);
}
void PlayerController::onVideoPts(int64_t ptsUs)
{
    if (!Player::instance().hasAudio()) {
        emit positionChanged(static_cast<double>(ptsUs) / 1e6);
    }
}

} // namespace view
