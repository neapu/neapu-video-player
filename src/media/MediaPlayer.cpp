//
// Created by liu86 on 2025/11/15.
//

#include "MediaPlayer.h"
#include "Demuxer.h"
#include "VideoDecoder.h"
#include "AudioDecoder.h"

namespace media {
MediaPlayer::MediaPlayer(const QSettings* settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_demuxer = new Demuxer(this);
    m_videoDecoder = new VideoDecoder();
    m_audioDecoder = new AudioDecoder();
}
bool MediaPlayer::open(const QString& url)
{
    // TODO: 打开 demuxer、初始化解码器、建立信号/槽、处理错误
    return true;
}

void MediaPlayer::close()
{
    // TODO: 关闭 demuxer、释放/重置解码器、清理播放状态
}

void MediaPlayer::play()
{
    // TODO: 启动播放流程并驱动解复用与解码
}

void MediaPlayer::pause()
{
    // TODO: 暂停播放，挂起解码/渲染
}

void MediaPlayer::stop()
{
    // TODO: 停止播放并复位相关状态
}

MediaFramePtr MediaPlayer::getVideoFrame()
{
    // TODO: 从视频解码器获取下一帧并返回
    return {};
}

MediaFramePtr MediaPlayer::getAudioFrame()
{
    // TODO: 从音频解码器获取下一帧并返回
    return {};
}
} // namespace media