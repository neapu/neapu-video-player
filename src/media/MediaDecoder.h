//
// Created by liu86 on 2025/11/18.
//

#pragma once
#include <memory>
#include <functional>
#include <string>
#include <expected>
#include "Frame.h"

namespace media {

class MediaDecoder {
public:
    struct CreateParam {
        std::string url;
        bool swDecode{false}; // 强制软件解码
#ifdef _WIN32
        void* d3d11Device{nullptr};
#endif
    };
    virtual ~MediaDecoder() = default;

    static auto createMediaDecoder(const CreateParam& param) -> std::expected<std::unique_ptr<MediaDecoder>, std::string>;

    virtual FramePtr getVideoFrame() = 0;
    virtual FramePtr getAudioFrame() = 0;

    virtual bool isEof() = 0;
    virtual void seek(double timePointSeconds) = 0;

    virtual int audioSampleRate() const = 0;
    virtual int audioChannelCount() const = 0;

    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;

    virtual double durationSeconds() const = 0;

};

} // namespace media
