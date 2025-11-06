//
// Created by liu86 on 2025/10/29.
//

#include "Player.h"
#include "PlayerPrivate.h"

namespace media {
Player::Player()
{
    m_d = new PlayerPrivate();
}
Player::~Player()
{
    if (m_d) {
        delete m_d;
        m_d = nullptr;
    }
}
bool Player::openMedia(const OpenMediaParams& params)
{
    return m_d->openMedia(params);
}
void Player::closeMedia()
{
    m_d->closeMedia();
}
bool Player::isPause()
{
    return m_d->isPause();
}
bool Player::isOpen()
{
    return m_d->isOpen();
}
void Player::play()
{
    m_d->play();
}
void Player::stop()
{
    m_d->stop();
}
VideoFramePtr Player::getVideoFrame()
{
    return m_d->getVideoFrame();
}
AudioFramePtr Player::getAudioFrame()
{
    return m_d->getAudioFrame();
}
int Player::audioSampleRate() const
{
    return m_d->audioSampleRate();
}
int Player::audioChannels() const
{
    return m_d->audioChannels();
}
} // namespace media