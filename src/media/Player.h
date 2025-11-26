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
        int baseSerial{0};
#ifdef _WIN32
        ID3D11Device* d3d11Device{nullptr};
#endif
    };
    virtual bool open(const OpenParam& param) = 0;
    virtual void close() = 0;

    virtual void seek(double seconds, int serial) = 0;

    virtual bool isOpened() const = 0;

    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;

    virtual double fps() const = 0;
    virtual int sampleRate() const = 0;
    virtual int channelCount() const = 0;

    virtual double durationSeconds() const = 0;

protected:
    Player() = default;
};

} // namespace media
