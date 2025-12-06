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
    param.onPlayingPtsUs = [this](int64_t ptsUs) {
        emit positionChanged(static_cast<double>(ptsUs) / 1e6);
    };
    param.onPlayFinished = [this]() {
        // 切换线程
        QMetaObject::invokeMethod(this, [this]() {
            onStreamEof();
        }, Qt::QueuedConnection);
    };
#ifdef _WIN32
    if (m_videoRenderer->useD3D11()) {
        param.targetPixelFormat = media::Frame::PixelFormat::D3D11Texture2D;
        param.d3d11Device = m_videoRenderer->getD3D11Device();
    } else {
        param.targetPixelFormat = media::Frame::PixelFormat::YUV420P;
    }
    param.downgradePixelFormat = media::Frame::PixelFormat::YUV420P;
    param.swDecodeOnly = false;
#elifdef __linux__
    if (m_videoRenderer->useOpenGL()) {
        param.targetPixelFormat = media::Frame::PixelFormat::Vaapi;
    } else {
        param.targetPixelFormat = media::Frame::PixelFormat::YUV420P;
    }
    param.downgradePixelFormat = media::Frame::PixelFormat::YUV420P;
    param.swDecodeOnly = false;
#elifdef __APPLE__
    param.targetPixelFormat = media::Frame::PixelFormat::YUV420P;
    param.downgradePixelFormat = media::Frame::PixelFormat::YUV420P;
    param.swDecodeOnly = false;
#else
    param.targetPixelFormat = media::Frame::PixelFormat::YUV420P;
    param.downgradePixelFormat = media::Frame::PixelFormat::YUV420P;
    param.swDecodeOnly = true;
#endif

    if (!Player::instance().open(param)) {
        NEAPU_LOGE("Failed to open media file: {}", param.url);
        QMessageBox::critical(nullptr, tr("Error"), tr("Failed to open media file."));
        return;
    }
    auto currentTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (Player::instance().hasVideo()) {
        m_videoRenderer->start(Player::instance().fps(), currentTimeUs);
    }
    if (Player::instance().hasAudio()) {
        m_audioRenderer->start(Player::instance().sampleRate(), Player::instance().channelCount(), currentTimeUs);
    }
    media::Player::instance().play();
    m_state = State::Playing;
    emit stateChanged(m_state);
    emit durationChanged(Player::instance().durationSeconds());
    emit fileNameChanged(QFileInfo(filePath).fileName());
}
void PlayerController::onClose()
{
    m_audioRenderer->stop();
    m_videoRenderer->stop();
    Player::instance().close();
    m_state = State::Stopped;
    emit stateChanged(m_state);
    emit fileNameChanged(QString());
}
void PlayerController::onPauseOrResume()
{
    if (m_state == State::Playing) {
        media::Player::instance().pause();
        m_state = State::Pause;
    } else if (m_state == State::Pause) {
        media::Player::instance().play();
        m_state = State::Playing;
    }
    emit stateChanged(m_state);
}
void PlayerController::seek(double seconds)
{
    if (m_state != State::Playing) {
        return;
    }
    Player::instance().seek(seconds);
}
void PlayerController::fastForward()
{
    if (m_state != State::Playing) {
        return;
    }
    double currentPos = (double)media::Player::instance().lastPlayPtsUs() / 1e6;
    double newPos = currentPos + 15.0; // 快进15秒
    if (newPos > Player::instance().durationSeconds()) {
        newPos = Player::instance().durationSeconds();
    } else if (newPos < 0.0) {
        newPos = 0.0;
    }
    seek(newPos);
}
void PlayerController::fastRewind()
{
    if (m_state != State::Playing) {
        return;
    }
    double currentPos = (double)media::Player::instance().lastPlayPtsUs() / 1e6;
    double newPos = currentPos - 15.0; // 快退15秒
    if (newPos < 0.0) {
        newPos = 0.0;
    }
    seek(newPos);
}

void PlayerController::onStreamEof()
{
    while (m_audioRenderer->isPlaying()) {
        QThread::msleep(1);
    }
    onClose();
}

} // namespace view
