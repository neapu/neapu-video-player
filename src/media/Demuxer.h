//
// Created by liu86 on 2025/10/29.
//

#pragma once
#include <string>

typedef struct AVPacket AVPacket;
typedef struct AVStream AVStream;
typedef struct AVFormatContext AVFormatContext;

namespace media {
class Demuxer {
public:
    bool open(const std::string& url);
    void close();

    bool isOpen() const;

    AVPacket* read();

    // 跳转到指定时间，以视频流为基准，将会跳到关键帧，单位为秒
    void seek(double timestamp);
    void seekToStart();

    int videoStreamIndex() const;
    int audioStreamIndex() const;

    AVStream* videoStream() const;
    AVStream* audioStream() const;

private:
    AVFormatContext* m_formatCtx{nullptr};
    AVStream* m_videoStream{nullptr};
    AVStream* m_audioStream{nullptr};
    AVPacket* m_packet{nullptr};
};
} // namespace media
