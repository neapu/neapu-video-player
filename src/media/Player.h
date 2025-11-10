//
// Created by liu86 on 2025/10/29.
//

#pragma once
#include "pub.h"
#include "VideoFrame.h"
#include "AudioFrame.h"

namespace media {
class PlayerPrivate;
class Player {
public:

    Player();
    ~Player();

    bool openMedia(const OpenMediaParams& params);
    void closeMedia();

    bool isPause();
    bool isOpen();

    void play();
    void stop();

    // 音频和视频的出帧方式不同，音频由render拉取，视频由解码线程推送
    // 这是因为音频数据的播放需要持续稳定，而视频帧可以丢弃以保持同步
    AudioFramePtr getAudioFrame();

    int audioSampleRate() const;
    int audioChannels() const;

    bool hasAudioStream() const;
    bool hasVideoStream() const;

    int64_t duration() const;

    void seek(int64_t second);

private:
    PlayerPrivate* m_d{nullptr};
};

} // namespace media
