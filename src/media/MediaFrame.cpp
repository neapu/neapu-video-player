//
// Created by liu86 on 2025/11/15.
//

#include "MediaFrame.h"
extern "C"{
#include <libavutil/frame.h>
}

#define GET_AV_FRAME() static_cast<AVFrame*>(m_avFrame)

namespace media {
MediaFrame::MediaFrame(void* avFrame, FrameType type)
    : m_type(type)
    , m_avFrame(avFrame)
{

}
MediaFrame::~MediaFrame()
{
    if (m_avFrame) {
        av_frame_free(reinterpret_cast<AVFrame**>(&m_avFrame));
    }
}
MediaFrame::MediaFrame(MediaFrame&& other) noexcept
    : m_type(other.m_type)
    , m_avFrame(other.m_avFrame)
{
    other.m_avFrame = nullptr;
}
MediaFrame& MediaFrame::operator=(MediaFrame&& other) noexcept
{
    if (this != &other) {
        if (m_avFrame) {
            av_frame_free(reinterpret_cast<AVFrame**>(&m_avFrame));
        }
        m_type = other.m_type;
        m_avFrame = other.m_avFrame;
        other.m_avFrame = nullptr;
    }
    return *this;
}

const uint8_t* MediaFrame::data(int index) const
{
    return GET_AV_FRAME()->data[index];
}

int MediaFrame::lineSize(int index) const
{
    return GET_AV_FRAME()->linesize[index];
}
} // namespace media