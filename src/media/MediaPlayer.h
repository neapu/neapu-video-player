//
// Created by liu86 on 2025/11/15.
//

#pragma once

#include "MediaFrame.h"

#include <QObject>
#include <QSettings>

namespace media {
class Demuxer;
class VideoDecoder;
class AudioDecoder;
class MediaPlayer : public QObject {
    Q_OBJECT
public:
    explicit MediaPlayer(const QSettings* settings, QObject *parent = nullptr);

    bool open(const QString& url);
    void close();

    void play();
    void pause();
    void stop();

    MediaFramePtr getVideoFrame();
    MediaFramePtr getAudioFrame();

signals:
    void videoFrameReady();
    // 音频不需要，由renderer控制速度

private:
    const QSettings* m_settings;
    Demuxer* m_demuxer{nullptr};
    VideoDecoder* m_videoDecoder{nullptr};
    AudioDecoder* m_audioDecoder{nullptr};
};

} // namespace media
