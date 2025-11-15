//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include <memory>
namespace media {
class MediaFrame {
public:
    enum class FrameType {
        Other,
        Video,
        Audio
    };
    MediaFrame(void* avFrame, FrameType type);
    virtual ~MediaFrame();
    MediaFrame(const MediaFrame&) = delete;
    MediaFrame& operator=(const MediaFrame&) = delete;
    MediaFrame(MediaFrame&& other) noexcept;
    MediaFrame& operator=(MediaFrame&& other) noexcept;

    virtual const uint8_t* data(int index) const;
    virtual int lineSize(int index) const;

protected:
    FrameType m_type{FrameType::Other};
    void* m_avFrame{nullptr};
};
using MediaFramePtr = std::unique_ptr<MediaFrame>;
} // namespace media
