//
// Created by liu86 on 2025/10/29.
//

#pragma once
#include <string>
#include <atomic>
#include <queue>
#include <thread>

typedef struct AVPacket AVPacket;
typedef struct AVStream AVStream;
typedef struct AVFormatContext AVFormatContext;

namespace media {
struct AVPacketFreeDeleter {
    void operator()(AVPacket* pkt) const;
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketFreeDeleter>;
class Demuxer {
public:
    bool open(const std::string& url);
    void close();

    bool isOpen() const;

    AVPacketPtr read();

    // 跳转到指定时间，以视频流为基准，将会跳到关键帧，单位为秒
    // 返回跳转后的时间戳（微妙），用于校准部分没有带pts的媒体文件
    int64_t seek(int64_t second);
    void seekToStart();

    int videoStreamIndex() const;
    int audioStreamIndex() const;

    AVStream* videoStream() const;
    AVStream* audioStream() const;

    int64_t mediaDuration() const; // 单位：微秒

private:
    AVFormatContext* m_formatCtx{nullptr};
    AVStream* m_videoStream{nullptr};
    AVStream* m_audioStream{nullptr};
};
} // namespace media
