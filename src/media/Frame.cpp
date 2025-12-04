//
// Created by liu86 on 2025/11/18.
//

#include "Frame.h"
// #include <algorithm>
#include <stdexcept>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/rational.h>
#include <libavutil/hwcontext.h>
}

#ifdef _WIN32
#include <d3d11.h>
#endif

namespace media {

Frame::Frame(FrameType type, int serial) : m_type(type), m_serial(serial)
{
    m_avFrame = av_frame_alloc();
    if (!m_avFrame) {
        throw std::runtime_error("Failed to allocate AVFrame");
    }
}

Frame::~Frame()
{
    // NEAPU_LOGD("Frame::~Frame() m_avFrame={:p}", static_cast<void*>(m_avFrame));
    if (m_avFrame) {
        av_frame_free(&m_avFrame);
        m_avFrame = nullptr;
    }
}

Frame::Frame(Frame&& other) noexcept : m_avFrame(other.m_avFrame)
{
    other.m_avFrame = nullptr;
}

Frame& Frame::operator=(Frame&& other) noexcept
{
    if (this != &other) {
        if (m_avFrame) {
            av_frame_free(&m_avFrame);
        }
        m_avFrame = other.m_avFrame;
        other.m_avFrame = nullptr;
    }
    return *this;
}

void Frame::copyMetaDataFrom(const Frame& other)
{
    if (!m_avFrame || !other.m_avFrame) return;
    m_avFrame->pts = other.m_avFrame->pts;
    m_avFrame->pkt_dts = other.m_avFrame->pkt_dts;
    m_avFrame->duration = other.m_avFrame->duration;
    m_avFrame->time_base = other.m_avFrame->time_base;
    m_avFrame->color_primaries = other.m_avFrame->color_primaries;
    m_avFrame->color_trc = other.m_avFrame->color_trc;
    m_avFrame->colorspace = other.m_avFrame->colorspace;
    m_avFrame->color_range = other.m_avFrame->color_range;
}
const uint8_t* Frame::data(int index) const
{
    if (!m_avFrame || index < 0 || index >= AV_NUM_DATA_POINTERS) return nullptr;
    return m_avFrame->data[index];
}

int Frame::lineSize(int index) const
{
    if (!m_avFrame || index < 0 || index >= AV_NUM_DATA_POINTERS) return 0;
    return m_avFrame->linesize[index];
}

int Frame::width() const
{
    return m_avFrame ? m_avFrame->width : 0;
}

int Frame::height() const
{
    return m_avFrame ? m_avFrame->height : 0;
}

int64_t Frame::ptsUs() const
{
    if (!m_avFrame) return 0;
    int64_t ts = 0;
    if (m_avFrame->pts != AV_NOPTS_VALUE)
        ts = m_avFrame->pts;
    else if (m_avFrame->best_effort_timestamp != AV_NOPTS_VALUE)
        ts = m_avFrame->best_effort_timestamp;
    else
        return 0;
    AVRational tb = m_avFrame->time_base;
    if (tb.num > 0 && tb.den > 0) {
        return av_rescale_q(ts, tb, AVRational{1, 1000000});
    }
    return ts;
}

int64_t Frame::durationUs() const
{
    if (!m_avFrame) return 0;
    int64_t dur = m_avFrame->duration;
    if (dur <= 0) return 0;
    AVRational tb = m_avFrame->time_base;
    if (tb.num > 0 && tb.den > 0) {
        return av_rescale_q(dur, tb, AVRational{1, 1000000});
    }
    return dur;
}

Frame::PixelFormat Frame::pixelFormat() const
{
    if (!m_avFrame) return PixelFormat::None;
    switch (static_cast<AVPixelFormat>(m_avFrame->format)) {
    case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
#ifdef _WIN32
    case AV_PIX_FMT_D3D11: return PixelFormat::D3D11Texture2D;
#endif
    default: return PixelFormat::None;
    }
}

Frame::ColorSpace Frame::colorSpace() const
{
    if (!m_avFrame) return ColorSpace::BT601;
    switch (m_avFrame->colorspace) {
    case AVCOL_SPC_BT709: return ColorSpace::BT709;
    default: return ColorSpace::BT601;
    }
}

Frame::ColorRange Frame::colorRange() const
{
    if (!m_avFrame) return ColorRange::Limited;
    switch (m_avFrame->color_range) {
    case AVCOL_RANGE_JPEG: return ColorRange::Full;
    default: return ColorRange::Limited;
    }
}

uint8_t* Frame::yData() const
{
    return m_avFrame ? m_avFrame->data[0] : nullptr;
}

uint8_t* Frame::uData() const
{
    return m_avFrame ? m_avFrame->data[1] : nullptr;
}

uint8_t* Frame::vData() const
{
    return m_avFrame ? m_avFrame->data[2] : nullptr;
}

int Frame::yLineSize() const
{
    return lineSize(0);
}

int Frame::uLineSize() const
{
    return lineSize(1);
}

int Frame::vLineSize() const
{
    return lineSize(2);
}

#ifdef _WIN32
ID3D11Texture2D* Frame::d3d11Texture2D() const
{
    if (!m_avFrame) return nullptr;
    if (static_cast<AVPixelFormat>(m_avFrame->format) != AV_PIX_FMT_D3D11) return nullptr;
    return reinterpret_cast<ID3D11Texture2D*>(m_avFrame->data[0]);
}

int Frame::subresourceIndex() const
{
    if (!m_avFrame) return 0;
    if (static_cast<AVPixelFormat>(m_avFrame->format) != AV_PIX_FMT_D3D11) return 0;
    return static_cast<int>(reinterpret_cast<uintptr_t>(m_avFrame->data[1]));
}
#endif

int Frame::sampleRate() const
{
    return m_avFrame ? m_avFrame->sample_rate : 0;
}

int Frame::channels() const
{
    if (!m_avFrame) return 0;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return m_avFrame->ch_layout.nb_channels;
#else
    return m_avFrame->channels;
#endif
}

int64_t Frame::nbSamples() const
{
    return m_avFrame ? m_avFrame->nb_samples : 0;
}
AVFrame* Frame::avFrame()
{
    return m_avFrame;
}
Frame::PixelFormat Frame::swFormat()
{
    if (!m_avFrame->hw_frames_ctx) {
        return pixelFormat();
    }

    AVHWFramesContext* hwFramesCtx = reinterpret_cast<AVHWFramesContext*>(m_avFrame->hw_frames_ctx->data);
    enum AVPixelFormat swFmt = hwFramesCtx->sw_format;
    switch (swFmt) {
    case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
    case AV_PIX_FMT_NV12: return PixelFormat::NV12;
    case AV_PIX_FMT_P010: return PixelFormat::P010;
    default: return PixelFormat::None;
    }
}
} // namespace media