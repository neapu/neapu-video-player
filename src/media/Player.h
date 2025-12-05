//
// Created by liu86 on 2025/11/24.
//

#pragma once
#include "Frame.h"
#include <functional>
#include <string>
#ifdef _WIN32
struct ID3D11Device;
#endif

namespace media {

class Player {
public:
    static Player& instance();
    virtual ~Player() = default;

    virtual FramePtr getVideoFrame() = 0;
    virtual FramePtr getAudioFrame() = 0;

    struct OpenParam {
        std::string url;
        bool swDecodeOnly{false};
        std::function<void(int64_t)> onPlayingPtsUs;
        std::function<void()> onPlayFinished;
        Frame::PixelFormat targetPixelFormat{Frame::PixelFormat::YUV420P};
        Frame::PixelFormat downgradePixelFormat{Frame::PixelFormat::YUV420P};
#ifdef _WIN32
        ID3D11Device* d3d11Device{nullptr};
#endif
    };
    virtual bool open(const OpenParam& param) = 0;
    virtual void close() = 0;

    virtual void seek(double seconds) = 0;

    virtual bool isOpened() const = 0;

    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;

    virtual double fps() const = 0;
    virtual int sampleRate() const = 0;
    virtual int channelCount() const = 0;

    virtual double durationSeconds() const = 0;

    virtual void play() = 0;
    virtual void pause() = 0;

    virtual bool isPlaying() const = 0;

    virtual int64_t lastPlayPtsUs() const = 0;

#ifdef __linux__
    virtual void* vaDisplay() const = 0;
#endif

protected:
    Player() = default;
};

} // namespace media
